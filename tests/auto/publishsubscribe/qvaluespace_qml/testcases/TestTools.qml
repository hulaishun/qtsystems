/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the test suite of the Qt Toolkit.
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
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0

import QtJsonDb 1.0 as JsonDb

Item {
    id: jsonDbHandler

    property var partition: JsonDb.partition("com.nokia.mt.System")
    property var objectsToDelete: []
    property var count

    function createObjects(objectsToCreate) {
        count = objectsToCreate.length

        for (var i = 0; i < objectsToCreate.length; i++)
            partition.create(objectsToCreate[i], createCallback)

        while (count > 0)
            wait(1)
    }

    function createCallback(error, response) {
        count--

        if (error)
            console.log(JSON.stringify(error))
        else
            objectsToDelete.push({"_uuid": response.items[0]._uuid})
    }

    function cleanUpJsonDb() {
        for (var i = 0; i < objectsToDelete.length; i++)
            partition.remove(objectsToDelete[i])
    }
}
