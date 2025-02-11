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
#include "monikielinen.h"

#include <QJsonDocument>
#include "kielet.h"

Monikielinen::Monikielinen()
{

}

Monikielinen::Monikielinen(const QVariant &var)
{
    Monikielinen::aseta(var);
}

Monikielinen::Monikielinen(const QString &str)
{
    QJsonDocument doc = QJsonDocument::fromJson( str.toUtf8() );
    if( doc.isEmpty() && !str.isEmpty()) {
        tekstit_.insert("fi", str);
    } else {
        Monikielinen::aseta( doc.toVariant());
    }
}


void Monikielinen::aseta(const QVariant &var)
{
    tekstit_.clear();
    if( !var.toMap().isEmpty() ) {
        QMapIterator<QString,QVariant> iter(var.toMap());
        while( iter.hasNext()) {
            iter.next();
            if( iter.key().isUpper()) continue;
            tekstit_.insert( iter.key(), iter.value().toString());
        }
    } else if(!var.toString().isEmpty()) {
        tekstit_.insert("fi", var.toString());
    }
}

void Monikielinen::aseta(const QString &nimi, const QString &kieli)
{
    if( nimi.isEmpty())
        tekstit_.remove(kieli);
    else
        tekstit_.insert(kieli, nimi);
}

void Monikielinen::aseta(const Monikielinen &kielinen)
{
    tekstit_ = kielinen.tekstit_;
}

QString Monikielinen::teksti(const QString &kieli) const
{
    if( kieli.isEmpty() && Kielet::instanssi()) {
        const QString& oletus = Kielet::instanssi()->nykyinen();
        if( tekstit_.contains(oletus)) {
            return tekstit_.value(oletus);
        }
    } else if( tekstit_.contains(kieli)) {
        return tekstit_.value(kieli);
    }
    if( tekstit_.contains("fi"))
        return tekstit_.value("fi");
    if( !tekstit_.isEmpty())
        return tekstit_.first();
    return QString();
}

QString Monikielinen::kaannos(const QString &kieli) const
{
    return tekstit_.value(kieli);
}

QVariantMap Monikielinen::map() const
{
    QVariantMap out;
    QMapIterator<QString,QString> iter(tekstit_);
    while( iter.hasNext()) {
        iter.next();
        out.insert( iter.key(), iter.value());
    }
    return out;
}
