/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology Co., Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file UWB2ProDemo.cpp
 * @brief M5Unified initialization and periodic demo updates.
 */

#include "UWB2ProDemo.h"

void UWB2ProDemo::begin()
{
    auto config            = M5.config();
    config.serial_baudrate = 115200;
    config.clear_display   = true;
    config.output_power    = true;
    config.internal_imu    = false;
    config.internal_rtc    = false;
    config.internal_spk    = false;
    config.internal_mic    = false;
    M5.begin(config);
    M5.Display.setBrightness(96);
    canvas.setColorDepth(8);
    canvas.createSprite(M5.Display.width(), M5.Display.height());
    canvas.setTextWrap(false);
    Serial.println("Unit UWB2-Pro Demo");
    render();
}

void UWB2ProDemo::update()
{
    M5.update();
    if (M5.BtnA.wasHold())
        handleAHold();
    else if (M5.BtnA.wasClicked())
        handleA();
    if (M5.BtnB.wasClicked()) handleB();
    if (M5.BtnC.wasClicked()) handleC();

    if (screen == Screen::Run && !showRunConfig && liveData && millis() - lastDataMs >= kDataIntervalMs) {
        lastDataMs = millis();
        readMeasurement();
        render();
    }
    delay(5);
}
