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

//TESTED_COMPONENT=src/systeminfo

#include <QtTest/QtTest>
#include <QDesktopWidget>

#include "qsysteminfo.h"
#include "qsystemdisplayinfo.h"
#include <QDesktopWidget>

QTM_USE_NAMESPACE

Q_DECLARE_METATYPE(QSystemDisplayInfo::DisplayOrientation);

class tst_QSystemDisplayInfo : public QObject
{
    Q_OBJECT

private slots:
    void tst_displayBrightness();
    void tst_colorDepth();
    void tst_orientation();
    void tst_contrast();
    void tst_getDPIWidth();
    void tst_getDPIHeight();
    void tst_physicalHeight();
    void tst_physicalWidth();

};

void tst_QSystemDisplayInfo::tst_displayBrightness()
{
    QSystemDisplayInfo di;
    QVERIFY(di.displayBrightness(0) > -2);
    QVERIFY(di.displayBrightness(999) == -1);
}

void tst_QSystemDisplayInfo::tst_colorDepth()
{
    QSystemDisplayInfo di;
    int depth = di.colorDepth(0);
    QDesktopWidget wid;
    if(wid.screenCount() > 1) {
        QVERIFY(depth == 0
                || depth == 8
                || depth == 16
                || depth == 24
                || depth == 32
                || depth == 64);
        }
    QVERIFY(di.colorDepth(999) == -1);
}

void tst_QSystemDisplayInfo::tst_orientation()
{
    QSystemDisplayInfo::DisplayOrientation orient = QSystemDisplayInfo::Unknown;
    QSystemDisplayInfo di;
    orient = di.orientation(0);
    QRect availableGeometry = QApplication::desktop()->availableGeometry(0);

    if(availableGeometry.width() > availableGeometry.height()) {
        QVERIFY(orient == QSystemDisplayInfo::Landscape
                || orient == QSystemDisplayInfo::InvertedLandscape);
    } else {
        QVERIFY(orient == QSystemDisplayInfo::Portrait
                || orient == QSystemDisplayInfo::InvertedPortrait);
    }
}

void tst_QSystemDisplayInfo::tst_contrast()
{
    QSystemDisplayInfo di;
    QVERIFY((di.contrast(0) >= 0 && di.contrast(0)< 1.0)
            || di.contrast(0) == -1);
    // contrast levels are between 0 and 1.0
}

void tst_QSystemDisplayInfo::tst_getDPIWidth()
{
    QSystemDisplayInfo di;
    QVERIFY(di.getDPIWidth(0) > -1);
}

void tst_QSystemDisplayInfo::tst_getDPIHeight()
{
    QSystemDisplayInfo di;
    QVERIFY(di.getDPIHeight(0) > -1);
}

void tst_QSystemDisplayInfo::tst_physicalHeight()
{
    QSystemDisplayInfo di;
    QVERIFY(di.physicalHeight(0) > -1);
}

void tst_QSystemDisplayInfo::tst_physicalWidth()
{
    QSystemDisplayInfo di;
    QVERIFY(di.physicalWidth(0) > -1);
}


QTEST_MAIN(tst_QSystemDisplayInfo)
#include "tst_qsystemdisplayinfo.moc"
