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


#include <QtCore/qurl.h>

#include "qxamediastreamscontrol.h"
#include "qxaplaysession.h"
#include "qxacommon.h"

QXAMediaStreamsControl::QXAMediaStreamsControl(QXAPlaySession *session, QObject *parent)
   :QMediaStreamsControl(parent), mSession(session)
{
    QT_TRACE_FUNCTION_ENTRY;
    connect(mSession, SIGNAL(activeStreamsChanged()),
            this, SIGNAL(activeStreamsChanged()));
    connect(mSession, SIGNAL(streamsChanged()),
            this, SIGNAL(streamsChanged()));
    QT_TRACE_FUNCTION_EXIT;
}

QXAMediaStreamsControl::~QXAMediaStreamsControl()
{
    QT_TRACE_FUNCTION_ENTRY_EXIT;
}

bool  QXAMediaStreamsControl::isActive (int stream)
{
    RET_s_IF_p_IS_NULL(mSession, false);
    return mSession->isStreamActive(stream);
}

QVariant  QXAMediaStreamsControl::metaData (int stream, QtMultimediaKit::MetaData key)
{
    QVariant var;
    RET_s_IF_p_IS_NULL(mSession, var);
    QT_TRACE_FUNCTION_ENTRY;
    var = mSession->metaData(stream, key);
    QT_TRACE_FUNCTION_EXIT;
    return var;
}

void  QXAMediaStreamsControl::setActive (int stream, bool state)
{
    Q_UNUSED(stream);
    Q_UNUSED(state);
}

int  QXAMediaStreamsControl::streamCount()
{
    RET_s_IF_p_IS_NULL(mSession, 0);
    return mSession->streamCount();
}

QMediaStreamsControl::StreamType QXAMediaStreamsControl::streamType (int stream)
{
    RET_s_IF_p_IS_NULL(mSession, QMediaStreamsControl::UnknownStream);
    return mSession->streamType(stream);
}

