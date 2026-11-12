/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology Co., Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file UWB2ProDevice.cpp
 * @brief Unit connection, configuration, and measurement handling.
 */

#include "UWB2ProDemo.h"

void UWB2ProDemo::setStatus(const char* text, bool error)
{
    snprintf(statusText, sizeof(statusText), "%s", text);
    statusError = error;
    Serial.println(statusText);
}

void UWB2ProDemo::clearStatus()
{
    statusText[0] = '\0';
    statusError   = false;
}

void UWB2ProDemo::setUnitError(const char* operation)
{
    char text[64];
    snprintf(text, sizeof(text), "%s: %.30s", operation, unit.lastError());
    setStatus(text, true);
}

bool UWB2ProDemo::beginUART()
{
    unit.end();
    delay(40);
    if (!unit.beginUART(Serial2, kHostRx, kHostTx, 1000) || !unit.probe()) return false;
    currentTransport = UWB2ProComm::UART;
    return true;
}

bool UWB2ProDemo::beginI2C(uint8_t address)
{
    unit.end();
    delay(40);
    if (!unit.beginI2C(Wire, kSDA, kSCL, address, kI2CFrequency, 1000) || !unit.probe()) return false;
    currentTransport = UWB2ProComm::I2C;
    currentAddress   = address;
    return true;
}

bool UWB2ProDemo::queryDevice()
{
    UWB2ProConfig config;
    char board[sizeof(boardName)];
    char version[sizeof(firmwareVersion)];
    if (!unit.getBoard(board, sizeof(board)) || !unit.getVersion(version, sizeof(version)) || !unit.getConfig(config)) {
        setUnitError("Query");
        return false;
    }
    snprintf(boardName, sizeof(boardName), "%s", board);
    snprintf(firmwareVersion, sizeof(firmwareVersion), "%s", version);
    activeConfig      = config;
    activeConfig.comm = currentTransport;
    if (currentTransport == UWB2ProComm::I2C) {
        activeConfig.i2cAddress = currentAddress;
        snprintf(macAddress, sizeof(macAddress), "--");
    } else if (!unit.getMAC(macAddress, sizeof(macAddress))) {
        snprintf(macAddress, sizeof(macAddress), "--");
    }
    return true;
}

void UWB2ProDemo::connectSelected()
{
    screen         = Screen::Connection;
    connectionBusy = true;
    connectionOK   = false;
    setStatus("Connecting");
    render();

    bool started = false;
    if (selectedTransport == UWB2ProComm::UART) {
        started = beginUART();
    } else {
        started = beginI2C(currentAddress);
    }
    connectionBusy = false;
    connectionOK   = started && queryDevice();
    if (connectionOK) {
        clearStatus();
    } else if (started)
        setUnitError("Firmware version");
    else
        setUnitError("Connect");
    render();
}

bool UWB2ProDemo::refreshConfig()
{
    UWB2ProConfig config;
    if (!unit.getConfig(config)) {
        setUnitError("Read config");
        return false;
    }
    config.comm = currentTransport;
    if (currentTransport == UWB2ProComm::I2C) config.i2cAddress = currentAddress;
    activeConfig = config;
    return true;
}

bool UWB2ProDemo::saveCurrentDraft(const char* operation)
{
    if (!unit.save()) {
        setUnitError(operation);
        unit.cancel();
        return false;
    }
    delay(250);
    return refreshConfig();
}

void UWB2ProDemo::enterRun()
{
    // Continue follows the actual runtime state and never starts or saves an app.
    if (!refreshConfig()) {
        render();
        return;
    }
    bool running = false;
    if (!unit.getAppRunning(running)) {
        setUnitError("Read application state");
        render();
        return;
    }
    screen           = running ? Screen::Run : Screen::Main;
    selection        = 0;
    liveData         = true;
    showRunConfig    = false;
    measurementValid = false;
    lastDataMs       = 0;
    clearStatus();
}

void UWB2ProDemo::applyApp()
{
    bool staged = false;
    if (pendingApp != UWB2ProApp::Ranging && pendingRole == UWB2ProRole::Tag)
        staged = unit.setLayout(pendingApp, layoutX, layoutY, layoutZ);
    else
        staged = unit.setApp(pendingApp, pendingRole, pendingValue);
    if (!staged || !unit.setBootApp(true) || !saveCurrentDraft("Save application")) {
        setUnitError(staged ? "Start application" : "Set application");
        unit.cancel();
        render();
        return;
    }
    setStatus("Application saved");
    enterRun();
    render();
}

void UWB2ProDemo::applyGroup(uint8_t group)
{
    if (!unit.setGroup(group) || !saveCurrentDraft("Save group")) {
        setUnitError("Set group");
        render();
        return;
    }
    screen    = Screen::Main;
    selection = 2;
    setStatus("Group saved");
    render();
}

void UWB2ProDemo::applyRate(UWB2ProDataRate rate)
{
    if (!unit.setDataRate(rate) || !saveCurrentDraft("Save data rate")) {
        setUnitError("Set data rate");
        render();
        return;
    }
    screen    = Screen::Main;
    selection = 3;
    setStatus("Data rate saved");
    render();
}

void UWB2ProDemo::applyLED(UWB2ProLED led)
{
    if (!unit.setLED(led) || !saveCurrentDraft("Save LED")) {
        setUnitError("Set LED");
        render();
        return;
    }
    screen    = Screen::Main;
    selection = 4;
    setStatus("LED mode saved");
    render();
}

bool UWB2ProDemo::reconnectTransport(UWB2ProComm target)
{
    delay(400);
    if (target == UWB2ProComm::UART) return beginUART();
    return beginI2C(currentAddress);
}

void UWB2ProDemo::applyComm(UWB2ProComm target)
{
    if (target == currentTransport) {
        screen    = Screen::Main;
        selection = 1;
        setStatus("Communication method already active");
        render();
        return;
    }
    if (!unit.setComm(target) || !unit.save()) {
        setUnitError("Save communication");
        unit.cancel();
        render();
        return;
    }
    // SAVE is acknowledged on the old interface. Probe the explicit new endpoint.
    selectedTransport = target;
    if (!reconnectTransport(target) || !queryDevice()) {
        connectionOK = false;
        screen       = Screen::Connection;
        setUnitError("Reconnect");
        render();
        return;
    }
    screen    = Screen::Main;
    selection = 1;
    setStatus("Communication method switched");
    render();
}

void UWB2ProDemo::applyAddress()
{
    if (currentTransport != UWB2ProComm::I2C) {
        screen = Screen::Comm;
        setStatus("Select I2C first", true);
        render();
        return;
    }
    // The Unit keeps its old address until SAVE commits the address draft.
    if (!unit.setI2CAddress(candidateAddress) || !unit.save()) {
        setUnitError("Save address");
        unit.cancel();
        render();
        return;
    }
    delay(300);
    if (!beginI2C(candidateAddress) || !queryDevice()) {
        connectionOK = false;
        screen       = Screen::Connection;
        setUnitError("Address reconnect");
        render();
        return;
    }
    screen    = Screen::Comm;
    selection = 2;
    setStatus("I2C address saved");
    render();
}

void UWB2ProDemo::restoreDefaults()
{
    if (!unit.restore()) {
        setUnitError("Restore");
        render();
        return;
    }
    delay(400);
    currentAddress = M5_UNIT_UWB2_PRO_DEFAULT_I2C_ADDRESS;
    if (!beginUART() || !queryDevice()) {
        connectionOK = false;
        screen       = Screen::Connection;
        setUnitError("Restore reconnect");
        render();
        return;
    }
    screen    = Screen::Main;
    selection = 6;
    setStatus("Defaults restored");
    render();
}

void UWB2ProDemo::readMeasurement()
{
    UWB2ProData next;
    if (!unit.readData(activeConfig, next)) {
        measurementValid = false;
        setUnitError("Read data");
        return;
    }
    measurement      = next;
    measurementValid = true;
    clearStatus();
}
