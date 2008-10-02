/***************************************************************************
 *   Copyright 2005-2008 Last.fm Ltd.                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301, USA.          *
 ***************************************************************************/

#ifndef FIREHOSE_H
#define FIREHOSE_H

#include <QWidget>


class Firehose : public QWidget
{
public:
    Firehose();
    
    virtual QSize sizeHint() const;
};


#include <QMap>
/** QSignalMapper is annoyingly limited */
class CoreSignalMapper : public QObject
{
    Q_OBJECT
    
    QMap<int, QString> m_map;
    
public:
    CoreSignalMapper( QObject* parent ) : QObject( parent )
    {}
    
    void setMapping( int i, const QString& s )
    {
        m_map[i] = s;
    }
    
public slots:
    void map( int i )
    {
        if (m_map.contains( i ))
            emit mapped( m_map[i] );
    }
    
signals:
    void mapped( const QString& );
};

#endif

