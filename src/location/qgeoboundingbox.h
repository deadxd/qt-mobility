/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Mobility Components.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QGEOBOUNDINGBOX_H
#define QGEOBOUNDINGBOX_H

#include "qgeoboundingarea.h"

#include <QSharedDataPointer>

QT_BEGIN_HEADER

QTM_BEGIN_NAMESPACE

class QGeoCoordinate;
class QGeoBoundingBoxPrivate;

class Q_LOCATION_EXPORT QGeoBoundingBox : public QGeoBoundingArea
{
public:
    QGeoBoundingBox();
    QGeoBoundingBox(const QGeoCoordinate &center, double degreesWidth, double degreesHeight);
    QGeoBoundingBox(const QGeoCoordinate &topLeft, const QGeoCoordinate &bottomRight);

    QGeoBoundingBox(const QGeoBoundingBox &other);
    ~QGeoBoundingBox();

    QGeoBoundingBox& operator = (const QGeoBoundingBox &other);

    bool operator == (const QGeoBoundingBox &other) const;
    bool operator != (const QGeoBoundingBox &other) const;

    QGeoBoundingArea::AreaType type() const;

    bool isValid() const;
    bool isEmpty() const;

    void setTopLeft(const QGeoCoordinate &topLeft);
    QGeoCoordinate topLeft() const;

    void setTopRight(const QGeoCoordinate &topRight);
    QGeoCoordinate topRight() const;

    void setBottomLeft(const QGeoCoordinate &bottomLeft);
    QGeoCoordinate bottomLeft() const;

    void setBottomRight(const QGeoCoordinate &bottomRight);
    QGeoCoordinate bottomRight() const;

    void setCenter(const QGeoCoordinate &center);
    QGeoCoordinate center() const;

    void setWidth(double degreesWidth);
    double width() const;

    void setHeight(double degreesHeight);
    double height() const;

    bool contains(const QGeoCoordinate &coordinate) const;
    bool contains(const QGeoBoundingBox &boundingBox) const;
    bool intersects(const QGeoBoundingBox &boundingBox) const;

    void translate(double degreesLatitude, double degreesLongitude);
    QGeoBoundingBox translated(double degreesLatitude, double degreesLongitude) const;

    QGeoBoundingBox united(const QGeoBoundingBox &boundingBox) const;
    QGeoBoundingBox operator | (const QGeoBoundingBox &boundingBox) const;
    QGeoBoundingBox& operator |= (const QGeoBoundingBox &boundingBox);

private:
    QSharedDataPointer<QGeoBoundingBoxPrivate> d_ptr;
};

inline QGeoBoundingBox QGeoBoundingBox::operator | (const QGeoBoundingBox &boundingBox) const
{
    return united(boundingBox);
}

QTM_END_NAMESPACE

QT_END_HEADER

#endif

