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
#ifndef TOSITELOKI_H
#define TOSITELOKI_H

#include <QAbstractTableModel>

class TositeLoki : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum TositeSarake {
        AIKA, KAYTTAJA, TILA
    };

    enum {
        DataRooli = Qt::UserRole,
        TilaRooli = Qt::UserRole + 1,
        AikaRooli = Qt::UserRole + 2
    };

    explicit TositeLoki(QObject *parent = nullptr);

    void lataa(const QVariant& data);

    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    QVariantList data_;
};

#endif // TOSITELOKI_H
