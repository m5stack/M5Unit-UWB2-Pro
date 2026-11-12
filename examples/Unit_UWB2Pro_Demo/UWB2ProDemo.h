/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology Co., Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file UWB2ProDemo.h
 * @brief Demo state and interfaces shared by device, view, and controls.
 */

#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <M5UnitUWB2Pro.h>
#include <Wire.h>

/** Owns the demo connection, canvas, and menu state; call update() from loop(). */
class UWB2ProDemo {
public:
    /** Initialize M5Unified, the canvas, and the transport selection screen. */
    void begin();
    /** Process buttons and refresh measurements for the active application. */
    void update();

private:
    static constexpr int kHostRx              = 22;
    static constexpr int kHostTx              = 21;
    static constexpr int kSDA                 = 21;
    static constexpr int kSCL                 = 22;
    static constexpr uint32_t kI2CFrequency   = 100000;
    static constexpr uint32_t kDataIntervalMs = 1000;
    static constexpr uint16_t kBackground     = 0xF7BE;
    static constexpr uint16_t kSurface        = TFT_WHITE;
    static constexpr uint16_t kText           = 0x2124;
    static constexpr uint16_t kMuted          = 0x7BEF;
    static constexpr uint16_t kDivider        = 0xDEFB;
    static constexpr uint16_t kAccent         = 0x2C1D;
    static constexpr uint16_t kAccentSoft     = 0xDF5F;
    static constexpr uint16_t kError          = 0xD987;

    enum class Screen : uint8_t {
        Transport,
        Connection,
        Main,
        FirmwareVersion,
        AppMode,
        AppRole,
        RangingValue,
        AnchorAxis,
        Layout,
        Group,
        Rate,
        LED,
        Comm,
        Address,
        AddressConfirm,
        RestoreConfirm,
        Run,
        ConnectAddress,
        ConnectAddressConfirm,
    };

    M5UnitUWB2Pro unit;
    M5Canvas canvas{&M5.Display};
    // Snapshot used to validate measurements against the Unit's applied configuration.
    UWB2ProConfig activeConfig;
    UWB2ProData measurement;

    Screen screen                 = Screen::Transport;
    UWB2ProComm selectedTransport = UWB2ProComm::UART;
    UWB2ProComm currentTransport  = UWB2ProComm::UART;
    UWB2ProApp pendingApp         = UWB2ProApp::Ranging;
    UWB2ProRole pendingRole       = UWB2ProRole::Tag;
    uint8_t currentAddress        = M5_UNIT_UWB2_PRO_DEFAULT_I2C_ADDRESS;
    uint8_t candidateAddress      = M5_UNIT_UWB2_PRO_DEFAULT_I2C_ADDRESS;
    uint8_t selection             = 0;
    uint8_t pendingValue          = 1;
    uint8_t layoutX               = 3;
    uint8_t layoutY               = 3;
    uint8_t layoutZ               = 1;
    bool layoutEditing            = false;
    bool connectionBusy           = false;
    bool connectionOK             = false;
    bool measurementValid         = false;
    bool liveData                 = true;
    bool showRunConfig            = false;
    bool switchToI2C              = false;
    uint32_t lastDataMs           = 0;
    char boardName[16]            = "--";
    char firmwareVersion[16]      = "--";
    char macAddress[18]           = "--";
    char statusText[64]           = "Choose UART or I2C";
    bool statusError              = false;

    // Device connection and configuration.
    void setStatus(const char* text, bool error = false);
    void clearStatus();
    void setUnitError(const char* operation);
    bool beginUART();
    bool beginI2C(uint8_t address);
    bool queryDevice();
    void connectSelected();
    bool refreshConfig();
    bool saveCurrentDraft(const char* operation);
    void enterRun();
    void applyApp();
    void applyGroup(uint8_t group);
    void applyRate(UWB2ProDataRate rate);
    void applyLED(UWB2ProLED led);
    bool reconnectTransport(UWB2ProComm target);
    void applyComm(UWB2ProComm target);
    void applyAddress();
    void restoreDefaults();
    void readMeasurement();

    // Drawing and value formatting.
    static const char* appName(UWB2ProApp value);
    static const char* roleName(UWB2ProRole value);
    static const char* commName(UWB2ProComm value);
    static const char* rateName(UWB2ProDataRate value);
    static const char* ledName(UWB2ProLED value);
    static const char* stateName(UWB2ProState value);
    void drawHeader(const char* title);
    void drawFooter(const char* a, const char* b, const char* c);
    void drawStatus();
    void drawRow(int y, const char* label, const String& value, bool selected = false);
    void drawCentered(const String& text, int y, const lgfx::IFont* font, uint16_t color = kText);
    static String formatDistance(const UWB2ProDistance& value);
    static uint16_t distanceColor(const UWB2ProDistance& value);
    void drawNativeText(const String& text, const lgfx::IFont* font, int x, int y, int width, int height,
                        uint16_t color);
    void drawGridText(const String& text, bool emphasized, int x, int y, int width, int height, uint16_t color);
    static String formatCoordinate(int16_t centimeters);
    void drawTransport();
    void drawConnection();
    static const char* mainLabel(uint8_t index);
    String mainValue(uint8_t index);
    void drawMain();
    void drawFirmwareVersion();
    void drawChoiceScreen(const char* title, const char* const* labels, uint8_t count);
    void drawAppMode();
    void drawAppRole();
    void drawRangingValue();
    void drawAnchorAxis();
    void drawLayout();
    void drawGroup();
    void drawRate();
    void drawLED();
    void drawComm();
    void drawAddress();
    void drawAddressConfirm();
    void drawConnectAddress(bool confirm);
    void drawRestore();
    void drawRunData();
    void drawRunConfig();
    void drawRun();
    void render();

    // Button navigation.
    void openMainItem();
    void handleA();
    void handleAHold();
    void handleB();
    void handleC();
};
