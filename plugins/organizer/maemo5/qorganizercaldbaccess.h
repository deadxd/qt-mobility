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

#ifndef QORGANIZERCALDBACCESS_H
#define QORGANIZERCALDBACCESS_H

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
#include <QtSql>

QTM_USE_NAMESPACE

class OrganizerDbCache;
class CEvent;
class CTodo;
class CJournal;
class CCalendar;

class OrganizerCalendarDatabaseAccess
{
public:
    OrganizerCalendarDatabaseAccess(OrganizerDbCache* dbCache);
    ~OrganizerCalendarDatabaseAccess();

    bool open(QString databasePathName);
    void close();

    int calIdOf(QOrganizerItemLocalId id);
    int typeOf(QOrganizerItemLocalId id);
    std::vector<CEvent *> getEvents(int calId, std::string guid, int &pErrorCode);
    std::vector<CTodo *> getTodos(int calId, std::string guid, int &pErrorCode);
    std::vector<CJournal *> getJournals(int calId, std::string guid, int &pErrorCode);

    CEvent* getEvent(CCalendar* cal, const std::string& id, int& calError);
    CTodo* getTodo(CCalendar* cal, const std::string& id, int& calError);
    CJournal* getJournal(CCalendar* cal, const std::string& id, int& calError);

    void getIdList(CCalendar* cal, int compType, int& calError, std::vector<std::string>& result);

    static void sqliteErrorMapper(const QSqlError &sqlError, int& errorCode);

private:
    QSqlDatabase m_db;
    OrganizerDbCache* m_dbCache;
};

#endif // QORGANIZERCALDBACCESS_H