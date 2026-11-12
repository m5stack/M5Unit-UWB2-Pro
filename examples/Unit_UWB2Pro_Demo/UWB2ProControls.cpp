/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology Co., Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file UWB2ProControls.cpp
 * @brief Three-button navigation and configuration flow.
 */

#include "UWB2ProDemo.h"

void UWB2ProDemo::openMainItem()
{
    switch (selection) {
        case 0:
            screen    = Screen::AppMode;
            selection = static_cast<uint8_t>(activeConfig.app);
            break;
        case 1:
            screen    = Screen::Comm;
            selection = currentTransport == UWB2ProComm::UART ? 0 : 1;
            break;
        case 2:
            screen    = Screen::Group;
            selection = activeConfig.group - 1;
            break;
        case 3:
            screen    = Screen::Rate;
            selection = static_cast<uint8_t>(activeConfig.dataRate);
            break;
        case 4:
            screen    = Screen::LED;
            selection = static_cast<uint8_t>(activeConfig.led);
            break;
        case 5:
            screen = Screen::FirmwareVersion;
            break;
        case 6:
            screen = Screen::RestoreConfirm;
            break;
    }
    clearStatus();
}

void UWB2ProDemo::handleA()
{
    switch (screen) {
        case Screen::Transport:
            selectedTransport = UWB2ProComm::UART;
            break;
        case Screen::Connection:
            if (!connectionOK) {
                screen = Screen::Transport;
                unit.end();
            }
            break;
        case Screen::Main:
            selection = selection == 0 ? 6 : selection - 1;
            break;
        case Screen::FirmwareVersion:
            screen    = Screen::Main;
            selection = 5;
            break;
        case Screen::Group:
            screen    = Screen::Main;
            selection = 2;
            break;
        case Screen::Rate:
            screen    = Screen::Main;
            selection = 3;
            break;
        case Screen::LED:
            screen    = Screen::Main;
            selection = 4;
            break;
        case Screen::Comm:
            screen    = Screen::Main;
            selection = 1;
            break;
        case Screen::RestoreConfirm:
            screen    = Screen::Main;
            selection = 6;
            break;
        case Screen::AppMode:
            screen    = Screen::Main;
            selection = 0;
            break;
        case Screen::AppRole:
            screen    = Screen::AppMode;
            selection = static_cast<uint8_t>(pendingApp);
            break;
        case Screen::RangingValue:
            pendingValue = pendingValue <= 1 ? 9 : pendingValue - 1;
            break;
        case Screen::AnchorAxis:
            screen    = Screen::AppRole;
            selection = static_cast<uint8_t>(pendingRole);
            break;
        case Screen::Layout:
            if (layoutEditing) {
                uint8_t* value = selection == 0 ? &layoutX : selection == 1 ? &layoutY : &layoutZ;
                *value         = *value <= 1 ? 9 : *value - 1;
            } else {
                screen    = Screen::AppRole;
                selection = static_cast<uint8_t>(pendingRole);
            }
            break;
        case Screen::Address:
        case Screen::ConnectAddress:
            candidateAddress = candidateAddress <= 0x08 ? 0x77 : candidateAddress - 1;
            break;
        case Screen::AddressConfirm:
            screen = Screen::Address;
            break;
        case Screen::ConnectAddressConfirm:
            screen = switchToI2C ? Screen::Comm : Screen::Transport;
            break;
        case Screen::Run:
            screen    = Screen::Main;
            selection = 0;
            break;
    }
    render();
}

void UWB2ProDemo::handleAHold()
{
    if (screen == Screen::RangingValue) {
        screen    = Screen::AppRole;
        selection = static_cast<uint8_t>(pendingRole);
        setStatus("Application unchanged");
        render();
    } else {
        handleA();
    }
}

void UWB2ProDemo::handleB()
{
    switch (screen) {
        case Screen::Transport:
            selectedTransport = UWB2ProComm::I2C;
            break;
        case Screen::Main:
            selection = (selection + 1) % 7;
            break;
        case Screen::AppMode:
            selection = (selection + 1) % 4;
            break;
        case Screen::AppRole:
            selection = (selection + 1) % 3;
            break;
        case Screen::RangingValue:
            pendingValue = pendingValue >= 9 ? 1 : pendingValue + 1;
            break;
        case Screen::AnchorAxis:
            selection = (selection + 1) % (pendingApp == UWB2ProApp::Pos2D ? 4 : 5);
            break;
        case Screen::Layout:
            if (layoutEditing) {
                uint8_t* value = selection == 0 ? &layoutX : selection == 1 ? &layoutY : &layoutZ;
                *value         = *value >= 9 ? 1 : *value + 1;
            } else {
                const uint8_t count = pendingApp == UWB2ProApp::Pos2D ? 4 : 5;
                selection           = (selection + 1) % count;
            }
            break;
        case Screen::Group:
            selection = (selection + 1) % 9;
            break;
        case Screen::Rate:
            selection = (selection + 1) % 3;
            break;
        case Screen::LED:
        case Screen::Comm:
            selection = (selection + 1) % 4;
            break;
        case Screen::Address:
        case Screen::ConnectAddress:
            candidateAddress = candidateAddress >= 0x77 ? 0x08 : candidateAddress + 1;
            break;
        case Screen::AddressConfirm:
            screen = Screen::Address;
            break;
        case Screen::ConnectAddressConfirm:
            screen = Screen::ConnectAddress;
            break;
        case Screen::Run:
            showRunConfig = !showRunConfig;
            break;
        default:
            break;
    }
    render();
}

void UWB2ProDemo::handleC()
{
    switch (screen) {
        case Screen::Transport:
            if (selectedTransport == UWB2ProComm::I2C) {
                switchToI2C      = false;
                candidateAddress = currentAddress;
                clearStatus();
                screen = Screen::ConnectAddress;
                render();
            } else {
                connectSelected();
            }
            return;
        case Screen::Connection:
            if (connectionOK) {
                enterRun();
                render();
            } else {
                connectSelected();
            }
            return;
        case Screen::Main:
            openMainItem();
            break;
        case Screen::FirmwareVersion:
            if (queryDevice()) {
                clearStatus();
            }
            break;
        case Screen::AppMode:
            if (selection == 3) {
                screen    = Screen::Main;
                selection = 0;
            } else {
                pendingApp = static_cast<UWB2ProApp>(selection);
                screen     = Screen::AppRole;
                selection  = 0;
            }
            break;
        case Screen::AppRole:
            if (selection == 2) {
                screen    = Screen::AppMode;
                selection = static_cast<uint8_t>(pendingApp);
                break;
            }
            pendingRole = static_cast<UWB2ProRole>(selection);
            if (pendingApp == UWB2ProApp::Ranging) {
                pendingValue =
                    activeConfig.app == pendingApp && activeConfig.role == pendingRole ? activeConfig.value : 1;
                screen = Screen::RangingValue;
            } else if (pendingRole == UWB2ProRole::Anchor) {
                selection = 0;
                screen    = Screen::AnchorAxis;
            } else {
                layoutX       = activeConfig.xMeters >= 1 ? activeConfig.xMeters : 3;
                layoutY       = activeConfig.yMeters >= 1 ? activeConfig.yMeters : 3;
                layoutZ       = activeConfig.zMeters >= 1 ? activeConfig.zMeters : 1;
                selection     = 0;
                layoutEditing = false;
                screen        = Screen::Layout;
            }
            break;
        case Screen::RangingValue:
            applyApp();
            return;
        case Screen::AnchorAxis: {
            const uint8_t axisCount = pendingApp == UWB2ProApp::Pos2D ? 3 : 4;
            if (selection >= axisCount) {
                screen    = Screen::AppRole;
                selection = static_cast<uint8_t>(pendingRole);
            } else {
                pendingValue = selection + 1;
                applyApp();
                return;
            }
            break;
        }
        case Screen::Layout: {
            const uint8_t axisCount = pendingApp == UWB2ProApp::Pos2D ? 2 : 3;
            if (layoutEditing) {
                layoutEditing = false;
            } else if (selection < axisCount) {
                layoutEditing = true;
            } else if (selection == axisCount) {
                applyApp();
                return;
            } else {
                screen    = Screen::AppRole;
                selection = static_cast<uint8_t>(pendingRole);
            }
            break;
        }
        case Screen::Group:
            applyGroup(selection + 1);
            return;
        case Screen::Rate:
            if (selection == 2) {
                screen    = Screen::Main;
                selection = 3;
            } else {
                applyRate(static_cast<UWB2ProDataRate>(selection));
                return;
            }
            break;
        case Screen::LED:
            if (selection == 3) {
                screen    = Screen::Main;
                selection = 4;
            } else {
                applyLED(static_cast<UWB2ProLED>(selection));
                return;
            }
            break;
        case Screen::Comm:
            if (selection == 0 || selection == 1) {
                if (selection == 1 && currentTransport == UWB2ProComm::UART) {
                    switchToI2C      = true;
                    candidateAddress = currentAddress;
                    clearStatus();
                    screen = Screen::ConnectAddress;
                    render();
                } else {
                    applyComm(selection == 0 ? UWB2ProComm::UART : UWB2ProComm::I2C);
                }
                return;
            }
            if (selection == 2) {
                if (currentTransport != UWB2ProComm::I2C) {
                    setStatus("Select I2C first", true);
                } else {
                    candidateAddress = currentAddress;
                    screen           = Screen::Address;
                }
            } else {
                screen    = Screen::Main;
                selection = 1;
            }
            break;
        case Screen::Address:
            screen = Screen::AddressConfirm;
            break;
        case Screen::AddressConfirm:
            applyAddress();
            return;
        case Screen::ConnectAddress:
            screen = Screen::ConnectAddressConfirm;
            break;
        case Screen::ConnectAddressConfirm:
            // This selects the host target; the Unit's saved address is unchanged.
            currentAddress = candidateAddress;
            if (switchToI2C)
                applyComm(UWB2ProComm::I2C);
            else
                connectSelected();
            return;
        case Screen::RestoreConfirm:
            restoreDefaults();
            return;
        case Screen::Run:
            liveData = !liveData;
            if (liveData)
                clearStatus();
            else
                setStatus("Paused");
            break;
    }
    render();
}
