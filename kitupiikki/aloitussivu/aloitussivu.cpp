/*
   Copyright (C) 2017 Arto Hyvättinen

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

#include <QFile>
#include <QStringList>
#include <QSqlQuery>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSettings>
#include <QFileDialog>
#include <QDesktopServices>
#include <QListWidget>

#include <QRegularExpression>

#include <QDialog>
#include <QDebug>

#include "ui_aboutdialog.h"

#include "aloitussivu.h"
#include "db/kirjanpito.h"
#include "uusikp/uusikirjanpito.h"
#include "maaritys/alvmaaritys.h"

AloitusSivu::AloitusSivu() :
    KitupiikkiSivu(0)
{

    ui = new Ui::Aloitus;
    ui->setupUi(this);

    ui->selain->setOpenLinks(false);

    connect( ui->uusiNappi, SIGNAL(clicked(bool)), this, SLOT(uusiTietokanta()));
    connect( ui->avaaNappi, SIGNAL(clicked(bool)), this, SLOT(avaaTietokanta()));
    connect( ui->tietojaNappi, SIGNAL(clicked(bool)), this, SLOT(abouttiarallaa()));
    connect( ui->viimeiset, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(viimeisinTietokanta(QListWidgetItem*)));
    connect( ui->tilikausiCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(siirrySivulle()));

    connect( ui->selain, SIGNAL(anchorClicked(QUrl)), this, SLOT(linkki(QUrl)));

    connect( kp(), SIGNAL(tietokantaVaihtui()), this, SLOT(kirjanpitoVaihtui()));

    lisaaViimetiedostot();

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    connect( manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(infoSaapui(QNetworkReply*)));
    QNetworkRequest req( QUrl("https://raw.githubusercontent.com/artoh/kitupiikki/master/updateinfo"));
    manager->get(req);

}

AloitusSivu::~AloitusSivu()
{
    delete ui;
}


void AloitusSivu::siirrySivulle()
{
    // Päivitetään aloitussivua
    if( kp()->asetukset()->onko("Nimi"))
    {
        QString txt("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"qrc:/aloitus/aloitus.css\"></head><body>");
        txt.append( paivitysInfo );

        txt.append( vinkit() );

        // Ei tulosteta tyhjiä otsikoita vaan possu jos ei kirjauksia
        if( kp()->asetukset()->onko("EkaTositeKirjattu") )
            txt.append(summat());
        else
            txt.append("<p><img src=qrc:/pic/aboutpossu.png></p>");

        ui->selain->setHtml(txt);
    }
    else
    {
        ui->selain->setSource(QUrl("qrc:/aloitus/tervetuloa.html"));
        ui->selain->insertHtml(  paivitysInfo );
        ui->tilikausiCombo->hide();
        ui->logoLabel->hide();
        ui->nimiLabel->hide();
    }

}

void AloitusSivu::kirjanpitoVaihtui()
{
    if( kp()->asetukset()->onko("Nimi"))
    {
        // Kirjanpito avattu
        ui->nimiLabel->show();
        ui->nimiLabel->setText( kp()->asetukset()->asetus("Nimi"));

        if( QFile::exists( kp()->hakemisto().absoluteFilePath("logo64.png") ) )
        {
            ui->logoLabel->show();
            ui->logoLabel->setPixmap( QPixmap( kp()->hakemisto().absoluteFilePath("logo64.png") ) );
        }
        else
            ui->logoLabel->hide();

        ui->tilikausiCombo->show();
        ui->tilikausiCombo->setModel( kp()->tilikaudet() );
        ui->tilikausiCombo->setModelColumn( 0 );


        // Valitaan nykyinen tilikausi
        // Pohjalle kuitenkin viimeinen tilikausi, jotta joku on aina valittuna
        ui->tilikausiCombo->setCurrentIndex( kp()->tilikaudet()->rowCount(QModelIndex()) - 1 );

        for(int i=0; i < kp()->tilikaudet()->rowCount(QModelIndex());i++)
        {
            Tilikausi kausi = kp()->tilikaudet()->tilikausiIndeksilla(i);
            if( kausi.alkaa() <= kp()->paivamaara() && kausi.paattyy() >= kp()->paivamaara())
            {
                ui->tilikausiCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    siirrySivulle();
}

void AloitusSivu::linkki(const QUrl &linkki)
{
    qDebug() << linkki;

    if( linkki.scheme() == "ohje")
    {
        QDesktopServices::openUrl( QUrl(QString("https://artoh.github.io/kitupiikki/") + linkki.fileName()
                                   + "#" + linkki.fragment() ));
    }
    else if( linkki.scheme() == "selaa")
    {
        Tilikausi kausi = kp()->tilikaudet()->tilikausiIndeksilla( ui->tilikausiCombo->currentIndex() );
        QString tiliteksti = linkki.fileName();
        emit selaus( tiliteksti.toInt(), kausi );
    }
    else if( linkki.scheme() == "ktp")
    {
        QString toiminto = linkki.path().mid(1);
        qDebug() << toiminto;

        if( toiminto == "uusi")
            uusiTietokanta();
        else
            emit ktpkasky(toiminto);
    }
}




void AloitusSivu::uusiTietokanta()
{
    QString uusitiedosto = UusiKirjanpito::aloitaUusiKirjanpito();
    if( !uusitiedosto.isEmpty())
        Kirjanpito::db()->avaaTietokanta(uusitiedosto + "/kitupiikki.sqlite");
}

void AloitusSivu::avaaTietokanta()
{
    QString polku = QFileDialog::getOpenFileName(this, "Avaa kirjanpito",
                                                 QDir::homePath(),"Kirjanpito (kitupiikki.sqlite)");
    if( !polku.isEmpty())
        Kirjanpito::db()->avaaTietokanta(polku);

}

void AloitusSivu::viimeisinTietokanta(QListWidgetItem *item)
{
    Kirjanpito::db()->avaaTietokanta( item->data(Qt::UserRole).toString());
}

void AloitusSivu::abouttiarallaa()
{
    Ui::AboutDlg aboutUi;
    QDialog aboutDlg;
    aboutUi.setupUi( &aboutDlg);
    connect( aboutUi.aboutQtNappi, SIGNAL(clicked(bool)), qApp, SLOT(aboutQt()));

    aboutUi.versioLabel->setText( tr("<b>Versio %1</b>")
                                  .arg( qApp->applicationVersion()) );


    aboutDlg.exec();
}

void AloitusSivu::infoSaapui(QNetworkReply *reply)
{
    bool tulosta = false;
    while( reply->canReadLine())
    {
        QString rivi = QString::fromUtf8( reply->readLine() );
        if( rivi.startsWith("#"))
        {
            QRegularExpression ehto( rivi.mid(1).trimmed(), QRegularExpression::UseUnicodePropertiesOption );
            tulosta = ehto.match( qApp->applicationVersion() ).hasMatch();

            qDebug() << ehto.pattern() << qApp->applicationVersion();
        }
        else
        {
            if( tulosta )
                paivitysInfo.append( rivi );
        }

    }
    siirrySivulle();
    reply->deleteLater();
}

QString AloitusSivu::vinkit()
{
    QString vinkki;
    // Ensin tietokannan alkutoimiin
    if( !kp()->asetukset()->onko("EkaTositeKirjattu") )
    {
        vinkki.append("<table class=vinkki width=100%><tr><td>");
        vinkki.append("<h3>Kirjanpidon aloittaminen</h3><ol>");
        vinkki.append("<li>Tarkista <a href=ktp:/maaritys/Perusvalinnat>perusvalinnat, logo ja arvonlisävelvollisuus</a> <a href='ohje:/maaritykset#perusvalinnat'>(Ohje)</a></li>");
        vinkki.append("<li>Tutustu <a href=ktp:/maaritys/Tilikartta>tilikarttaan</a> ja tee tarpeelliset muutokset <a href='ohje:/maaritykset#tilikartta'>(Ohje)</a></li>");
        vinkki.append("<li>Tutustu <a href=ktp:/maaritys/Tositelajit>tositelajeihin</a> ja lisää tarvitsemasi tositelajit <a href='ohje:/maaritykset#tositelajit'>(Ohje)</a></li>");
        vinkki.append("<li>Lisää tarvitsemasi <a href=ktp:/maaritys/Kohdennukset>kohdennukset</a> <a href='ohje:/maaritykset#kohdennukset'>(Ohje)</a></li>");
        if( kp()->asetukset()->luku("Tilinavaus")==2)
            vinkki.append("<li>Tee <a href=ktp:/maaritys/Tilinavaus>tilinavaus</a> <a href='ohje:/maaritykset#tilinavaus'>(Ohje)</a></li>");
        vinkki.append("<li>Voit aloittaa <a href=ktp:/kirjaa>kirjausten tekemisen</a> <a href='ohje:/kirjaaminen'>(Ohje)</a></li>");
        vinkki.append("</ol></td></tr></table>");

    }
    else if( kp()->asetukset()->luku("Tilinavaus")==2 && kp()->asetukset()->pvm("TilinavausPvm") <= kp()->tilitpaatetty() )
        vinkki.append(tr("<table class=vinkki width=100%><tr><td><h3><a href=ktp:/maaritys/Tilinavaus>Tee tilinavaus</a></h3><p>Syötä viimeisimmältä tilinpäätökseltä tilien "
                      "avaavat saldot %1 järjestelmään <a href='ohje:/maaritykset#tilinavaus'>(Ohje)</a></p></td></tr></table>").arg( kp()->asetukset()->pvm("TilinavausPvm").toString(Qt::SystemLocaleShortDate) ) );

    // Muistutus arvonlisäverolaskelmasta
    if(  kp()->asetukset()->onko("AlvVelvollinen") )
    {
        QDate kausialkaa = kp()->asetukset()->pvm("AlvIlmoitus").addDays(1);
        QDate kausipaattyy = kp()->asetukset()->pvm("AlvIlmoitus").addMonths( kp()->asetukset()->luku("AlvKausi")).addDays(-1);
        QDate erapaiva = AlvMaaritys::erapaiva(kausipaattyy);

        int paivaaIlmoitukseen = kp()->paivamaara().daysTo( erapaiva );
        if( paivaaIlmoitukseen < 0)
        {
            vinkki.append( tr("<table class=varoitus width=100%><tr><td>"
                              "<h3><a href=ktp:/maaritys/Arvonlisävero>Arvonlisäveroilmoitus myöhässä</a></h3>"
                              "Arvonlisäveroilmoitus kaudelta %1 - %2 olisi pitänyt antaa %3 mennessä.</td></tr></table>")
                           .arg(kausialkaa.toString(Qt::SystemLocaleShortDate)).arg(kausipaattyy.toString(Qt::SystemLocaleShortDate))
                           .arg(erapaiva.toString(Qt::SystemLocaleShortDate)));

        }
        else if( paivaaIlmoitukseen < 30)
        {
            vinkki.append( tr("<table class=vinkki width=100%><tr><td>"
                              "<h3><a href=ktp:/maaritys/Arvonlisävero>Tee arvonlisäverotilitys</a></h3>"
                              "Arvonlisäveroilmoitus kaudelta %1 - %2 on annettava %3 mennessä.</td></tr></table>")
                           .arg(kausialkaa.toString(Qt::SystemLocaleShortDate)).arg(kausipaattyy.toString(Qt::SystemLocaleShortDate))
                           .arg(erapaiva.toString(Qt::SystemLocaleShortDate)));
        }
    }


    // Uuden tilikauden aloittaminen
    if( kp()->paivamaara().daysTo(kp()->tilikaudet()->kirjanpitoLoppuu()) < 30 )
    {
        vinkki.append(tr("<table class=vinkki width=100%><tr><td>"
                      "<h3><a href=ktp:/uusitilikausi>Aloita uusi tilikausi</a></h3>"
                      "<p>Tilikausi päättyy %1, jonka jälkeiselle ajalle ei voi tehdä kirjauksia ennen kuin uusi tilikausi aloitetaan.</p>"
                      "<p>Voit tehdä kirjauksia myös aiempaan tilikauteen, kunnes se on päätetty</p></td></tr></table>").arg( kp()->tilikaudet()->kirjanpitoLoppuu().toString(Qt::SystemLocaleShortDate) ));

    }

    // Tilinpäätöksen laatiminen
    for(int i=0; i<kp()->tilikaudet()->rowCount(QModelIndex()); i++)
    {
        Tilikausi kausi = kp()->tilikaudet()->tilikausiIndeksilla(i);
        if( kausi.paattyy().daysTo(kp()->paivamaara()) > 1 &&
                                   kausi.paattyy().daysTo( kp()->paivamaara()) < 5 * 30
                && ( kausi.tilinpaatoksenTila() == Tilikausi::ALOITTAMATTA || kausi.tilinpaatoksenTila() == Tilikausi::KESKEN) )
        {
            vinkki.append(QString("<table class=vinkki width=100%><tr><td>"
                          "<h3><a href=ktp:/arkisto>Aika laatia tilinpäätös tilikaudelle %1</a></h3>").arg(kausi.kausivaliTekstina()));

            if( kausi.tilinpaatoksenTila() == Tilikausi::ALOITTAMATTA)
            {
                vinkki.append("<p>Tee loppuun kaikki tilikaudelle kuuluvat kirjaukset ja laadi sen jälkeen <a href=ktp:/arkisto>tilinpäätös</a>.</p>");
            }
            else
            {
                vinkki.append("<p>Viimeiste ja vahvista <a href=ktp:/arkisto>tilinpäätös</a>.</p>");
            }
            vinkki.append("<p>Katso <a href=ohje:/tilinpaatos>ohjeet</a> tilinpäätösen laatimisesta</p></td></tr></table>");
        }
    }

    // Tarkistetaan, että alv-tilit paikallaan
    if( kp()->asetukset()->onko("AlvVelvollinen") &&
        ( !kp()->tilit()->tiliTyypilla(TiliLaji::ALVVELKA).onkoValidi() ||
          !kp()->tilit()->tiliTyypilla(TiliLaji::ALVSAATAVA).onkoValidi() ||
          !kp()->tilit()->tiliTyypilla(TiliLaji::VEROVELKA).onkoValidi()))
    {
        vinkki.append( tr("<table class=varoitus width=100%><tr><td>"
                          "<h3><a href=ktp:/maaritys/Tilikartta>Tilikartta puutteellinen</a></h3>"
                          "<p>Tilikartassa pitää olla tilit alv-kirjauksille, alv-vähennyksille ja verovelalle.</td></tr></table>") );

    }

    return vinkki;
}

QString AloitusSivu::summat()
{
    QString txt;

    Tilikausi tilikausi = kp()->tilikaudet()->tilikausiIndeksilla( ui->tilikausiCombo->currentIndex() );

    txt.append(tr("<h2>Tilikausi %1 - %2</h2>").arg(tilikausi.alkaa().toString(Qt::SystemLocaleShortDate))
             .arg(tilikausi.paattyy().toString(Qt::SystemLocaleShortDate)));

    txt.append("<table width=100%>");

    QSqlQuery kysely;

    kysely.exec(QString("select tilinro, tilinimi, sum(debetsnt), sum(kreditsnt) from vientivw where tilityyppi like \"AR%\" and pvm <= \"%1\" group by tilinro")
                .arg(tilikausi.paattyy().toString(Qt::ISODate)));


    // Rahavara-tilien saldot

    txt.append("<tr><td colspan=2 class=otsikko>Rahavarat</td></tr>");

    kysely.exec(QString("select tilinro, tilinimi, sum(debetsnt), sum(kreditsnt) from vientivw where tilityyppi like \"AR%\" and pvm <= \"%1\" group by tilinro")
                .arg(tilikausi.paattyy().toString(Qt::ISODate)));
    int saldosumma = 0;
    while( kysely.next())
    {
        int saldosnt = kysely.value(2).toInt() - kysely.value(3).toInt();
        saldosumma += saldosnt;
        txt.append( tr("<tr><td><a href=\"selaa:%1\">%1 %2</a></td><td class=euro>%L3 €</td></tr>").arg(kysely.value(0).toInt())
                                                           .arg(kysely.value(1).toString())
                                                           .arg( ((double) saldosnt ) / 100,0,'f',2 ) );
    }
    txt.append( tr("<tr class=summa><td>Rahavarat yhteensä</td><td class=euro>%L1 €</td></tr>").arg( ((double) saldosumma ) / 100,0,'f',2 ) );
    txt.append("<tr><td colspan=2>&nbsp;</td></tr>");

    // Sitten tulot
    kysely.exec(QString("select tilinro, tilinimi, sum(debetsnt), sum(kreditsnt) from vientivw where tilityyppi like \"C%\" AND pvm BETWEEN \"%1\" AND \"%2\" group by tilinro")
                .arg(tilikausi.alkaa().toString(Qt::ISODate)  )
                .arg(tilikausi.paattyy().toString(Qt::ISODate)));

    txt.append("<tr><td colspan=2 class=otsikko>Tulot</td></tr>");
    int summatulot = 0;

    while( kysely.next())
    {
        int saldosnt = kysely.value(3).toInt() - kysely.value(2).toInt();
        summatulot += saldosnt;
        txt.append( tr("<tr><td><a href=\"selaa:%1\">%1 %2</a></td><td class=euro>%L3 €</td></tr>").arg(kysely.value(0).toInt())
                                                           .arg(kysely.value(1).toString())
                                                           .arg( ((double) saldosnt ) / 100,0,'f',2 ) );
    }
    txt.append( tr("<tr class=summa><td>Tulot yhteensä</td><td class=euro>%L1 €</td></tr>").arg( ((double) summatulot ) / 100,0,'f',2 ) );
    txt.append("<tr><td colspan=2>&nbsp;</td></tr>");

    // ja menot
    kysely.exec(QString("select tilinro, tilinimi, sum(debetsnt), sum(kreditsnt) from vientivw where tilityyppi like \"D%\" AND pvm BETWEEN \"%1\" AND \"%2\" group by tilinro")
                .arg(tilikausi.alkaa().toString(Qt::ISODate)  )
                .arg(tilikausi.paattyy().toString(Qt::ISODate)));


    txt.append("<tr><td colspan=2 class=otsikko>Menot</td></tr>");
    int summamenot = 0;

    while( kysely.next())
    {
        int saldosnt = kysely.value(2).toInt() - kysely.value(3).toInt();
        summamenot += saldosnt;
        txt.append( tr("<tr><td><a href=\"selaa:%1\">%1 %2</a></td><td class=euro>%L3 €</td></tr>").arg(kysely.value(0).toInt())
                                                           .arg(kysely.value(1).toString())
                                                           .arg( ((double) saldosnt ) / 100,0,'f',2 ) );
    }
    txt.append( tr("<tr class=summa><td>Menot yhteensä</td><td class=euro>%L1 €</td></tr>").arg( ((double) summamenot ) / 100,0,'f',2 ) );
    txt.append("<tr><td colspan=2>&nbsp;</td></tr>");



    // Yli/alijäämä
    txt.append( tr("<tr class=kokosumma><td>Yli/alijäämä</td><td class=euro> %L1 €</td></tr></table>").arg(( ((double) (summatulot - summamenot) ) / 100), 0,'f',2 )) ;

    txt.append("</table><p>&nbsp;</p><table width=100%>");

    // Kohdennukset
    txt.append("<tr><td class=otsikko>Kohdennukset</td><th>Tuloa</th><th>Menoa</th><th>Yli/alijäämä</th></tr>");

    kysely.exec( QString("select kohdennus.nimi, sum(kreditsnt), sum(debetsnt) from vienti, kohdennus, tili "
                         " where pvm between '%1' and '%2' and vienti.tili=tili.id and vienti.kohdennus=kohdennus.id and tili.ysiluku >= 300000000 "
                         " group by kohdennus.id order by kohdennus.id")
                 .arg(tilikausi.alkaa().toString(Qt::ISODate)  )
                 .arg(tilikausi.paattyy().toString(Qt::ISODate)));

    while(kysely.next())
    {
        txt.append(QString("<tr><td>%1</td><td class=euro>%L2 €</td><td class=euro>%L3 €</td><td class=euro>%L4 €</td></tr>")
                   .arg( kysely.value(0).toString())
                   .arg( ((double) kysely.value(1).toInt() ) / 100,0,'f',2 )
                   .arg( ((double) kysely.value(2).toInt() ) / 100,0,'f',2 )
                   .arg( ((double) (kysely.value(1).toInt() - kysely.value(2).toInt())) / 100,0,'f',2 ));
    }
    txt.append("</table>");


    return txt;

}



void AloitusSivu::lisaaViimetiedostot()
{
    QSettings settings;
    QStringList lista = settings.value("viimeiset").toStringList();

    ui->viimeiset->clear();

    foreach (QString rivi, lista)
    {
        QStringList palat = rivi.split(';');
        QString polku = palat.at(0);
        QString nimi = palat.at(1);
        QString kuvake = QFileInfo(polku).absoluteDir().absoluteFilePath("logo64.png");

        if( polku.contains(".sqlite") && QFile::exists(polku))
        {

            QListWidgetItem *item = new QListWidgetItem;
            item->setText( nimi );
            if( QFile::exists(kuvake))
                item->setIcon( QIcon(kuvake));
            item->setData(Qt::UserRole, polku);
            ui->viimeiset->addItem(item);
        }
    }


}
