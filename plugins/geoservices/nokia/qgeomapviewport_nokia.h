/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Mobility Components.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QGEOMAPVIEWPORT_NOKIA_H
#define QGEOMAPVIEWPORT_NOKIA_H

#include <qgeoserviceprovider.h>
#include <qgeomapviewport.h>
#include <qgeomaprequestoptions.h>
#include <qgeomapwidget.h>

#include <QHash>
#include <QRectF>
#include <QMap>
#include <QTimer>

QTM_USE_NAMESPACE

class QGeoMappingManagerNokia;

class QGeoMapViewportNokia : public QGeoMapViewport
{
    Q_OBJECT

private:
    struct QuadTileInfo
    {
        int row;
        int col;
        int zoomLevel;
        QSize size;
        QGeoMapRequestOptions options;
    };

    class TileIterator
    {
    public:
        TileIterator(const QRectF &boundingBox, qint32 zoomLevel, const QSize &tileSize);

        bool hasNext() const;
        bool isValid() const;
        void next();
        qint32 col() const;
        qint32 row() const;
        qint64 index() const;
        QRectF tileRect() const;

    private:
        QSize m_tileSize;
        qint32 m_numColRow;
        qint32 m_col;
        qint32 m_row;
        qint64 m_tileIndex;
        bool m_hasNext;
        QRectF m_boundingBox;
        qint64 m_currX;
        qint64 m_currY;
        QRectF m_rect;
        bool m_valid;
    };

public:
    QGeoMapViewportNokia(QGeoMappingManager *manager,
                         const QMap<QString, QString> &parameters,
                         QGeoServiceProvider::Error *error,
                         QString *errorString);
    virtual ~QGeoMapViewportNokia();

    virtual void setZoomLevel(int zoomLevel);
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option);
    virtual void setCenter(const QGeoCoordinate &center);
    virtual QGeoCoordinate center() const;
    virtual void pan(int startX, int startY, int endX, int endY);
    virtual QGeoBoundingBox viewBounds() const;
    virtual QPointF coordinateToScreenPosition(const QGeoCoordinate &coordinate) const;
    virtual QGeoCoordinate screenPositionToCoordinate(QPointF screenPosition) const;

    static qint64 getTileIndex(qint32 row, qint32 col, qint32 zoomLevel);

private slots:
    void tileFinished(QGeoMapReply* reply);
    void releaseUnusedTiles();

private:
    void requestTile(qint32 row, qint32 col);
    void requestMissingMapTiles();
    QRectF getTileRect(qint32 row, qint32 col) const;
    void updateMapWidget() const;

    Q_DISABLE_COPY(QGeoMapViewportNokia)

private:
    QTimer m_releaseTimer;
    QHash<QGeoMapReply*, QuadTileInfo*> m_pendingReplies;
    QHash<qint64, QPair<QPixmap, bool> > m_mapTiles;
    QRectF m_boundingBox;
    QSize m_tileSize;
    int m_horizontalPadding;
    int m_verticalPadding;
};

#endif