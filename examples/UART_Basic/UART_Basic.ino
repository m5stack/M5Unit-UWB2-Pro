/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology Co., Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file UART_Basic.ino
 * @brief Configure a Ranging Tag or Anchor 1 and print distances over UART.
 */

#include <Arduino.h>
#include <M5UnitUWB2Pro.h>

// Set to 1 for a Tag with one Anchor, or 0 for Anchor ID 1.
// Use opposite roles on two Units with the same group and data rate.
#ifndef UWB2PRO_ROLE_TAG
#define UWB2PRO_ROLE_TAG 1
#endif

static_assert(UWB2PRO_ROLE_TAG == 0 || UWB2PRO_ROLE_TAG == 1, "UWB2PRO_ROLE_TAG must be 0 (Anchor 1) or 1 (Tag)");

constexpr UWB2ProRole kRole         = UWB2PRO_ROLE_TAG ? UWB2ProRole::Tag : UWB2ProRole::Anchor;
constexpr uint8_t kGroup            = 1;
constexpr UWB2ProDataRate kDataRate = UWB2ProDataRate::Rate850K;

M5UnitUWB2Pro unit;
UWB2ProConfig activeConfig;
bool ready = false;

static const char* stateName(UWB2ProState state)
{
    switch (state) {
        case UWB2ProState::NoData:
            return "NoData";
        case UWB2ProState::Valid:
            return "Valid";
        case UWB2ProState::Timeout:
            return "Timeout";
        case UWB2ProState::Error:
            return "Error";
    }
    return "Unknown";
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("Unit UWB2-Pro UART Ranging example");

    // M5Stack Basic: host RX=22 <- Unit TX=12, host TX=21 -> Unit RX=13.
    if (!unit.beginUART(Serial2, 22, 21) || !unit.probe()) {
        Serial.printf("Probe failed: %s\n", unit.lastError());
        return;
    }

    char board[16], version[16];
    if (!unit.getBoard(board, sizeof(board)) || !unit.getVersion(version, sizeof(version))) {
        Serial.printf("Identity read failed: %s\n", unit.lastError());
        return;
    }
    Serial.printf("%s firmware %s\n", board, version);

    // Discard any old draft. TAG value=1 means one Anchor; ANCHOR value=1 means ID 1.
    // SAVE persists the radio settings and starts the app on this communication interface.
    if (!unit.cancel() || !unit.setApp(UWB2ProApp::Ranging, kRole, 1) || !unit.setGroup(kGroup) ||
        !unit.setDataRate(kDataRate) || !unit.setBootApp(true) || !unit.save()) {
        Serial.printf("Configuration failed: %s\n", unit.lastError());
        unit.cancel();
        return;
    }
    delay(500);

    // Read back the saved settings as the applied snapshot required by readData().
    if (!unit.getConfig(activeConfig)) {
        Serial.printf("Config read failed: %s\n", unit.lastError());
        return;
    }
    if (activeConfig.app != UWB2ProApp::Ranging || activeConfig.role != kRole || activeConfig.value != 1 ||
        activeConfig.group != kGroup || activeConfig.dataRate != kDataRate || !activeConfig.bootApp) {
        Serial.println("Configuration readback mismatch");
        return;
    }
    Serial.printf("RANGING %s | %s | Group %u | %s\n", kRole == UWB2ProRole::Tag ? "TAG" : "ANCHOR",
                  kRole == UWB2ProRole::Tag ? "Anchor Count 1" : "Anchor ID 1", activeConfig.group,
                  activeConfig.dataRate == UWB2ProDataRate::Rate850K ? "850K" : "6.8M");
    ready = true;
}

void loop()
{
    delay(1000);
    if (!ready) return;

    UWB2ProData data;
    if (!unit.readData(activeConfig, data)) {
        Serial.printf("Data read failed: %s\n", unit.lastError());
        return;
    }

    // Both roles use anchors[0] for Anchor ID 1. Never print missing/stale numbers.
    const UWB2ProDistance& distance = data.anchors[0];
    if (distance.state == UWB2ProState::Valid) {
        Serial.printf("Anchor 1: distance=%ld cm, state=Valid\n", static_cast<long>(distance.centimeters));
    } else {
        Serial.printf("Anchor 1: distance=--, state=%s\n", stateName(distance.state));
    }
}
