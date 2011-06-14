/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtSystemKit module of the Qt Toolkit.
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

#include <qdeviceprofile.h>

QT_BEGIN_NAMESPACE
class QDeviceProfilePrivate
{
public:
    QDeviceProfilePrivate(QDeviceProfile *) {}

    bool vibrationActived() { return false; }
    int messageRingtoneVolume() { return -1; }
    int voiceRingtoneVolume() { return -1; }
    QDeviceProfile current() { return QDeviceProfile(); }
    QDeviceProfile::ProfileType profileType() { return QDeviceProfile::UnknownProfile; }
};
QT_END_NAMESPACE

QT_BEGIN_NAMESPACE

/*!
    \class QDeviceProfile
    \inmodule QtSystemKit
    \brief The QDeviceProfile class provides details for the profile of the device.
*/

/*!
    \enum QDeviceProfile::ProfileType
    This enum describes the type of the current profile.

    \value UnknownProfile   Profile unknown or on error.
    \value SilentProfile    Silent profile.
    \value NormalProfile    Normal profile.
    \value LoudProfile      Loud profile.
    \value VibProfile       Vibrate profile.
    \value BeepProfile      Beep profile.
    \value CustomProfile    Custom profile.
*/

/*!
    \fn  void QDeviceProfile::currentProfileChanged(const QDeviceProfile &profile)

    This signal is emitted whenever the activated profile has changed to \a profile.
*/

/*!
    Constructs a QDeviceProfile object with the given \a parent.
*/
QDeviceProfile::QDeviceProfile(QObject *parent)
    : QObject(parent)
    , d_ptr(new QDeviceProfilePrivate(this))
{
}

/*!
    Constructs a QDeviceProfile object from \a other.
*/
QDeviceProfile::QDeviceProfile(const QDeviceProfile &other)
    : QObject(0)
    , d_ptr(new QDeviceProfilePrivate(this))
{
    Q_UNUSED(other)
}

/*!
    Assigns \a other to this profile and returns a reference to this profile.
*/
QDeviceProfile &QDeviceProfile::operator=(const QDeviceProfile &other)
{
    Q_UNUSED(other)
    return *this;
}

/*!
    Destroys the object
*/
QDeviceProfile::~QDeviceProfile()
{
    delete d_ptr;
}

/*!
    Returns the whether the vibration is active for this profile.
*/
bool QDeviceProfile::vibrationActived() const
{
    return d_ptr->vibrationActived();
}

/*!
    Returns the message ringtone volume for this profile, from 0 to 100. If this information is unknown,
    or error occurs, -1 is returned.
*/
int QDeviceProfile::messageRingtoneVolume() const
{
    return d_ptr->messageRingtoneVolume();
}

/*!
    Returns the voice ringtone volume for this profile, from 0 to 100. If this information is unknown,
    or error occurs, -1 is returned.
*/
int QDeviceProfile::voiceRingtoneVolume() const
{
    return d_ptr->voiceRingtoneVolume();
}

/*!
    Returns the type for this profile.
*/
QDeviceProfile::ProfileType QDeviceProfile::profileType() const
{
    return d_ptr->profileType();
}

/*!
    Returns the currently activated profile. If this information is unknown, or error occurs, a profile
    of type UnknownProfile is returned.
*/
QDeviceProfile QDeviceProfile::current()
{
    QDeviceProfilePrivate priv(0);
    return priv.current();
}

QT_END_NAMESPACE