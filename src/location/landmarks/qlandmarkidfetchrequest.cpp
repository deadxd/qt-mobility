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

#include "qlandmarkidfetchrequest.h"
#include "qlandmarkfilter.h"
#include "qlandmarksortorder.h"
#include "qlandmarkrequests_p.h"


QTM_BEGIN_NAMESPACE

/*!
    \class QLandmarkIdFetchRequest
    \brief The QLandmarkIdFetchRequest class allows a client to asynchronously
    request a list of landmark identifiers from a landmark manager.

    For a QLandmarkIdFetchRequest, the resultsAvailable() signal will be emitted when the resultant
    identifiers (which may be retrieved by calling ids()), are updated, as well as if
    the overall operation error (which may be retrieved by calling error()) is updated.


    \inmodule QtLocation

    \ingroup landmarks-request
*/

/*!
    Creates a new landmark id fetch request object with the given \a manager and \a parent.
*/
QLandmarkIdFetchRequest::QLandmarkIdFetchRequest(QLandmarkManager *manager, QObject *parent)
    : QLandmarkAbstractRequest(new QLandmarkIdFetchRequestPrivate(manager), parent)
{
}

/*!
    Destroys the request object.
*/
QLandmarkIdFetchRequest::~QLandmarkIdFetchRequest()
{
}

/*!
    Returns the filter which will be used to select the landmarks whose identifiers
    will be returned.

    By default, the filter's type will be a LandmarkFilter::DefaultFilter
    and thus match all landmarks.
*/
QLandmarkFilter QLandmarkIdFetchRequest::filter() const
{
    Q_D(const QLandmarkIdFetchRequest);
    QMutexLocker ml(&d->mutex);
    return d->filter;
}

/*!
    Sets the \a filter which will be used to select landmark identifiers.

*/
void QLandmarkIdFetchRequest::setFilter(const QLandmarkFilter &filter)
{
    Q_D(QLandmarkIdFetchRequest);
    QMutexLocker ml(&d->mutex);
    d->filter = filter;
}

/*!
    Returns the sort ordering which is used to sort the result.
*/
QList<QLandmarkSortOrder> QLandmarkIdFetchRequest::sorting() const
{
    Q_D(const QLandmarkIdFetchRequest);
    QMutexLocker ml(&d->mutex);
    return d->sorting;
}

/*!
    Sets the sort ordering of the request to \a sorting.  This
    function will only have an effect on the results if invoked
    prior to calling QLandmarkAbstractRequest::start().
*/
void QLandmarkIdFetchRequest::setSorting(const QList<QLandmarkSortOrder> &sorting)
{
    Q_D(QLandmarkIdFetchRequest);
    QMutexLocker ml(&d->mutex);
    d->sorting = sorting;
}

/*!
    Sets the sort ordering of the request to a single \a sorting.  This
    function will only have an effect on the results if invoked
    prior to calling QLandmarkAbstractRequest::start().
*/
void QLandmarkIdFetchRequest::setSorting(const QLandmarkSortOrder &sorting)
{
    Q_D(QLandmarkIdFetchRequest);
    QMutexLocker ml(&d->mutex);
    d->sorting.clear();
    d->sorting.append(sorting);
}

/*!
    Returns the maximum number of landmark ids to be returned.
*/
int QLandmarkIdFetchRequest::limit() const
{
    Q_D(const QLandmarkIdFetchRequest);
    QMutexLocker ml(&d->mutex);
    return d->limit;
}

/*!
    Sets the maximum number of landmarks to be returned to \a limit.
 */
void QLandmarkIdFetchRequest::setLimit(int limit)
{
    Q_D(QLandmarkIdFetchRequest);
    QMutexLocker ml(&d->mutex);
    d->limit = limit;
}

/*!
    Returns the index offset for the request.
*/
int QLandmarkIdFetchRequest::offset() const
{
    Q_D(const QLandmarkIdFetchRequest);
    QMutexLocker ml(&d->mutex);
    return d->offset;
}

/*!
    Sets the index \a offset for the request.
*/
void QLandmarkIdFetchRequest::setOffset(int offset) {
    Q_D(QLandmarkIdFetchRequest);
    QMutexLocker ml(&d->mutex);
    d->offset = offset;
}

/*!
    Returns the list of identifiers of landmarks which matched the
    filter.
*/
QList<QLandmarkId> QLandmarkIdFetchRequest::landmarkIds() const
{
    Q_D(const QLandmarkIdFetchRequest);
    QMutexLocker ml(&d->mutex);
    return d->landmarkIds;
}

#include "moc_qlandmarkidfetchrequest.cpp"

QTM_END_NAMESPACE










