/****************************************************************************
 **
 ** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

//Backend
#include "qmdegalleryqueryresultset.h"
#include "qgallerymdsutility.h"
#include "qmdesession.h"
//API
#include "qdocumentgallery.h"
#include "qgalleryqueryrequest.h"

QTM_BEGIN_NAMESPACE

QMDEGalleryQueryResultSet::QMDEGalleryQueryResultSet(QMdeSession *session, QObject *parent)
:QMDEGalleryResultSet(parent)
{
    m_request = static_cast<QGalleryQueryRequest *>(parent);
    m_live = m_request->autoUpdate();
    m_session = session;
    m_query = NULL;
    m_launchUpdateQuery = false;
    m_query_running = false;

    createQuery();
}

QMDEGalleryQueryResultSet::~QMDEGalleryQueryResultSet()
{
    if (m_query) {
        m_query->RemoveObserver( *this );
        m_query->Cancel();
    }
    delete m_query;
    m_query = NULL;

    m_session->RemoveObjectObserver( *this );
}

void QMDEGalleryQueryResultSet::HandleQueryNewResults( CMdEQuery &aQuery,
    TInt firstNewItemIndex,
    TInt newItemCount )
{
    if (m_launchUpdateQuery) {
        if (aQuery.ResultMode() == EQueryResultModeItem) {
            int max = aQuery.Count();
            for ( TInt i = firstNewItemIndex; i < max; i++ ) {
                CMdEObject *item = static_cast<CMdEObject *>(aQuery.TakeOwnershipOfResult( i ));
                m_updatedItemArray.Append( item );
                m_updatedObjectIDs.Append( item->Id() );
            }
        }
    }
    else {
        if (aQuery.ResultMode() == EQueryResultModeItem) {
            int max = aQuery.Count();
            for ( TInt i = firstNewItemIndex; i < max; i++ ) {
                CMdEObject *item = static_cast<CMdEObject *>(aQuery.TakeOwnershipOfResult( i ));
                m_itemArray.Append( item );
                m_currentObjectIDs.Append( item->Id() );
            }
        }

        //Signals that items have been inserted into a result set at
        emit itemsInserted(firstNewItemIndex, newItemCount);

        emit progressChanged(aQuery.Count(), KMdEQueryDefaultMaxCount);
    }
}

void QMDEGalleryQueryResultSet::HandleQueryCompleted( CMdEQuery &aQuery, TInt aError )
{
    m_query_running = false;

    if (m_launchUpdateQuery) {
        handleUpdatedResults();
        m_launchUpdateQuery = false;
    } else {
        emit progressChanged(aQuery.Count(), aQuery.Count());

        if (aError == KErrNone) {
            if (m_live) {
                TRAPD( err,
                    m_session->AddItemAddedObserverL( *this, m_queryConditions );
                    m_session->AddItemChangedObserverL( *this, m_currentObjectIDs );
                );
                if (err) {
                    m_live = false;
                }
            }
            finish(m_live);
        } else {
            error(QDocumentGallery::ConnectionError);
        }
    }
}

void QMDEGalleryQueryResultSet::HandleObjectNotification( CMdESession& /*aSession*/,
    TObserverNotificationType aType,
    const RArray<TItemId>& aObjectIdArray )
{
    if (aType == ENotifyAdd) {
        m_launchUpdateQuery = true;

        if (m_query_running) {
            m_query->RemoveObserver( *this );
            m_query->Cancel();
        }
        createQuery();
    } else if (aType == ENotifyRemove) {
        const int count = aObjectIdArray.Count();
        // Linear search as the result set might be sorted by the query
        for (int i = 0; i < count; i++) {
            const int index = m_currentObjectIDs.Find( aObjectIdArray[i]);
            if (index >= 0) {
                CMdEObject *item = m_itemArray[index];
                m_itemArray.Remove( index );
                delete item;
                item = NULL;
                emit itemsRemoved(index, 1);
                if (index == m_cursorPosition) {
                    if ( itemCount() == 0 ) {
                        m_isValid = false;
                        emit currentItemChanged();
                    } else {
                        if ( fetchFirst()) {
                            emit currentIndexChanged( m_cursorPosition );
                        }
                    }
                }
            }
        }
    } else if (aType == ENotifyModify) {
        const int count = aObjectIdArray.Count();
        // Linear search as the result set might be sorted by the query
        for (int i = 0; i < count; i++) {
            const int index = m_currentObjectIDs.Find( aObjectIdArray[i]);
            if (index >= 0) {
                CMdEObject *oldItem = m_itemArray[index];
                CMdEObject *newItem = NULL;
                TRAP_IGNORE( newItem = m_session->GetFullObjectL( aObjectIdArray[i] ) );
                if (newItem) {
                    m_itemArray[index] = newItem;
                    delete oldItem;
                    oldItem = NULL;

                    QString type = QDocumentGalleryMDSUtility::GetItemTypeFromMDEObject( newItem );
                    QStringList propertyList;
                    QDocumentGalleryMDSUtility::GetDataFieldsForItemType( propertyList, type );
                    QList<int> keys;
                    const int propertyCount = propertyList.count();
                    for (int i = 0; i < propertyCount; i++) {
                        keys.append( QDocumentGalleryMDSUtility::GetPropertyKey( propertyList[i] ));
                    }
                    emit metaDataChanged( index, 1, keys );
                } else {
                    m_itemArray.Remove( index );
                    delete oldItem;
                    oldItem = NULL;
                    emit itemsRemoved(index, 1);
                    if (index == m_cursorPosition) {
                        if ( itemCount() == 0 ) {
                            m_isValid = false;
                            emit currentItemChanged();
                        } else if (fetchFirst()) {
                            emit currentIndexChanged(m_cursorPosition);
                        }
                    }
                }
            }
        }
    }
}

void QMDEGalleryQueryResultSet::createQuery()
{
    delete m_query;
    m_query = NULL;

    if (!m_session) {
        if (!m_launchUpdateQuery) {
            error(QDocumentGallery::ConnectionError);
        } else {
            m_launchUpdateQuery = false;
        }
        return;
    }

    int error = QDocumentGallery::NoError;
    TRAPD( err, m_query = m_session->NewObjectQueryL( this, m_request, error ) );
    if (err) {
        m_query = NULL;
        if (!m_launchUpdateQuery) {
            QGalleryAbstractResponse::error(QDocumentGallery::ConnectionError);
        } else {
            m_launchUpdateQuery = false;
        }
        return;
    } else if (error != QDocumentGallery::NoError) {
        m_query->RemoveObserver( *this );
        delete m_query;
        m_query = NULL;
        if (!m_launchUpdateQuery) {
            QGalleryAbstractResponse::error(error);
        } else {
            m_launchUpdateQuery = false;
        }
        return;
    }

    // NewObjectQuery will return NULL if object type is not supported
    if (m_query) {
        m_queryConditions = &m_query->Conditions();
        m_query_running = true;
        TRAP( err, m_query->FindL() );
        if (err) {
            m_query->RemoveObserver( *this );
            delete m_query;
            m_query = NULL;
            if (!m_launchUpdateQuery) {
                QGalleryAbstractResponse::error(QDocumentGallery::ConnectionError);
            } else {
                m_launchUpdateQuery = false;
            }
        }
    } else if (!m_launchUpdateQuery) {
        QGalleryAbstractResponse::error(QDocumentGallery::ItemTypeError);
    } else {
        m_launchUpdateQuery = false;
    }
}

void QMDEGalleryQueryResultSet::handleUpdatedResults()
{
    m_itemArray.ResetAndDestroy();
    m_itemArray = m_updatedItemArray;
    const int updatedCount = m_updatedObjectIDs.Count();
    for( int i = 0; i < updatedCount; i++ ) {
        const int index = m_currentObjectIDs.Find( m_updatedObjectIDs[i]);
        // If the object ID is not found in current id list, new object
        // has been added
        if( index < 0 ) {
            emit itemsInserted( i, 1 );
            if (i == m_cursorPosition) {
                emit currentItemChanged();
            } else if (m_cursorPosition >= m_itemArray.Count()) {
                if (fetchFirst()) {
                    currentIndexChanged( m_cursorPosition );
                }
            }
        }
    }
    m_currentObjectIDs = m_updatedObjectIDs;
}

#include "moc_qmdegalleryqueryresultset.cpp"
QTM_END_NAMESPACE