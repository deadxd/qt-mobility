/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Mobility Components.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial Usage
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Solutions Commercial License Agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and Nokia.
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** Please note Third Party Software included with Qt Solutions may impose
** additional restrictions and it is the user's responsibility to ensure
** that they have met the licensing requirements of the GPL, LGPL, or Qt
** Solutions Commercial license and the relevant license of the Third
** Party Software they are using.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at qt-sales@nokia.com.
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QORGANIZERITEMTRANSFORM_H
#define QORGANIZERITEMTRANSFORM_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qtorganizer.h"
#include "qorganizerrecurrencetransform.h"

QTM_USE_NAMESPACE

class OrganizerItemTransform
{
public:
    OrganizerItemTransform();
    ~OrganizerItemTransform();

    // manager uri
    void setManagerUri(QString managerUri);
    QString managerUri() const;

    // get recurrence transformer instance
    OrganizerRecurrenceTransform* recurrenceTransformer();

public: // transform functions
    // item conversion functions
    QOrganizerEvent convertCEventToQEvent(CEvent *cevent);

    QOrganizerEventOccurrence convertCEventToQEventOccurrence(CEvent *cevent);
    QOrganizerEventOccurrence convertCEventToQEventOccurrence(CEvent *cevent, const QDateTime &instanceStartDate, const QDateTime &instanceEndDate);
    QOrganizerEventOccurrence convertCEventToQEventOccurrence(CEvent *cevent, const QDateTime &instanceStartDate, const QDateTime &instanceEndDate, QOrganizerItemLocalId parentLocalId);

    QOrganizerTodo convertCTodoToQTodo(CTodo *ctodo);
    QOrganizerTodoOccurrence convertCTodoToQTodoOccurrence(CTodo *ctodo);
    QOrganizerJournal convertCJournalToQJournal(CJournal *cjournal );

    // conversions between CComponent and QOrganizerItem (common details for all items)
    void fillInCommonCComponentDetails(QOrganizerItem *item, CComponent *component, bool setId = true);
    CComponent* createCComponent(CCalendar *cal, const QOrganizerItem *item, QOrganizerItemManager::Error *error);

    // recurrence information conversions
    CRecurrence* createCRecurrence(const QOrganizerItem *item, QOrganizerItemManager::Error *error);

    // alarm setting
    void setAlarm(CCalendar *cal, QOrganizerItem *item, CComponent *component);

    // error code conversion
    QOrganizerItemManager::Error calErrorToManagerError(int calError) const;

    // calendar (collection) metadata conversions
    QString fromCalendarColour(CalendarColour calendarColour);
    CalendarColour toCalendarColour(QString calendarColour);
    QMap<CalendarColour, QString> calendarColourMap() const;

    // calendar (collection) type conversions
    QString fromCalendarType(CalendarType calendarType);
    CalendarType toCalendarType(QString calendarType);
    QMap<CalendarType, QString> calendarTypeMap() const;

private:
    // random GUID generation
    QString randomGuid() const;

private:
    // manager uri
    QString m_managerUri;

    // recurrence rule converter instance
    OrganizerRecurrenceTransform m_recTransformer;
};

#endif // QORGANIZERITEMTRANSFORM_H