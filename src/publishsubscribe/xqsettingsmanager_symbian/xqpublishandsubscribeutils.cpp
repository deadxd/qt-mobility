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

#include "xqpublishandsubscribeutils.h"
#include "xqpublishandsubscribeutils_p.h"

/*
    \class XQPublishAndSubscribeUtils
    \internal

    \brief The XQPublishAndSubscribeUtils implements Publish and Subscribe related special
    features in SettingsManager. This class is just a Qt-like wrapper to
    Symbian OS' RPropery class.
*/


/*
    Constructs an XQPublishAndSubscribeUtils object.
    \param settingsManager Reference to XQSettingsManager instance
*/
XQPublishAndSubscribeUtils::XQPublishAndSubscribeUtils(XQSettingsManager& settingsManager, QObject* parent)
    : QObject(parent), d(new XQPublishAndSubscribeUtilsPrivate(settingsManager))
{
}

/*
    Destructs an XQPublishAndSubscribeUtils object.
*/
XQPublishAndSubscribeUtils::~XQPublishAndSubscribeUtils()
{
}

/*
    Defines a propery
    \param key XQPublishAndSubscribeSettingsKey which defines the propery details
    \param type Propery type (NOTE: TypeDouble is not supported by Publish and Subscribe.)
    \return True if the property was successfully defined, otherwise return false.
*/
bool XQPublishAndSubscribeUtils::defineProperty(const XQPublishAndSubscribeSettingsKey& key, XQSettingsManager::Type type)
{
    return d->defineProperty(key, type);
}

/*
    Defines a propery with security policies
    \param key XQPublishAndSubscribeSettingsKey which defines the propery details
    \param type Propery type (NOTE: TypeDouble is not supported by Publish and Subscribe.)
    \param readPolicy Policy definition for read operations
    \param writePolicy Policy definition for write operations
    \return True if the property was successfully defined, otherwise return false.
*/
bool XQPublishAndSubscribeUtils::defineProperty(const XQPublishAndSubscribeSettingsKey& key, XQSettingsManager::Type type,
    const XQPublishAndSubscribeSecurityPolicy& readPolicy, const XQPublishAndSubscribeSecurityPolicy& writePolicy)
{
    return d->defineProperty(key, type, readPolicy, writePolicy);
}

/*
    Deletes a propery
    \param key XQPublishAndSubscribeSettingsKey which defines the propery details
    \return True if the property was successfully defined, otherwise return false.
*/
bool XQPublishAndSubscribeUtils::deleteProperty(const XQPublishAndSubscribeSettingsKey& key)
{
    return d->deleteProperty(key);
}
