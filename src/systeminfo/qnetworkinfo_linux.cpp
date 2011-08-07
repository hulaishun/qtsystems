/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtSystems module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qnetworkinfo_linux_p.h"

#if !defined(QT_NO_OFONO)
#include "qofonowrapper_p.h"
#endif // QT_NO_OFONO

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qtimer.h>

#if !defined(QT_NO_BLUEZ)
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#endif // QT_NO_BLUEZ

#if !defined(QT_NO_UDEV)
#include <QtCore/qsocketnotifier.h>
#include <libudev.h>
#endif // QT_NO_UDEV

#include <math.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/wireless.h>

QT_BEGIN_NAMESPACE

static const QString BLUETOOTH_SYSFS_PATH(QString::fromAscii("/sys/class/bluetooth/"));
static const QString NETWORK_SYSFS_PATH(QString::fromAscii("/sys/class/net/"));

static const QStringList WLAN_MASK(QString::fromAscii("wlan*"));
static const QStringList ETHERNET_MASK(QStringList() << QString::fromAscii("eth*") << QString::fromAscii("usb*"));

QNetworkInfoPrivate::QNetworkInfoPrivate(QNetworkInfo *parent)
    : QObject(parent)
    , q_ptr(parent)
    , watchCurrentNetworkMode(false)
    , watchNetworkInterfaceCount(false)
    , watchNetworkSignalStrength(false)
    , watchNetworkStatus(false)
    , watchNetworkName(false)
    , timer(0)
#if !defined(QT_NO_OFONO)
    , ofonoWrapper(0)
#endif // QT_NO_OFONO
#if !defined(QT_NO_UDEV)
    , udevNotifier(0)
    , udevHandle(0)
    , udevMonitor(0)
#endif // QT_NO_UDEV
{
}

QNetworkInfoPrivate::~QNetworkInfoPrivate()
{
#if !defined(QT_NO_UDEV)
    if (udevMonitor)
        udev_monitor_unref(udevMonitor);

    if (udevHandle)
        udev_unref(udevHandle);
#endif // QT_NO_UDEV
}

int QNetworkInfoPrivate::networkInterfaceCount(QNetworkInfo::NetworkMode mode)
{
    if (watchNetworkInterfaceCount)
        return networkInterfaceCounts.value(mode);
    else
        return getNetworkInterfaceCount(mode);
}

int QNetworkInfoPrivate::networkSignalStrength(QNetworkInfo::NetworkMode mode, int interface)
{
    if (watchNetworkSignalStrength)
        return networkSignalStrengths.value(QPair<QNetworkInfo::NetworkMode, int>(mode, interface));
    else
        return getNetworkSignalStrength(mode, interface);
}

QNetworkInfo::CellDataTechnology QNetworkInfoPrivate::currentCellDataTechnology(int interface)
{
#if !defined(QT_NO_OFONO)
    if (QOfonoWrapper::isOfonoAvailable()) {
        if (!ofonoWrapper)
            ofonoWrapper = new QOfonoWrapper(this);
        QString modem = ofonoWrapper->allModems().at(interface);
        if (!modem.isEmpty())
            return ofonoWrapper->currentCellDataTechnology(modem);
    }
#else
    Q_UNUSED(interface)
#endif
    return QNetworkInfo::UnknownDataTechnology;
}

QNetworkInfo::NetworkMode QNetworkInfoPrivate::currentNetworkMode()
{
    if (watchCurrentNetworkMode)
        return currentMode;
    else
        return getCurrentNetworkMode();
}

QNetworkInfo::NetworkStatus QNetworkInfoPrivate::networkStatus(QNetworkInfo::NetworkMode mode, int interface)
{
    if (watchNetworkStatus)
        return networkStatuses.value(QPair<QNetworkInfo::NetworkMode, int>(mode, interface));
    else
        return getNetworkStatus(mode, interface);
}

QNetworkInterface QNetworkInfoPrivate::interfaceForMode(QNetworkInfo::NetworkMode mode, int interface)
{
    switch (mode) {
    case QNetworkInfo::WlanMode: {
        const QString dir = QDir(NETWORK_SYSFS_PATH).entryList(WLAN_MASK).at(interface);
        if (dir.isEmpty())
            break;
        QNetworkInterface interface = QNetworkInterface::interfaceFromName(dir);
        if (interface.isValid())
            return interface;
        break;
    }

    case QNetworkInfo::EthernetMode: {
        const QString dir = QDir(NETWORK_SYSFS_PATH).entryList(ETHERNET_MASK).at(interface);
        if (dir.isEmpty())
            break;
        QNetworkInterface interface = QNetworkInterface::interfaceFromName(dir);
        if (interface.isValid())
            return interface;
        break;
    }

//    case QNetworkInfo::BluetoothMode:
//    case QNetworkInfo::GsmMode:
//    case QNetworkInfo::CdmaMode:
//    case QNetworkInfo::WcdmaMode:
//    case QNetworkInfo::WimaxMode:
//    case QNetworkInfo::LteMode:
    default:
        break;
    };

    return QNetworkInterface();
}

QString QNetworkInfoPrivate::cellId(int interface)
{
#if !defined(QT_NO_OFONO)
    if (QOfonoWrapper::isOfonoAvailable()) {
        if (!ofonoWrapper)
            ofonoWrapper = new QOfonoWrapper(this);
        QString modem = ofonoWrapper->allModems().at(interface);
        if (!modem.isEmpty())
            return ofonoWrapper->cellId(modem);
    }
#else
    Q_UNUSED(interface)
#endif
    return QString();
}

QString QNetworkInfoPrivate::currentMobileCountryCode(int interface)
{
#if !defined(QT_NO_OFONO)
    if (QOfonoWrapper::isOfonoAvailable()) {
        if (!ofonoWrapper)
            ofonoWrapper = new QOfonoWrapper(this);
        QString modem = ofonoWrapper->allModems().at(interface);
        if (!modem.isEmpty())
            return ofonoWrapper->currentMcc(modem);
    }
#else
    Q_UNUSED(interface)
#endif
    return QString();
}

QString QNetworkInfoPrivate::currentMobileNetworkCode(int interface)
{
#if !defined(QT_NO_OFONO)
    if (QOfonoWrapper::isOfonoAvailable()) {
        if (!ofonoWrapper)
            ofonoWrapper = new QOfonoWrapper(this);
        QString modem = ofonoWrapper->allModems().at(interface);
        if (!modem.isEmpty())
            return ofonoWrapper->currentMnc(modem);
    }
#else
    Q_UNUSED(interface)
#endif
    return QString();
}

QString QNetworkInfoPrivate::homeMobileCountryCode(int interface)
{
#if !defined(QT_NO_OFONO)
    if (QOfonoWrapper::isOfonoAvailable()) {
        if (!ofonoWrapper)
            ofonoWrapper = new QOfonoWrapper(this);
        QString modem = ofonoWrapper->allModems().at(interface);
        if (!modem.isEmpty())
            return ofonoWrapper->homeMcc(modem);
    }
#else
    Q_UNUSED(interface)
#endif
    return QString();
}

QString QNetworkInfoPrivate::homeMobileNetworkCode(int interface)
{
#if !defined(QT_NO_OFONO)
    if (QOfonoWrapper::isOfonoAvailable()) {
        if (!ofonoWrapper)
            ofonoWrapper = new QOfonoWrapper(this);
        QString modem = ofonoWrapper->allModems().at(interface);
        if (!modem.isEmpty())
            return ofonoWrapper->homeMnc(modem);
    }
#else
    Q_UNUSED(interface)
#endif
    return QString();
}

QString QNetworkInfoPrivate::imsi(int interface)
{
#if !defined(QT_NO_OFONO)
    if (QOfonoWrapper::isOfonoAvailable()) {
        if (!ofonoWrapper)
            ofonoWrapper = new QOfonoWrapper(this);
        QString modem = ofonoWrapper->allModems().at(interface);
        if (!modem.isEmpty())
            return ofonoWrapper->imsi(modem);
    }
#else
    Q_UNUSED(interface)
#endif
    return QString();
}

QString QNetworkInfoPrivate::locationAreaCode(int interface)
{
#if !defined(QT_NO_OFONO)
    if (QOfonoWrapper::isOfonoAvailable()) {
        if (!ofonoWrapper)
            ofonoWrapper = new QOfonoWrapper(this);
        QString modem = ofonoWrapper->allModems().at(interface);
        if (!modem.isEmpty())
            return ofonoWrapper->lac(modem);
    }
#else
    Q_UNUSED(interface)
#endif
    return QString();
}

QString QNetworkInfoPrivate::macAddress(QNetworkInfo::NetworkMode mode, int interface)
{
    switch (mode) {
    case QNetworkInfo::WlanMode: {
        const QString dir = QDir(NETWORK_SYSFS_PATH).entryList(WLAN_MASK).at(interface);
        if (dir.isEmpty())
            break;
        QFile carrier(NETWORK_SYSFS_PATH + dir + QString::fromAscii("/address"));
        if (carrier.open(QIODevice::ReadOnly))
            return QString::fromAscii(carrier.readAll().simplified().data());
        break;
    }

    case QNetworkInfo::EthernetMode: {
        const QString dir = QDir(NETWORK_SYSFS_PATH).entryList(ETHERNET_MASK).at(interface);
        if (dir.isEmpty())
            break;
        QFile carrier(NETWORK_SYSFS_PATH + dir + QString::fromAscii("/address"));
        if (carrier.open(QIODevice::ReadOnly))
            return QString::fromAscii(carrier.readAll().simplified().data());
        break;
    }

    case QNetworkInfo::BluetoothMode: {
#if !defined(QT_NO_BLUEZ)
        int ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
        if (ctl < 0)
            break;
        struct hci_dev_list_req *deviceList = (struct hci_dev_list_req *)malloc(HCI_MAX_DEV * sizeof(struct hci_dev_req) + sizeof(uint16_t));
        deviceList->dev_num = HCI_MAX_DEV;
        QString macAddress;
        if (ioctl(ctl, HCIGETDEVLIST, deviceList) == 0) {
            int count = deviceList->dev_num;
            if (interface < count) {
                struct hci_dev_info deviceInfo;
                deviceInfo.dev_id = (deviceList->dev_req + interface)->dev_id;
                if (ioctl(ctl, HCIGETDEVINFO, &deviceInfo) == 0) {
                    // do not use BDADDR_ANY, fails with gcc 4.6
                    bdaddr_t bdaddr_any = (bdaddr_t) {{0, 0, 0, 0, 0, 0}};
                    if (hci_test_bit(HCI_RAW, &deviceInfo.flags) && !bacmp(&deviceInfo.bdaddr, &bdaddr_any)) {
                        int hciDevice = hci_open_dev(deviceInfo.dev_id);
                        hci_read_bd_addr(hciDevice, &deviceInfo.bdaddr, 1000);
                        hci_close_dev(hciDevice);
                    }
                    char address[18];
                    ba2str(&deviceInfo.bdaddr, address);
                    macAddress = QString::fromAscii(address);
                }
            }
        }
        free(deviceList);
        close(ctl);
        return macAddress;
#endif // QT_NO_BLUEZ
    }

//    case QNetworkInfo::GsmMode:
//    case QNetworkInfo::CdmaMode:
//    case QNetworkInfo::WcdmaMode:
//    case QNetworkInfo::WimaxMode:
//    case QNetworkInfo::LteMode:
    default:
        break;
    };

    return QString();
}

QString QNetworkInfoPrivate::networkName(QNetworkInfo::NetworkMode mode, int interface)
{
    if (watchNetworkName)
        return networkNames.value(QPair<QNetworkInfo::NetworkMode, int>(mode, interface));
    else
        return getNetworkName(mode, interface);
}

void QNetworkInfoPrivate::connectNotify(const char *signal)
{
#if !defined(QT_NO_OFONO)
    if (QOfonoWrapper::isOfonoAvailable()) {
        if (!ofonoWrapper)
            ofonoWrapper = new QOfonoWrapper(this);
        connect(ofonoWrapper, signal, this, signal, Qt::UniqueConnection);
    }
#endif // // QT_NO_OFONO

    if (strcmp(signal, SIGNAL(networkInterfaceCountChanged(QNetworkInfo::NetworkMode,int))) == 0) {
        QList<QNetworkInfo::NetworkMode> modes;
        modes << QNetworkInfo::WlanMode << QNetworkInfo::EthernetMode << QNetworkInfo::BluetoothMode;
        networkInterfaceCounts.clear();
        foreach (QNetworkInfo::NetworkMode mode, modes)
            networkInterfaceCounts[mode] = getNetworkInterfaceCount(mode);
#if !defined(QT_NO_UDEV)
        if (!udevHandle) {
            udevHandle = udev_new();
            udevMonitor = udev_monitor_new_from_netlink(udevHandle, "udev");
            udev_monitor_filter_add_match_subsystem_devtype(udevMonitor, "net", NULL);
            udev_monitor_enable_receiving(udevMonitor);
            udevNotifier = new QSocketNotifier(udev_monitor_get_fd(udevMonitor), QSocketNotifier::Read, this);
            connect(udevNotifier, SIGNAL(activated(int)), this, SLOT(onUdevChanged()));
        }
        udevNotifier->setEnabled(true);

        watchNetworkInterfaceCount = true;

        return;
#else
        watchNetworkInterfaceCount = true;
#endif // QT_NO_UDEV
    } else if (strcmp(signal, SIGNAL(networkSignalStrengthChanged(QNetworkInfo::NetworkMode,int,int))) == 0) {
        QList<QNetworkInfo::NetworkMode> modes;
        modes << QNetworkInfo::WlanMode << QNetworkInfo::EthernetMode << QNetworkInfo::BluetoothMode;
        networkSignalStrengths.clear();
        foreach (QNetworkInfo::NetworkMode mode, modes) {
            int count = networkInterfaceCount(mode);
            for (int i = 0; i < count; ++i)
                networkSignalStrengths[QPair<QNetworkInfo::NetworkMode, int>(mode, i)] = getNetworkSignalStrength(mode, i);
        }

        watchNetworkSignalStrength = true;
    } else if (strcmp(signal, SIGNAL(networkStatusChanged(QNetworkInfo::NetworkMode,int,QNetworkInfo::NetworkStatus))) == 0) {
        QList<QNetworkInfo::NetworkMode> modes;
        modes << QNetworkInfo::WlanMode << QNetworkInfo::EthernetMode << QNetworkInfo::BluetoothMode;
        networkStatuses.clear();
        foreach (QNetworkInfo::NetworkMode mode, modes) {
            int count = networkInterfaceCount(mode);
            for (int i = 0; i < count; ++i)
                networkStatuses[QPair<QNetworkInfo::NetworkMode, int>(mode, i)] = getNetworkStatus(mode, i);
        }

        watchNetworkStatus = true;
    } else if (strcmp(signal, SIGNAL(networkNameChanged(QNetworkInfo::NetworkMode,int,QString))) == 0) {
        QList<QNetworkInfo::NetworkMode> modes;
        modes << QNetworkInfo::WlanMode << QNetworkInfo::EthernetMode << QNetworkInfo::BluetoothMode;
        networkNames.clear();
        foreach (QNetworkInfo::NetworkMode mode, modes) {
            int count = networkInterfaceCount(mode);
            for (int i = 0; i < count; ++i)
                networkNames[QPair<QNetworkInfo::NetworkMode, int>(mode, i)] = getNetworkName(mode, i);
        }

        watchNetworkName = true;
    } else if (strcmp(signal, SIGNAL(currentNetworkModeChanged(QNetworkInfo::NetworkMode))) == 0) {
        currentMode = getCurrentNetworkMode();
        watchCurrentNetworkMode = true;
    } else {
        return;
    }

    if (!timer) {
        timer = new QTimer(this);
        timer->setInterval(2000);
        connect(timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
    }

    if (!timer->isActive())
        timer->start();
}

void QNetworkInfoPrivate::disconnectNotify(const char *signal)
{
#if !defined(QT_NO_OFONO)
    if (!QOfonoWrapper::isOfonoAvailable() || !ofonoWrapper)
        return;

    disconnect(ofonoWrapper, signal, this, signal);
#endif // // QT_NO_OFONO

    if (strcmp(signal, SIGNAL(networkInterfaceCountChanged(QNetworkInfo::NetworkMode,int))) == 0) {
#if !defined(QT_NO_UDEV)
        udevNotifier->setEnabled(false);
        watchNetworkInterfaceCount = false;
        return;
#endif // QT_NO_UDEV
    } else if (strcmp(signal, SIGNAL(networkSignalStrengthChanged(QNetworkInfo::NetworkMode,int,int))) == 0) {
        watchNetworkSignalStrength = false;
    } else if (strcmp(signal, SIGNAL(networkStatusChanged(QNetworkInfo::NetworkMode,int,QNetworkInfo::NetworkStatus))) == 0) {
        watchNetworkStatus = false;
    } else if (strcmp(signal, SIGNAL(networkNameChanged(QNetworkInfo::NetworkMode,int,QString))) == 0) {
        watchNetworkName = false;
    } else if (strcmp(signal, SIGNAL(currentNetworkModeChanged(QNetworkInfo::NetworkMode))) == 0) {
        watchCurrentNetworkMode = false;
    } else {
        return;
    }

    if (!watchNetworkSignalStrength && !watchNetworkStatus && !watchNetworkName && !watchCurrentNetworkMode)
        timer->stop();
}

#if !defined(QT_NO_UDEV)
void QNetworkInfoPrivate::onUdevChanged()
{
    struct udev_device *udevDevice = udev_monitor_receive_device(udevMonitor);
    if (!udevDevice)
        return;

    if (0 != strcmp(udev_device_get_subsystem(udevDevice), "net"))
        return;

    QString sysname(QString::fromLocal8Bit(udev_device_get_sysname(udevDevice)));
    if (watchNetworkInterfaceCount) {
        if (sysname.startsWith(QString::fromUtf8("eth")) || sysname.startsWith(QString::fromUtf8("usb"))) {
            if (0 == strcmp(udev_device_get_action(udevDevice), "add"))
                ++networkInterfaceCounts[QNetworkInfo::EthernetMode];
            else if (0 == strcmp(udev_device_get_action(udevDevice), "remove"))
                --networkInterfaceCounts[QNetworkInfo::EthernetMode];
            Q_EMIT networkInterfaceCountChanged(QNetworkInfo::EthernetMode,
                                                networkInterfaceCounts[QNetworkInfo::EthernetMode]);
        } else if (sysname.startsWith(QString::fromUtf8("wlan"))) {
            if (0 == strcmp(udev_device_get_action(udevDevice), "add"))
                ++networkInterfaceCounts[QNetworkInfo::WlanMode];
            else if (0 == strcmp(udev_device_get_action(udevDevice), "remove"))
                --networkInterfaceCounts[QNetworkInfo::WlanMode];
            Q_EMIT networkInterfaceCountChanged(QNetworkInfo::WlanMode,
                                                networkInterfaceCounts[QNetworkInfo::WlanMode]);
        }
    }

    udev_device_unref(udevDevice);
}
#endif // QT_NO_UDEV

void QNetworkInfoPrivate::onTimeout()
{
#if defined(QT_NO_UDEV)
    if (watchNetworkInterfaceCount) {
        QList<QNetworkInfo::NetworkMode> modes;
        modes << QNetworkInfo::WlanMode << QNetworkInfo::EthernetMode << QNetworkInfo::BluetoothMode;
        foreach (QNetworkInfo::NetworkMode mode, modes) {
            int value = getNetworkInterfaceCount(mode);
            if (networkInterfaceCounts.value(mode) != value) {
                networkInterfaceCounts[mode] = value;
                Q_EMIT networkInterfaceCountChanged(mode, value);
            }
        }
    }
#endif // QT_NO_UDEV

    if (watchCurrentNetworkMode) {
        QNetworkInfo::NetworkMode value = getCurrentNetworkMode();
        if (currentMode != value) {
            currentMode = value;
            Q_EMIT currentNetworkModeChanged(value);
        }
    }

    if (!watchNetworkSignalStrength && !watchNetworkStatus && !watchNetworkName)
        return;

    QList<QNetworkInfo::NetworkMode> modes;
    modes << QNetworkInfo::WlanMode << QNetworkInfo::EthernetMode << QNetworkInfo::BluetoothMode;
    foreach (QNetworkInfo::NetworkMode mode, modes) {
        int count = networkInterfaceCount(mode);
        for (int i = 0; i < count; ++i) {
            if (watchNetworkSignalStrength) {
                int value = getNetworkSignalStrength(mode, i);
                QPair<QNetworkInfo::NetworkMode, int> key(mode, i);
                if (networkSignalStrengths.value(key) != value) {
                    networkSignalStrengths[key] = value;
                    Q_EMIT networkSignalStrengthChanged(mode, i, value);
                }
            }

            if (watchNetworkStatus) {
                QNetworkInfo::NetworkStatus value = getNetworkStatus(mode, i);
                QPair<QNetworkInfo::NetworkMode, int> key(mode, i);
                if (networkStatuses.value(key) != value) {
                    networkStatuses[key] = value;
                    Q_EMIT networkStatusChanged(mode, i, value);
                }
            }

            if (watchNetworkSignalStrength) {
                QString value = getNetworkName(mode, i);
                QPair<QNetworkInfo::NetworkMode, int> key(mode, i);
                if (networkNames.value(key) != value) {
                    networkNames[key] = value;
                    Q_EMIT networkNameChanged(mode, i, value);
                }
            }
        }
    }
}

int QNetworkInfoPrivate::getNetworkInterfaceCount(QNetworkInfo::NetworkMode mode)
{
    switch (mode) {
    case QNetworkInfo::WlanMode:
        return QDir(NETWORK_SYSFS_PATH).entryList(WLAN_MASK).size();

    case QNetworkInfo::EthernetMode:
        return QDir(NETWORK_SYSFS_PATH).entryList(ETHERNET_MASK).size();

    case QNetworkInfo::BluetoothMode: {
#if !defined(QT_NO_BLUEZ)
        int ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
        if (ctl < 0)
            return -1;
        struct hci_dev_list_req *deviceList = (struct hci_dev_list_req *)malloc(HCI_MAX_DEV * sizeof(struct hci_dev_req) + sizeof(uint16_t));
        deviceList->dev_num = HCI_MAX_DEV;
        int count = -1;
        if (ioctl(ctl, HCIGETDEVLIST, deviceList) == 0)
            count = deviceList->dev_num;
        free(deviceList);
        close(ctl);
        return count;
#endif // QT_NO_BLUEZ
    }

    case QNetworkInfo::GsmMode:
    case QNetworkInfo::CdmaMode:
    case QNetworkInfo::WcdmaMode:
    case QNetworkInfo::LteMode:
#if !defined(QT_NO_OFONO)
        if (QOfonoWrapper::isOfonoAvailable()) {
            if (!ofonoWrapper)
                ofonoWrapper = new QOfonoWrapper(this);
            return ofonoWrapper->allModems().size();
        }
#endif // QT_NO_OFONO

//    case QNetworkInfo::WimaxMode:
    default:
        return -1;
    };
}

int QNetworkInfoPrivate::getNetworkSignalStrength(QNetworkInfo::NetworkMode mode, int interface)
{
    switch (mode) {
    case QNetworkInfo::WlanMode: {
        QFile file(QString::fromAscii("/proc/net/wireless"));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return -1;

        QTextStream in(&file);
        QString interfaceName = interfaceForMode(QNetworkInfo::WlanMode, interface).name();
        QString line = in.readLine();
        while (!line.isNull()) {
            if (line.left(6).contains(interfaceName)) {
                QString token = line.section(QString::fromAscii(" "), 4, 5).simplified();
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

    case QNetworkInfo::EthernetMode:
        if (networkStatus(QNetworkInfo::EthernetMode, interface) == QNetworkInfo::HomeNetwork)
            return 100;
        else
            return -1;

    case QNetworkInfo::GsmMode:
    case QNetworkInfo::CdmaMode:
    case QNetworkInfo::WcdmaMode:
    case QNetworkInfo::LteMode:
#if !defined(QT_NO_OFONO)
        if (QOfonoWrapper::isOfonoAvailable()) {
            if (!ofonoWrapper)
                ofonoWrapper = new QOfonoWrapper(this);
            QString modem = ofonoWrapper->allModems().at(interface);
            if (!modem.isEmpty() && ofonoWrapper->networkMode(modem) == mode)
                return ofonoWrapper->signalStrength(modem);
        }
#endif // QT_NO_OFONO
        break;

    case QNetworkInfo::BluetoothMode: {
        int signalStrength = -1;
#if !defined(QT_NO_BLUEZ)
        int ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
        if (ctl < 0)
            break;
        struct hci_dev_list_req *deviceList = (struct hci_dev_list_req *)malloc(HCI_MAX_DEV * sizeof(struct hci_dev_req) + sizeof(uint16_t));
        deviceList->dev_num = HCI_MAX_DEV;
        if (ioctl(ctl, HCIGETDEVLIST, deviceList) == 0) {
            int count = deviceList->dev_num;
            if (interface < count) {
                signalStrength = 0; // Valid interface

                struct hci_conn_list_req *connectionList = (struct hci_conn_list_req *)malloc(sizeof(struct hci_conn_info) + sizeof(struct hci_conn_list_req));
                connectionList->dev_id = (deviceList->dev_req + interface)->dev_id;
                connectionList->conn_num = 1;
                if (ioctl(ctl, HCIGETCONNLIST, connectionList) == 0) {
                    if (connectionList->conn_num == 1) {
                        int fd = hci_open_dev((deviceList->dev_req + interface)->dev_id);
                        if (fd > 0) {
                            struct hci_conn_info_req *connectionInfo = (struct hci_conn_info_req *)malloc(sizeof(struct hci_conn_info_req) + sizeof(struct hci_conn_info));
                            bacpy(&connectionInfo->bdaddr, &connectionList->conn_info->bdaddr);
                            connectionInfo->type = ACL_LINK;
                            if (ioctl(fd, HCIGETCONNINFO, connectionInfo) == 0) {
                                uint8_t linkQuality;
                                if (hci_read_link_quality(fd, htobs(connectionInfo->conn_info->handle), &linkQuality, 0) == 0)
                                    signalStrength = linkQuality * 100 / 255;
                            }
                            free(connectionInfo);
                        }
                    }
                }
                free (connectionList);
            }
        }
        free(deviceList);
        close(ctl);
#endif // QT_NO_BLUEZ
        return signalStrength;
    }

//    case QNetworkInfo::WimaxMode:
    default:
        break;
    };

    return -1;
}

QNetworkInfo::NetworkMode QNetworkInfoPrivate::getCurrentNetworkMode()
{
    // TODO multiple-interface support
    if (networkStatus(QNetworkInfo::EthernetMode, 0) == QNetworkInfo::HomeNetwork)
        return QNetworkInfo::EthernetMode;
    else if (networkStatus(QNetworkInfo::WlanMode, 0) == QNetworkInfo::HomeNetwork)
        return QNetworkInfo::WlanMode;
    else if (networkStatus(QNetworkInfo::BluetoothMode, 0) == QNetworkInfo::HomeNetwork)
        return QNetworkInfo::BluetoothMode;
    else if (networkStatus(QNetworkInfo::WimaxMode, 0) == QNetworkInfo::HomeNetwork)
        return QNetworkInfo::WimaxMode;
    else if (networkStatus(QNetworkInfo::LteMode, 0) == QNetworkInfo::HomeNetwork)
        return QNetworkInfo::LteMode;
    else if (networkStatus(QNetworkInfo::WcdmaMode, 0) == QNetworkInfo::HomeNetwork)
        return QNetworkInfo::WcdmaMode;
    else if (networkStatus(QNetworkInfo::CdmaMode, 0) == QNetworkInfo::HomeNetwork)
        return QNetworkInfo::CdmaMode;
    else if (networkStatus(QNetworkInfo::GsmMode, 0) == QNetworkInfo::HomeNetwork)
        return QNetworkInfo::GsmMode;
    else if (networkStatus(QNetworkInfo::WimaxMode, 0) == QNetworkInfo::Roaming)
        return QNetworkInfo::WimaxMode;
    else if (networkStatus(QNetworkInfo::LteMode, 0) == QNetworkInfo::Roaming)
        return QNetworkInfo::LteMode;
    else if (networkStatus(QNetworkInfo::WcdmaMode, 0) == QNetworkInfo::Roaming)
        return QNetworkInfo::WcdmaMode;
    else if (networkStatus(QNetworkInfo::CdmaMode, 0) == QNetworkInfo::Roaming)
        return QNetworkInfo::CdmaMode;
    else if (networkStatus(QNetworkInfo::GsmMode, 0) == QNetworkInfo::Roaming)
        return QNetworkInfo::GsmMode;
    else
        return QNetworkInfo::UnknownMode;
}

QNetworkInfo::NetworkStatus QNetworkInfoPrivate::getNetworkStatus(QNetworkInfo::NetworkMode mode, int interface)
{
    switch (mode) {
    case QNetworkInfo::WlanMode: {
        const QString dir = QDir(NETWORK_SYSFS_PATH).entryList(WLAN_MASK).at(interface);
        if (dir.isEmpty())
            return QNetworkInfo::UnknownStatus;
        QFile carrier(NETWORK_SYSFS_PATH + dir + QString::fromAscii("/carrier"));
        if (carrier.open(QIODevice::ReadOnly)) {
            char state;
            if (carrier.read(&state, 1) == 1 && state == '1')
                return QNetworkInfo::HomeNetwork;
        }
        return QNetworkInfo::NoNetworkAvailable;
    }

    case QNetworkInfo::EthernetMode: {
        const QString dir = QDir(NETWORK_SYSFS_PATH).entryList(ETHERNET_MASK).at(interface);
        if (dir.isEmpty())
            return QNetworkInfo::UnknownStatus;
        QFile carrier(NETWORK_SYSFS_PATH + dir + QString::fromAscii("/carrier"));
        if (carrier.open(QIODevice::ReadOnly)) {
            char state;
            if (carrier.read(&state, 1) == 1 && state == '1')
                return QNetworkInfo::HomeNetwork;
        }
        return QNetworkInfo::NoNetworkAvailable;
    }

    case QNetworkInfo::BluetoothMode: {
        QNetworkInfo::NetworkStatus status = QNetworkInfo::UnknownStatus;

#if !defined(QT_NO_BLUEZ)
        int ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
        if (ctl < 0)
            break;
        struct hci_dev_list_req *deviceList = (struct hci_dev_list_req *)malloc(HCI_MAX_DEV * sizeof(struct hci_dev_req) + sizeof(uint16_t));
        deviceList->dev_num = HCI_MAX_DEV;
        if (ioctl(ctl, HCIGETDEVLIST, deviceList) == 0) {
            int count = deviceList->dev_num;
            if (interface < count) {
                status = QNetworkInfo::NoNetworkAvailable; // Valid interface, so either not connected or connected
                                                           // TODO add support for searching and denied
                struct hci_conn_list_req *connectionList = (struct hci_conn_list_req *)malloc(sizeof(struct hci_conn_info) + sizeof(struct hci_conn_list_req));
                connectionList->dev_id = (deviceList->dev_req + interface)->dev_id;
                connectionList->conn_num = 1;
                if (ioctl(ctl, HCIGETCONNLIST, connectionList) == 0) {
                    if (connectionList->conn_num == 1)
                        status = QNetworkInfo::HomeNetwork;
                }
                free (connectionList);
            }
        }
        free(deviceList);
        close(ctl);
#endif // QT_NO_BLUEZ

        return status;
    }

    case QNetworkInfo::GsmMode:
    case QNetworkInfo::CdmaMode:
    case QNetworkInfo::WcdmaMode:
    case QNetworkInfo::LteMode:
#if !defined(QT_NO_OFONO)
        if (QOfonoWrapper::isOfonoAvailable()) {
            if (!ofonoWrapper)
                ofonoWrapper = new QOfonoWrapper(this);
            QString modem = ofonoWrapper->allModems().at(interface);
            if (!modem.isEmpty() && ofonoWrapper->networkMode(modem) == mode)
                return ofonoWrapper->networkStatus(modem);
    }
#endif
        break;

//    case QNetworkInfo::WimaxMode:
    default:
        break;
    };

    return QNetworkInfo::UnknownStatus;
}

QString QNetworkInfoPrivate::getNetworkName(QNetworkInfo::NetworkMode mode, int interface)
{
    switch (mode) {
    case QNetworkInfo::WlanMode: {
        const QString dir = QDir(NETWORK_SYSFS_PATH).entryList(WLAN_MASK).at(interface);
        if (dir.isEmpty())
            break;
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
                return QString::fromAscii((const char *)iwInfo.u.essid.pointer);
            }

            close(sock);
        }
        break;
    }

    case QNetworkInfo::EthernetMode: {
        // TODO multiple-interface support
        char domainName[64];
        if (getdomainname(domainName, 64) == 0) {
            if (strcmp(domainName, "(none)") != 0)
                return QString::fromAscii(domainName);
        }
        break;
    }

    case QNetworkInfo::BluetoothMode: {
#if !defined(QT_NO_BLUEZ)
        int ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
        if (ctl < 0)
            break;
        struct hci_dev_list_req *deviceList = (struct hci_dev_list_req *)malloc(HCI_MAX_DEV * sizeof(struct hci_dev_req) + sizeof(uint16_t));
        deviceList->dev_num = HCI_MAX_DEV;
        QString networkName;
        if (ioctl(ctl, HCIGETDEVLIST, deviceList) == 0) {
            int count = deviceList->dev_num;
            if (interface < count) {
                int fd = hci_open_dev((deviceList->dev_req + interface)->dev_id);
                if (fd > 0) {
                    char name[249];
                    if (hci_read_local_name(fd, sizeof(name), name, 0) == 0)
                        networkName = QString::fromAscii(name);
                }
            }
        }
        free(deviceList);
        close(ctl);
        return networkName;
#endif // QT_NO_BLUEZ
    }

    case QNetworkInfo::GsmMode:
    case QNetworkInfo::CdmaMode:
    case QNetworkInfo::WcdmaMode:
    case QNetworkInfo::LteMode:
#if !defined(QT_NO_OFONO)
        if (QOfonoWrapper::isOfonoAvailable()) {
            if (!ofonoWrapper)
                ofonoWrapper = new QOfonoWrapper(this);
            QString modem = ofonoWrapper->allModems().at(interface);
            if (!modem.isEmpty())
                return ofonoWrapper->operatorName(modem);
        }
#endif // QT_NO_OFONO
        break;

//    case QNetworkInfo::WimaxMode:
    default:
        break;
    };

    return QString();
}

QT_END_NAMESPACE
