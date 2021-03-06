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

#include "organizeritemtransform.h"
#include "qorganizersymbian_p.h"

#include <QDebug>
#include <calentry.h>
#include <calinstance.h>

#include "qorganizeritem.h"
#include "qorganizereventoccurrence.h"
#include "organizereventtimerangetransform.h"
#include "organizeritemdescriptiontransform.h"
#include "organizeritemdetailtransform.h"
#include "organizeritemdisplaylabeltransform.h"
#include "organizeritemguidtransform.h"
#include "organizeritemparenttransform.h"
#include "organizeritemlocationtransform.h"
#include "organizeritemprioritytransform.h"
#include "organizeritemrecurrencetransform.h"
#include "organizeritemtimestamptransform.h"
#include "organizeritemtypetransform.h"
#include "organizerjournaltimerangetransform.h"
#include "organizertodoprogresstransform.h"
#include "organizertodotimerangetransform.h"
#include "organizeritemremindertransform.h"

QTM_USE_NAMESPACE

void debugEntryL(const CCalEntry &entry)
{
    qDebug() << QString("CCalEntry uid-%1 localUid-%2")
        .arg(toQString(entry.UidL()))
        .arg(entry.LocalUidL());
    qDebug() << "Type            :" << entry.EntryTypeL();
    qDebug() << "Summary         :" << toQString(entry.SummaryL());
    qDebug() << "Description     :" << toQString(entry.DescriptionL());
    qDebug() << "Method          :" << entry.MethodL();
    qDebug() << "SequenceNumber  :" << entry.SequenceNumberL();
    qDebug() << "RecurrenceId    :" << toQDateTimeL(entry.RecurrenceIdL());
    qDebug() << "RecurrenceRange :" << entry.RecurrenceRangeL();
    qDebug() << "StartTime       :" << toQDateTimeL(entry.StartTimeL());
    qDebug() << "EndTime         :" << toQDateTimeL(entry.EndTimeL());
}

void debugInstanceL(const CCalInstance &instance)
{
    qDebug() << QString("CCalInstance time:%1 starttime:%2 endtime:%3")
        .arg(toQDateTimeL(instance.Time()).toString())
        .arg(toQDateTimeL(instance.StartTimeL()).toString())
        .arg(toQDateTimeL(instance.EndTimeL()).toString());
    debugEntryL(instance.Entry());
}

void debugSchemaDefs(const QMap<QString, QMap<QString, QOrganizerItemDetailDefinition> > &schemaDefs)
{
    foreach (QString itemTypeName, schemaDefs.keys()) {
        qDebug() << itemTypeName;
        QMap<QString, QOrganizerItemDetailDefinition> details = schemaDefs.value(itemTypeName);
        foreach (QString detailName, details.keys()) {
            qDebug() << "\t" << detailName;
            QOrganizerItemDetailDefinition detDef = details.value(detailName);
            foreach (QString detailField, detDef.fields().keys())
                qDebug() << "\t\t" << detailField;
        }
    }
}

OrganizerItemTransform::OrganizerItemTransform()
{
    m_detailTransforms.append(new OrganizerItemTypeTransform()); // this must be first always
    m_detailTransforms.append(new OrganizerEventTimeTransform());
    m_detailTransforms.append(new OrganizerItemDescriptionTransform());
    m_detailTransforms.append(new OrganizerItemDisplayLabelTransform());
    m_detailTransforms.append(new OrganizerItemGuidTransform());
    m_detailTransforms.append(new OrganizerItemParentTransform());
    m_detailTransforms.append(new OrganizerItemLocationTransform());
    m_detailTransforms.append(new OrganizerItemPriorityTransform());
    m_detailTransforms.append(new OrganizerItemTimeStampTransform());
    m_detailTransforms.append(new OrganizerJournalTimeTransform());
    m_detailTransforms.append(new OrganizerTodoProgressTransform());
    m_detailTransforms.append(new OrganizerTodoTimeTransform());
    m_detailTransforms.append(new OrganizerItemRecurrenceTransform());
	m_detailTransforms.append(new OrganizerItemReminderTransform());
}

OrganizerItemTransform::~OrganizerItemTransform()
{
    // Delete transform objects
    foreach (OrganizerItemDetailTransform *i, m_detailTransforms)
        delete i;
}

void OrganizerItemTransform::modifyBaseSchemaDefinitions(QMap<QString, QMap<QString, QOrganizerItemDetailDefinition> > &schemaDefs) const
{
    //qDebug() << "Base schema definitions:";
    //debugSchemaDefs(schemaDefs);
    
    // Get all supported detail definition names
    QStringList supportedDetails;
    foreach (OrganizerItemDetailTransform *t, m_detailTransforms)
        supportedDetails << t->detailDefinitionName();
    
    // Remove all not supported details
    foreach (QString itemTypeName, schemaDefs.keys()) {
        QMap<QString, QOrganizerItemDetailDefinition> details = schemaDefs.value(itemTypeName);
        foreach (QString detailName, details.keys()) {
            if (!supportedDetails.contains(detailName))
                schemaDefs[itemTypeName].remove(detailName);
        }
    }
    
    // Make detail specific modifications
    foreach (OrganizerItemDetailTransform *t, m_detailTransforms)
        t->modifyBaseSchemaDefinitions(schemaDefs);
    
    //qDebug() << "Modified schema definitions:";
    //debugSchemaDefs(schemaDefs);
}

void OrganizerItemTransform::toEntryL(const QOrganizerItem &item, CCalEntry *entry)
{
    // Loop through transform objects
    foreach (OrganizerItemDetailTransform *i, m_detailTransforms) {
        i->transformToEntryL(item, entry);
    }

    entry->SetLastModifiedDateL();
}

void OrganizerItemTransform::toItemL(const CCalEntry &entry, QOrganizerItem *item) const
{
    //debugEntryL(entry);
    
    // Loop through transform objects
    foreach (OrganizerItemDetailTransform *i, m_detailTransforms) {
        i->transformToDetailL(entry, item);
    }
}

void OrganizerItemTransform::toItemPostSaveL(const CCalEntry &entry, QOrganizerItem *item, QString managerUri) const
{
    Q_UNUSED(managerUri);
    //debugEntryL(entry);
    // Loop through transform objects
    foreach (OrganizerItemDetailTransform *i, m_detailTransforms) {
        i->transformToDetailPostSaveL(entry, item);
    }
}

void OrganizerItemTransform::toItemOccurrenceL(
    const CCalInstance &instance,
    QOrganizerItem *itemOccurrence) const
{
    //debugInstanceL(instance);

    // Loop through transform objects
    foreach (OrganizerItemDetailTransform *i, m_detailTransforms) {
        i->transformToDetailL(instance, itemOccurrence);
    }
}
