/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology Co., Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file Unit_UWB2Pro_Demo.ino
 * @brief Entry point for the Unit UWB2-Pro demonstration on M5Stack Basic.
 */

#include "UWB2ProDemo.h"

UWB2ProDemo demo;

void setup()
{
    demo.begin();
}

void loop()
{
    demo.update();
}
