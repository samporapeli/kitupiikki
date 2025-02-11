/*
   Copyright (C) 2019 Arto Hyvättinen

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include "tilioteapuri.h"
#include "ui_tilioteapuri.h"

#include "tiliotekirjaaja.h"
#include "model/tosite.h"
#include "model/tositeviennit.h"

#include <QDate>
#include <QSortFilterProxyModel>

#include "kirjaus/kirjauswg.h"

#include "tiliotekirjaaja.h"
#include "db/kirjanpito.h"

#include "tiliotemodel.h"

#include "lisaikkuna.h"
#include "kirjaus/kirjaussivu.h"
#include "kirjaus/kirjauswg.h"

#include <QDebug>
#include <QSettings>
#include <QTimer>


TilioteApuri::TilioteApuri(QWidget *parent, Tosite *tosite)
    : ApuriWidget (parent,tosite),
      ui( new Ui::TilioteApuri),
      model_(new TilioteModel(this, kp())),
      kirjaaja_(new TilioteKirjaaja(this))
{
    ui->setupUi(this);

    proxy_ = new QSortFilterProxyModel(this);
    proxy_->setSourceModel( model_ );

    ui->oteView->setModel(proxy_);
    proxy_->setSortRole(TilioteRivi::LajitteluRooli);
    proxy_->setFilterRole(TilioteRivi::TilaRooli);

    ui->tiliCombo->suodataTyypilla("ARP");
    laitaPaivat( tosite->data(Tosite::PVM).toDate() );

    connect( ui->lisaaRiviNappi, &QPushButton::clicked, this, [this]  {this->lisaaRivi(true);} );
    connect( ui->lisaaTyhjaBtn, &QPushButton::clicked, this, [this] {this->lisaaRivi(false);} );

    connect( ui->oteView->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
             this, SLOT(riviValittu()));
    connect(ui->muokkaaNappi, &QPushButton::clicked, this, &TilioteApuri::muokkaa);
    connect(ui->poistaNappi, &QPushButton::clicked, this, &TilioteApuri::poista);
    connect(ui->tositeNappi, &QPushButton::clicked, this, &TilioteApuri::naytaTosite);

    connect( model_, &TilioteModel::dataChanged, this, &TilioteApuri::tositteelle);
    connect( model_, &TilioteModel::rowsInserted, this, &TilioteApuri::tositteelle);
    connect( model_, &TilioteModel::rowsRemoved, this, &TilioteApuri::tositteelle);
    connect( model_, &TilioteModel::modelReset, this, &TilioteApuri::tositteelle);

    connect( ui->alkuDate, &KpDateEdit::dateChanged, this, &TilioteApuri::tiliPvmMuutos);
    connect( ui->loppuDate, &KpDateEdit::dateChanged, this, &TilioteApuri::tiliPvmMuutos);

    connect( tosite, &Tosite::pvmMuuttui, this, &TilioteApuri::laitaPaivat);

    connect( ui->harmaaNappi, &QPushButton::toggled, this, &TilioteApuri::naytaHarmaat);
    ui->harmaaNappi->setChecked(!kp()->settings()->value("TiliotePiilotaHarmaat",false).toBool());

    connect( ui->tiliCombo, &TiliCombo::currentTextChanged, this, &TilioteApuri::kysyAlkusumma);
    connect( ui->tiliCombo, &TiliCombo::currentTextChanged, this, &TilioteApuri::teeTositteelle);
    connect( ui->tiliCombo, &TiliCombo::currentTextChanged, this, &TilioteApuri::tiliPvmMuutos);

    connect( ui->oteView, &QTableView::doubleClicked, this, &TilioteApuri::muokkaa);
    connect( kirjaaja_, &TilioteKirjaaja::rejected, this, &TilioteApuri::tositteelle);

    QTimer::singleShot(100, this, &TilioteApuri::lataaHarmaat);

}

TilioteApuri::~TilioteApuri()
{
    delete ui;
}

void TilioteApuri::tuo(QVariantMap map)
{
    tuodaan_ = true;

    qDebug() << map;

    const QDate alku = map.value("alkupvm").toDate();
    const QDate loppu = map.value("loppupvm").toDate();

    if( alku.isValid())
        ui->alkuDate->setDate(alku);
    if( loppu.isValid())
        ui->loppuDate->setDate(loppu);

    if( map.contains("iban")) {
        QString iban = map.value("iban").toString();
        Tili tili = kp()->tilit()->tiliIbanilla(iban);
        if( tili.onko(TiliLaji::PANKKITILI))
            ui->tiliCombo->valitseTili(tili.numero());
    } else if( map.contains("tili"))
        ui->tiliCombo->valitseTili( map.value("tili").toInt());

    model()->asetaTilinumero(ui->tiliCombo->valittuTilinumero());
    model()->tuo( map.value("tapahtumat").toList() );


    if( map.contains("kausitunnus")) {
        if( map.value("iban").toString().length() > 8) {
            QString tilinimi = kp()->tilit()->tiliIbanilla(map.value("iban").toString()).nimi();
            tosite()->asetaOtsikko(tr("Tiliote %1 %2").arg(map.value("kausitunnus").toString(), tilinimi));
        }
    }

    if( alku.isValid() && loppu.isValid())
        lataaHarmaatAjalta(alku, loppu);

    tuodaan_ = false;
}

QDate TilioteApuri::tiliotteenAlkupaiva() const
{
    return ui->alkuDate->date();
}

QDate TilioteApuri::tiliotteenLoppupaiva() const
{
    return ui->loppuDate->date();
}

void TilioteApuri::salliMuokkaus(bool sallitaanko)
{
    for( QObject* object : children()) {
        QWidget* widget = qobject_cast<QWidget*>(object);
        if(widget && widget != ui->oteView)
          widget->setEnabled(sallitaanko);
    }
    model_->salliMuokkaus(sallitaanko);
    riviValittu();
}

bool TilioteApuri::teeTositteelle()
{
    if( resetoidaanko())
        return false;

    model()->asetaTilinumero(ui->tiliCombo->valittuTilinumero());
    tosite()->viennit()->asetaViennit( model_->viennit() );
    QVariantMap tilioteMap;
    tilioteMap.insert("alkupvm", ui->alkuDate->date());
    tilioteMap.insert("loppupvm", ui->loppuDate->date());
    tilioteMap.insert("tili", ui->tiliCombo->valittuTilinumero());
    tosite()->setData(Tosite::TILIOTE,tilioteMap);

    tosite()->asetaLaskupvm(QDate());
    tosite()->asetaKumppani(QVariantMap());
    tosite()->asetaTilioterivi(0);

    if( tosite()->data(Tosite::OTSIKKO).toString().isEmpty())
        tosite()->setData( Tosite::OTSIKKO,
                       tr("Tiliote %1 - %2 %3")
                       .arg( ui->alkuDate->date().toString("dd.MM.yyyy") , ui->loppuDate->date().toString("dd.MM.yyyy"), kp()->tilit()->tiliNumerolla(ui->tiliCombo->valittuTilinumero()).nimi()));

    naytaSummat();    
    return true;
}

void TilioteApuri::teeReset()
{
    kirjaaja_->close();
    const auto& viennit = tosite()->viennit()->viennit();
    if( viennit.count() > 1) {
        TositeVienti ekarivi = viennit.first();

        if( kp()->tilit()->tiliNumerolla(ekarivi.tili()).onko(TiliLaji::PANKKITILI)) {
            model_->asetaTilinumero(ekarivi.tili());
            ui->tiliCombo->valitseTili(ekarivi.tili());
        }
    }
    QVariantMap tilioteMap = tosite()->data(Tosite::TILIOTE).toMap();
    if( !tilioteMap.isEmpty()) {
        ui->tiliCombo->valitseTili( tilioteMap.value("tili").toInt() );
        ui->alkuDate->setDate( tilioteMap.value("alkupvm").toDate());
        ui->loppuDate->setDate( tilioteMap.value("loppupvm").toDate() );
    } else {
        ui->loppuDate->setDate(tosite()->pvm());
        ui->alkuDate->setDate( tosite()->pvm().addDays(1).addMonths(-1) );
    }

    model_->asetaTilinumero( ui->tiliCombo->valittuTilinumero() );
    model_->lataa(tosite()->viennit()->tallennettavat() );
    if( kp()->yhteysModel() && !tilioteMap.isEmpty())
        lataaHarmaatAjalta( tilioteMap.value("alkupvm").toDate(), tilioteMap.value("loppupvm").toDate() );
}

void TilioteApuri::lisaaRivi(bool dialogi)
{
    const QModelIndex& index = ui->oteView->currentIndex();
    QDate pvm = index.isValid()
            ? index.sibling(index.row(), TilioteRivi::PVM).data(Qt::EditRole).toDate()
            : ui->alkuDate->date();

    if(dialogi)
        kirjaaja_->kirjaaUusia(pvm);
    else
        model()->lisaaRivi(pvm);        
}

void TilioteApuri::riviValittu()
{
    const QModelIndex& index = ui->oteView->currentIndex();
    bool muokattavaRivi = index.data(TilioteRivi::TilaRooli).toString() == "AA" ;
    ui->muokkaaNappi->setEnabled( muokattavaRivi );
    ui->poistaNappi->setEnabled( muokattavaRivi );
    ui->tositeNappi->setEnabled( index.isValid() &&
                                 !index.sibling(index.row(), TilioteKirjausRivi::EURO).data(Qt::DisplayRole).toString().isEmpty());
}

void TilioteApuri::muokkaa()
{
    const QModelIndex& index = ui->oteView->selectionModel()->currentIndex();
    if(index.data(TilioteRivi::TositeIdRooli).toInt()) {
        LisaIkkuna* ikkuna = new LisaIkkuna;
        ikkuna->naytaTosite(index.data(TilioteRivi::TositeIdRooli).toInt());
    } else {
        kirjaaja_->show();
        kirjaaja_->muokkaaRivia( proxy_->mapToSource( index ).row()  );
    }
}

void TilioteApuri::poista()
{
    model_->poistaRivi( proxy_->mapToSource( ui->oteView->currentIndex()).row() );
}

void TilioteApuri::naytaSummat()
{
    QPair<qlonglong, qlonglong> muutos = model()->summat();
    qlonglong panot = muutos.first;
    qlonglong otot = muutos.second;

    double loppusaldo = alkusaldo_ + (panot - otot) / 100.0;

    ui->infoLabel->setText(tr("Alkusaldo %L3 € \tPanot %L1 € \tOtot %L2 € \tLoppusaldo %L4 €")
                           .arg((panot / 100.0), 0, 'f', 2)
                           .arg((otot / 100.0), 0, 'f', 2)
                           .arg(alkusaldo_,0,'f',2)
                           .arg(loppusaldo,0,'f',2));
}

void TilioteApuri::naytaTosite()
{

    qDebug() << "naytaTosite \n";

    const QModelIndex& index = ui->oteView->selectionModel()->currentIndex();
    if( !index.isValid())
        return;

    LisaIkkuna* ikkuna = new LisaIkkuna;
    if(index.data(TilioteRivi::TositeIdRooli).toInt()) {
        ikkuna->naytaTosite(index.data(TilioteRivi::TositeIdRooli).toInt());
    } else {
        const int omaIndeksi = proxy_->mapToSource(index).row();
        const TilioteKirjausRivi& rivi = model()->rivi(omaIndeksi);

        qDebug() << " i "  << omaIndeksi << " rivi " << rivi.pankkivienti().selite() << "\n";

        KirjausSivu* sivu = ikkuna->kirjaa(-1, TositeTyyppi::MUU);

        const TositeVienti& pankki = rivi.pankkivienti();

        int tositetyyppi = TositeTyyppi::MENO;
        if( pankki.tyyppi() == TositeVienti::VASTAKIRJAUS && pankki.debetEuro() ) {
            tositetyyppi = pankki.debetEuro() ? TositeTyyppi::TULO : TositeTyyppi::MENO;
        } else if( pankki.tyyppi() == TositeVienti::VASTAKIRJAUS + TositeVienti::MYYNTI) {
            tositetyyppi = TositeTyyppi::TULO;
        } else if ( pankki.tyyppi() == TositeVienti::VASTAKIRJAUS + TositeVienti::SIIRTO ||
                        pankki.tyyppi() == TositeVienti::VASTAKIRJAUS + TositeVienti::SUORITUS) {
            tositetyyppi = TositeTyyppi::SIIRTO;
        }

        Tosite tosite;
        tosite.asetaPvm(pankki.pvm());
        tosite.asetaKumppani(pankki.kumppaniMap());
        tosite.asetaOtsikko(pankki.selite());
        tosite.asetaViite(pankki.viite());
        tosite.asetaTilioterivi( omaIndeksi );
        tosite.asetaTyyppi( tositetyyppi );

        QList<TositeVienti> viennit = rivi.viennit();
        QVariantList ladattavat;

        for(auto vienti : qAsConst(viennit)) {
            vienti.setTyyppi( tositetyyppi + vienti.tyyppi() % 100 );
            vienti.setId(0);
            if(!vienti.tili() && tositetyyppi == TositeTyyppi::MENO) {
                vienti.setTili( kp()->asetukset()->luku(AsetusModel::OletusMenotili) );
            } else if( !vienti.tili() && tositetyyppi == TositeTyyppi::TULO) {
                vienti.setTili( kp()->asetukset()->luku(AsetusModel::OletusMyyntitili));
            }
            ladattavat << vienti;
        }
        tosite.viennit()->asetaViennit( ladattavat );

        qDebug() << " LATAA " << ladattavat;

        sivu->kirjausWg()->tosite()->lataa(tosite.tallennettava());
        connect( sivu->kirjausWg()->tosite(), &Tosite::talletettu, this, &TilioteApuri::lataaHarmaat);
    }
    qDebug() << " naytaTositeLoppu ";
}


void TilioteApuri::tiliPvmMuutos()
{
    if( tosite()->resetoidaanko() || paivanlaitto_ )
        return;

    // Otsikon päivittäminen
    if( !tuodaan_)
        lataaHarmaat();

    Tili tili = kp()->tilit()->tiliNumerolla( ui->tiliCombo->valittuTilinumero() );    

    QString otsikko = tosite()->otsikko();
    if(( otsikko.isEmpty() || otsikko.contains(QRegularExpression(R"(\d{2}.\d{2}.\d{4} - \d{2}.\d{2}.\d{4})"))) &&
            ui->alkuDate->date().isValid() && ui->loppuDate->date().isValid()) {
        tosite()->setData( Tosite::OTSIKKO,
                       tr("Tiliote %1 - %2 %3")
                       .arg( ui->alkuDate->date().toString("dd.MM.yyyy"), ui->loppuDate->date().toString("dd.MM.yyyy"), tili.tyyppiKoodi() == "ARP" ? tili.nimi() : ""));
    }

    tosite()->setData(Tosite::PVM, ui->loppuDate->date());
}

void TilioteApuri::lataaHarmaat()
{
    lataaHarmaatAjalta( ui->alkuDate->date(), ui->loppuDate->date());
}

void TilioteApuri::lataaHarmaatAjalta(const QDate &mista, const QDate &mihin)
{    
    model_->asetaTositeId(tosite()->id());
    model_->lataaHarmaat( mista, mihin);
    kysyAlkusumma();
    proxy_->sort(TilioteRivi::PVM);
}

void TilioteApuri::laitaPaivat(const QDate &pvm)
{
    if( pvm != ui->loppuDate->date() && pvm.isValid()) {
        paivanlaitto_ = true;
        ui->loppuDate->setDate(pvm);
        ui->alkuDate->setDate( pvm.addDays(1).addMonths(-1) );
        paivanlaitto_ = false;
        tiliPvmMuutos();
    }
}

void TilioteApuri::kysyAlkusumma()
{
    int tilinumero = ui->tiliCombo->valittuTilinumero();
    QDate alkupvm = ui->alkuDate->date();
    KpKysely *kysely = kpk("/saldot");
    if( kysely ) {
        kysely->lisaaAttribuutti("tili", tilinumero);
        kysely->lisaaAttribuutti("pvm", alkupvm);
        kysely->lisaaAttribuutti("tase");
        kysely->lisaaAttribuutti("alkusaldot");
        connect(kysely, &KpKysely::vastaus, this, &TilioteApuri::alkusummaSaapuu);
        kysely->kysy();
    }
}

void TilioteApuri::alkusummaSaapuu(QVariant* data)
{
    QVariantMap map = data->toMap();
    int tilinumero = ui->tiliCombo->valittuTilinumero();
    alkusaldo_ = map.value(QString::number(tilinumero)).toDouble();
    naytaSummat();
}

void TilioteApuri::naytaHarmaat(bool nayta)
{
    proxy_->setFilterFixedString( nayta ? "A" : "AA");
    kp()->settings()->setValue("'TiliotePiilotaHarmaat", !nayta);
}


