/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef QORGANIZERITEMTODOTIMERANGE_H
#define QORGANIZERITEMTODOTIMERANGE_H

#include <QString>

#include "qtorganizeritemsglobal.h"
#include "qorganizeritemdetail.h"
#include "qorganizeritem.h"

QTM_BEGIN_NAMESPACE

/* Leaf class */
class Q_CALENDAR_EXPORT QOrganizerItemTodoTimeRange : public QOrganizerItemDetail
{
public:
#ifdef Q_QDOC
    const char* DefinitionName;
    const char* FieldDueDateTime;
    const char* FieldNotBeforeDateTime;
#else
    Q_DECLARE_CUSTOM_CALENDAR_DETAIL(QOrganizerItemTodoTimeRange, "TodoTimeRange")
    Q_DECLARE_LATIN1_CONSTANT(FieldDueDateTime, "DueDateTime");
    Q_DECLARE_LATIN1_CONSTANT(FieldNotBeforeDateTime, "NotBeforeDateTime");
#endif

    void setDueDateTime(const QDateTime& dueDateTime) {setValue(FieldDueDateTime, dueDateTime);}
    QDateTime dueDateTime() const {return value<QDateTime>(FieldDueDateTime);}
    void setNotBeforeDateTime(const QDateTime& notBeforeDateTime) {setValue(FieldNotBeforeDateTime, notBeforeDateTime);}
    QDateTime notBeforeDateTime() const {return value<QDateTime>(FieldNotBeforeDateTime);}
};

QTM_END_NAMESPACE

#endif
