/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology Co., Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file UWB2ProView.cpp
 * @brief Drawing and value formatting for the 320x240 demo interface.
 */

#include "UWB2ProDemo.h"

const char* UWB2ProDemo::appName(UWB2ProApp value)
{
    static const char* const names[] = {"Ranging", "2D Positioning", "3D Positioning"};
    const uint8_t index              = static_cast<uint8_t>(value);
    return index < 3 ? names[index] : "?";
}

const char* UWB2ProDemo::roleName(UWB2ProRole value)
{
    return value == UWB2ProRole::Tag ? "Tag" : "Anchor";
}

const char* UWB2ProDemo::commName(UWB2ProComm value)
{
    return value == UWB2ProComm::I2C ? "I2C" : "UART";
}

const char* UWB2ProDemo::rateName(UWB2ProDataRate value)
{
    return value == UWB2ProDataRate::Rate850K ? "850K" : "6.8M";
}

const char* UWB2ProDemo::ledName(UWB2ProLED value)
{
    static const char* const names[] = {"Off", "On", "Auto"};
    const uint8_t index              = static_cast<uint8_t>(value);
    return index < 3 ? names[index] : "?";
}

const char* UWB2ProDemo::stateName(UWB2ProState value)
{
    static const char* const names[] = {"No Data", "Valid", "Timeout", "Error"};
    const uint8_t index              = static_cast<uint8_t>(value);
    return index < 4 ? names[index] : "?";
}

void UWB2ProDemo::drawHeader(const char* title)
{
    canvas.fillRect(0, 0, 320, 36, kSurface);
    canvas.setTextSize(1);
    canvas.setFont(&fonts::FreeSans9pt7b);
    const int transportWidth = connectionOK ? canvas.textWidth(commName(currentTransport)) : 0;
    const int titleWidth     = connectionOK ? 284 - transportWidth : 296;
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    if (canvas.textWidth(title) > titleWidth) canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(kText, kSurface);
    canvas.drawString(title, 12, 18);
    if (connectionOK) {
        canvas.setFont(&fonts::FreeSans9pt7b);
        canvas.setTextDatum(middle_right);
        canvas.setTextColor(kAccent, kSurface);
        canvas.drawString(commName(currentTransport), 308, 18);
    }
    canvas.fillRect(0, 34, 320, 2, kAccent);
    canvas.setTextDatum(top_left);
}

void UWB2ProDemo::drawFooter(const char* a, const char* b, const char* c)
{
    canvas.fillRect(0, 210, 320, 30, kSurface);
    canvas.drawFastHLine(0, 210, 320, kDivider);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(middle_center);
    const char* labels[] = {a, b, c};
    for (uint8_t index = 0; index < 3; ++index) {
        canvas.setTextColor(index == 2 ? kAccent : kMuted, kSurface);
        canvas.drawString(labels[index], 53 + index * 107, 226);
        if (index < 2) canvas.drawFastVLine(106 + index * 107, 216, 18, kDivider);
    }
    canvas.setTextDatum(top_left);
}

void UWB2ProDemo::drawStatus()
{
    // A paused run is already identified by the footer's Live action. Give
    // complete measurements the full body; read errors still use this line.
    if (screen == Screen::Run && measurementValid && !showRunConfig && !statusError) return;
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(statusError ? kError : kMuted, kBackground);
    canvas.setTextDatum(middle_left);
    canvas.drawString(statusText, 10, 198);
    canvas.setTextDatum(top_left);
}

void UWB2ProDemo::drawRow(int y, const char* label, const String& value, bool selected)
{
    const uint16_t background = selected ? kSurface : kBackground;
    canvas.fillRect(8, y - 2, 304, 23, background);
    if (selected) canvas.fillRect(8, y - 2, 4, 23, kAccent);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(selected ? kText : kMuted, background);
    canvas.drawString(label, 20, y);
    canvas.setTextDatum(top_right);
    canvas.setTextColor(selected ? kAccent : kText, background);
    canvas.drawString(value, 302, y);
    canvas.setTextDatum(top_left);
}

void UWB2ProDemo::drawCentered(const String& text, int y, const lgfx::IFont* font, uint16_t color)
{
    canvas.setFont(font);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(color, kBackground);
    canvas.drawString(text, 160, y);
    canvas.setTextDatum(top_left);
}

String UWB2ProDemo::formatDistance(const UWB2ProDistance& value)
{
    if (value.state != UWB2ProState::Valid) return stateName(value.state);
    char text[16];
    snprintf(text, sizeof(text), "%ld.%02ld m", static_cast<long>(value.centimeters / 100),
             static_cast<long>(value.centimeters % 100));
    return text;
}

uint16_t UWB2ProDemo::distanceColor(const UWB2ProDistance& value)
{
    return value.state == UWB2ProState::Valid ? kAccent : kMuted;
}

void UWB2ProDemo::drawNativeText(const String& text, const lgfx::IFont* font, int x, int y, int width, int height,
                                 uint16_t color)
{
    canvas.setTextSize(1);
    canvas.setFont(font);
    canvas.setClipRect(x, y, width, height);
    canvas.setTextDatum(middle_center);
    // render() already clears the canvas. Transparent glyphs cannot erase the label above.
    canvas.setTextColor(color);
    canvas.drawString(text, x + width / 2, y + height / 2);
    canvas.clearClipRect();
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);
}

void UWB2ProDemo::drawGridText(const String& text, bool emphasized, int x, int y, int width, int height, uint16_t color)
{
    const lgfx::IFont* const valueFonts[] = {&fonts::FreeSansBold18pt7b, &fonts::FreeSansBold12pt7b,
                                             &fonts::FreeSansBold9pt7b};
    const lgfx::IFont* selected           = &fonts::FreeSans9pt7b;
    if (emphasized) {
        // Choose a native bitmap size; fractional scaling discards small glyph strokes.
        canvas.setTextSize(1);
        for (const auto* font : valueFonts) {
            selected = font;
            canvas.setFont(font);
            if (canvas.textWidth(text) <= width - 2 && canvas.fontHeight() <= height - 2) break;
        }
    }
    drawNativeText(text, selected, x, y, width, height, color);
}

String UWB2ProDemo::formatCoordinate(int16_t centimeters)
{
    if (centimeters == INT16_MIN) return "--.--";
    const bool negative    = centimeters < 0;
    const int32_t absolute = negative ? -static_cast<int32_t>(centimeters) : centimeters;
    char text[16];
    snprintf(text, sizeof(text), "%s%ld.%02ld", negative ? "-" : "", static_cast<long>(absolute / 100),
             static_cast<long>(absolute % 100));
    return text;
}

void UWB2ProDemo::drawTransport()
{
    drawHeader("Unit UWB2-Pro");
    drawCentered("Choose connection method", 57, &fonts::FreeSans9pt7b, kMuted);
    const bool uart = selectedTransport == UWB2ProComm::UART;
    canvas.fillRect(18, 82, 134, 76, kSurface);
    canvas.fillRect(168, 82, 134, 76, kSurface);
    canvas.drawRect(18, 82, 134, 76, uart ? kAccent : kDivider);
    canvas.drawRect(168, 82, 134, 76, uart ? kDivider : kAccent);
    if (uart)
        canvas.fillRect(18, 82, 4, 76, kAccent);
    else
        canvas.fillRect(168, 82, 4, 76, kAccent);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(uart ? kAccent : kText, kSurface);
    canvas.drawString("UART", 85, 108);
    canvas.setTextColor(uart ? kText : kAccent, kSurface);
    canvas.drawString("I2C", 235, 108);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(kMuted, kSurface);
    canvas.drawString("115200 8N1", 85, 137);
    char address[24];
    snprintf(address, sizeof(address), "0x%02X  100 kHz", currentAddress);
    canvas.drawString(address, 235, 137);
    canvas.setTextDatum(top_left);
    drawFooter("UART", "I2C", "Connect");
}

void UWB2ProDemo::drawConnection()
{
    drawHeader("Connection Test");
    if (connectionBusy) {
        drawCentered("Connecting...", 92, &fonts::FreeSansBold12pt7b, kAccent);
        drawCentered(commName(selectedTransport), 128, &fonts::FreeSans9pt7b);
        drawFooter("", "", "Wait");
        return;
    }
    drawCentered(connectionOK ? "Connected" : "Connection failed", 62, &fonts::FreeSansBold12pt7b,
                 connectionOK ? kAccent : kError);
    drawRow(90, "Interface", commName(selectedTransport));
    const bool i2c = selectedTransport == UWB2ProComm::I2C;
    if (i2c) {
        char address[8];
        snprintf(address, sizeof(address), "0x%02X", currentAddress);
        drawRow(118, "Address", address);
    }
    if (connectionOK) {
        drawRow(i2c ? 146 : 118, i2c ? "Protocol" : "Board", boardName);
        drawRow(i2c ? 174 : 146, "Firmware Version", firmwareVersion);
    }
    if (connectionOK)
        drawFooter("", "", "Continue");
    else
        drawFooter("Back", "", "Retry");
}

const char* UWB2ProDemo::mainLabel(uint8_t index)
{
    static const char* const labels[] = {"Application", "Communication",    "Group",        "Data Rate",
                                         "LED Mode",    "Firmware Version", "Factory Reset"};
    return labels[index];
}

String UWB2ProDemo::mainValue(uint8_t index)
{
    switch (index) {
        case 0:
            return appName(activeConfig.app);
        case 1:
            return commName(currentTransport);
        case 2:
            return String(activeConfig.group);
        case 3:
            return rateName(activeConfig.dataRate);
        case 4:
            return ledName(activeConfig.led);
        case 5:
            return firmwareVersion;
        default:
            return "";
    }
}

void UWB2ProDemo::drawMain()
{
    drawHeader("Unit UWB2-Pro");
    for (uint8_t index = 0; index < 7; ++index)
        drawRow(42 + index * 21, mainLabel(index), mainValue(index), selection == index);
    drawFooter("Prev", "Next", "Select");
}

void UWB2ProDemo::drawFirmwareVersion()
{
    drawHeader("Firmware Version");
    drawRow(42, currentTransport == UWB2ProComm::I2C ? "Protocol" : "Board", boardName);
    drawRow(70, "Firmware Version", firmwareVersion);
    drawRow(98, "Device MAC", macAddress);
    drawRow(126, "Interface", commName(currentTransport));
    if (currentTransport == UWB2ProComm::I2C) {
        char address[8];
        snprintf(address, sizeof(address), "0x%02X", currentAddress);
        drawRow(154, "Address", address);
    }
    drawFooter("Back", "", "Refresh");
}

void UWB2ProDemo::drawChoiceScreen(const char* title, const char* const* labels, uint8_t count)
{
    drawHeader(title);
    const int start   = count <= 4 ? 56 : 40;
    const int spacing = count <= 4 ? 30 : 22;
    for (uint8_t index = 0; index < count; ++index)
        drawRow(start + index * spacing, labels[index], "", selection == index);
    drawFooter("Back", "Next", "Select");
}

void UWB2ProDemo::drawAppMode()
{
    static const char* const labels[] = {"Ranging", "2D Positioning", "3D Positioning", "Back"};
    drawChoiceScreen("Application", labels, 4);
}

void UWB2ProDemo::drawAppRole()
{
    static const char* const labels[] = {"Tag", "Anchor", "Back"};
    drawChoiceScreen(appName(pendingApp), labels, 3);
}

void UWB2ProDemo::drawRangingValue()
{
    drawHeader(pendingRole == UWB2ProRole::Tag ? "Anchor Count" : "Anchor ID");
    drawCentered(String(pendingValue), 105, &fonts::FreeSansBold24pt7b, kAccent);
    drawCentered("Range 1-9", 148, &fonts::FreeSans9pt7b, kMuted);
    drawFooter("Decrease", "Increase", "Run");
}

void UWB2ProDemo::drawAnchorAxis()
{
    static const char* const labels3[] = {"Origin", "X Axis", "Y Axis", "Back"};
    static const char* const labels4[] = {"Origin", "X Axis", "Y Axis", "Z Axis", "Back"};
    drawChoiceScreen("Anchor Position", pendingApp == UWB2ProApp::Pos2D ? labels3 : labels4,
                     pendingApp == UWB2ProApp::Pos2D ? 4 : 5);
}

void UWB2ProDemo::drawLayout()
{
    drawHeader(pendingApp == UWB2ProApp::Pos2D ? "2D Layout" : "3D Layout");
    const uint8_t axisCount  = pendingApp == UWB2ProApp::Pos2D ? 2 : 3;
    const uint8_t values[]   = {layoutX, layoutY, layoutZ};
    const char* const axes[] = {"X", "Y", "Z"};
    for (uint8_t index = 0; index < axisCount; ++index)
        drawRow(50 + index * 30, axes[index], String(values[index]) + " m", selection == index);
    drawRow(50 + axisCount * 30, "Run", "", selection == axisCount);
    drawRow(80 + axisCount * 30, "Back", "", selection == axisCount + 1);
    drawFooter(layoutEditing ? "Decrease" : "Back", layoutEditing ? "Increase" : "Next",
               layoutEditing ? "Confirm" : "Select");
}

void UWB2ProDemo::drawGroup()
{
    drawHeader("Group");
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    for (uint8_t index = 0; index < 9; ++index) {
        const int x               = 62 + (index % 3) * 98;
        const int y               = 68 + (index / 3) * 42;
        const uint16_t background = selection == index ? kAccentSoft : kSurface;
        canvas.fillRect(x - 34, y - 16, 68, 32, background);
        canvas.setTextDatum(middle_center);
        canvas.setTextColor(selection == index ? kAccent : kText, background);
        canvas.drawString(String(index + 1), x, y);
        if (selection == index) canvas.fillRect(x - 34, y + 14, 68, 2, kAccent);
    }
    canvas.setTextDatum(top_left);
    drawFooter("Back", "Next", "Save");
}

void UWB2ProDemo::drawRate()
{
    static const char* const labels[] = {"850K", "6.8M", "Back"};
    drawChoiceScreen("Data Rate", labels, 3);
}

void UWB2ProDemo::drawLED()
{
    static const char* const labels[] = {"Off", "On", "Auto", "Back"};
    drawChoiceScreen("LED Mode", labels, 4);
}

void UWB2ProDemo::drawComm()
{
    static const char* const labels[] = {"UART", "I2C", "I2C Address", "Back"};
    drawChoiceScreen("Communication", labels, 4);
}

void UWB2ProDemo::drawAddress()
{
    drawHeader("I2C Address");
    char address[8];
    snprintf(address, sizeof(address), "0x%02X", candidateAddress);
    drawCentered(address, 106, &fonts::FreeSansBold24pt7b, kAccent);
    drawCentered("Valid range 0x08-0x77", 151, &fonts::FreeSans9pt7b, kMuted);
    drawFooter("Decrease", "Increase", "Next");
}

void UWB2ProDemo::drawAddressConfirm()
{
    drawHeader("Confirm Address");
    char address[8];
    snprintf(address, sizeof(address), "0x%02X", candidateAddress);
    drawCentered(address, 94, &fonts::FreeSansBold24pt7b, kAccent);
    drawCentered("Apply and reconnect", 142, &fonts::FreeSans9pt7b, kMuted);
    drawFooter("Cancel", "Edit", "Save");
}

void UWB2ProDemo::drawConnectAddress(bool confirm)
{
    drawHeader("I2C Connection");
    char address[8];
    snprintf(address, sizeof(address), "0x%02X", candidateAddress);
    drawCentered("Device Address", 60, &fonts::FreeSans9pt7b, kMuted);
    drawCentered(address, 105, &fonts::FreeSansBold24pt7b, kAccent);
    drawCentered(
        confirm ? (switchToI2C ? "Switch to I2C and reconnect" : "Connect to this address") : "0x08-0x77  |  100 kHz",
        151, &fonts::FreeSans9pt7b, kMuted);
    if (confirm)
        drawFooter("Back", "Edit", switchToI2C ? "Switch" : "Connect");
    else
        drawFooter("Decrease", "Increase", "Next");
}

void UWB2ProDemo::drawRestore()
{
    drawHeader("Factory Reset");
    drawCentered("Restore factory settings?", 88, &fonts::FreeSansBold12pt7b, kAccent);
    drawCentered("UART  |  0x64  |  Group 1", 130, &fonts::FreeSans9pt7b, kMuted);
    drawFooter("Back", "", "Restore");
}

void UWB2ProDemo::drawRunData()
{
    if (!measurementValid) {
        drawCentered("No measurement data", 104, &fonts::FreeSansBold12pt7b, kMuted);
        return;
    }
    if (activeConfig.app == UWB2ProApp::Ranging) {
        if (activeConfig.role == UWB2ProRole::Anchor) {
            const uint8_t id = activeConfig.value - 1;
            drawCentered("Anchor " + String(activeConfig.value), 72, &fonts::FreeSansBold12pt7b, kMuted);
            drawCentered(formatDistance(measurement.anchors[id]), 122, &fonts::FreeSansBold18pt7b,
                         distanceColor(measurement.anchors[id]));
            drawCentered(stateName(measurement.anchors[id].state), 160, &fonts::FreeSans9pt7b, kMuted);
        } else {
            const uint8_t count = activeConfig.value >= 1 && activeConfig.value <= 9 ? activeConfig.value : 1;
            if (count == 1) {
                drawCentered("Anchor 1", 70, &fonts::FreeSans9pt7b, kMuted);
                drawCentered(formatDistance(measurement.anchors[0]), 120, &fonts::FreeSansBold24pt7b,
                             distanceColor(measurement.anchors[0]));
                const uint16_t stateColor = measurement.anchors[0].state == UWB2ProState::Valid ? kAccent : kMuted;
                drawCentered(stateName(measurement.anchors[0].state), 166, &fonts::FreeSans9pt7b, stateColor);
                return;
            }
            const uint8_t columns = count <= 3 ? count : 3;
            const uint8_t rows    = (count + columns - 1) / columns;
            const int cellWidth   = 304 / columns;
            // Equal-height rows use the body down to 204, with six pixels
            // clear of the footer. Labels and values retain native font sizes.
            const int rowHeight   = 162 / rows;
            const int labelHeight = 20;  // Native FreeSans9 height 18, plus two clear pixels.
            const int valueHeight = rows == 3 ? 25 : rows == 2 ? 40 : 60;
            for (uint8_t index = 0; index < count; ++index) {
                const int column   = index % columns;
                const int row      = index / columns;
                const int x        = 8 + column * cellWidth;
                const int cellTop  = 42 + row * rowHeight;
                const int labelTop = cellTop + (rowHeight - labelHeight - 3 - valueHeight) / 2;
                drawGridText("Anchor " + String(index + 1), false, x + 4, labelTop, cellWidth - 8, labelHeight, kMuted);
                drawGridText(formatDistance(measurement.anchors[index]),
                             measurement.anchors[index].state == UWB2ProState::Valid, x + 4, labelTop + labelHeight + 3,
                             cellWidth - 8, valueHeight, distanceColor(measurement.anchors[index]));
                if (row + 1 < rows) canvas.drawFastHLine(x + 6, cellTop + rowHeight - 1, cellWidth - 12, kDivider);
            }
        }
        return;
    }

    if (activeConfig.role == UWB2ProRole::Anchor) {
        const uint8_t id                = activeConfig.value - 1;
        static const char* const axes[] = {"O", "X", "Y", "Z"};
        drawNativeText(String("Anchor ") + axes[id], &fonts::FreeSansBold12pt7b, 8, 58, 304, 28, kMuted);
        drawNativeText(formatDistance(measurement.anchors[id]), &fonts::FreeSansBold18pt7b, 8, 98, 304, 40,
                       distanceColor(measurement.anchors[id]));
        drawNativeText(stateName(measurement.anchors[id].state), &fonts::FreeSans9pt7b, 8, 150, 304, 20, kMuted);
        return;
    }

    const uint8_t count         = activeConfig.app == UWB2ProApp::Pos2D ? 2 : 3;
    const int16_t coordinates[] = {measurement.xCentimeters, measurement.yCentimeters, measurement.zCentimeters};
    const char* const axes[]    = {"X", "Y", "Z"};
    drawNativeText("Coordinates (m)", &fonts::FreeSans9pt7b, 8, 38, 304, 20, kMuted);
    const int coordinateWidth = 304 / count;
    for (uint8_t index = 0; index < count; ++index) {
        const int x = 8 + index * coordinateWidth;
        drawNativeText(axes[index], &fonts::FreeSans9pt7b, x + 2, 59, coordinateWidth - 4, 20, kMuted);
        const bool valid = measurement.positionState == UWB2ProState::Valid && coordinates[index] != INT16_MIN;
        drawGridText(valid ? formatCoordinate(coordinates[index]) : "--.--", true, x + 2, 80, coordinateWidth - 4, 34,
                     valid ? kAccent : kMuted);
    }
    drawNativeText(stateName(measurement.positionState), &fonts::FreeSans9pt7b, 8, 114, 304, 20,
                   measurement.positionState == UWB2ProState::Valid ? kAccent : kMuted);
    canvas.drawFastHLine(12, 137, 296, kDivider);
    drawNativeText("Anchor Distances (m)", &fonts::FreeSans9pt7b, 8, 139, 304, 20, kMuted);
    const uint8_t distanceCount = activeConfig.app == UWB2ProApp::Pos2D ? 3 : 4;
    const int distanceWidth     = 304 / distanceCount;
    for (uint8_t index = 0; index < distanceCount; ++index) {
        const int x = 8 + index * distanceWidth;
        drawNativeText(String("OXYZ").substring(index, index + 1), &fonts::FreeSans9pt7b, x + 2, 160, distanceWidth - 4,
                       20, kMuted);
        String distance = formatDistance(measurement.anchors[index]);
        if (measurement.anchors[index].state == UWB2ProState::Valid)
            distance.remove(distance.length() - 2);  // Metres are identified by the section heading.
        drawNativeText(distance, &fonts::FreeSans9pt7b, x + 2, 182, distanceWidth - 4, 22,
                       distanceColor(measurement.anchors[index]));
    }
    canvas.setTextDatum(top_left);
}

void UWB2ProDemo::drawRunConfig()
{
    drawRow(44, "Application", appName(activeConfig.app));
    drawRow(70, "Role", roleName(activeConfig.role));
    drawRow(96, "Group", String(activeConfig.group));
    drawRow(122, "Data Rate", rateName(activeConfig.dataRate));
    if (activeConfig.app != UWB2ProApp::Ranging && activeConfig.role == UWB2ProRole::Tag) {
        String layout = String(activeConfig.xMeters) + "/" + String(activeConfig.yMeters);
        if (activeConfig.app == UWB2ProApp::Pos3D) layout += "/" + String(activeConfig.zMeters);
        drawRow(148, "Layout", layout + " m");
    } else {
        drawRow(148, activeConfig.role == UWB2ProRole::Tag ? "Anchor Count" : "Anchor ID", String(activeConfig.value));
    }
}

void UWB2ProDemo::drawRun()
{
    String title = String(appName(activeConfig.app)) + " " + roleName(activeConfig.role);
    drawHeader(title.c_str());
    if (showRunConfig)
        drawRunConfig();
    else
        drawRunData();
    drawFooter("Setup", showRunConfig ? "Data" : "Details", liveData ? "Pause" : "Live");
}

void UWB2ProDemo::render()
{
    canvas.fillSprite(kBackground);
    switch (screen) {
        case Screen::Transport:
            drawTransport();
            break;
        case Screen::Connection:
            drawConnection();
            break;
        case Screen::Main:
            drawMain();
            break;
        case Screen::FirmwareVersion:
            drawFirmwareVersion();
            break;
        case Screen::AppMode:
            drawAppMode();
            break;
        case Screen::AppRole:
            drawAppRole();
            break;
        case Screen::RangingValue:
            drawRangingValue();
            break;
        case Screen::AnchorAxis:
            drawAnchorAxis();
            break;
        case Screen::Layout:
            drawLayout();
            break;
        case Screen::Group:
            drawGroup();
            break;
        case Screen::Rate:
            drawRate();
            break;
        case Screen::LED:
            drawLED();
            break;
        case Screen::Comm:
            drawComm();
            break;
        case Screen::Address:
            drawAddress();
            break;
        case Screen::AddressConfirm:
            drawAddressConfirm();
            break;
        case Screen::RestoreConfirm:
            drawRestore();
            break;
        case Screen::Run:
            drawRun();
            break;
        case Screen::ConnectAddress:
            drawConnectAddress(false);
            break;
        case Screen::ConnectAddressConfirm:
            drawConnectAddress(true);
            break;
    }
    drawStatus();
    canvas.pushSprite(0, 0);
}
