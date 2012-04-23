/**
* @file controller.h
* @author Sean Pappalardo spappalardo@mixxx.org
* @date Sat Apr 30 2011
* @brief Base class representing a physical (or software) controller.
*/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include <qapplication.h>   // For command line arguments
#include "controller.h"
#include "defs_controllers.h"

Controller::Controller() : QObject() {
    m_bIsOutputDevice = false;
    m_bIsInputDevice = false;
    m_bIsOpen = false;
    m_pEngine = NULL;
    m_bPolling = false;
    m_bLearning = false;

    // Get --controllerDebug command line option
    QStringList commandLineArgs = QApplication::arguments();
    m_bDebug = commandLineArgs.contains("--controllerDebug", Qt::CaseInsensitive);
}

Controller::~Controller() {
    // We can't close the device here.
    //close();
}

QString Controller::defaultPreset() {
    return USER_PRESETS_PATH.append("controllers/")
            .append(m_sDeviceName.replace(" ", "_") + presetExtension());
}

QString Controller::presetExtension() {
    return CONTROLLER_PRESET_EXTENSION;
}

void Controller::startEngine()
{
    if (debugging()) {
        qDebug() << "  Starting engine";
    }
    if (m_pEngine != NULL) {
        qWarning() << "Controller: Engine already exists! Restarting:";
        stopEngine();
    }
    m_pEngine = new ControllerEngine(this);
}

void Controller::stopEngine()
{
    if (debugging()) {
        qDebug() << "  Shutting down engine";
    }
    if (m_pEngine == NULL) {
        qWarning() << "Controller::stopEngine(): No engine exists!";
        return;
    }
    m_pEngine->gracefulShutdown();
    delete m_pEngine;
    m_pEngine = NULL;
}

void Controller::applyPreset() {
    qDebug() << "Applying controller preset...";

    const ControllerPreset* preset = getPreset();

    // Load the script code into the engine
    if (m_pEngine != NULL) {
        const QStringList scriptFunctions = m_pEngine->getScriptFunctions();
        if (scriptFunctions.isEmpty() && preset->scriptFileNames.isEmpty()) {
            qWarning() << "No script functions available! Did the XML file(s) load successfully? See above for any errors.";
        } else {
            if (scriptFunctions.isEmpty()) {
                m_pEngine->loadScriptFiles(m_pConfig->getConfigPath(),
                                           preset->scriptFileNames);
            }
            m_pEngine->initializeScripts(preset->scriptFunctionPrefixes);
        }
    } else {
        qWarning() << "Controller::applyPreset(): No engine exists!";
    }
}

void Controller::learn(MixxxControl control) {
    qDebug() << m_sDeviceName << ": Learning" << control.group() << "," << control.item();
    m_controlToLearn = control;
    m_bLearning = true;
}

void Controller::cancelLearn() {
    m_controlToLearn = MixxxControl();
    m_bLearning = false;
    //qDebug() << m_sDeviceName << ": Aborted learning.";
}

void Controller::send(QList<int> data, unsigned int length) {
    // If you change this implementation, also change it in HidController (That
    // function is required due to HID devices having report IDs)

    QByteArray msg;

    for (unsigned int i=0; i<length; i++) {
        msg[i] = data.at(i);
    }
    send(msg);
}

void Controller::send(QByteArray data) {
    Q_UNUSED(data);
    qWarning() << "Error: data sending not yet implemented for this API or platform!";
}

void Controller::receive(const QByteArray data) {
    if (m_pEngine == NULL) {
        //qWarning() << "Controller::receive called with no active engine!";
        // Don't complain, since this will always show after closing a device as
        //  queued signals flush out
        return;
    }

    int length = data.size();
    if (debugging()) {
        // Formatted packet display
        QString message = QString("%1: %2 bytes:\n").arg(m_sDeviceName).arg(length);
        for(int i=0; i<length; i++) {
            QString spacer=" ";
            if ((i+1) % 4 == 0) spacer="  ";
            if ((i+1) % 16 == 0) spacer="\n";
            message += QString("%1%2")
                        .arg((unsigned char)(data.at(i)), 2, 16, QChar('0')).toUpper()
                        .arg(spacer);
        }
        qDebug() << message;
    }

    QListIterator<QString> prefixIt(m_pEngine->getScriptFunctionPrefixes());
    while (prefixIt.hasNext()) {
        QString function = prefixIt.next();
        if (function!="") {
            function.append(".incomingData");

            if (!m_pEngine->execute(function, data)) {
                qWarning() << "Controller: Invalid script function" << function;
            }
        }
    }
    return;
}

void Controller::timerEvent(QTimerEvent *event, bool poll) {
    if (poll) return; // Sub-classes use the poll flag

    // Pass it on to the engine
    m_pEngine->timerEvent(event);
}
