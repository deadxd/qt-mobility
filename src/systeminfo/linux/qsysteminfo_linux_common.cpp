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

#include "qsysteminfo_linux_common_p.h"

#include <QTimer>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QVariantMap>
#include <QProcess>
#include <QApplication>
#include <QDesktopWidget>

#include <locale.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/vfs.h>
#include <mntent.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef Q_WS_X11
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#endif

#if !defined(QT_NO_BLKID)
#include <blkid/blkid.h>
#include <linux/fs.h>
#endif // QT_NO_BLKID

#if !defined(QT_NO_BLUEZ)
#include <bluetooth/bluetooth.h>
#include <bluetooth/bnep.h>
#endif // QT_NO_BLUEZ

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/inotify.h>

#include <linux/videodev2.h>

#if !defined(Q_WS_MAEMO_6)
#if !defined V4L2_CAP_MODULATOR
#define V4L2_CAP_MODULATOR 0x00080000
#endif
#endif

#include <linux/fb.h>
#include <fcntl.h>

//we cannot include iwlib.h as the platform may not have it installed
//there we have to go via the kernel's wireless.h
//#include <iwlib.h>
//must be defined to be able to include kernel includes
#ifndef __user
#define __user
#endif

#include <linux/types.h>    /* required for wireless.h */
#include <sys/socket.h>     /* required for wireless.h */
#include <net/if.h>         /* required for wireless.h */

/* A lot of wireless.h have kernel includes which should be protected by
   #ifdef __KERNEL__. They course include errors due to redefinitions of types.
   This prevents those kernel headers being included by Qtopia.
   */
#ifndef _LINUX_IF_H
#define _LINUX_IF_H
#endif
#ifndef _LINUX_SOCKET_H
#define _LINUX_SOCKET_H
#endif
#include <linux/wireless.h>
#include <sys/ioctl.h>

#if !defined(QT_NO_UDEV)
#include "qudevservice_linux_p.h"
#endif // QT_NO_UDEV

#include "qsysteminfo_dbus_p.h"

#if !defined(Q_WS_MAEMO_6) && !defined(Q_WS_MAEMO_5) && !defined(Q_WS_MEEGO)
#define STORAGEPOLL 5 * 1000 //5 seconds for desktop
#else
#define STORAGEPOLL 2 * 60 *1000 // 2 minutes for maemo/meego
#endif

#if !defined(QT_NO_DBUS)
#if !defined(QT_NO_HAL)
bool halIsAvailable;
#endif // QT_NO_HAL

#if !defined(QT_NO_UDISKS)
bool udisksIsAvailable;
#endif // QT_NO_UDISKS

#if !defined(QT_NO_CONNMAN)
bool connmanIsAvailable;
bool ofonoIsAvailable;
#endif // QT_NO_CONNMAN
#endif // QT_NO_DBUS

static struct fb_var_screeninfo* allocFrameBufferInfo(int screen)
{
    QString frameBufferDevicePath = QString("/dev/fb%1").arg(screen);
    int fd;
    struct fb_var_screeninfo *vi = 0;

    if (-1 == (fd = open(frameBufferDevicePath.toStdString().c_str(), O_RDONLY | O_NONBLOCK))) {
        qDebug() << "Failed to open the frame buffer device " << frameBufferDevicePath
                 << strerror(errno);
    } else {
        vi = (struct fb_var_screeninfo *)calloc(1, sizeof *vi);
        if (vi) {
            if (-1 == ioctl(fd, FBIOGET_VSCREENINFO, vi)) {
                qDebug() << "Failed to ioctl() the frame buffer device " << frameBufferDevicePath
                         << strerror(errno);
            }
        }
    }

    if (fd != -1) {
        close(fd);
    }
    return vi;
}

QTM_BEGIN_NAMESPACE

QSystemInfoLinuxCommonPrivate::QSystemInfoLinuxCommonPrivate(QObject *parent)
    : QObject(parent)
    , langTimer(0)
{
}

QSystemInfoLinuxCommonPrivate::~QSystemInfoLinuxCommonPrivate()
{
}

void QSystemInfoLinuxCommonPrivate::connectNotify(const char *signal)
{
    if (QLatin1String(signal) == SIGNAL(currentLanguageChanged(QString))) {
        if (!langTimer) {
            currentLang = currentLanguage();
            langTimer = new QTimer(this);
            connect(langTimer, SIGNAL(timeout()), this, SLOT(pollCurrentLanguage()));
            langTimer->start(5000);
        }
    }
}

void QSystemInfoLinuxCommonPrivate::disconnectNotify(const char *signal)
{
    if (QLatin1String(signal) == SIGNAL(currentLanguageChanged(QString))) {
        currentLang.clear();
        langTimer->stop();
    }
}

void QSystemInfoLinuxCommonPrivate::pollCurrentLanguage()
{
    QString oldLang = currentLang;
    currentLang = currentLanguage();
    if (oldLang != currentLang)
        Q_EMIT currentLanguageChanged(currentLang);
}

QString QSystemInfoLinuxCommonPrivate::currentLanguage() const
{
    QString lang;

    if (currentLang.isEmpty()) {
        lang = QLocale::system().name().left(2);
        if (lang.isEmpty() || lang == QLatin1String("C"))
            lang = QLatin1String("en");
    } else {
        lang = currentLang;
    }

    return lang;
}

bool QSystemInfoLinuxCommonPrivate::fmTransmitterAvailable()
{
    bool available = false;
#if defined( __LINUX_VIDEODEV2_H)
    static const char *devices[] = {"/dev/radio","/dev/radio0",0};
    int fd;
    struct v4l2_capability capability;
    const char *device;
    size_t i = 0;

    while ((device = devices[i++]) && !available) {
        memset(&capability, 0, sizeof(struct v4l2_capability));

        if (-1 != (fd = open(device, O_RDWR)) &&  (-1 != ioctl(fd, VIDIOC_QUERYCAP, &capability))) {
            if ((capability.capabilities & (V4L2_CAP_RADIO | V4L2_CAP_MODULATOR))
                == (V4L2_CAP_RADIO | V4L2_CAP_MODULATOR)) {
                available = true;
            }
        }

        if (fd != -1) {
            close(fd);
            fd = -1;
        }
    }
#endif // __LINUX_VIDEODEV2_H
    return available;
}

bool QSystemInfoLinuxCommonPrivate::hasFeatureSupported(QSystemInfo::Feature feature)
{
#if !defined(QT_NO_UDEV)
    QUdevService udevService;
#endif // QT_NO_UDEV

    bool featureSupported = false;
    switch (feature) {
    case QSystemInfo::BluetoothFeature: {
#if !defined(QT_NO_UDEV)
        return udevService.isSubsystemAvailable(UDEV_SUBSYSTEM_BLUETOOTH);
#endif // QT_NO_UDEV
        const QString sysPath("/sys/class/bluetooth/");
        const QDir sysDir(sysPath);
        QStringList filters;
        filters << "*";
        const QStringList sysList = sysDir.entryList(filters, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        foreach (const QString &dir, sysList) {
            const QFileInfo btFile(sysPath + dir + "/address");
            if (btFile.exists())
                return true;
        }
        break;
    }

    case QSystemInfo::CameraFeature: {
#if !defined(QT_NO_UDEV)
        return udevService.isPropertyAvailable(UDEV_PROPERTY_V4L_CAP, "*:capture:*");
#endif // QT_NO_UDEV
 #if !defined(QT_NO_HAL)
        featureSupported = hasHalUsbFeature(0x06); // image
        if (featureSupported)
            return featureSupported;
 #endif // QT_NO_HAL
        featureSupported = hasSysFeature("video");
        break;
    }

    case QSystemInfo::FmradioFeature: {
#if !defined(QT_NO_UDEV)
        return udevService.isPropertyAvailable(UDEV_PROPERTY_V4L_CAP, "*:radio:*");
#endif // QT_NO_UDEV
        const QString sysPath("/sys/class/video4linux/");
        const QDir sysDir(sysPath);
        QStringList filters;
        filters << "*";
        QStringList sysList = sysDir.entryList(filters, QDir::Dirs, QDir::Name);
        foreach (const QString &dir, sysList) {
            if (dir.contains("radio"))
                featureSupported = true;
        }
        break;
    }

    case QSystemInfo::IrFeature: {
#if !defined(QT_NO_UDEV)
        return udevService.isPropertyAvailable(UDEV_PROPERTY_DRIVER, "*irda*");
#endif // QT_NO_UDEV
#if !defined(QT_NO_HAL)
        featureSupported = hasHalUsbFeature(0xFE);
        if (featureSupported)
            return featureSupported;
#endif // QT_NO_HAL
        featureSupported = hasSysFeature("irda"); //?
        break;
    }

    case QSystemInfo::LedFeature: {
#if !defined(QT_NO_UDEV)
        return udevService.isSubsystemAvailable(UDEV_SUBSYSTEM_LEDS);
#endif // QT_NO_UDEV
        featureSupported = hasSysFeature("led"); //?
        break;
    }

    case QSystemInfo::MemcardFeature: {
#if !defined(QT_NO_UDEV)
        bool ok = udevService.isSubsystemAvailable(UDEV_SUBSYSTEM_MEMCARD);
        if (!ok) {
#if !defined(QT_NO_UDISKS)
            // try harder
            //this only works when a drive is in
            if (udisksAvailable()) {
                QUDisksInterface udisksIface;
                foreach (const QDBusObjectPath &device, udisksIface.enumerateDevices() ) {
                    QUDisksDeviceInterface devIface(device.path());
                    if (devIface.deviceIsDrive()) {
                        if (devIface.driveMedia().contains("flash"))
                            return true;

                        // just guess
                        if (devIface.driveCanDetach() && devIface.deviceIsRemovable() && !devIface.driveIsMediaEjectable())
                            return true;
                    }
                }
            }
#endif // QT_NO_UDISKS
        }
        return ok;
#endif // QT_NO_UDEV
#if !defined(QT_NO_HAL)
        QHalInterface iface;
        if (iface.isValid()) {
            QHalInterface halIface;
            const QStringList halDevices = halIface.findDeviceByCapability("mmc_host");
            foreach (const QString &device, halDevices) {
                QHalDeviceInterface ifaceDevice(device);
                if (ifaceDevice.isValid()) {
                    if (ifaceDevice.getPropertyString("info.subsystem") == "mmc_host")
                        return true;
                    if (ifaceDevice.getPropertyBool("storage.removable"))
                        return true;
                }
            }
        }
#endif // QT_NO_HAL
        break;
    }

    case QSystemInfo::UsbFeature: {
#if !defined(QT_NO_UDEV)
        if (udevService.isPropertyAvailable(UDEV_PROPERTY_DRIVER, UDEV_DRIVER_MUSB))
            return true;
        if(udevService.isPropertyAvailable(UDEV_PROPERTY_DRIVER, UDEV_DRIVER_USB))
            return true;
        if(udevService.isPropertyAvailable(UDEV_PROPERTY_INTERFACE, "usb*"))
            return true;
#endif // QT_NO_UDEV
#if !defined(QT_NO_HAL)
        featureSupported = hasHalDeviceFeature("usb");
        if (featureSupported)
            return featureSupported;
#endif // QT_NO_HAL
        featureSupported = hasSysFeature("usb_host");
        break;
    }

    case QSystemInfo::VibFeature:
#if !defined(QT_NO_UDEV)
        if (udevService.isPropertyAvailable(UDEV_PROPERTY_NAME, "*vibra*"))
            return true;
        if(udevService.isPropertyAvailable(UDEV_PROPERTY_NAME, "*Vibra*"))
            return true;
#endif // QT_NO_UDEV
#if !defined(QT_NO_HAL)
        if (hasHalDeviceFeature("vibrator") || hasHalDeviceFeature("vib"))
            return true;
#endif // QT_NO_HAL
        break;

    case QSystemInfo::WlanFeature: {
#if !defined(QT_NO_UDEV)
        return udevService.isSubsystemAvailable(UDEV_SUBSYSTEM_WLAN);
#endif // QT_NO_UDEV
#if !defined(QT_NO_HAL)
        QHalInterface iface;
        if (iface.isValid()) {
            const QStringList list = iface.findDeviceByCapability("net.80211");
            if (!list.isEmpty()) {
                featureSupported = true;
                break;
            }
        }
#endif // QT_NO_HAL
        featureSupported = hasSysFeature("80211");
        break;
    }

    case QSystemInfo::SimFeature: {
#if !defined(QT_NO_CONNMAN)
        if (ofonoAvailable()) {
            QOfonoManagerInterface ofonoManager(this);
            QString modempath = ofonoManager.currentModem().path();
            if (!modempath.isEmpty()) {
                QOfonoSimInterface simInterface(modempath, this);
                return simInterface.isValid();
            }
        }
#endif // QT_NO_CONNMAN
        break;
    }

    case QSystemInfo::LocationFeature:
#if !defined(QT_NO_UDEV)
        return udevService.isPropertyAvailable(UDEV_PROPERTY_DRIVER, "*gps*");
#endif // QT_NO_UDEV
#if !defined(QT_NO_HAL)
        featureSupported = hasHalDeviceFeature("gps"); //might not always be true
        if (featureSupported)
            return featureSupported;

#endif // QT_NO_HAL
        break;

    case QSystemInfo::VideoOutFeature: {
#if !defined(QT_NO_UDEV)
        return udevService.isPropertyAvailable(UDEV_PROPERTY_V4L_CAP, "*:video_output:*");
#endif // QT_NO_UDEV
        const QString sysPath("/sys/class/video4linux/");
        const QDir sysDir(sysPath);
        QStringList filters;
        filters << "*";
        const QStringList sysList = sysDir.entryList(filters, QDir::Dirs, QDir::Name);
        if (sysList.contains("video"))
            featureSupported = true;
        break;
    }

    case QSystemInfo::HapticsFeature:
        break;

    case QSystemInfo::FmTransmitterFeature:
        featureSupported = fmTransmitterAvailable();
        break;

    default:
        featureSupported = false;
        break;
    };

    return featureSupported;
 }

#if !defined(QT_NO_HAL)
bool QSystemInfoLinuxCommonPrivate::hasHalDeviceFeature(const QString &param)
{
    QHalInterface halIface;
    const QStringList halDevices = halIface.getAllDevices();
    foreach (const QString &device, halDevices) {
        if (device.contains(param))
            return true;
    }
    return false;
}

bool QSystemInfoLinuxCommonPrivate::hasHalUsbFeature(qint32 usbClass)
{
    QHalInterface halIface;
    const QStringList halDevices = halIface.findDeviceByCapability("usb_device");
    foreach (const QString &device, halDevices) {
        QHalDeviceInterface ifaceDevice(device);
        if (ifaceDevice.isValid()) {
            if (ifaceDevice.getPropertyString("info.subsystem") == "usb_device") {
                if (ifaceDevice.getPropertyInt("usb.interface.class") == usbClass)
                    return true;
            }
        }
    }
    return false;
}
#endif // QT_NO_HAL

QString QSystemInfoLinuxCommonPrivate::version(QSystemInfo::Version type, const QString &parameter)
{
    Q_UNUSED(parameter)

    switch(type) {
    case QSystemInfo::Firmware: {
        QFile firmware("/proc/sys/kernel/osrelease");
        if (firmware.open(QIODevice::ReadOnly))
            return firmware.readAll().simplified();
        break;
    }

    case QSystemInfo::Os: {
        QFile os("/etc/issue");
        if (os.open(QIODevice::ReadOnly)) {
            QByteArray content = os.readAll();
            if (!content.isEmpty()) {
                QList<QByteArray> list(content.split(' '));
                bool ok = false;
                foreach (const QByteArray &field, list) {
                    field.toDouble(&ok);
                    if (ok)
                        return field;
                }
            }
        }

        break;
    }

    default:
        break;
    };

    return QString();
}

QString QSystemInfoLinuxCommonPrivate::currentCountryCode() const
{
    return QLocale::system().name().mid(3,2);
}

bool QSystemInfoLinuxCommonPrivate::hasSysFeature(const QString &featureStr)
{
    const QString sysPath("/sys/class/");
    const QDir sysDir(sysPath);
    QStringList filters;
    filters << QLatin1String("*");
    const QStringList sysList = sysDir.entryList(filters, QDir::Dirs, QDir::Name);
    foreach (const QString &dir, sysList) {
        const QDir sysDir2(sysPath + dir);
        if (dir.contains(featureStr)) {
            const QStringList sysList2 = sysDir2.entryList(filters, QDir::Dirs, QDir::Name);
            if (!sysList2.isEmpty())
                return true;
        }
    }
    return false;
}

QSystemNetworkInfoLinuxCommonPrivate::QSystemNetworkInfoLinuxCommonPrivate(QObject *parent)
    : QObject(parent)
{
#if !defined(QT_NO_CONNMAN)
    connmanIsAvailable = connmanAvailable();
    if (connmanIsAvailable)
        initConnman();

    ofonoIsAvailable = ofonoAvailable();
    if (ofonoIsAvailable)
        initOfono();
#endif // QT_NO_CONNMAN
}

QSystemNetworkInfoLinuxCommonPrivate::~QSystemNetworkInfoLinuxCommonPrivate()
{
}

QSystemNetworkInfo::NetworkStatus QSystemNetworkInfoLinuxCommonPrivate::networkStatus(QSystemNetworkInfo::NetworkMode mode)
{
    switch(mode) {
    case QSystemNetworkInfo::WlanMode: {
        const QString sysfsNetPath("/sys/class/net/");
        const QStringList dirs = QDir(sysfsNetPath).entryList(QStringList() << "wlan*");
        if (dirs.size() == 0)
            return QSystemNetworkInfo::UndefinedStatus;
        foreach (const QString &dir, dirs) {
            QFile carrier(sysfsNetPath + dir + "/carrier");
            if (carrier.open(QIODevice::ReadOnly)) {
                char state;
                if (carrier.read(&state, 1) == 1 && state == '1')
                    return QSystemNetworkInfo::Connected;
            }
        }
        return QSystemNetworkInfo::NoNetworkAvailable;
    }

    case QSystemNetworkInfo::EthernetMode: {
        const QString sysfsNetPath("/sys/class/net/");
        const QStringList dirs = QDir(sysfsNetPath).entryList(QStringList() << "eth*" << "usb*");
        if (dirs.size() == 0)
            return QSystemNetworkInfo::UndefinedStatus;
        foreach (const QString &dir, dirs) {
            QFile carrier(sysfsNetPath + dir + "/carrier");
            if (carrier.open(QIODevice::ReadOnly)) {
                char state;
                if (carrier.read(&state, 1) == 1 && state == '1')
                    return QSystemNetworkInfo::Connected;
            }
        }
        return QSystemNetworkInfo::NoNetworkAvailable;
    }

    case QSystemNetworkInfo::BluetoothMode:
        return getBluetoothNetStatus();

//    case QSystemNetworkInfo::GsmMode:
//    case QSystemNetworkInfo::CdmaMode:
//    case QSystemNetworkInfo::WcdmaMode:
//    case QSystemNetworkInfo::WimaxMode:
//    case QSystemNetworkInfo::LteMode:
    default:
        break;
    };

#if !defined(QT_NO_CONNMAN)
    if (connmanIsAvailable) {
        QDBusObjectPath path = connmanManager->lookupService(interfaceForMode(mode).name());
        if (!path.path().isEmpty()) {
            QConnmanServiceInterface serviceIface(path.path(), this);
            if (mode == QSystemNetworkInfo::GsmMode
                || mode == QSystemNetworkInfo::CdmaMode
                || mode == QSystemNetworkInfo::WcdmaMode) {
                return getOfonoStatus(mode);
            }
//            if (serviceIface.isRoaming())
//                return QSystemNetworkInfo::Roaming;
            if (serviceIface.isValid())
                return stateToStatus(serviceIface.getState());
        }
    }
#endif // QT_NO_CONNMAN

    return QSystemNetworkInfo::UndefinedStatus;
}

#if !defined(QT_NO_CONNMAN)
QSystemNetworkInfo::NetworkStatus QSystemNetworkInfoLinuxCommonPrivate::getOfonoStatus(QSystemNetworkInfo::NetworkMode mode)
{
    QSystemNetworkInfo::NetworkStatus networkStatus = QSystemNetworkInfo::UndefinedStatus;
    if (ofonoIsAvailable) {
        QString modempath = ofonoManager->currentModem().path();
        if (!modempath.isEmpty()) {
            QOfonoNetworkRegistrationInterface ofonoNetwork(modempath, this);
            if (ofonoNetwork.isValid()) {
                foreach (const QDBusObjectPath &op,ofonoNetwork.getOperators()) {
                    if (!op.path().isEmpty()) {
                        QOfonoNetworkOperatorInterface opIface(op.path(), this);
                        foreach (const QString &opTech, opIface.getTechnologies()) {
                            if (mode == ofonoTechToMode(opTech)) {
                                networkStatus = ofonoStatusToStatus(ofonoNetwork.getStatus());
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    return networkStatus;
}
#endif // QT_NO_CONNMAN

QString QSystemNetworkInfoLinuxCommonPrivate::networkName(QSystemNetworkInfo::NetworkMode mode)
{
    switch(mode) {
    case QSystemNetworkInfo::WlanMode: {
        const QString sysfsNetPath("/sys/class/net/");
        const QStringList dirs = QDir(sysfsNetPath).entryList(QStringList() << "wlan*");
        foreach (const QString &dir, dirs) {
            int sock = socket(PF_INET, SOCK_DGRAM, 0);
            if (sock > 0) {
                char buffer[IW_ESSID_MAX_SIZE + 1];
                iwreq iwInfo;

                iwInfo.u.essid.pointer = (caddr_t)&buffer;
                iwInfo.u.essid.length = IW_ESSID_MAX_SIZE + 1;
                iwInfo.u.essid.flags = 0;

                strncpy(iwInfo.ifr_name, dir.toLocal8Bit().data(), IFNAMSIZ);

                if (ioctl(sock, SIOCGIWESSID, &iwInfo) == 0) {
                    close(sock);
                    return (const char *)iwInfo.u.essid.pointer;
                }

                close(sock);
            }
        }
        break;
    }

    case QSystemNetworkInfo::EthernetMode: {
        char domainName[64];
        if (getdomainname(domainName, 64) == 0) {
            if (strcmp(domainName, "(none)") != 0)
                return domainName;
        }
        return QString("Wired");
    }

    case QSystemNetworkInfo::BluetoothMode: {
        const QString sysfsBluetoothPath("/sys/class/bluetooth/");
        const QStringList dirs = QDir(sysfsBluetoothPath).entryList(QStringList() << "*");
        foreach (const QString &dir, dirs) {
            QFile carrier(sysfsBluetoothPath + dir + "/name");
            if (carrier.open(QIODevice::ReadOnly))
                return carrier.readAll().simplified();
        }
        break;
    }

//    case QSystemNetworkInfo::GsmMode:
//    case QSystemNetworkInfo::CdmaMode:
//    case QSystemNetworkInfo::WcdmaMode:
//    case QSystemNetworkInfo::WimaxMode:
//    case QSystemNetworkInfo::LteMode:
    default:
        break;
    };

#if !defined(QT_NO_CONNMAN)
    QString tech = modeToTechnology(mode);
    if (tech.contains("cellular")) {
        if (ofonoIsAvailable) {
            QString modenPath = ofonoManager->currentModem().path();
            if (!modenPath.isEmpty()) {
                QOfonoNetworkRegistrationInterface ofonoOpNetwork(modenPath, this);
                if (ofonoOpNetwork.isValid())
                    return ofonoOpNetwork.getOperatorName();
            }
        }
    } else {
        if (connmanIsAvailable) {
            QDBusObjectPath path = connmanManager->lookupService(tech);
            if (!path.path().isEmpty()) {
                QConnmanServiceInterface service(path.path(), this);
                if (service.isValid()) {
                    QString name = service.getName();
                    if (name.isEmpty())
                        name = service.getAPN();

                    if (name.isEmpty())
                        name = service.getDomains().join(" ");

                    if (name.isEmpty())
                        name = service.getMacAddress();

                    if (!name.isEmpty())
                        return name;
                }
            }
        }
    }
#endif // QT_NO_CONNMAN

    return QString();
}

QString QSystemNetworkInfoLinuxCommonPrivate::macAddress(QSystemNetworkInfo::NetworkMode mode)
{
    switch(mode) {
    case QSystemNetworkInfo::WlanMode: {
        const QString sysfsNetPath("/sys/class/net/");
        const QStringList dirs = QDir(sysfsNetPath).entryList(QStringList() << "wlan*");
        foreach (const QString &dir, dirs) {
            QFile carrier(sysfsNetPath + dir + "/address");
            if (carrier.open(QIODevice::ReadOnly))
                return carrier.readAll().simplified();
        }
        break;
    }

    case QSystemNetworkInfo::EthernetMode: {
        const QString sysfsNetPath("/sys/class/net/");
        const QStringList dirs = QDir(sysfsNetPath).entryList(QStringList() << "eth*" << "usb*");
        foreach (const QString &dir, dirs) {
            QFile carrier(sysfsNetPath + dir + "/address");
            if (carrier.open(QIODevice::ReadOnly))
                return carrier.readAll().simplified();
        }
        break;
    }

    case QSystemNetworkInfo::BluetoothMode: {
        const QString sysfsBluetoothPath("/sys/class/bluetooth/");
        const QStringList dirs = QDir(sysfsBluetoothPath).entryList(QStringList() << "*");
        foreach (const QString &dir, dirs) {
            QFile carrier(sysfsBluetoothPath + dir + "/address");
            if (carrier.open(QIODevice::ReadOnly))
                return carrier.readAll().simplified();
        }
        break;
    }

//    case QSystemNetworkInfo::GsmMode:
//    case QSystemNetworkInfo::CdmaMode:
//    case QSystemNetworkInfo::WcdmaMode:
//    case QSystemNetworkInfo::WimaxMode:
//    case QSystemNetworkInfo::LteMode:
    default:
        break;
    };

#if !defined(QT_NO_CONNMAN)
    if (connmanIsAvailable) {
        foreach (const QString &servicePath, connmanManager->getServices()) {
            QConnmanServiceInterface *serviceIface = new QConnmanServiceInterface(servicePath, this);
            if (serviceIface->getType() == modeToTechnology(mode)) {
                if (mode ==  QSystemNetworkInfo::WlanMode
                    || mode == QSystemNetworkInfo::EthernetMode
                    || mode == QSystemNetworkInfo::BluetoothMode) {
                    return QNetworkInterface::interfaceFromName(serviceIface->getInterface()).hardwareAddress();
                } else {
//                    QOfonoConnectionContextInterface context(servicePath);
//                    if (context.active())
//                        return QNetworkInterface::interfaceFromName(context.interface()).hardwareAddress();
                }
            }
        }
    }
#endif // QT_NO_CONNMAN

    return QString();
}

QSystemNetworkInfo::NetworkStatus QSystemNetworkInfoLinuxCommonPrivate::getBluetoothNetStatus()
{
#if !defined(QT_NO_BLUEZ)
    int ctl = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_BNEP);
    if (ctl < 0) {
        qDebug() << "Cannot open bnep socket";
        return QSystemNetworkInfo::UndefinedStatus;
    }

    struct bnep_conninfo info[36];
    struct bnep_connlist_req req;

    req.ci = info;
    req.cnum = 36;

    if (ioctl(ctl, BNEPGETCONNLIST, &req) < 0) {
        qDebug() << "Cannot get bnep connection list.";
        return QSystemNetworkInfo::UndefinedStatus;
    }

    for (uint j = 0; j< req.cnum; j++) {
        if (info[j].state == BT_CONNECTED) {
            return QSystemNetworkInfo::Connected;
        }
    }

    close(ctl);
#endif // QT_NO_BLUEZ

    return QSystemNetworkInfo::UndefinedStatus;
}

int QSystemNetworkInfoLinuxCommonPrivate::networkSignalStrength(QSystemNetworkInfo::NetworkMode mode)
{
    switch(mode) {
    case QSystemNetworkInfo::WlanMode: {
        QString iface = interfaceForMode(QSystemNetworkInfo::WlanMode).name();
        QFile file("/proc/net/wireless");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return -1;

        QTextStream in(&file);
        QString line = in.readLine();
        while (!line.isNull()) {
            if (line.left(6).contains(iface)) {
                QString token = line.section(" ", 4, 5).simplified();
                token.chop(1);
                bool ok;
                int signalStrength = (int)rint((log(token.toInt(&ok)) / log(92)) * 100.0);
                if (ok)
                    return signalStrength;
                else
                    return -1;
            }
            line = in.readLine();
        }

        break;
    }

    case QSystemNetworkInfo::EthernetMode:
        if (networkStatus(QSystemNetworkInfo::EthernetMode) == QSystemNetworkInfo::Connected)
            return 100;
        else
            return -1;

//    case QSystemNetworkInfo::BluetoothMode:
//    case QSystemNetworkInfo::GsmMode:
//    case QSystemNetworkInfo::CdmaMode:
//    case QSystemNetworkInfo::WcdmaMode:
//    case QSystemNetworkInfo::WimaxMode:
//    case QSystemNetworkInfo::LteMode:
    default:
        break;
    };

#if !defined(QT_NO_CONNMAN)
    if (connmanIsAvailable) {
        QString tech = modeToTechnology(mode);
        if (ofonoIsAvailable && tech.contains("cellular")) {
            QString modempath = ofonoManager->currentModem().path();
            if (!modempath.isEmpty()) {
                QOfonoNetworkRegistrationInterface ofonoNetwork(modempath, this);
                if (ofonoNetwork.isValid())
                    return ofonoNetwork.getSignalStrength();
            }
        } else {
            qint32 signalStrength = 0;
            QDBusObjectPath path = connmanManager->lookupService(tech);
            if (!path.path().isEmpty()) {
                QConnmanServiceInterface service(path.path(), this);
                if (service.isValid()) {
                    signalStrength = service.getSignalStrength();
                    if (signalStrength == 0 && (service.getState() == "ready"
                                                || service.getState() == "online")) {
                        signalStrength = 100;
                    }
                }
            }
            return signalStrength;
        }
    }
#endif // QT_NO_CONNMAN

    return -1;
}

QNetworkInterface QSystemNetworkInfoLinuxCommonPrivate::interfaceForMode(QSystemNetworkInfo::NetworkMode mode)
{
    switch(mode) {
    case QSystemNetworkInfo::WlanMode: {
        const QStringList dirs = QDir("/sys/class/net/").entryList(QStringList() << "wlan*");
        foreach (const QString &dir, dirs) {
            QNetworkInterface interface = QNetworkInterface::interfaceFromName(dir);
            if (interface.isValid())
                return interface;
        }
        break;
    }

    case QSystemNetworkInfo::EthernetMode: {
        const QStringList dirs = QDir("/sys/class/net/").entryList(QStringList() << "eth*" << "usb*");
        foreach (const QString &dir, dirs) {
            QNetworkInterface interface = QNetworkInterface::interfaceFromName(dir);
            if (interface.isValid())
                return interface;
        }
        break;
    }

//    case QSystemNetworkInfo::BluetoothMode:
//    case QSystemNetworkInfo::GsmMode:
//    case QSystemNetworkInfo::CdmaMode:
//    case QSystemNetworkInfo::WcdmaMode:
//    case QSystemNetworkInfo::WimaxMode:
//    case QSystemNetworkInfo::LteMode:
    default:
        break;
    };

#if !defined(QT_NO_CONNMAN)
    if (connmanIsAvailable) {
        foreach (const QString &servicePath, connmanManager->getServices()) {
            QConnmanServiceInterface *serviceIface = new QConnmanServiceInterface(servicePath, this);
            if (serviceIface->getType() == modeToTechnology(mode))
                return QNetworkInterface::interfaceFromName(serviceIface->getInterface());
        }
   }
#endif // QT_NO_CONNMAN

    return QNetworkInterface();
}

#if !defined(QT_NO_CONNMAN)
void QSystemNetworkInfoLinuxCommonPrivate::initConnman()
{
    connmanManager = new QConnmanManagerInterface(this);
    connect(connmanManager, SIGNAL(propertyChangedContext(QString,QString,QDBusVariant)),
            this, SLOT(connmanPropertyChangedContext(QString,QString,QDBusVariant)));

    foreach (const QString &servicePath, connmanManager->getServices()) {
        QConnmanServiceInterface *serviceIface = new QConnmanServiceInterface(servicePath, this);

        connect(serviceIface, SIGNAL(propertyChangedContext(QString,QString,QDBusVariant)),
                this, SLOT(connmanServicePropertyChangedContext(QString,QString,QDBusVariant)));
    }
}

void QSystemNetworkInfoLinuxCommonPrivate::initOfono()
{
    ofonoManager = new QOfonoManagerInterface(this);
    connect(ofonoManager, SIGNAL(propertyChangedContext(QString,QString,QDBusVariant)),
            this, SLOT(ofonoPropertyChangedContext(QString,QString,QDBusVariant)));

    foreach (const QDBusObjectPath &path, ofonoManager->getModems())
        initModem(path.path());
}

void QSystemNetworkInfoLinuxCommonPrivate::initModem(const QString &path)
{
    QOfonoNetworkRegistrationInterface *ofonoNetworkInterface = new QOfonoNetworkRegistrationInterface(path, this);
    connect(ofonoNetworkInterface, SIGNAL(propertyChangedContext(QString,QString,QDBusVariant)),
            this, SLOT(ofonoNetworkPropertyChangedContext(QString,QString,QDBusVariant)));
}

QString QSystemNetworkInfoLinuxCommonPrivate::modeToTechnology(QSystemNetworkInfo::NetworkMode mode)
{
    switch(mode) {
    case QSystemNetworkInfo::WlanMode:
        return "wifi";
    case QSystemNetworkInfo::EthernetMode:
        return "ethernet";
    case QSystemNetworkInfo::BluetoothMode:
        return "bluetooth";
    case QSystemNetworkInfo::GsmMode:
    case QSystemNetworkInfo::CdmaMode:
    case QSystemNetworkInfo::WcdmaMode:
        return "cellular";
    case QSystemNetworkInfo::WimaxMode:
        return "wimax";
    case QSystemNetworkInfo::LteMode:
        return "lte";
    default:
        break;
    };
    return QString();
}

QSystemNetworkInfo::NetworkStatus QSystemNetworkInfoLinuxCommonPrivate::stateToStatus(const QString &state)
{
    if (state == "idle") {
        return QSystemNetworkInfo::NoNetworkAvailable;
    } else if (state == "failure") {
        return QSystemNetworkInfo::Denied;
    } else if (state == "association" || state == "configuration" || state == "login") {
        return QSystemNetworkInfo::Searching;
    } else if (state == "ready" || state == "online") {
        return QSystemNetworkInfo::Connected;
    }
    return QSystemNetworkInfo::UndefinedStatus;
}

QSystemNetworkInfo::NetworkMode QSystemNetworkInfoLinuxCommonPrivate::typeToMode(const QString &type)
{
    if (type == "ethernet") {
        return QSystemNetworkInfo::EthernetMode;
    } else if (type == "wifi") {
        return QSystemNetworkInfo::WlanMode;
    } else if (type == "bluetooth") {
        return QSystemNetworkInfo::BluetoothMode;
    } else if (type == "wimax") {
        return QSystemNetworkInfo::WimaxMode;
    } else if (type == "cellular") {
        if (ofonoIsAvailable) {
            QOfonoNetworkRegistrationInterface ofonoNetwork(ofonoManager->currentModem().path(), this);
            return ofonoTechToMode(ofonoNetwork.getTechnology());
        }
    }
    return QSystemNetworkInfo::UnknownMode;
}

QSystemNetworkInfo::NetworkMode QSystemNetworkInfoLinuxCommonPrivate::ofonoTechToMode(const QString &ofonoTech)
{
    if (ofonoTech == "gsm")
        return QSystemNetworkInfo::GsmMode;
    else if (ofonoTech == "umts")
        return QSystemNetworkInfo::WcdmaMode;
    else if (ofonoTech == "lte")
        return QSystemNetworkInfo::LteMode;
    return QSystemNetworkInfo::GsmMode; //its ofono, default to gsm
}

QSystemNetworkInfo::NetworkStatus QSystemNetworkInfoLinuxCommonPrivate::ofonoStatusToStatus(const QString &status)
{
    QSystemNetworkInfo::NetworkStatus networkStatus = QSystemNetworkInfo::UndefinedStatus;
    if (status == "unregistered")
        networkStatus = QSystemNetworkInfo::EmergencyOnly;
    else if (status == "registered")
        networkStatus = QSystemNetworkInfo::HomeNetwork;
    else if (status == "searching")
        networkStatus = QSystemNetworkInfo::Searching;
    else if (status == "denied")
        networkStatus = QSystemNetworkInfo::Denied;
    else if (status == "roaming")
        networkStatus = QSystemNetworkInfo::Roaming;
    return networkStatus;
}

void QSystemNetworkInfoLinuxCommonPrivate::connmanPropertyChangedContext(const QString &path, const QString &item, const QDBusVariant &value)
{
    if (item == "DefaultTechnology") {
        // qDebug() << value.variant();
        QConnmanServiceInterface serviceIface(path, this);
        Q_EMIT networkNameChanged(typeToMode(value.variant().toString()), serviceIface.getName());
        Q_EMIT networkModeChanged(typeToMode(value.variant().toString()));
    }
}

void QSystemNetworkInfoLinuxCommonPrivate::connmanServicePropertyChangedContext(const QString &path, const QString &item, const QDBusVariant &value)
{
    if (item == "State") {
        QConnmanServiceInterface serviceIface(path, this);
        QString state = value.variant().toString();
        QSystemNetworkInfo::NetworkMode mode = typeToMode(serviceIface.getType());
        Q_EMIT networkStatusChanged(mode, stateToStatus(state));
        if (state == "idle" || state == "failure") {
            Q_EMIT networkNameChanged(mode, QString());
            Q_EMIT networkSignalStrengthChanged(mode, 0);
        } else  if (state == "ready" || state == "online") {
            Q_EMIT networkNameChanged(mode, serviceIface.getName());
        }
        if (serviceIface.isRoaming()) {
            Q_EMIT networkStatusChanged(mode, QSystemNetworkInfo::Roaming);
        }
    } else if (item == "Strength") {
        QConnmanServiceInterface serviceIface(path, this);
        Q_EMIT networkSignalStrengthChanged(typeToMode(serviceIface.getType()), value.variant().toUInt());
    } else if (item == "Roaming") {
        QConnmanServiceInterface serviceIface(path, this);
        Q_EMIT networkStatusChanged(typeToMode(serviceIface.getType()), QSystemNetworkInfo::Roaming);
    } else if (item == "MCC") {
        Q_EMIT currentMobileCountryCodeChanged(value.variant().toString());
    } else if (item == "MNC") {
        Q_EMIT currentMobileNetworkCodeChanged(value.variant().toString());
    } else if (item == "Mode") {
        //"gprs" "edge" "umts"
        QConnmanServiceInterface serviceIface(path, this);
        if (serviceIface.getType() == "cellular")
            Q_EMIT networkModeChanged(ofonoTechToMode(value.variant().toString()));
    }
}

void QSystemNetworkInfoLinuxCommonPrivate::ofonoPropertyChangedContext(const QString &/*path*/, const QString &item, const QDBusVariant &value)
{
    if (item == "Modems") {
        QList<QDBusObjectPath> modems = qdbus_cast<QList<QDBusObjectPath> >(value.variant());
        if (modems.count() > knownModems.count()) {
            //add a modem
            foreach (const QDBusObjectPath &path, modems) {
                if (!knownModems.contains(path.path()))
                    initModem(path.path());
            }
        }  else {
            //remove one
            QStringList newModemList;
            foreach (const QDBusObjectPath &path, modems)
                newModemList << path.path();

            foreach (const QString &path, knownModems) {
                if (!newModemList.contains(path))
                    knownModems.removeAll(path);
            }
        }
    } else if (item == "Bearer") {
        Q_EMIT cellDataTechnologyChanged(ofonoTechToCDT(value.variant().toString()));
    }
}

void QSystemNetworkInfoLinuxCommonPrivate::ofonoNetworkPropertyChangedContext(const QString &path, const QString &item, const QDBusVariant &value)
{
    if (item == "Strength") {
        QOfonoNetworkRegistrationInterface interface(path);
        Q_EMIT networkSignalStrengthChanged(ofonoTechToMode(interface.getTechnology()),value.variant().toInt());
    } else if (item == "Status") {
        QOfonoNetworkRegistrationInterface interface(path);
        Q_EMIT networkStatusChanged(ofonoTechToMode(interface.getTechnology()), ofonoStatusToStatus(value.variant().toString()));
    } else if (item == "CellId") {
        Q_EMIT cellIdChanged(value.variant().toUInt());
    } else if (item == "Name") {
        QOfonoNetworkRegistrationInterface interface(path);
        Q_EMIT networkNameChanged(ofonoTechToMode(interface.getTechnology()), value.variant().toString());
    }
}

QSystemNetworkInfo::CellDataTechnology QSystemNetworkInfoLinuxCommonPrivate::ofonoTechToCDT(const QString &tech)
{
    if (tech == "gsm")
        return QSystemNetworkInfo::GprsDataTechnology;
    else if (tech == "edge")
        return QSystemNetworkInfo::EdgeDataTechnology;
    else if (tech == "umts")
        return QSystemNetworkInfo::UmtsDataTechnology;
    else if (tech == "hspa" || tech == "hsdpa" || tech == "hsupa")
        return QSystemNetworkInfo::HspaDataTechnology;
    return QSystemNetworkInfo::UnknownDataTechnology;
}
#endif // QT_NO_CONNMAN

QSystemNetworkInfo::NetworkMode QSystemNetworkInfoLinuxCommonPrivate::currentMode()
{
#if !defined(QT_NO_CONNMAN)
    if (connmanIsAvailable) {
        QString mode = connmanManager->getDefaultTechnology();
        if (mode == "wifi")
            return QSystemNetworkInfo::WlanMode;
        else if (mode == "ethernet")
            return QSystemNetworkInfo::EthernetMode;
        else if (mode == "bluetooth")
            return QSystemNetworkInfo::BluetoothMode;
        else if (mode == "wimax")
            return QSystemNetworkInfo::WimaxMode;
        else if (mode == "cellular")
            return typeToMode(mode);
    }
#endif

    if (networkStatus(QSystemNetworkInfo::EthernetMode) == QSystemNetworkInfo::Connected)
        return QSystemNetworkInfo::EthernetMode;
    else if (networkStatus(QSystemNetworkInfo::WlanMode) == QSystemNetworkInfo::Connected)
        return QSystemNetworkInfo::WlanMode;
    else if (networkStatus(QSystemNetworkInfo::BluetoothMode) == QSystemNetworkInfo::Connected)
        return QSystemNetworkInfo::BluetoothMode;
    else if (networkStatus(QSystemNetworkInfo::WimaxMode) == QSystemNetworkInfo::Connected)
        return QSystemNetworkInfo::WimaxMode;
    else if (networkStatus(QSystemNetworkInfo::LteMode) == QSystemNetworkInfo::Connected)
        return QSystemNetworkInfo::LteMode;
    else if (networkStatus(QSystemNetworkInfo::WcdmaMode) == QSystemNetworkInfo::Connected)
        return QSystemNetworkInfo::WcdmaMode;
    else if (networkStatus(QSystemNetworkInfo::CdmaMode) == QSystemNetworkInfo::Connected)
        return QSystemNetworkInfo::GsmMode;
    else if (networkStatus(QSystemNetworkInfo::GsmMode) == QSystemNetworkInfo::Connected)
        return QSystemNetworkInfo::WlanMode;
    else
        return QSystemNetworkInfo::UnknownMode;
}

int QSystemNetworkInfoLinuxCommonPrivate::cellId()
{
#if !defined(QT_NO_CONNMAN)
    if (ofonoIsAvailable) {
        QString modempath = ofonoManager->currentModem().path();
        if (!modempath.isEmpty()) {
            QOfonoNetworkRegistrationInterface ofonoNetwork(modempath, this);
            return ofonoNetwork.getCellId();
        }
    }
#endif

    return -1;
}

int QSystemNetworkInfoLinuxCommonPrivate::locationAreaCode()
{
#if !defined(QT_NO_CONNMAN)
    if (ofonoIsAvailable) {
        QString modempath = ofonoManager->currentModem().path();
        if (!modempath.isEmpty()) {
            QOfonoNetworkRegistrationInterface ofonoNetwork(modempath, this);
            return ofonoNetwork.getLac();
        }
    }
#endif

    return -1;
}

QString QSystemNetworkInfoLinuxCommonPrivate::currentMobileCountryCode()
{
#if !defined(QT_NO_CONNMAN)
    if (ofonoIsAvailable) {
        QString modempath = ofonoManager->currentModem().path();
        if (!modempath.isEmpty()) {
            QOfonoNetworkRegistrationInterface ofonoNetworkOperator(modempath, this);
            foreach (const QDBusObjectPath &opPath, ofonoNetworkOperator.getOperators()) {
                if (!opPath.path().isEmpty()) {
                    QOfonoNetworkOperatorInterface netop(opPath.path(), this);
                    if (netop.getStatus() == "current")
                        return netop.getMcc();
                }
            }
        }
    }
#endif

    return QString();
}

QString QSystemNetworkInfoLinuxCommonPrivate::currentMobileNetworkCode()
{
#if !defined(QT_NO_CONNMAN)
    if (ofonoIsAvailable) {
        QString modempath = ofonoManager->currentModem().path();
        if (!modempath.isEmpty()) {
            QOfonoNetworkRegistrationInterface ofonoNetworkOperator(modempath, this);
            foreach (const QDBusObjectPath &opPath, ofonoNetworkOperator.getOperators()) {
                if (!opPath.path().isEmpty()) {
                    QOfonoNetworkOperatorInterface netop(opPath.path(), this);
                    return netop.getMnc();
                }
            }
        }
    }
#endif

    return QString();
}

QString QSystemNetworkInfoLinuxCommonPrivate::homeMobileCountryCode()
{
#if !defined(QT_NO_CONNMAN)
    if (ofonoIsAvailable) {
        QString modem = ofonoManager->currentModem().path();
        if (!modem.isEmpty()) {
            QOfonoSimInterface simInterface(modem, this);
            if (simInterface.isPresent()) {
                QString home = simInterface.getHomeMcc();
                if (!home.isEmpty())
                    return home;
                else
                    return currentMobileCountryCode();
            }
        }
    }
#endif

    return QString();
}

QString QSystemNetworkInfoLinuxCommonPrivate::homeMobileNetworkCode()
{
#if !defined(QT_NO_CONNMAN)
    if (ofonoIsAvailable) {
        QString modem = ofonoManager->currentModem().path();
        if (!modem.isEmpty()) {
            QOfonoSimInterface simInterface(modem, this);
            if (simInterface.isPresent()) {
                QString home = simInterface.getHomeMnc();
                if (!home.isEmpty()) {
                    return home;
                } else {
                    QOfonoNetworkRegistrationInterface netIface(modem, this);
                    if (netIface.getStatus() == "registered") {
                        //on home network, not roaming
                        return currentMobileNetworkCode();
                    }
                }
            }
        }
    }
#endif

    return QString();
}

QSystemNetworkInfo::CellDataTechnology QSystemNetworkInfoLinuxCommonPrivate::cellDataTechnology()
{
#if !defined(QT_NO_CONNMAN)
    if (ofonoIsAvailable) {
        QString modem = ofonoManager->currentModem().path();
        if (!modem.isEmpty()) {
            QOfonoConnectionManagerInterface cmInterface(modem, this);
            QString bearer = cmInterface.bearer();
            return ofonoTechToCDT(bearer);
        }
    }
#endif

    return QSystemNetworkInfo::UnknownDataTechnology;
}

QSystemDisplayInfoLinuxCommonPrivate *QSystemDisplayInfoLinuxCommonPrivate::self = 0;

#if defined(Q_WS_X11)
bool q_XEventFilter(void *message)
{
    XEvent *xev = (XEvent *)message;
    if (xev->type == QTM_NAMESPACE::QSystemDisplayInfoLinuxCommonPrivate::instance()->xEventBase + RRScreenChangeNotify) {
        int rot = QTM_NAMESPACE::QSystemDisplayInfoLinuxCommonPrivate::instance()->lastRotation;

        XRRScreenChangeNotifyEvent *rrev = (XRRScreenChangeNotifyEvent *)(xev);
        if (rrev->rotation != rot) {
            rot = rrev->rotation;
            QTM_NAMESPACE::QSystemDisplayInfoLinuxCommonPrivate::instance()->emitOrientationChanged(rot);
            QTM_NAMESPACE::QSystemDisplayInfoLinuxCommonPrivate::instance()->lastRotation = rot;
        }
        return true;
    }
    return false;
}
#endif // Q_WS_X11

QSystemDisplayInfoLinuxCommonPrivate::QSystemDisplayInfoLinuxCommonPrivate(QObject *parent)
    : QObject(parent)
{
    if (!self)
        self = this;

#if defined(Q_WS_X11)
    QAbstractEventDispatcher::instance()->setEventFilter(q_XEventFilter);
    Display *display = QX11Info::display();
    if (display) {
        XRRQueryExtension(display, &xEventBase, &xErrorBase);

        Window desktopWindow = QX11Info::appRootWindow(0);
        XRRSelectInput(display, desktopWindow,0);
        XRRSelectInput(display, desktopWindow, RRScreenChangeNotifyMask);

        XRRScreenConfiguration *sc;
        Rotation cur_rotation;
        sc = XRRGetScreenInfo(display, RootWindow(display, 0));
        if (sc) {
            XRRConfigRotations(sc, &cur_rotation);
            lastRotation = cur_rotation;
        }
    }
#endif // Q_WS_X11
}

QSystemDisplayInfoLinuxCommonPrivate::~QSystemDisplayInfoLinuxCommonPrivate()
{
}

#if defined(Q_WS_X11)
void QSystemDisplayInfoLinuxCommonPrivate::emitOrientationChanged(int curRotation)
{
    switch(curRotation) {
    case 1:
        Q_EMIT orientationChanged(QSystemDisplayInfo::Landscape);
        break;
    case 2:
        Q_EMIT orientationChanged(QSystemDisplayInfo::Portrait);
        break;
    case 4:
        Q_EMIT orientationChanged(QSystemDisplayInfo::InvertedLandscape);
        break;
    case 8:
        Q_EMIT orientationChanged(QSystemDisplayInfo::InvertedPortrait);
        break;
    };
}
#endif // Q_WS_X11

bool QSystemDisplayInfoLinuxCommonPrivate::isScreenValid(int screen)
{
    if (screen > QApplication::desktop()->screenCount() || screen < 0)
        return false;

    return true;
}

int QSystemDisplayInfoLinuxCommonPrivate::colorDepth(int screen)
{
    if (!isScreenValid(screen))
        return -1;

#if defined(Q_WS_MAEMO_6) || defined(Q_WS_MEEGO)
    struct fb_var_screeninfo *screenInfo = allocFrameBufferInfo(screen);
    if (screenInfo) {
        int colorDepth = screenInfo->bits_per_pixel;
        free(screenInfo), screenInfo = 0;
        return colorDepth;
    }
#endif

#ifdef Q_WS_X11
    return QApplication::desktop()->screen(screen)->x11Info().depth();
#endif // Q_WS_X11

    // as a last resort, use the default depth
    return QPixmap::defaultDepth();
}

int QSystemDisplayInfoLinuxCommonPrivate::displayBrightness(int screen)
{
    if (!isScreenValid(screen))
        return -1;

#if !defined(QT_NO_HAL)
    halIsAvailable = halAvailable();
    if (halIsAvailable) {
        QHalInterface iface;
        if (iface.isValid()) {
            const QStringList list = iface.findDeviceByCapability("laptop_panel");
            if (!list.isEmpty()) {
                foreach (const QString &lapDev, list) {
                    QHalDeviceInterface ifaceDevice(lapDev);
                    QHalDeviceLaptopPanelInterface lapIface(lapDev);
                    const float numLevels = ifaceDevice.getPropertyInt("laptop_panel.num_levels") - 1;
                    const float curLevel = lapIface.getBrightness();
                    return curLevel / numLevels * 100;
                }
            }
        }
    }
#endif // QT_NO_HAL

    const QString backlightPath("/proc/acpi/video/");
    const QDir videoDir(backlightPath);
    QStringList filters;
    filters << "*";
    const QStringList brightnessList = videoDir.entryList(filters,
                                                          QDir::Dirs | QDir::NoDotAndDotDot,
                                                          QDir::Name);
    foreach (const QString &brightnessFileName, brightnessList) {
        float numLevels = 0.0;
        float curLevel = 0.0;

        const QDir videoSubDir(backlightPath + "/" + brightnessFileName);

        const QStringList vidDirList = videoSubDir.entryList(filters,
                                                             QDir::Dirs | QDir::NoDotAndDotDot,
                                                             QDir::Name);
        foreach (const QString &vidFileName, vidDirList) {
            QFile curBrightnessFile(backlightPath+brightnessFileName+"/"+vidFileName+"/brightness");
            if (!curBrightnessFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qDebug()<<"File not opened";
            } else {
                QTextStream bri(&curBrightnessFile);
                QString line = bri.readLine();
                while (!line.isNull()) {
                    if (!line.contains("not supported")) {
                        if (line.contains("levels")) {
                            QString level = line.section(" ", -1);
                            bool ok;
                            numLevels = level.toFloat(&ok);
                            if (!ok)
                                numLevels = -1;
                        } else if (line.contains("current")) {
                            QString level = line.section(": ",-1);
                            bool ok;
                            curLevel = level.toFloat(&ok);
                            if (!ok)
                                curLevel = 0;
                        }
                    }
                    line = bri.readLine();
                }
                curBrightnessFile.close();
                if (curLevel > -1 && numLevels > 0)
                    return curLevel / numLevels * 100;
            }
        }
    }

    return -1;
}

float QSystemDisplayInfoLinuxCommonPrivate::contrast(int screen)
{
    Q_UNUSED(screen);

    return -1;
}

QSystemDisplayInfo::BacklightState QSystemDisplayInfoLinuxCommonPrivate::backlightStatus(int screen)
{
    Q_UNUSED(screen);

    return QSystemDisplayInfo::BacklightStateUnknown;
}

QSystemDisplayInfo::DisplayOrientation QSystemDisplayInfoLinuxCommonPrivate::orientation(int screen)
{
    QSystemDisplayInfo::DisplayOrientation orientation = QSystemDisplayInfo::Unknown;

#if defined(Q_WS_X11)
    XRRScreenConfiguration *sc;
    Rotation cur_rotation;
    Display *display = QX11Info::display();

    if (display && (sc = XRRGetScreenInfo(display, RootWindow(display, screen)))) {
        XRRConfigRotations(sc, &cur_rotation);

        if (screen < 16 && screen > -1) {
            switch(cur_rotation) {
            case RR_Rotate_0:
                orientation = QSystemDisplayInfo::Landscape;
                break;
            case RR_Rotate_90:
                orientation = QSystemDisplayInfo::Portrait;
                break;
            case RR_Rotate_180:
                orientation = QSystemDisplayInfo::InvertedLandscape;
                break;
            case RR_Rotate_270:
                orientation = QSystemDisplayInfo::InvertedPortrait;
                break;
            };
            return orientation;
        }
    }
#endif

    if (QApplication::desktop()->width() > QApplication::desktop()->height()) {//landscape
        if (orientation == QSystemDisplayInfo::Unknown || orientation == QSystemDisplayInfo::Portrait)
            orientation = QSystemDisplayInfo::Landscape;
    } else { //portrait
        if (orientation == QSystemDisplayInfo::Unknown || orientation == QSystemDisplayInfo::Landscape)
            orientation = QSystemDisplayInfo::Portrait;
    }

    return orientation;
}

int QSystemDisplayInfoLinuxCommonPrivate::physicalHeight(int screen)
{
    if (!isScreenValid(screen))
        return -1;

    int height = -1;

#if defined(Q_WS_X11)
    XRRScreenResources *sr;

    sr = XRRGetScreenResources(QX11Info::display(), RootWindow(QX11Info::display(), screen));
    for (int i = 0; i < sr->noutput; ++i) {
        XRROutputInfo *output = XRRGetOutputInfo(QX11Info::display(),sr,sr->outputs[i]);
        if (output->crtc)
           height = output->mm_height;
        XRRFreeOutputInfo(output);
    }
    XRRFreeScreenResources(sr);
#endif // Q_WS_X11

    if (height == 0) {
        struct fb_var_screeninfo *screenInfo = allocFrameBufferInfo(screen);
        if (screenInfo) {
            height = screenInfo->height;
            free(screenInfo);
        }
    }

    return height;
}

int QSystemDisplayInfoLinuxCommonPrivate::physicalWidth(int screen)
{
    if (!isScreenValid(screen))
        return -1;

    int width = -1;

#if defined(Q_WS_X11)
    XRRScreenResources *sr;

    sr = XRRGetScreenResources(QX11Info::display(), RootWindow(QX11Info::display(), screen));
    for (int i = 0; i < sr->noutput; ++i) {
        XRROutputInfo *output = XRRGetOutputInfo(QX11Info::display(),sr,sr->outputs[i]);
        if (output->crtc)
            width = output->mm_width;
        XRRFreeOutputInfo(output);
    }
    XRRFreeScreenResources(sr);
#endif // Q_WS_X11

    if (width == 0) {
        struct fb_var_screeninfo *screenInfo = allocFrameBufferInfo(screen);
        if (screenInfo) {
            width = screenInfo->width;
            free(screenInfo);
        }
    }

    return width;
}

int QSystemDisplayInfoLinuxCommonPrivate::getDPIWidth(int screen)
{
    if (!isScreenValid(screen))
        return -1;

#if defined(Q_WS_X11)
    return QX11Info::appDpiY(screen);
#else
    return -1;
#endif
}

int QSystemDisplayInfoLinuxCommonPrivate::getDPIHeight(int screen)
{
    if (!isScreenValid(screen))
        return -1;

#if defined(Q_WS_X11)
    return QX11Info::appDpiX(screen);
#else
    return -1;
#endif
}

QSystemStorageInfoPrivate::QSystemStorageInfoPrivate(QObject *parent)
    : QObject(parent)
    , storageTimer(0)
{
#if !defined(QT_NO_UDISKS)
    udisksIsAvailable = udisksAvailable();
    if (udisksIsAvailable)
        udisksIface = new QUDisksInterface(this);
#endif // QT_NO_UDISKS
}

QSystemStorageInfoPrivate::~QSystemStorageInfoPrivate()
{
    ::close(inotifyFileDescriptor);
}

void QSystemStorageInfoPrivate::connectNotify(const char *signal)
{
    if (QLatin1String(signal) == SIGNAL(logicalDriveChanged(bool,QString))) {
        updateMountedEntries();
#if !defined(QT_NO_UDISKS)
        if (udisksIsAvailable) {
            connect(udisksIface, SIGNAL(deviceChanged(QDBusObjectPath)),
                    this, SLOT(udisksDeviceChanged(QDBusObjectPath)));
            return;
        }
#endif // QT_NO_UDISKS
        inotifyFileDescriptor = ::inotify_init();
        inotifyWatcher = ::inotify_add_watch(inotifyFileDescriptor, "/etc/mtab", IN_MODIFY);
        if (inotifyWatcher > 0) {
            QSocketNotifier *notifier = new QSocketNotifier(inotifyFileDescriptor, QSocketNotifier::Read, this);
            connect(notifier, SIGNAL(activated(int)), this, SLOT(inotifyActivated()));
        }
    } else if (QLatin1String(signal) == SIGNAL(storageStateChanged(QString,QSystemStorageInfo::StorageState))) {
        foreach (const QString &vol, logicalDrives())
            storageStates.insert(vol, getStorageState(vol));
        storageTimer = new QTimer(this);
        connect(storageTimer, SIGNAL(timeout()), this, SLOT(updateStorageStates()));
        storageTimer->start(STORAGEPOLL);
    }
}

void QSystemStorageInfoPrivate::disconnectNotify(const char *signal)
{
    if (QLatin1String(signal) == SIGNAL(logicalDriveChanged(bool,QString))) {
#if !defined(QT_NO_UDISKS)
        if (udisksIsAvailable) {
            disconnect(udisksIface, SIGNAL(deviceChanged(QDBusObjectPath)),
                       this, SLOT(udisksDeviceChanged(QDBusObjectPath)));
            return;
        }
#endif // QT_NO_UDISKS
        ::inotify_rm_watch(inotifyFileDescriptor, inotifyWatcher);
    }
}

void QSystemStorageInfoPrivate::inotifyActivated()
{
    char buffer[1024];
    int len = ::read(inotifyFileDescriptor, (void *)buffer, sizeof(buffer));
    if (len > 0) {
        struct inotify_event *event = (struct inotify_event *)buffer;
        if (event->wd == inotifyWatcher /*&& (event->mask & IN_IGNORED) == 0*/) {
            ::inotify_rm_watch(inotifyFileDescriptor, inotifyWatcher);
            QTimer::singleShot(1000, this, SLOT(deviceChanged())); //give this time to finish write
            inotifyWatcher = ::inotify_add_watch(inotifyFileDescriptor, "/etc/mtab", IN_MODIFY);
        }
    }
}

void QSystemStorageInfoPrivate::deviceChanged()
{
    QMap<QString, QString> oldDrives = mountedEntries;
    updateMountedEntries();

    if (mountedEntries.count() < oldDrives.count()) {
        QMapIterator<QString, QString> i(oldDrives);
        while (i.hasNext()) {
            i.next();
            if (!mountedEntries.contains(i.key()))
                Q_EMIT logicalDriveChanged(false, i.key());
        }
    } else if (mountedEntries.count() > oldDrives.count()) {
        QMapIterator<QString, QString> i(mountedEntries);
        while (i.hasNext()) {
            i.next();
            if (!oldDrives.contains(i.key()))
                Q_EMIT logicalDriveChanged(true, i.key());
        }
    }
}

#if !defined(QT_NO_UDISKS)
void QSystemStorageInfoPrivate::udisksDeviceChanged(const QDBusObjectPath &path)
{
    if (udisksIsAvailable) {
        QUDisksDeviceInterface devIface(path.path());
        QString mountp;
        if (devIface.deviceMountPaths().count() > 0)
            mountp = devIface.deviceMountPaths().at(0);

        if (devIface.deviceIsMounted())
            Q_EMIT logicalDriveChanged(true, mountp);
        else
            Q_EMIT logicalDriveChanged(false, mountedEntries.key(devIface.deviceFilePresentation()));
    }
    updateMountedEntries();
}
#endif // QT_NO_UDISKS

qlonglong QSystemStorageInfoPrivate::availableDiskSpace(const QString &driveVolume)
{
    if (driveVolume.left(2) == "//")
        return 0;

    struct statfs fs;
    if (statfs(driveVolume.toLatin1(), &fs) == 0) {
        qlonglong blockSize = fs.f_bsize;
        qlonglong availBlocks = fs.f_bavail;
        return availBlocks * blockSize;
    }

    return 0;
}

qlonglong QSystemStorageInfoPrivate::totalDiskSpace(const QString &driveVolume)
{
    if (driveVolume.left(2) == "//")
        return 0;

    struct statfs fs;
    if (statfs(driveVolume.toLatin1(), &fs) == 0) {
        qlonglong blockSize = fs.f_bsize;
        qlonglong totalBlocks = fs.f_blocks;
        return totalBlocks * blockSize;
    }

    return 0;
}

void QSystemStorageInfoPrivate::updateStorageStates()
{
    foreach (const QString &vol, logicalDrives()) {
        QSystemStorageInfo::StorageState state = getStorageState(vol);
        if (!storageStates.contains(vol)) {
            storageStates.insert(vol, state);
            Q_EMIT storageStateChanged(vol, state);
        } else {
            if (storageStates.value(vol) != state) {
                storageStates.insert(vol, state);
                Q_EMIT storageStateChanged(vol, state);
            }
        }
    }
}

QSystemStorageInfo::DriveType QSystemStorageInfoPrivate::typeForDrive(const QString &driveVolume)
{
    updateMountedEntries();

#if !defined(QT_NO_UDISKS)
    if (udisksIsAvailable) {
        QString udiskdev = mountedEntries.value(driveVolume);
        udiskdev.chop(1);
        QUDisksDeviceInterface devIfaceParent(udiskdev);
        QUDisksDeviceInterface devIface(mountedEntries.value(driveVolume));

        if (devIface.deviceIsMounted()) {
            if (devIfaceParent.deviceIsRemovable()) {
                return QSystemStorageInfo::RemovableDrive;
            } else if (devIfaceParent.deviceIsSystemInternal()) {
                if (devIfaceParent.driveIsRotational())
                    return QSystemStorageInfo::InternalDrive;
                else
                    return QSystemStorageInfo::InternalFlashDrive;
            } else {
                return QSystemStorageInfo::RemoteDrive;
            }
        } else {
            //udisks cannot see mounted samba shares
            if (mountedEntries.value(driveVolume).left(2) == "//")
                return QSystemStorageInfo::RemoteDrive;
        }
    }
#endif // QT_NO_UDISKS

    if (driveVolume.left(2) == "//")
        return QSystemStorageInfo::RemoteDrive;

    // manually read sys file for block device
    // not perfect, more complete than hal

    QString dmFile;

    if (mountedEntries.value(driveVolume).contains("mapper")) {
        struct stat stat_buf;
        stat(mountedEntries.value(driveVolume).toLatin1(), &stat_buf);
        dmFile = QString("/sys/block/dm-%1/removable").arg(stat_buf.st_rdev & 0377);
    } else {
        dmFile = mountedEntries.value(driveVolume).section("/", 2, 3);

        //assume this dev is removable sd/mmc card.
        if (dmFile.left(3) == "mmc")
            return QSystemStorageInfo::RemovableDrive;

        if (dmFile.left(3) == "ram")
            return QSystemStorageInfo::RamDrive;

        //if device has number, we need the 'parent' device
        if (dmFile.length() > 3) {
            dmFile.chop(1);
            if (dmFile.right(1) == "p") //get rid of partition number
                dmFile.chop(1);
        }
        dmFile = "/sys/block/" + dmFile + "/removable";
    }

    QFile file(dmFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open sys file";
    } else {
        QTextStream sysinfo(&file);
        QString line = sysinfo.readAll();
        if (line.contains("1"))
            return QSystemStorageInfo::RemovableDrive;
    }

    return QSystemStorageInfo::InternalDrive;
}

QStringList QSystemStorageInfoPrivate::logicalDrives()
{
    updateMountedEntries();
    return mountedEntries.keys();
}

void QSystemStorageInfoPrivate::updateMountedEntries()
{
    mountedEntries.clear();

#if !defined(QT_NO_UDISKS)
    if (udisksAvailable()) {
        foreach (const QDBusObjectPath &device, udisksIface->enumerateDevices()) {
            QUDisksDeviceInterface devIface(device.path());
            if (devIface.deviceIsMounted()) {
                QString fsname = devIface.deviceMountPaths().at(0);
                if (!mountedEntries.keys().contains(fsname))
                    mountedEntries[fsname.toLatin1()] = device.path()/*.section("/")*/;
            }
        }
    }
#endif // QT_NO_UDISKS

    FILE *mntfp = setmntent(_PATH_MOUNTED, "r");
    mntent *me = getmntent(mntfp);
    bool ok;
    while (me != NULL) {
        struct statfs fs;
        ok = false;
        if (strcmp(me->mnt_type, "cifs") != 0) { //smb has probs with statfs
            if (statfs(me->mnt_dir, &fs ) ==0 ) {
                // weed out a few types
                if (fs.f_type != 0x01021994 //tmpfs
                    && fs.f_type != 0x9fa0 //procfs
                    && fs.f_type != 0x1cd1 //
                    && fs.f_type != 0x62656572
                    && (unsigned)fs.f_type != 0xabababab // ???
                    && fs.f_type != 0x52654973
                    && fs.f_type != 0x42494e4d
                    && fs.f_type != 0x64626720
                    && fs.f_type != 0x73636673 //securityfs
                    && fs.f_type != 0x65735543 //fusectl
                    && fs.f_type != 0x65735546 // fuse.gvfs-fuse-daemon
                   ) {
                    ok = true;
                }
            }
        } else {
            ok = true;
        }

        if (ok && !mountedEntries.keys().contains(me->mnt_dir)
            && QString(me->mnt_fsname).left(1) == "/") {
            mountedEntries[me->mnt_dir] = me->mnt_fsname;
        }

        me = getmntent(mntfp);
    }

    endmntent(mntfp);
}

QString QSystemStorageInfoPrivate::getUuid(const QString &vol)
{
    QDir uuidDir("/dev/disk/by-uuid");
    if (uuidDir.exists()) {
        QFileInfoList fileList = uuidDir.entryInfoList();
        updateMountedEntries();
        foreach (const QFileInfo &fi, fileList) {
            if (fi.isSymLink()) {
                if (fi.symLinkTarget().contains(mountedEntries.value(vol).section("/", -1)))
                    return fi.baseName();
            }
        }
    }

    return QString();
}

QString QSystemStorageInfoPrivate::uriForDrive(const QString &driveVolume)
{
#if !defined(QT_NO_UDISKS)
    if (udisksIsAvailable) {
        updateMountedEntries();
        QUDisksDeviceInterface devIface(mountedEntries.value(driveVolume));
        if (devIface.deviceIsMounted())
            return devIface.uuid();
        return QString();
    }
#endif // QT_NO_UDISKS
#if !defined(QT_NO_HAL)
    halIsAvailable = halAvailable();
    if (halIsAvailable) {
        QHalInterface iface;
        QStringList list = iface.findDeviceByCapability("volume");
        if (!list.isEmpty()) {
            foreach (const QString &dev, list) {
                QHalDeviceInterface halDeviceInterface(dev);
                if (halDeviceInterface.isValid() && (halDeviceInterface.getPropertyString("volume.mount_point") == driveVolume))
                    return halDeviceInterface.getPropertyString("volume.uuid");
            }
        }
    }
#endif // QT_NO_HAL

    QString driveUuid = getUuid(driveVolume);
    if (!driveUuid.isEmpty())
        return  driveUuid;

    //last resort
#if !defined(QT_NO_BLKID)
    int fd;
    blkid_probe pr = NULL;
    uint64_t size;
    const char *label;
    int result = 0;

    updateMountedEntries();
    QFile dev(mountedEntries.value(driveVolume));
    if (!dev.open(QIODevice::ReadOnly))
        return QString();

    fd = dev.handle();

    if (fd < 0) {
        return QString();
    } else {
        pr = blkid_new_probe();
        blkid_probe_set_request(pr, BLKID_PROBREQ_UUID);
        ::ioctl(fd, BLKGETSIZE64, &size);
        blkid_probe_set_device(pr, fd, 0, size);
        result = blkid_do_safeprobe(pr);

        if (result == -1)
            qDebug() << "Unable to probe device";

        blkid_probe_lookup_value(pr, "UUID", &label, NULL);

        blkid_free_probe (pr);
        close(fd);
        return label;
    }
#endif // QT_NO_BLKID

    return QString();
}

QSystemStorageInfo::StorageState QSystemStorageInfoPrivate::getStorageState(const QString &driveVolume)
{
    QSystemStorageInfo::StorageState storState = QSystemStorageInfo::UnknownStorageState;
    float available = availableDiskSpace(driveVolume);
    float total = totalDiskSpace(driveVolume);
    float percent = available / total;
    if (percent < 0.02)
        storState = QSystemStorageInfo::CriticalStorageState;
    else if (percent < 0.1)
        storState = QSystemStorageInfo::VeryLowStorageState;
    else if (percent < 0.4)
        storState = QSystemStorageInfo::LowStorageState;
    else
        storState = QSystemStorageInfo::NormalStorageState;

    return storState;
}

QSystemDeviceInfoLinuxCommonPrivate::QSystemDeviceInfoLinuxCommonPrivate(QObject *parent)
    : QObject(parent)
    , btPowered(false)
    , hasWirelessKeyboardConnected(false)
{
#if !defined(QT_NO_DBUS)
#if !defined(QT_NO_HAL)
    halIsAvailable = halAvailable();
#endif // QT_NO_HAL
#if !defined(Q_WS_MAEMO_6)
    currentPowerState();
#endif // Q_WS_MAEMO_6
#endif // QT_NO_DBUS

    currentBatStatus            = QSystemDeviceInfo::NoBatteryLevel;
    currentBatLevel             = 0;
    currentBatStatusInitialized = false;
    currentBatLevelInitialized  = false;
}

QSystemDeviceInfoLinuxCommonPrivate::~QSystemDeviceInfoLinuxCommonPrivate()
{
}

QString QSystemDeviceInfoLinuxCommonPrivate::imei()
{
#if !defined(QT_NO_CONNMAN)
    if (ofonoAvailable()) {
        QOfonoManagerInterface ofonoManager;
        QString modem = ofonoManager.currentModem().path();
        if (!modem.isEmpty()) {
            QOfonoModemInterface modemIface(modem,this);
            QString imei = modemIface.getSerial();
            if (!imei.isEmpty())
                return imei;
        }
    }
#endif // QT_NO_CONNMAN

    return QString();
}

QString QSystemDeviceInfoLinuxCommonPrivate::imsi()
{
#if !defined(QT_NO_CONNMAN)
    if (ofonoAvailable()) {
        QOfonoManagerInterface ofonoManager;
        QString modem = ofonoManager.currentModem().path();
        if (!modem.isEmpty()) {
            QOfonoSimInterface simInterface(modem,this);
            if (simInterface.isPresent()) {
                QString id = simInterface.getImsi();
                if (!id.isEmpty())
                    return id;
            }
        }
    }
#endif // QT_NO_CONNMAN

    return QString();
}

QSystemDeviceInfo::SimStatus QSystemDeviceInfoLinuxCommonPrivate::simStatus()
{
#if !defined(QT_NO_CONNMAN)
    if (ofonoAvailable()) {
        QOfonoManagerInterface ofonoManager;
        QString modem = ofonoManager.currentModem().path();
        if(!modem.isEmpty()) {
            QOfonoSimInterface simInterface(modem, this);
            QString simpin = simInterface.pinRequired();
            if (simpin == "pin"
                || simpin == "phone"
                || simpin == "firstphone"
                || simpin == "pin2"
                || simpin == "puk"
                || simpin == "firstphonepuk"
                || simpin == "puk2") {
                return QSystemDeviceInfo::SimLocked;
            }

            if (simInterface.isPresent())
                return QSystemDeviceInfo::SingleSimAvailable;
        }
    }
#endif // QT_NO_CONNMAN

     return QSystemDeviceInfo::SimNotAvailable;
}

void QSystemDeviceInfoLinuxCommonPrivate::initBatteryStatus()
{
    if (!currentBatLevelInitialized) {
        const int level = batteryLevel();
        currentBatLevel = level;
        currentBatLevelInitialized = true;
    }

    if (!currentBatStatusInitialized) {
        const int level = currentBatLevel;
        QSystemDeviceInfo::BatteryStatus stat = QSystemDeviceInfo::NoBatteryLevel;

        if (level < 4)
            stat = QSystemDeviceInfo::BatteryCritical;
        else if (level < 11)
            stat = QSystemDeviceInfo::BatteryVeryLow;
        else if (level < 41)
            stat =  QSystemDeviceInfo::BatteryLow;
        else if (level > 40)
           stat = QSystemDeviceInfo::BatteryNormal;

        currentBatStatus = stat;
        currentBatStatusInitialized = true;
    }
}

void QSystemDeviceInfoLinuxCommonPrivate::connectNotify(const char *signal)
{
#if !defined(QT_NO_DBUS)
#if !defined(QT_NO_UPOWER)
    if ((QLatin1String(signal) == SIGNAL(batteryLevelChanged(int)))
        || (QLatin1String(signal) == SIGNAL(batteryStatusChanged(QSystemDeviceInfo::BatteryStatus)))
        || (QLatin1String(signal) == SIGNAL(powerStateChanged(QSystemDeviceInfo::PowerState)))) {
        if (uPowerAvailable()) {
            QUPowerInterface *power;
            power = new QUPowerInterface(this);
            connect(power, SIGNAL(changed()), this, (SLOT(upowerChanged())));
            foreach (const QDBusObjectPath &objpath, power->enumerateDevices()) {
                QUPowerDeviceInterface *powerDevice;
                powerDevice = new QUPowerDeviceInterface(objpath.path(),this);
                if (powerDevice->getType() == 2)
                    connect(powerDevice, SIGNAL(changed()), this, SLOT(upowerDeviceChanged()));
            }
        }
    } else
#endif // QT_NO_UPOWER
#if !defined(QT_NO_HAL)
    if ((QLatin1String(signal) == SIGNAL(batteryLevelChanged(int)))
        || (QLatin1String(signal) == SIGNAL(batteryStatusChanged(QSystemDeviceInfo::BatteryStatus)))
        || (QLatin1String(signal) == SIGNAL(powerStateChanged(QSystemDeviceInfo::PowerState)))) {
        if (halIsAvailable) {
            QHalInterface iface;
            QStringList list;
            list = iface.findDeviceByCapability("battery");
            if (!list.isEmpty()) {
                QString lastdev;
                foreach (const QString &dev, list) {
                    if (lastdev == dev)
                        continue;
                    lastdev = dev;
                    halIfaceDevice = new QHalDeviceInterface(dev, this);
                    if (halIfaceDevice->isValid()) {
                        const QString batType = halIfaceDevice->getPropertyString("battery.type");
                        if ((batType == "primary" || batType == "pda")
                            && halIfaceDevice->setConnections()) {
                            if (!connect(halIfaceDevice, SIGNAL(propertyModified(int, QVariantList)),
                                         this, SLOT(halChanged(int,QVariantList)))) {
                                qDebug() << "connection malfunction";
                            }
                        }
                        break;
                    }
                }
            }

            list = iface.findDeviceByCapability("ac_adapter");
            if (!list.isEmpty()) {
                foreach (const QString &dev, list) {
                    halIfaceDevice = new QHalDeviceInterface(dev, this);
                    if (halIfaceDevice->isValid()) {
                        if (halIfaceDevice->setConnections() ) {
                            if (!connect(halIfaceDevice, SIGNAL(propertyModified(int, QVariantList)),
                                         this, SLOT(halChanged(int,QVariantList)))) {
                                qDebug() << "connection malfunction";
                            }
                        }
                        break;
                    }
                }
            }
        }
    } else
#endif // QT_NO_HAL
    if ((QLatin1String(signal) == SIGNAL(bluetoothStateChanged(bool)))
               || (QLatin1String(signal) == SIGNAL(wirelessKeyboardConnected(bool)))) {
        QDBusInterface *connectionInterface = new QDBusInterface("org.bluez", "/", "org.bluez.Manager",
                                                                 QDBusConnection::systemBus(), this);
        if (connectionInterface->isValid()) {
            if (!QDBusConnection::systemBus().connect("org.bluez", "/", "org.bluez.Manager", "PropertyChanged",
                                                      this, SLOT(bluezPropertyChanged(QString,QDBusVariant)))) {
                qDebug() << "bluez could not connect signal";
            }

            if (QLatin1String(signal) == SIGNAL(bluetoothStateChanged(bool))) {
                connectedBtPower = true;
                connectBtPowered("");
            } else if ((QLatin1String(signal) == SIGNAL(wirelessKeyboardConnected(bool)))) {
                connectedWirelessKeyboard = true;
                connectBtKeyboard("");
            }
        }
    }
#endif // QT_NO_DBUS
}

void QSystemDeviceInfoLinuxCommonPrivate::disconnectNotify(const char */*signal*/)
{
}

#if !defined(QT_NO_DBUS)
#if !defined(QT_NO_HAL)
void QSystemDeviceInfoLinuxCommonPrivate::halChanged(int,QVariantList map)
{
    for (int i = 0; i < map.count(); i ++) {
        if (map.at(i).toString() == "battery.charge_level.percentage") {
            const int level = batteryLevel();
            if (currentBatLevel != level) {
                currentBatLevel = level;
                currentBatLevelInitialized = true;
                emit batteryLevelChanged(level);
            }

            QSystemDeviceInfo::BatteryStatus stat = QSystemDeviceInfo::NoBatteryLevel;
            if (level < 4)
                stat = QSystemDeviceInfo::BatteryCritical;
            else if (level < 11)
                 stat = QSystemDeviceInfo::BatteryVeryLow;
            else if (level < 41)
                 stat =  QSystemDeviceInfo::BatteryLow;
            else if (level > 40)
                 stat = QSystemDeviceInfo::BatteryNormal;

            if (currentBatStatus != stat) {
                currentBatStatus = stat;
                currentBatStatusInitialized = true;
                Q_EMIT batteryStatusChanged(stat);
            }
        }

        if ((map.at(i).toString() == "ac_adapter.present")
            || (map.at(i).toString() == "battery.rechargeable.is_charging")) {
            QSystemDeviceInfo::PowerState state = currentPowerState();
            emit powerStateChanged(state);
        } else if ((map.at(i).toString() == "input.keyboard")) {
            qDebug()<<"keyboard changed";
        }
    }
}
#endif // QT_NO_HAL

void QSystemDeviceInfoLinuxCommonPrivate::upowerChanged()
{
    currentPowerState();
}

void QSystemDeviceInfoLinuxCommonPrivate::upowerDeviceChanged()
{
    initBatteryStatus();
}
#endif // QT_NO_DBUS

QString QSystemDeviceInfoLinuxCommonPrivate::manufacturer()
{
#if !defined(QT_NO_HAL)
    if (halIsAvailable) {
        QHalDeviceInterface iface("/org/freedesktop/Hal/devices/computer");
        QString manu;
        if (iface.isValid()) {
            manu = iface.getPropertyString("system.firmware.vendor");
            if (manu.isEmpty()) {
                manu = iface.getPropertyString("system.hardware.vendor");
                if (!manu.isEmpty())
                    return manu;
            }
        }
    }
#endif // QT_NO_HAL
    QFile vendorId("/sys/devices/virtual/dmi/id/board_vendor");
    if (vendorId.open(QIODevice::ReadOnly)) {
        QTextStream cpuinfo(&vendorId);
        return cpuinfo.readLine().trimmed();
    } else {
        QFile file("/proc/cpuinfo");
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "Could not open /proc/cpuinfo";
        } else {
            QTextStream cpuinfo(&file);
            QString line = cpuinfo.readLine();
            while (!line.isNull()) {
                line = cpuinfo.readLine();
                if (line.contains("vendor_id"))
                    return line.split(": ").at(1).trimmed();
            }
        }
    }

    return QString();
}

QSystemDeviceInfo::InputMethodFlags QSystemDeviceInfoLinuxCommonPrivate::inputMethodType()
{
    QSystemDeviceInfo::InputMethodFlags methods = 0x0;
#if !defined(QT_NO_HAL) && !defined(Q_WS_MEEGO)
    if (halIsAvailable) {
        QHalInterface iface2;
        if (iface2.isValid()) {
            QStringList capList;
            capList << QLatin1String("input.keyboard")
                    << QLatin1String("input.keys")
                    << QLatin1String("input.keypad")
                    << QLatin1String("input.mouse")
                    << QLatin1String("input.tablet")
                    << QLatin1String("input.touchpad");
            for (int i = 0; i < capList.count(); i++) {
                QStringList list = iface2.findDeviceByCapability(capList.at(i));
                if (!list.isEmpty()) {
                    switch(i) {
                    case 0:
                        methods = (methods | QSystemDeviceInfo::Keyboard);
                        break;
                    case 1:
                        methods = (methods | QSystemDeviceInfo::Keys);
                        break;
                    case 2:
                        methods = (methods | QSystemDeviceInfo::Keypad);
                        break;
                    case 3:
                        methods = (methods | QSystemDeviceInfo::Mouse);
                        break;
                    case 4:
                        methods = (methods | QSystemDeviceInfo::SingleTouch);
                        break;
                    case 5:
                        methods = (methods | QSystemDeviceInfo::SingleTouch);
                        break;
                    }
                }
            }
            if (methods != 0)
                return methods;
        }
    }
#endif // QT_NO_HAL && Q_WS_MEEGO
    const QString inputsPath("/sys/class/input/");
    const QDir inputDir(inputsPath);
    QStringList filters;
    filters << "event*";
    const QStringList inputList = inputDir.entryList(filters, QDir::Dirs, QDir::Name);
    foreach (const QString &inputFileName, inputList) {
        QFile file(inputsPath + inputFileName + "/device/name");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug()<<"File not opened";
        } else {
            QString strvalue;
            strvalue = file.readLine();
            file.close();
            if (strvalue.contains("keyboard",Qt::CaseInsensitive)) {
                if ((methods & QSystemDeviceInfo::Keyboard) != QSystemDeviceInfo::Keyboard)
                    methods = (methods | QSystemDeviceInfo::Keyboard);
            } else if (strvalue.contains("Mouse",Qt::CaseInsensitive)) {
                if ((methods & QSystemDeviceInfo::Mouse) != QSystemDeviceInfo::Mouse)
                    methods = (methods | QSystemDeviceInfo::Mouse);
            } else if (strvalue.contains("Button",Qt::CaseInsensitive)) {
                if ((methods & QSystemDeviceInfo::Keys) != QSystemDeviceInfo::Keys)
                    methods = (methods | QSystemDeviceInfo::Keypad);
            } else if (strvalue.contains("keypad",Qt::CaseInsensitive)) {
                if ((methods & QSystemDeviceInfo::Keypad) != QSystemDeviceInfo::Keypad)
                    methods = (methods | QSystemDeviceInfo::Keys);
            } else if (strvalue.contains("Multi Touch",Qt::CaseInsensitive)) {
                if ((methods & QSystemDeviceInfo::MultiTouch) != QSystemDeviceInfo::MultiTouch)
                    methods = (methods | QSystemDeviceInfo::MultiTouch);
            } else if (strvalue.contains("Touch",Qt::CaseInsensitive)) {
                if ((methods & QSystemDeviceInfo::SingleTouch) != QSystemDeviceInfo::SingleTouch)
                    methods = (methods | QSystemDeviceInfo::SingleTouch);
            }
        }
    }
    return methods;
}

int QSystemDeviceInfoLinuxCommonPrivate::batteryLevel() const
{
    float levelWhenFull = 0.0;
    float level = 0.0;
#if !defined(QT_NO_HAL)
    if (halAvailable()) {
        QHalInterface iface;
        const QStringList list = iface.findDeviceByCapability("battery");
        if (!list.isEmpty()) {
            foreach (const QString &dev, list) {
                QHalDeviceInterface ifaceDevice(dev);
                if (ifaceDevice.isValid()) {
                    if (!ifaceDevice.getPropertyBool("battery.present")
                        && (ifaceDevice.getPropertyString("battery.type") != "pda"
                            || ifaceDevice.getPropertyString("battery.type") != "primary")) {
                        return 0;
                    } else {
                        level = ifaceDevice.getPropertyInt("battery.charge_level.percentage");
                        return level;
                    }
                }
            }
        }
    }
#endif // QT_NO_HAL
#if !defined(QT_NO_UPOWER)
    if (uPowerAvailable()) {
        QUPowerInterface power;
        foreach (const QDBusObjectPath &objpath, power.enumerateDevices()) {
            QUPowerDeviceInterface powerDevice(objpath.path());
            if (powerDevice.getType() == 2)
                return powerDevice.percentLeft();
        }
    }
#endif // QT_NO_UPOWER
    QFile infofile("/proc/acpi/battery/BAT0/info");
    if (!infofile.open(QIODevice::ReadOnly)) {
        return QSystemDeviceInfo::NoBatteryLevel;
    } else {
        QTextStream batinfo(&infofile);
        QString line = batinfo.readLine();
        while (!line.isNull()) {
            if (line.contains("last full capacity")) {
                bool ok;
                line = line.simplified();
                QString levels = line.section(" ", 3,3);
                levelWhenFull = levels.toFloat(&ok);
                infofile.close();
                break;
            }

            line = batinfo.readLine();
        }
        infofile.close();
    }

    QFile statefile("/proc/acpi/battery/BAT0/state");
    if (!statefile.open(QIODevice::ReadOnly)) {
        return QSystemDeviceInfo::NoBatteryLevel;
    } else {
        QTextStream batstate(&statefile);
        QString line = batstate.readLine();
        while (!line.isNull()) {
            if (line.contains("remaining capacity")) {
                bool ok;
                line = line.simplified();
                QString levels = line.section(" ", 2,2);
                level = levels.toFloat(&ok);
                statefile.close();
                break;
            }
            line = batstate.readLine();
        }
    }
    if (level != 0 && levelWhenFull != 0) {
        level = level / levelWhenFull * 100;
        return level;
    }
    return 0;
}

QSystemDeviceInfo::PowerState QSystemDeviceInfoLinuxCommonPrivate::currentPowerState()
{
#if !defined(QT_NO_HAL)
    if (halIsAvailable) {
        QHalInterface iface;
        QStringList list = iface.findDeviceByCapability("battery");
        if (!list.isEmpty()) {
            foreach (const QString &dev, list) {
                QHalDeviceInterface ifaceDevice(dev);
                if (iface.isValid()) {
                    if (ifaceDevice.getPropertyBool("battery.rechargeable.is_charging"))
                        return QSystemDeviceInfo::WallPowerChargingBattery;
                }
            }
        }

        list = iface.findDeviceByCapability("ac_adapter");
        if (!list.isEmpty()) {
            foreach (const QString &dev, list) {
                QHalDeviceInterface ifaceDevice(dev);
                if (ifaceDevice.isValid()) {
                    if (ifaceDevice.getPropertyBool("ac_adapter.present"))
                        return QSystemDeviceInfo::WallPower;
                    else
                        return QSystemDeviceInfo::BatteryPower;
                }
            }
        }
    }
#endif // QT_NO_HAL
#if !defined(QT_NO_UPOWER)
    if (uPowerAvailable()) {
        QSystemDeviceInfo::PowerState pState = QSystemDeviceInfo::UnknownPower;

        QUPowerInterface power(this);
        foreach (const QDBusObjectPath &objpath, power.enumerateDevices()) {
            QUPowerDeviceInterface powerDevice(objpath.path(),this);
            if (powerDevice.getType() == 2) {
                switch(powerDevice.getState()) {
                case 0:
                    break;
                case 1:
                case 5:
                    pState = QSystemDeviceInfo::WallPowerChargingBattery;
                    break;
                case 2:
                case 6:
                    pState = QSystemDeviceInfo::BatteryPower;
                    break;
                case 4:
                    pState = QSystemDeviceInfo::WallPower;
                    break;
                default:
                    pState = QSystemDeviceInfo::UnknownPower;
                };
            }
        }
        if (!power.onBattery() && pState == QSystemDeviceInfo::UnknownPower)
            pState = QSystemDeviceInfo::WallPower;
        if (curPowerState != pState) {
            curPowerState = pState;
            Q_EMIT powerStateChanged(pState);
        }
        return pState;
    }
#endif // QT_NO_UPOWER
    QFile statefile("/proc/acpi/battery/BAT0/state");
    if (statefile.open(QIODevice::ReadOnly)) {
        QTextStream batstate(&statefile);
        QString line = batstate.readLine();
        while (!line.isNull()) {
            if (line.contains("charging state")) {
                QString batstate = (line.simplified()).split(" ").at(2);
                if (batstate == "discharging")
                    return QSystemDeviceInfo::BatteryPower;
                else if (batstate == "charging")
                    return QSystemDeviceInfo::WallPowerChargingBattery;
            }
            line = batstate.readLine();
        }
    }
    return QSystemDeviceInfo::WallPower;
}

#if !defined(QT_NO_DBUS)
void QSystemDeviceInfoLinuxCommonPrivate::bluezPropertyChanged(const QString &str, QDBusVariant v)
{
    if (str == "Powered") {
        if (btPowered != v.variant().toBool()) {
            btPowered = !btPowered;
            emit bluetoothStateChanged(btPowered);
        }
    } else if (str == "Connected") {
        bool conn = v.variant().toBool();

        QDBusInterface *devadapterInterface = new QDBusInterface("org.bluez", str, "org.bluez.Device",
                                                                 QDBusConnection::systemBus(), this);
        QDBusReply<QVariantMap > reply = devadapterInterface->call(QLatin1String("GetProperties"));
        QVariantMap map = reply.value();
        QString property="Class";
        if (map.contains(property)) {
            uint classId = map.value(property).toUInt();
            if ((classId = 9536) &&  conn) {
                hasWirelessKeyboardConnected = conn;
                emit wirelessKeyboardConnected(conn);
            }
        }
    } else if (str == "Adapters") {
        connectBtPowered("");
        bool oldPoweredState = btPowered;
        if (oldPoweredState != currentBluetoothPowerState())
            emit bluetoothStateChanged(btPowered);
        bool oldKeyConnected = connectedWirelessKeyboard;
        if (oldKeyConnected != isWirelessKeyboardConnected())
            emit wirelessKeyboardConnected(connectedWirelessKeyboard);
    }
}

void QSystemDeviceInfoLinuxCommonPrivate::connectBtPowered(const QString &str)
{
    if (connectedBtPower) {
        QDBusInterface *connectionInterface;
        connectionInterface = new QDBusInterface("org.bluez", "/", "org.bluez.Manager",
                                                 QDBusConnection::systemBus(), this);
        if (connectionInterface->isValid()) {
            QDBusReply<  QDBusObjectPath > reply = connectionInterface->call("DefaultAdapter");
            if (reply.isValid() && !reply.value().path().isEmpty()
                && (reply.value().path() == str || str.isEmpty())) {
                if (!QDBusConnection::systemBus().connect("org.bluez", reply.value().path(), "org.bluez.Adapter", "PropertyChanged",
                                                          this, SLOT(bluezPropertyChanged(QString,QDBusVariant)))) {
                    qDebug() << "bluez could not connect signal";
                }
            }
        }
    }
}

void QSystemDeviceInfoLinuxCommonPrivate::connectBtKeyboard(const QString &str)
{
    if (!connectedWirelessKeyboard)
        return;

    QDBusInterface *connectionInterface = new QDBusInterface("org.bluez", "/", "org.bluez.Manager",
                                                             QDBusConnection::systemBus(), this);
    if (connectionInterface->isValid()) {
        QDBusReply<QDBusObjectPath> reply = connectionInterface->call("DefaultAdapter");
        if (reply.isValid()) {
            QDBusInterface *adapterInterface = new QDBusInterface("org.bluez", reply.value().path(), "org.bluez.Adapter",
                                                                  QDBusConnection::systemBus(), this);
            if (adapterInterface->isValid()) {
                QDBusReply<QVariantMap> reply2 =  adapterInterface->call(QLatin1String("GetProperties"));
                QString property("Devices");
                QVariantMap map = reply2.value();
                if (map.contains(property)) {
                    QList<QDBusObjectPath> devicesList = qdbus_cast<QList<QDBusObjectPath> >(map.value(property));
                    foreach (const QDBusObjectPath &device, devicesList) {
                        if (!QDBusConnection::systemBus().connect("org.bluez", device.path(), "org.bluez.Device", "PropertyChanged",
                                                                  this, SLOT(bluezPropertyChanged(QString,QDBusVariant)))) {
                            qDebug() << "bluez could not connect signal";
                        }
                    }

                    QDBusInterface *devadapterInterface = new QDBusInterface("org.bluez", reply.value().path(), "org.bluez.Input",
                                                                             QDBusConnection::systemBus(), this);
                    if (devadapterInterface->isValid() && !reply.value().path().isEmpty()
                        && (str == reply.value().path() || str.isEmpty())) {
                        if (!QDBusConnection::systemBus().connect("org.bluez", reply.value().path(), "org.bluez.Input", "PropertyChanged",
                                                                  this, SLOT(bluezPropertyChanged(QString,QDBusVariant)))) {
                            qDebug() << "bluez could not connect Input signal";
                        }
                    } else {
                        qDebug() << "not valid";
                    }
                }
            }
        }
    }
}
#endif // QT_NO_DBUS

bool QSystemDeviceInfoLinuxCommonPrivate::currentBluetoothPowerState()
{
    bool powered = false;
    QDBusInterface *connectionInterface = new QDBusInterface("org.bluez", "/", "org.bluez.Manager",
                                                             QDBusConnection::systemBus(), this);
    if (connectionInterface->isValid()) {
        QDBusReply<QDBusObjectPath> reply = connectionInterface->call("DefaultAdapter");
        if (reply.isValid() && !reply.value().path().isEmpty()) {
            QDBusInterface *adapterInterface = new QDBusInterface("org.bluez", reply.value().path(), "org.bluez.Adapter",
                                                                  QDBusConnection::systemBus(), this);
            if (adapterInterface->isValid()) {
                QDBusReply<QVariantMap> reply =  adapterInterface->call(QLatin1String("GetProperties"));
                QVariantMap map = reply.value();
                QString property = "Powered";
                if (map.contains(property))
                    powered =  map.value(property).toBool();
            } else {
                powered = false;
            }
        } else {
            powered = false;
        }
    }
    return btPowered = powered;
}

bool QSystemDeviceInfoLinuxCommonPrivate::isWirelessKeyboardConnected()
{
#if !defined(QT_NO_DBUS)
    QDBusInterface *connectionInterface = new QDBusInterface("org.bluez", "/", "org.bluez.Manager",
                                                             QDBusConnection::systemBus(), this);
    if (connectionInterface->isValid()) {
        QDBusReply<QDBusObjectPath> reply = connectionInterface->call("DefaultAdapter");
        if (reply.isValid()) {
            QDBusInterface *adapterInterface = new QDBusInterface("org.bluez", reply.value().path(), "org.bluez.Adapter",
                                                                  QDBusConnection::systemBus(), this);
            if (adapterInterface->isValid() && !reply.value().path().isEmpty()) {
                QDBusReply<QVariantMap > reply =  adapterInterface->call(QLatin1String("GetProperties"));
                QVariantMap map = reply.value();
                QString dproperty = "Devices";
                if (map.contains(dproperty)) {
                    QList<QDBusObjectPath> devicesList = qdbus_cast<QList<QDBusObjectPath> >(map.value(dproperty));
                    foreach (const QDBusObjectPath &device, devicesList) {
                        Q_UNUSED(device)
                        map = reply.value();
                        dproperty = "Class";
                        if (map.contains(dproperty)) {
                            uint classId = map.value(dproperty).toUInt();
                            if ((classId = 9536) &&  map.value("Connected").toBool()) {
                                hasWirelessKeyboardConnected = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
#endif // QT_NO_DBUS
    return hasWirelessKeyboardConnected;
}

QSystemDeviceInfo::KeyboardTypeFlags QSystemDeviceInfoLinuxCommonPrivate::keyboardTypes()
{
    QSystemDeviceInfo::InputMethodFlags methods = inputMethodType();
    QSystemDeviceInfo::KeyboardTypeFlags keyboardFlags;

    if ((methods & QSystemDeviceInfo::Keyboard))
        keyboardFlags = (keyboardFlags | QSystemDeviceInfo::FullQwertyKeyboard);

    if (isWirelessKeyboardConnected())
        keyboardFlags = (keyboardFlags | QSystemDeviceInfo::WirelessKeyboard);

    // how to check softkeys on desktop?
    return keyboardFlags;
}

QByteArray QSystemDeviceInfoLinuxCommonPrivate::uniqueDeviceID()
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
#if !defined(QT_NO_DBUS)
#if !defined(QT_NO_HAL)
    if (halIsAvailable) {
        QHalDeviceInterface iface("/org/freedesktop/Hal/devices/computer", this);
        QString id;
        if (iface.isValid()) {
            id = iface.getPropertyString("system.hardware.uuid");
            hash.addData(id.toLocal8Bit());
            return hash.result().toHex();
        }
    }
#endif // QT_NO_HAL
#if defined(Q_WS_MEEGO)
    QDBusInterface connectionInterface("org.freedesktop.PolicyKit1", "/org/freedesktop/PolicyKit1/Authority", "org.freedesktop.DBus.Peer",
                                       QDBusConnection::systemBus(), this);
    if (!connectionInterface.isValid())
        qDebug() <<connectionInterface.lastError().message()<< "not valid";

    QDBusReply<QString> reply = connectionInterface.call("GetMachineId");
    QString uid = reply.value();
    hash.addData(uid.toLocal8Bit());
    return hash.result().toHex();
#endif // Q_WS_MEEGO
#endif // QT_NO_DBUS
    hash.addData(QString::number(gethostid()).toLocal8Bit());
    return hash.result().toHex();
}

QSystemBatteryInfoLinuxCommonPrivate::QSystemBatteryInfoLinuxCommonPrivate(QObject *parent)
    : QObject(parent)
    , batteryIsPresent(false)
{
#if !defined(QT_NO_HAL)
    halIsAvailable = halAvailable();
    setConnection();
#endif // QT_NO_HAL
    getBatteryStats();
}

QSystemBatteryInfoLinuxCommonPrivate::~QSystemBatteryInfoLinuxCommonPrivate()
{
}


QSystemBatteryInfo::ChargerType QSystemBatteryInfoLinuxCommonPrivate::chargerType() const
{
    return curChargeType;
}

QSystemBatteryInfo::ChargingState QSystemBatteryInfoLinuxCommonPrivate::chargingState() const
{
    return curChargeState;
}


int QSystemBatteryInfoLinuxCommonPrivate::nominalCapacity() const
{
    return capacity;
}

int QSystemBatteryInfoLinuxCommonPrivate::remainingCapacityPercent() const
{
    return currentBatLevelPercent;
}

int QSystemBatteryInfoLinuxCommonPrivate::remainingCapacity() const
{
    return remainingEnergy;
}


int QSystemBatteryInfoLinuxCommonPrivate::voltage() const
{
    return currentVoltage;
}

int QSystemBatteryInfoLinuxCommonPrivate::remainingChargingTime() const
{
    return timeToFull;
}

int QSystemBatteryInfoLinuxCommonPrivate::currentFlow() const
{
    return dischargeRate;
}

int QSystemBatteryInfoLinuxCommonPrivate::remainingCapacityBars() const
{
    return 0;
}

int QSystemBatteryInfoLinuxCommonPrivate::maxBars() const
{
    return 0;
}

QSystemBatteryInfo::BatteryStatus QSystemBatteryInfoLinuxCommonPrivate::batteryStatus() const
{
    return currentBatStatus;
}

void QSystemBatteryInfoLinuxCommonPrivate::connectNotify(const char *signal)
{
    if (QLatin1String(signal) == QLatin1String(QMetaObject::normalizedSignature(SIGNAL(
            batteryLevelChanged(int))))) {
    }
    if (QLatin1String(signal) == QLatin1String(QMetaObject::normalizedSignature(SIGNAL(
            batteryStatusChanged(QSystemBatteryInfo::BatteryStatus))))) {
    }

    if (QLatin1String(signal) == QLatin1String(QMetaObject::normalizedSignature(SIGNAL(
            chargingStateChanged(QSystemBatteryInfo::ChargingState))))) {
    }

    if (QLatin1String(signal) == QLatin1String(QMetaObject::normalizedSignature(SIGNAL(
            chargerTypeChanged(QSystemBatteryInfo::ChargerType))))) {
    }

    if (QLatin1String(signal) == QLatin1String(QMetaObject::normalizedSignature(SIGNAL(
            nominalCapacityChanged(int))))) {
    }

    if (QLatin1String(signal) == QLatin1String(QMetaObject::normalizedSignature(SIGNAL(
            remainingCapacityPercentChanged(int))))) {
    }

    if (QLatin1String(signal) == QLatin1String(QMetaObject::normalizedSignature(SIGNAL(
            remainingCapacityChanged(int))))) {
    }

    if (QLatin1String(signal) == QLatin1String(QMetaObject::normalizedSignature(SIGNAL(
            currentFlowChanged(int))))) {
    }

    if (QLatin1String(signal) == QLatin1String(QMetaObject::normalizedSignature(SIGNAL(
            remainingCapacityBarsChanged(int))))) {
    }
    if (QLatin1String(signal) == QLatin1String(QMetaObject::normalizedSignature(SIGNAL(
            remainingChargingTimeChanged(int))))) {
    }

}

void QSystemBatteryInfoLinuxCommonPrivate::disconnectNotify(const char */*signal*/)
{

}


void QSystemBatteryInfoLinuxCommonPrivate::setConnection()
{
#if !defined(QT_NO_UPOWER)
    if (uPowerAvailable()) {
        QUPowerInterface *power;
        power = new QUPowerInterface(this);
   //     connect(power,SIGNAL(changed()),this,(SLOT(upowerChanged())));
        foreach (const QDBusObjectPath &objpath, power->enumerateDevices()) {
            QUPowerDeviceInterface *powerDevice;
            powerDevice = new QUPowerDeviceInterface(objpath.path(),this);
            if (powerDevice->getType() == 2) {
                battery = new QUPowerDeviceInterface(objpath.path(),this);
                //pMap = battery->getProperties();
                connect(battery,SIGNAL(propertyChanged(QString,QVariant)),
                        this,SLOT(uPowerPropertyChanged(QString,QVariant)));
            }
//            connect(powerDevice,SIGNAL(changed()),this,SLOT(upowerDeviceChanged()));
//            connect(powerDevice,SIGNAL(propertyChanged(QString,QVariant)),
//                    this,SLOT(propertyChanged(QString,QVariant)));
            //    return powerDevice.percentLeft();
           // }
        }
    }
#endif // QT_NO_UPOWER
#if !defined(QT_NO_HAL)
    if (halIsAvailable) {
        QHalInterface iface;
        QStringList list = iface.findDeviceByCapability("battery");
        if (!list.isEmpty()) {
            QString lastdev;
            foreach (const QString &dev, list) {
                if (lastdev == dev)
                    continue;
                lastdev = dev;
                halIfaceDevice = new QHalDeviceInterface(dev);
                if (halIfaceDevice->isValid()) {
                    const QString batType = halIfaceDevice->getPropertyString("battery.type");
                    if ((batType == "primary" || batType == "pda") &&
                            halIfaceDevice->setConnections() ) {
                        if (!connect(halIfaceDevice,SIGNAL(propertyModified(int, QVariantList)),
                                    this,SLOT(halChanged(int,QVariantList)))) {
                            qDebug() << "connection malfunction";
                        }
                    }
                }
            }
        }

        list = iface.findDeviceByCapability("ac_adapter");
        if (!list.isEmpty()) {
            foreach (const QString &dev, list) {
                halIfaceDevice = new QHalDeviceInterface(dev);
                if (halIfaceDevice->isValid()) {
                    if (halIfaceDevice->setConnections() ) {
                        if (!connect(halIfaceDevice,SIGNAL(propertyModified(int, QVariantList)),
                                    this,SLOT(halChanged(int,QVariantList)))) {
                            qDebug() << "connection malfunction";
                        }
                    }
                    return;
                }
            }
        }

        list = iface.findDeviceByCapability("battery");
        if (!list.isEmpty()) {
            foreach (const QString &dev, list) {
                halIfaceDevice = new QHalDeviceInterface(dev);
                if (halIfaceDevice->isValid()) {
                    if (halIfaceDevice->setConnections()) {
                        if (!connect(halIfaceDevice,SIGNAL(propertyModified(int, QVariantList)),
                                    this,SLOT(halChanged(int,QVariantList)))) {
                            qDebug() << "connection malfunction";
                        }
                    }
                    return;
                }
            }
        }
    }
#endif // QT_NO_HAL
}

#if !defined(QT_NO_HAL)
void QSystemBatteryInfoLinuxCommonPrivate::halChanged(int count,QVariantList map)
{
    QHalInterface iface;
    QStringList list = iface.findDeviceByCapability("battery");
    QHalDeviceInterface ifaceDevice(list.at(0)); //default battery
    if (ifaceDevice.isValid()) {
        for(int i=0; i < count; i++) {
            QString mapS = map.at(i).toString();
          //  qDebug() << mapS;
            if (mapS == "battery.present") {
                batteryIsPresent = true;
            }
            if (mapS == "battery.charge_level.percentage") {
                int level = ifaceDevice.getPropertyInt("battery.charge_level.percentage");
                if(currentBatLevelPercent != level) {
                    currentBatLevelPercent = level;
                    emit remainingCapacityPercentChanged(currentBatLevelPercent);
                }

                QSystemBatteryInfo::BatteryStatus stat = QSystemBatteryInfo::BatteryUnknown;

                if (currentBatLevelPercent == 0) {
                    stat = QSystemBatteryInfo::BatteryEmpty;
                } else if (currentBatLevelPercent < 4) {
                    stat = QSystemBatteryInfo::BatteryCritical;
                } else if (currentBatLevelPercent < 11) {
                    stat = QSystemBatteryInfo::BatteryVeryLow;
                } else if (currentBatLevelPercent < 41) {
                    stat =  QSystemBatteryInfo::BatteryLow;
                } else if (currentBatLevelPercent > 40 && currentBatLevelPercent < 100) {
                    stat = QSystemBatteryInfo::BatteryOk;
                } else if (currentBatLevelPercent == 100) {
                    stat = QSystemBatteryInfo::BatteryFull;
                }
                Q_EMIT batteryStatusChanged(currentBatStatus);
            }

            if (mapS == "ac_adapter.present") {
                QSystemBatteryInfo::ChargerType chargerType = QSystemBatteryInfo::UnknownCharger;
                chargerType = currentChargerType();
                if (chargerType == QSystemBatteryInfo::UnknownCharger) {
                    chargerType = QSystemBatteryInfo::WallCharger;
                }
                if(chargerType != curChargeType) {
                    curChargeType = chargerType;
                    Q_EMIT chargerTypeChanged(curChargeType);
                }
            }

            if (mapS == "battery.rechargeable.is_charging") {

                QSystemBatteryInfo::ChargingState bState;
                if (ifaceDevice.getPropertyBool("battery.rechargeable.is_charging")) {
                    bState = QSystemBatteryInfo::Charging;
                } else {
                    bState = QSystemBatteryInfo::NotCharging;
                }
                if(curChargeState != bState) {
                    curChargeState = bState;
                    Q_EMIT chargingStateChanged(curChargeState);
                }
            }
            if (mapS == "battery.rechargeable.is_discharging") {
                QSystemBatteryInfo::ChargingState bState = QSystemBatteryInfo::NotCharging;
                if (curChargeState != bState) {
                    curChargeState = bState;
                    Q_EMIT chargingStateChanged(curChargeState);
                }
            }

            if (mapS == "battery.voltage.current") {
                currentVoltage = ifaceDevice.getPropertyInt("battery.voltage.current");
            }

            if (mapS == "battery.reporting.rate") {
                dischargeRate = ifaceDevice.getPropertyInt("battery.reporting.rate");
              //  qDebug() << "current flow" << dischargeRate;
                Q_EMIT currentFlowChanged(dischargeRate);
            }

            if (mapS == "battery.reporting.last_full") {
                capacity = ifaceDevice.getPropertyInt("battery.reporting.last_full");
                Q_EMIT nominalCapacityChanged(capacity);
            }

            if (mapS == "battery.reporting.current") {
                remainingEnergy = ifaceDevice.getPropertyInt("battery.reporting.current");
                Q_EMIT remainingCapacityChanged(remainingEnergy);
            }

            if (mapS == "battery.remaining_time") {
                if(ifaceDevice.getPropertyBool("battery.rechargeable.is_charging")) {
                    remainingEnergy = ifaceDevice.getPropertyInt("battery.remaining_time");
                    Q_EMIT remainingChargingTimeChanged(remainingEnergy);
                }
            }

        }
    } else {
        currentBatLevelPercent = 0;
        currentBatStatus = QSystemBatteryInfo::BatteryUnknown;
        curChargeType = QSystemBatteryInfo::WallCharger;
        curChargeState = QSystemBatteryInfo::NotCharging;
        currentVoltage = 0;
        dischargeRate = 0;
        capacity = 0;
        remainingEnergy = 0;
    }
}
#endif // QT_NO_HAL

#if !defined(QT_NO_DBUS)
void QSystemBatteryInfoLinuxCommonPrivate::getBatteryStats()
{
    int cLevel = 0;
    int cEnergy = 0;
    int cVoltage = 0;
    int cTime = 0;
    int rEnergy = 0;

    QSystemBatteryInfo::ChargingState cState = QSystemBatteryInfo::ChargingError;
    QSystemBatteryInfo::ChargerType cType = QSystemBatteryInfo::UnknownCharger;

#if !defined(QT_NO_UPOWER)
    if (uPowerAvailable()) {

        QUPowerInterface power(this);
        foreach (const QDBusObjectPath &objpath, power.enumerateDevices()) {
            QUPowerDeviceInterface powerDevice(objpath.path(),this);
            if (!powerDevice.isPowerSupply())
                continue;
            if (powerDevice.getType() == 1) { //line power
                if(powerDevice.isOnline()) {
                    cType = QSystemBatteryInfo::WallCharger;
                } else {
                    cType = QSystemBatteryInfo::NoCharger;
                }
            }

            if (powerDevice.getType() == 2) { //battery power
                batteryIsPresent = true;
                switch(powerDevice.getState()) {
                case 1: // charging
                    {
                        cState = QSystemBatteryInfo::Charging;
                    }
                    break;
                case 2: //discharging
                case 3: //empty
                case 4: //fully charged
                case 5: //pending charge
                case 6: //pending discharge
                    cState = QSystemBatteryInfo::NotCharging;
                    break;
                default:
                    cState = QSystemBatteryInfo::ChargingError;
                    break;
                };
                //energy flow
                cVoltage = powerDevice.voltage();
                cEnergy = (powerDevice.energyDischargeRate() / cVoltage) * 1000;
                cLevel = powerDevice.percentLeft();
                capacity = (powerDevice.energyWhenFull()) * 1000;
                cTime = powerDevice.timeToFull();
                if (cState != QSystemBatteryInfo::Charging) {
                    cTime = -1;
                }
                rEnergy = (powerDevice.currentEnergy()) * 1000;
                cVoltage =  powerDevice.voltage() * 1000; // mV
                break;
            }

        } //end enumerateDevices
    }
#endif // QT_NO_UPOWER
#if !defined(QT_NO_HAL)
    if (halIsAvailable) {
        QHalInterface iface;
        QStringList list = iface.findDeviceByCapability("battery");
        if (!list.isEmpty()) {
            foreach (const QString &dev, list) {
                QHalDeviceInterface ifaceDevice(dev);
                if (iface.isValid()) {
                    batteryIsPresent = (ifaceDevice.getPropertyBool("battery.present")
                    && (ifaceDevice.getPropertyString("battery.type") == "pda"
                        || ifaceDevice.getPropertyString("battery.type") == "primary"));
                    if (ifaceDevice.getPropertyBool("battery.rechargeable.is_charging")) {
                        cState = QSystemBatteryInfo::Charging;
                        cTime = ifaceDevice.getPropertyInt("battery.remaining_time");
                    } else {
                        cState = QSystemBatteryInfo::NotCharging;
                        cTime = -1;
                    }
                    cVoltage = ifaceDevice.getPropertyInt("battery.voltage.current");

                    cEnergy = ifaceDevice.getPropertyInt("battery.charge_level.rate");
                    if (cEnergy == 0)
                        cEnergy = ifaceDevice.getPropertyInt("battery.reporting.rate");

                    cLevel = ifaceDevice.getPropertyInt("battery.charge_level.percentage");

                    rEnergy = ifaceDevice.getPropertyInt("battery.reporting.current");
                    if(rEnergy == 0)
                        rEnergy = ifaceDevice.getPropertyInt("battery.charge_level.current");

                    capacity =  ifaceDevice.getPropertyInt("battery.reporting.last_full");
                    if(capacity == 0)
                        capacity = ifaceDevice.getPropertyInt("battery.charge_level.last_full");
                    if(capacity == 0)
                        capacity = ifaceDevice.getPropertyInt("battery.reporting.design");

                    break;
                }
            }
        } else {
            cLevel = -1;
            cType = QSystemBatteryInfo::UnknownCharger;
            cState = QSystemBatteryInfo::NotCharging;
            cVoltage = -1;
            cEnergy = 0;
            capacity = -1;
            rEnergy = -1;
            cTime = -1;
        }

        list = iface.findDeviceByCapability("ac_adapter");
        if (!list.isEmpty()) {
            foreach (const QString &dev, list) {
                QHalDeviceInterface ifaceDevice(dev);
                if (ifaceDevice.isValid()) {
                    if (ifaceDevice.getPropertyBool("ac_adapter.present")) {
                        if (cType == QSystemBatteryInfo::UnknownCharger) {
                         cType = currentChargerType();
                        }
                        break;
                    } else {
                        cType = QSystemBatteryInfo::NoCharger;
                        break;
                    }
                }
            }
        }
    }
#endif // QT_NO_HAL

    if(cType == QSystemBatteryInfo::UnknownCharger && !batteryIsPresent) {
        cType = QSystemBatteryInfo::WallCharger;
    }

    if(cTime == 0 && currentBatLevelPercent != 100)
        cTime = -1;

    curChargeType = cType;
    currentVoltage = cVoltage;
    curChargeState = cState;
    if(cEnergy == 0) cEnergy = -1;
    dischargeRate = cEnergy;
    currentBatLevelPercent = cLevel;
    timeToFull = cTime;
    remainingEnergy = rEnergy;

    QSystemBatteryInfo::BatteryStatus stat = QSystemBatteryInfo::BatteryUnknown;
    if(batteryIsPresent) {
        if (cLevel == 0) {
            stat = QSystemBatteryInfo::BatteryEmpty;
        } else if (cLevel < 4) {
            stat = QSystemBatteryInfo::BatteryCritical;
        } else if (cLevel < 11) {
            stat = QSystemBatteryInfo::BatteryVeryLow;
        } else if (cLevel < 41) {
            stat =  QSystemBatteryInfo::BatteryLow;
        } else if (cLevel > 40 && cLevel < 99) {
            stat = QSystemBatteryInfo::BatteryOk;
        } else if (cLevel == 100) {
            stat = QSystemBatteryInfo::BatteryFull;
        }
    }
    currentBatStatus = stat;
}

QSystemBatteryInfo::ChargerType QSystemBatteryInfoLinuxCommonPrivate::currentChargerType()
{
    QSystemBatteryInfo::ChargerType chargerType = QSystemBatteryInfo::UnknownCharger;
    QFile acfile("/sys/class/power_supply/AC/online");
    if (acfile.open(QIODevice::ReadOnly)) {
        const QString acpresent = acfile.readAll().simplified();
        acfile.close();
        if (acpresent == "1") {
            return QSystemBatteryInfo::WallCharger;
        } else  {
            return QSystemBatteryInfo::NoCharger;
        }
    }
    QFile presentfile("/sys/class/power_supply/usb/present");
    if (presentfile.open(QIODevice::ReadOnly)) {
        const QString usbpresent = presentfile.readAll().simplified();
        presentfile.close();
        if (usbpresent == "0") {
            return QSystemBatteryInfo::NoCharger;
        }
    }
   QFile typefile("/sys/class/power_supply/usb/type");
    if (typefile.open(QIODevice::ReadOnly)) {
        const QString result = typefile.readAll().simplified();
        typefile.close();
        if (result == "USB_DCP") {
            chargerType = QSystemBatteryInfo::WallCharger;
        } else if (result == "USB") {
            chargerType = QSystemBatteryInfo::USBCharger;
            QFile typefile2("/sys/class/power_supply/usb/current_now");
            if (typefile2.open(QIODevice::ReadOnly)) {
                const QString result2 = typefile2.readAll().simplified();
                typefile2.close();
                if (result2 == "500") {
                    chargerType = QSystemBatteryInfo::USB_500mACharger;
                }
                if (result2 == "100") {
                    chargerType = QSystemBatteryInfo::USB_100mACharger;
                }
            }
        }
    }
    return chargerType;
}
#endif


void QSystemBatteryInfoLinuxCommonPrivate::timeout()
{

}

QSystemBatteryInfo::EnergyUnit QSystemBatteryInfoLinuxCommonPrivate::energyMeasurementUnit() const
{
#if !defined(QT_NO_UPOWER)
    if(uPowerAvailable() && batteryIsPresent) {
        return QSystemBatteryInfo::UnitmWh;
    }
#endif // QT_NO_UPOWER
#if !defined(QT_NO_HAL)
    if (halIsAvailable && batteryIsPresent) {
        QHalInterface iface;
        const QStringList list = iface.findDeviceByCapability("battery");
        if (!list.isEmpty()) {
            foreach (const QString &dev, list) {
                QHalDeviceInterface ifaceDevice(dev);
                if (ifaceDevice.isValid()) {
                    const QString unit = ifaceDevice.getPropertyString("battery.reporting.unit");
                    if (unit == "mAh") {
                        return QSystemBatteryInfo::UnitmAh;
                    } else if (unit == "mWh") {
                        return QSystemBatteryInfo::UnitmWh;
                    }
                }
            }
        }
    }
#endif // QT_NO_HAL
return QSystemBatteryInfo::UnitUnknown;
}

int QSystemBatteryInfoLinuxCommonPrivate::batteryLevel() const
{
    float levelWhenFull = 0.0;
    float level = 0.0;
#if !defined(QT_NO_HAL)
    if (halAvailable()) {
        QHalInterface iface;
        const QStringList list = iface.findDeviceByCapability("battery");
        if (!list.isEmpty()) {
            foreach (const QString &dev, list) {
                QHalDeviceInterface ifaceDevice(dev);
                if (ifaceDevice.isValid()) {
                    if (!ifaceDevice.getPropertyBool("battery.present")
                        && (ifaceDevice.getPropertyString("battery.type") != "pda"
                             || ifaceDevice.getPropertyString("battery.type") != "primary")) {
                        return 0;
                    } else {
                        level = ifaceDevice.getPropertyInt("battery.charge_level.percentage");
                        return level;
                    }
                }
            }
        }
    }
#endif // QT_NO_HAL
#if !defined(QT_NO_UPOWER)
    if (uPowerAvailable()) {
        QUPowerInterface power;
        foreach (const QDBusObjectPath &objpath, power.enumerateDevices()) {
            QUPowerDeviceInterface powerDevice(objpath.path());
            if (powerDevice.getType() == 2) {
                return powerDevice.percentLeft();
            }
        }
    }
#endif // QT_NO_UPOWER
    QFile infofile("/proc/acpi/battery/BAT0/info");
    if (!infofile.open(QIODevice::ReadOnly)) {
        return QSystemDeviceInfo::NoBatteryLevel;
    } else {
        QTextStream batinfo(&infofile);
        QString line = batinfo.readLine();
        while (!line.isNull()) {
            if (line.contains("last full capacity")) {
                bool ok;
                line = line.simplified();
                QString levels = line.section(" ", 3,3);
                levelWhenFull = levels.toFloat(&ok);
                infofile.close();
                break;
            }

            line = batinfo.readLine();
        }
        infofile.close();
    }

    QFile statefile("/proc/acpi/battery/BAT0/state");
    if (!statefile.open(QIODevice::ReadOnly)) {
        return QSystemDeviceInfo::NoBatteryLevel;
    } else {
        QTextStream batstate(&statefile);
        QString line = batstate.readLine();
        while (!line.isNull()) {
            if (line.contains("remaining capacity")) {
                bool ok;
                line = line.simplified();
                QString levels = line.section(" ", 2,2);
                level = levels.toFloat(&ok);
                statefile.close();
                break;
            }
            line = batstate.readLine();
        }
    }
    if (level != 0 && levelWhenFull != 0) {
        level = level / levelWhenFull * 100;
        return level;
    }
    return 0;
}

#if !defined(QT_NO_UPOWER)
void QSystemBatteryInfoLinuxCommonPrivate::uPowerPropertyChanged(const QString & prop, const QVariant &v)
{
 //   qDebug() << __FUNCTION__ << prop << v;

   if (prop == QLatin1String("Energy")) {
        remainingEnergy = (v.toDouble()) * 1000;
        emit remainingCapacityChanged(remainingEnergy);
    } else if (prop == QLatin1String("EnergyRate")) {
        dischargeRate = (v.toDouble() / battery->voltage()) * 1000;
       emit currentFlowChanged(dischargeRate);
    } else if (prop == QLatin1String("Percentage")) {
        currentBatLevelPercent = v.toUInt();
        emit remainingCapacityPercentChanged(currentBatLevelPercent);

        QSystemBatteryInfo::BatteryStatus stat = QSystemBatteryInfo::BatteryUnknown;

        if (currentBatLevelPercent < 4) {
            stat = QSystemBatteryInfo::BatteryCritical;
        } else if (currentBatLevelPercent < 11) {
             stat = QSystemBatteryInfo::BatteryVeryLow;
        } else if (currentBatLevelPercent < 41) {
             stat =  QSystemBatteryInfo::BatteryLow;
        } else if (currentBatLevelPercent > 40 && currentBatLevelPercent < 100) {
             stat = QSystemBatteryInfo::BatteryOk;
        } else if (currentBatLevelPercent == 100) {
             stat = QSystemBatteryInfo::BatteryFull;
        }
        if(currentBatStatus != stat) {
            currentBatStatus = stat;
            emit batteryStatusChanged(stat);
        }

    } else if (prop == QLatin1String("Voltage")) {
        currentVoltage = v.toDouble() * 1000;
    } else if (prop == QLatin1String("State")) {
        switch(v.toUInt()) {
        case 1: // charging
            {
                curChargeState = QSystemBatteryInfo::Charging;
                curChargeType = QSystemBatteryInfo::WallCharger;
            }
            break;
        case 2: //discharging
        case 3: //empty
        case 5: //pending charge
        case 6: //pending discharge
            curChargeState = QSystemBatteryInfo::NotCharging;
            curChargeType = QSystemBatteryInfo::NoCharger;
            break;
        case 4: //fully charged
            curChargeState = QSystemBatteryInfo::NotCharging;
            curChargeType = QSystemBatteryInfo::WallCharger;
            break;

        default:
            curChargeState = QSystemBatteryInfo::ChargingError;
            break;
        };
        emit chargingStateChanged(curChargeState);
        emit chargerTypeChanged(curChargeType);
    } else if (prop == QLatin1String("Capacity")) {
    } else if (prop == QLatin1String("UpdateTime")) {
    } else if (prop == QLatin1String("TimeToFull")) {
        timeToFull = v.toUInt();
        emit remainingChargingTimeChanged(timeToFull);
    } else if (prop == QLatin1String("Type")) {
    }

}
#endif // QT_NO_UPOWER

#include "moc_qsysteminfo_linux_common_p.cpp"

QTM_END_NAMESPACE
