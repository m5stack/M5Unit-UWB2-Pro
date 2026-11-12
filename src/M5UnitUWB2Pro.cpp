/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology Co., Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file M5UnitUWB2Pro.cpp
 * @brief Factory v0.1 protocol parsing, configuration, and measurement access.
 */

#include "M5UnitUWB2Pro.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

const char* const kApps[]  = {"RANGING", "POS2D", "POS3D"};
const char* const kComms[] = {"I2C", "UART", "USB"};

uint8_t raw(UWB2ProApp value)
{
    return static_cast<uint8_t>(value);
}
uint8_t raw(UWB2ProRole value)
{
    return static_cast<uint8_t>(value);
}
uint8_t raw(UWB2ProComm value)
{
    return static_cast<uint8_t>(value);
}
uint8_t raw(UWB2ProLED value)
{
    return static_cast<uint8_t>(value);
}
uint8_t raw(UWB2ProDataRate value)
{
    return static_cast<uint8_t>(value);
}

bool parseDecimal(const char* text, int32_t minimum, int32_t maximum, int32_t& value)
{
    if (!text || !*text) return false;
    const char* p = text;
    if (*p == '-') ++p;
    if (!*p) return false;
    for (; *p; ++p)
        if (*p < '0' || *p > '9') return false;
    char* end         = nullptr;
    const long parsed = strtol(text, &end, 10);
    if (*end || parsed < minimum || parsed > maximum) return false;
    value = static_cast<int32_t>(parsed);
    return true;
}

bool parseField(const char* line, const char* key, char* output, size_t size)
{
    if (!line || !key || !output || !size) return false;
    for (const char* p = line; p && *p; p = strchr(p, ',')) {
        if (*p == ',') ++p;
        const char* end        = strchr(p, ',');
        size_t length          = end ? static_cast<size_t>(end - p) : strlen(p);
        const size_t keyLength = strlen(key);
        if (length > keyLength && !strncmp(p, key, keyLength)) {
            length -= keyLength;
            if (length >= size) return false;
            memcpy(output, p + keyLength, length);
            output[length] = 0;
            return true;
        }
    }
    return false;
}

bool parseResponse(const char* response, const char* prefix, char* output, size_t size, char* error, size_t errorSize)
{
    // Ignore unrelated asynchronous lines; accept the expected payload only once.
    // A complete terminal line is required before the response can succeed.
    bool terminal = false;
    bool found    = prefix == nullptr;
    if (output && size) output[0] = 0;
    for (const char* p = response; p && *p;) {
        const char* end = strpbrk(p, "\r\n");
        if (!end) break;
        const size_t length = static_cast<size_t>(end - p);
        if (length == 2 && !strncmp(p, "OK", 2)) {
            if (terminal) {
                snprintf(error, errorSize, "DUPLICATE TERMINAL");
                return false;
            }
            terminal = true;
        } else if ((length >= 6 && !strncmp(p, "ERROR:", 6)) || (length == 5 && !strncmp(p, "ERROR", 5))) {
            snprintf(error, errorSize, "%.*s", static_cast<int>(length), p);
            return false;
        } else if (prefix && length >= strlen(prefix) && !strncmp(p, prefix, strlen(prefix))) {
            const size_t valueLength = length - strlen(prefix);
            if (found || !output || valueLength >= size) {
                snprintf(error, errorSize, "FORMAT");
                return false;
            }
            memcpy(output, p + strlen(prefix), valueLength);
            output[valueLength] = 0;
            found               = true;
        }
        while (*end == '\r' || *end == '\n') ++end;
        p = end;
    }
    if (!terminal || !found) snprintf(error, errorSize, "FORMAT");
    return terminal && found;
}

bool validApp(UWB2ProApp app, UWB2ProRole role, uint8_t value)
{
    if (raw(app) > raw(UWB2ProApp::Pos3D) || raw(role) > raw(UWB2ProRole::Anchor) || value < 1) return false;
    const uint8_t maximum = app == UWB2ProApp::Ranging ? 9
                            : role == UWB2ProRole::Tag ? 9
                            : app == UWB2ProApp::Pos2D ? 3
                                                       : 4;
    return value <= maximum;
}

bool parseApp(char* text, UWB2ProConfig& config)
{
    char* role = strchr(text, ',');
    if (!role) return false;
    *role++     = 0;
    char* value = strchr(role, ',');
    if (!value) return false;
    *value++             = 0;
    const int app        = !strcmp(text, "RANGING") ? 0 : !strcmp(text, "POS2D") ? 1 : !strcmp(text, "POS3D") ? 2 : -1;
    const int parsedRole = !strcmp(role, "TAG") ? 0 : !strcmp(role, "ANCHOR") ? 1 : -1;
    if (app < 0 || parsedRole < 0) return false;

    config.app     = static_cast<UWB2ProApp>(app);
    config.role    = static_cast<UWB2ProRole>(parsedRole);
    config.xMeters = config.yMeters = config.zMeters = 0;
    if (app != 0 && parsedRole == 0) {
        uint8_t axes[3]{};
        const unsigned dimensions = app == 1 ? 2 : 3;
        unsigned count            = 0;
        for (char* part = value; part && count < 3;) {
            char* next = strchr(part, ',');
            if (next) *next++ = 0;
            if (strlen(part) != 2 || part[0] < '1' || part[0] > '9' || part[1] != 'M') return false;
            axes[count++] = static_cast<uint8_t>(part[0] - '0');
            part          = next;
            if (!part && count != dimensions) return false;
            if (part && count == dimensions) return false;
        }
        if (count != dimensions) return false;
        config.xMeters = axes[0];
        config.yMeters = axes[1];
        config.zMeters = app == 2 ? axes[2] : 0;
        config.value   = config.xMeters;
        return true;
    }

    if (strchr(value, ',')) return false;
    int32_t parsedValue = 0;
    if (app != 0 && parsedRole == 1) {
        if (strlen(value) != 1) return false;
        const char* id = strchr("OXYZ", value[0]);
        if (!id || (app == 1 && value[0] == 'Z')) return false;
        parsedValue = static_cast<int32_t>(id - "OXYZ" + 1);
    } else if (!parseDecimal(value, 1, 9, parsedValue)) {
        return false;
    }
    config.value = static_cast<uint8_t>(parsedValue);
    return validApp(config.app, config.role, config.value);
}

bool parseSnapshotHeader(const char* text, const UWB2ProConfig& config, UWB2ProState& state)
{
    char value[24];
    int32_t number = 0;
    if (!parseField(text, "APP=", value, sizeof(value)) || strcmp(value, kApps[raw(config.app)])) return false;
    if (!parseField(text, "ROLE=", value, sizeof(value)) ||
        strcmp(value, config.role == UWB2ProRole::Tag ? "TAG" : "ANCHOR"))
        return false;
    if (!parseField(text, "GROUP=", value, sizeof(value)) || !parseDecimal(value, 1, 9, number) ||
        number != config.group)
        return false;
    if (!parseField(text, "STATUS=", value, sizeof(value))) return false;
    const char* const states[] = {"NO_DATA", "VALID", "TIMEOUT", "ERROR"};
    for (uint8_t index = 0; index < 4; ++index) {
        if (!strcmp(value, states[index])) {
            state = static_cast<UWB2ProState>(index);
            return true;
        }
    }
    return false;
}

bool parseDistanceField(const char* text, const char* key, unsigned id, UWB2ProState state, UWB2ProData& output)
{
    char value[24];
    int32_t number = 0;
    if (!parseField(text, key, value, sizeof(value))) return false;
    auto& distance       = output.anchors[id];
    distance.centimeters = INT32_MIN;
    distance.state       = state == UWB2ProState::Valid ? UWB2ProState::NoData : state;
    if (strcmp(value, "NA")) {
        if (!parseDecimal(value, 0, 10000, number)) return false;
        distance.centimeters = number;
        distance.state       = state;
    }
    return true;
}

bool parseDistances(const char* text, const UWB2ProConfig& config, UWB2ProData& output, int selected = -1)
{
    UWB2ProState state = UWB2ProState::NoData;
    if (!parseSnapshotHeader(text, config, state)) return false;
    if (config.role == UWB2ProRole::Anchor || selected >= 0) {
        char value[24];
        if (!parseField(text, "ID=", value, sizeof(value))) return false;
        const unsigned id = selected >= 0 ? static_cast<unsigned>(selected) : config.value - 1;
        if (config.app == UWB2ProApp::Ranging) {
            int32_t number = 0;
            if (!parseDecimal(value, 1, 9, number) || number != static_cast<int32_t>(id + 1)) return false;
        } else if (strlen(value) != 1 || value[0] != "OXYZ"[id]) {
            return false;
        }
        return parseDistanceField(text, "D=", id, state, output);
    }

    const unsigned count = config.app == UWB2ProApp::Ranging ? config.value : config.app == UWB2ProApp::Pos2D ? 3 : 4;
    for (unsigned index = 0; index < count; ++index) {
        char key[16];
        if (config.app == UWB2ProApp::Ranging)
            snprintf(key, sizeof(key), "D%u=", index + 1);
        else
            snprintf(key, sizeof(key), "D_%c=", "OXYZ"[index]);
        if (!parseDistanceField(text, key, index, state, output)) return false;
    }
    return true;
}

bool parsePosition(const char* text, const UWB2ProConfig& config, UWB2ProData& output)
{
    if (config.role != UWB2ProRole::Tag || config.app == UWB2ProApp::Ranging) return false;
    UWB2ProState state = UWB2ProState::NoData;
    if (!parseSnapshotHeader(text, config, state)) return false;
    int16_t* axes[]          = {&output.xCentimeters, &output.yCentimeters, &output.zCentimeters};
    const char* const keys[] = {"X=", "Y=", "Z="};
    const unsigned count     = config.app == UWB2ProApp::Pos2D ? 2 : 3;
    for (unsigned index = 0; index < count; ++index) {
        char value[24];
        int32_t number = 0;
        if (!parseField(text, keys[index], value, sizeof(value))) return false;
        *axes[index] = INT16_MIN;
        if (strcmp(value, "NA")) {
            if (!parseDecimal(value, -32767, 32767, number)) return false;
            *axes[index] = static_cast<int16_t>(number);
        }
    }
    output.positionState = state;
    return true;
}

}  // namespace

M5UnitUWB2Pro::M5UnitUWB2Pro() = default;
M5UnitUWB2Pro::~M5UnitUWB2Pro()
{
    end();
}

bool M5UnitUWB2Pro::beginWithIO(UWB2ProComm comm, const UWB2ProIO& io, uint8_t address, uint32_t timeoutMs)
{
    if ((comm != UWB2ProComm::UART && comm != UWB2ProComm::I2C) || !timeoutMs ||
        (comm == UWB2ProComm::UART ? io.exchange == nullptr : io.read == nullptr || io.write == nullptr) ||
        (comm == UWB2ProComm::I2C && (address < 0x08 || address > 0x77)))
        return fail(UWB2ProError::InvalidArgument, "INVALID TRANSPORT");
    end();
    io_   = io;
    comm_ = stagedComm_ = comm;
    address_ = stagedAddress_ = comm == UWB2ProComm::I2C ? address : 0;
    timeoutMs_                = timeoutMs;
    connected_                = true;
    clearError();
    return true;
}

void M5UnitUWB2Pro::end()
{
    if (connected_ && io_.destroy) io_.destroy(io_.context);
    io_        = {};
    connected_ = false;
    address_ = stagedAddress_ = 0;
    errorCode_                = UWB2ProError::NotConnected;
    snprintf(errorText_, sizeof(errorText_), "NOT CONNECTED");
}

void M5UnitUWB2Pro::clearError()
{
    errorCode_    = UWB2ProError::None;
    errorText_[0] = 0;
}

bool M5UnitUWB2Pro::fail(UWB2ProError code, const char* text)
{
    errorCode_ = code;
    snprintf(errorText_, sizeof(errorText_), "%s", text ? text : "ERROR");
    return false;
}

bool M5UnitUWB2Pro::at(const char* command, const char* prefix, char* value, size_t size)
{
    if (!connected_ || comm_ != UWB2ProComm::UART || !io_.exchange)
        return fail(UWB2ProError::NotConnected, "UART NOT CONNECTED");
    char response[512]{};
    const auto result = io_.exchange(io_.context, command, response, sizeof(response), timeoutMs_);
    if (result == UWB2ProIOResult::Timeout) return fail(UWB2ProError::Timeout, "UART TIMEOUT");
    if (result != UWB2ProIOResult::Ok) return fail(UWB2ProError::Transport, "UART IO");
    char parseError[64]{};
    if (!parseResponse(response, prefix, value, size, parseError, sizeof(parseError))) {
        const auto code = !strncmp(parseError, "ERROR", 5) ? UWB2ProError::RemoteError : UWB2ProError::Protocol;
        return fail(code, parseError);
    }
    clearError();
    return true;
}

bool M5UnitUWB2Pro::readRegister(uint8_t reg, uint8_t* data, size_t size)
{
    if (!connected_ || comm_ != UWB2ProComm::I2C || !io_.read)
        return fail(UWB2ProError::NotConnected, "I2C NOT CONNECTED");
    if (!data || size < 1 || size > 16) return fail(UWB2ProError::InvalidArgument, "I2C READ LENGTH");
    const auto result = io_.read(io_.context, reg, data, size, timeoutMs_);
    if (result == UWB2ProIOResult::Timeout) return fail(UWB2ProError::Timeout, "I2C TIMEOUT");
    if (result != UWB2ProIOResult::Ok) {
        char text[40];
        snprintf(text, sizeof(text), "I2C READ %02X/%u", reg, static_cast<unsigned>(size));
        return fail(UWB2ProError::Transport, text);
    }
    clearError();
    return true;
}

bool M5UnitUWB2Pro::writeRegister(uint8_t reg, const uint8_t* data, size_t size, bool handover)
{
    if (!connected_ || comm_ != UWB2ProComm::I2C || !io_.write)
        return fail(UWB2ProError::NotConnected, "I2C NOT CONNECTED");
    if (!data || size < 1 || size > 16) return fail(UWB2ProError::InvalidArgument, "I2C WRITE LENGTH");
    auto result = io_.write(io_.context, reg, data, size, timeoutMs_);
    if (result == UWB2ProIOResult::Timeout) return fail(UWB2ProError::Timeout, "I2C WRITE TIMEOUT");
    if (result != UWB2ProIOResult::Ok) return fail(UWB2ProError::Transport, "I2C WRITE");
    if (io_.delayMs) {
        const bool saving = reg == 0xF0 && size == 1 && (data[0] == 1 || data[0] == 3);
        io_.delayMs(io_.context, saving ? 100 : 10);
    }
    // The result latch belongs to the old endpoint and may disappear after SAVE
    // changes the interface/address. The caller confirms that handover by reconnecting.
    uint8_t remoteError = 0;
    if (!readRegister(0xF1, &remoteError, 1)) {
        if (handover) {
            clearError();
            return true;
        }
        return false;
    }
    if (handover && remoteError == 0xFF) {
        clearError();
        return true;
    }
    if (remoteError) {
        char text[32];
        snprintf(text, sizeof(text), "I2C ERROR:%u", remoteError);
        return fail(UWB2ProError::RemoteError, text);
    }
    clearError();
    return true;
}

bool M5UnitUWB2Pro::setNumber(uint8_t reg, const char* atPrefix, uint8_t value, const char* atValue)
{
    if (comm_ == UWB2ProComm::I2C) return writeRegister(reg, &value, 1);
    char command[64];
    snprintf(command, sizeof(command), "%s%s", atPrefix, atValue ? atValue : "");
    if (!atValue) snprintf(command, sizeof(command), "%s%u", atPrefix, value);
    return at(command);
}

bool M5UnitUWB2Pro::control(const char* atCommand, uint8_t i2cCommand)
{
    const bool handover = i2cCommand == 3 ? comm_ != UWB2ProComm::UART
                                          : i2cCommand == 1 && (stagedComm_ != comm_ || stagedAddress_ != address_);
    return comm_ == UWB2ProComm::I2C ? writeRegister(0xF0, &i2cCommand, 1, handover) : at(atCommand);
}

bool M5UnitUWB2Pro::probe()
{
    if (!connected_) return fail(UWB2ProError::NotConnected, "NOT CONNECTED");
    if (comm_ == UWB2ProComm::UART && !at("AT")) {
        // Enabling a host UART can create one partial line while TX changes
        // from GPIO input to idle-high. One bounded AT retry synchronizes the
        // line parser; all subsequent command failures remain visible.
        if (io_.delayMs) io_.delayMs(io_.context, 20);
        if (!at("AT")) return false;
    }
    if (comm_ == UWB2ProComm::UART) return true;
    // Read a version register to test the selected endpoint, without gating on its model.
    char version[8];
    return getVersion(version, sizeof(version));
}

bool M5UnitUWB2Pro::getBoard(char* output, size_t size)
{
    if (!output || size < 2) return fail(UWB2ProError::InvalidArgument, "BOARD BUFFER");
    char name[32];
    if (comm_ == UWB2ProComm::I2C) {
        uint8_t board = 0xFF;
        if (!readRegister(0xFD, &board, 1)) return false;
        if (board > 1) return fail(UWB2ProError::Protocol, "BOARD ID");
        snprintf(name, sizeof(name), "%s", board == 0 ? "UWB2-Pro" : "UWB Angle-Pro");
    } else {
        if (!at("AT+BOARD?", "+BOARD:", name, sizeof(name))) return false;
        if (!name[0]) return fail(UWB2ProError::Protocol, "BOARD EMPTY");
    }
    if (strlen(name) >= size) return fail(UWB2ProError::InvalidArgument, "BOARD BUFFER");
    strcpy(output, name);
    clearError();
    return true;
}

bool M5UnitUWB2Pro::getVersion(char* output, size_t size)
{
    if (!output || size < 4) return fail(UWB2ProError::InvalidArgument, "VERSION BUFFER");
    if (comm_ == UWB2ProComm::I2C) {
        uint8_t version = 0;
        if (!readRegister(0xFE, &version, 1)) return false;
        if ((version >> 4) > 9 || (version & 0x0F) > 9) return fail(UWB2ProError::Protocol, "VERSION BCD");
        snprintf(output, size, "%u.%u", version >> 4, version & 0x0F);
    } else {
        char value[32];
        if (!at("AT+VERSION?", "+VERSION:", value, sizeof(value))) return false;
        const char* dot = strchr(value, '.');
        if (!dot || dot == value || !dot[1] || strchr(dot + 1, '.'))
            return fail(UWB2ProError::Protocol, "VERSION FORMAT");
        for (const char* p = value; *p; ++p)
            if (p != dot && (*p < '0' || *p > '9')) return fail(UWB2ProError::Protocol, "VERSION FORMAT");
        if (strlen(value) >= size) return fail(UWB2ProError::InvalidArgument, "VERSION BUFFER");
        strcpy(output, value);
    }
    clearError();
    return true;
}

bool M5UnitUWB2Pro::getMAC(char* output, size_t size)
{
    if (comm_ != UWB2ProComm::UART) return fail(UWB2ProError::Unsupported, "MAC IS UART ONLY");
    if (!output || size < 18) return fail(UWB2ProError::InvalidArgument, "MAC BUFFER");
    char value[64], mac[24];
    if (!at("AT+MAC?", "+MAC:", value, sizeof(value))) return false;
    if (!parseField(value, "BASE=", mac, sizeof(mac)) || strlen(mac) != 17)
        return fail(UWB2ProError::Protocol, "MAC FORMAT");
    for (size_t index = 0; index < 17; ++index) {
        if (index % 3 == 2) {
            if (mac[index] != ':') return fail(UWB2ProError::Protocol, "MAC FORMAT");
        } else if (!((mac[index] >= '0' && mac[index] <= '9') || (mac[index] >= 'A' && mac[index] <= 'F') ||
                     (mac[index] >= 'a' && mac[index] <= 'f'))) {
            return fail(UWB2ProError::Protocol, "MAC FORMAT");
        }
    }
    strcpy(output, mac);
    clearError();
    return true;
}

bool M5UnitUWB2Pro::getAppRunning(bool& output)
{
    if (!connected_) return fail(UWB2ProError::NotConnected, "NOT CONNECTED");
    bool running = false;
    if (comm_ == UWB2ProComm::I2C) {
        // The adjacent format and running bytes belong to one register snapshot.
        uint8_t context[2];
        if (!readRegister(0x20, context, sizeof(context))) return false;
        if (context[0] != 1 || context[1] > 1) return fail(UWB2ProError::Protocol, "ACTIVE CONTEXT FORMAT");
        running = context[1] != 0;
    } else {
        // DIST is read-only. STATE means SETUP; a NO_DATA payload still means APP.
        char line[256];
        if (!at("AT+DIST?", "+DIST:", line, sizeof(line))) {
            if (errorCode_ != UWB2ProError::RemoteError || strcmp(errorText_, "ERROR:STATE")) return false;
        } else {
            UWB2ProConfig config;
            char value[24];
            int app = -1;
            if (!parseField(line, "APP=", value, sizeof(value))) return fail(UWB2ProError::Protocol, "DIST FORMAT");
            for (int index = 0; index < 3; ++index)
                if (!strcmp(value, kApps[index])) app = index;
            if (app < 0 || !parseField(line, "ROLE=", value, sizeof(value)) ||
                (strcmp(value, "TAG") && strcmp(value, "ANCHOR")))
                return fail(UWB2ProError::Protocol, "DIST FORMAT");
            config.app    = static_cast<UWB2ProApp>(app);
            config.role   = !strcmp(value, "TAG") ? UWB2ProRole::Tag : UWB2ProRole::Anchor;
            int32_t group = 0;
            if (!parseField(line, "GROUP=", value, sizeof(value)) || !parseDecimal(value, 1, 9, group))
                return fail(UWB2ProError::Protocol, "DIST FORMAT");
            config.group = static_cast<uint8_t>(group);
            UWB2ProState state;
            if (!parseSnapshotHeader(line, config, state)) return fail(UWB2ProError::Protocol, "DIST FORMAT");
            running = true;
        }
    }
    output = running;
    clearError();
    return true;
}

bool M5UnitUWB2Pro::getConfig(UWB2ProConfig& output)
{
    if (!connected_) return fail(UWB2ProError::NotConnected, "NOT CONNECTED");
    // Decode into a candidate so a partial response cannot alter the caller's draft.
    UWB2ProConfig config{};
    if (comm_ == UWB2ProComm::I2C) {
        uint8_t app[8]{}, system[6]{}, address = 0;
        if (!readRegister(0x00, app, sizeof(app)) || !readRegister(0x10, system, sizeof(system)) ||
            !readRegister(0xFF, &address, 1))
            return false;
        if (app[0] > 2 || app[1] > 1 || system[0] > 2 || system[1] > 1 || system[2] > 2 || system[3] < 1 ||
            system[3] > 9 || system[4] > 1 || system[5] > 1 || address < 0x08 || address > 0x77)
            return fail(UWB2ProError::Protocol, "CONFIG RANGE");
        config.app        = static_cast<UWB2ProApp>(app[0]);
        config.role       = static_cast<UWB2ProRole>(app[1]);
        config.value      = app[0] == 0 ? app[app[1] == 0 ? 2 : 3] : app[app[1] == 0 ? 5 : 3];
        config.comm       = static_cast<UWB2ProComm>(system[0]);
        config.bootApp    = system[1] != 0;
        config.led        = static_cast<UWB2ProLED>(system[2]);
        config.group      = system[3];
        config.dataRate   = static_cast<UWB2ProDataRate>(system[4]);
        config.valid      = system[5] != 0;
        config.i2cAddress = address;
        if (config.app != UWB2ProApp::Ranging && config.role == UWB2ProRole::Tag) {
            config.xMeters = app[5];
            config.yMeters = app[6];
            config.zMeters = config.app == UWB2ProApp::Pos3D ? app[7] : 0;
            config.value   = config.xMeters;
        }
    } else {
        char value[64];
        if (!at("AT+APP?", "+APP:", value, sizeof(value))) return false;
        if (!parseApp(value, config)) return fail(UWB2ProError::Protocol, "APP FORMAT");
        int32_t number = 0;
        if (!at("AT+GROUP?", "+GROUP:", value, sizeof(value))) return false;
        if (!parseDecimal(value, 1, 9, number)) return fail(UWB2ProError::Protocol, "GROUP FORMAT");
        config.group = static_cast<uint8_t>(number);
        if (!at("AT+RATE?", "+RATE:", value, sizeof(value))) return false;
        if (!strcmp(value, "850K"))
            config.dataRate = UWB2ProDataRate::Rate850K;
        else if (!strcmp(value, "6.8M"))
            config.dataRate = UWB2ProDataRate::Rate6M8;
        else
            return fail(UWB2ProError::Protocol, "RATE FORMAT");
        if (!at("AT+COMM?", "+COMM:", value, sizeof(value))) return false;
        int comm = -1;
        for (int index = 0; index < 3; ++index)
            if (!strcmp(value, kComms[index])) comm = index;
        if (comm < 0) return fail(UWB2ProError::Protocol, "COMM FORMAT");
        config.comm = static_cast<UWB2ProComm>(comm);
        if (!at("AT+LED?", "+LED:", value, sizeof(value)) || !parseDecimal(value, 0, 2, number))
            return fail(UWB2ProError::Protocol, "LED FORMAT");
        config.led = static_cast<UWB2ProLED>(number);
        if (!at("AT+BOOT?", "+BOOT:", value, sizeof(value))) return false;
        if (strcmp(value, "APP") && strcmp(value, "SETUP")) return fail(UWB2ProError::Protocol, "BOOT FORMAT");
        config.bootApp    = !strcmp(value, "APP");
        config.valid      = true;
        config.i2cAddress = 0;
    }
    if (config.app != UWB2ProApp::Ranging && config.role == UWB2ProRole::Tag) {
        if (config.xMeters < 1 || config.xMeters > 9 || config.yMeters < 1 || config.yMeters > 9 ||
            (config.app == UWB2ProApp::Pos3D && (config.zMeters < 1 || config.zMeters > 9)))
            return fail(UWB2ProError::Protocol, "CONFIG AXIS RANGE");
    } else if (!validApp(config.app, config.role, config.value)) {
        return fail(UWB2ProError::Protocol, "CONFIG VALUE RANGE");
    }
    output      = config;
    stagedComm_ = config.comm;
    if (config.i2cAddress) stagedAddress_ = config.i2cAddress;
    clearError();
    return true;
}

bool M5UnitUWB2Pro::setApp(UWB2ProApp app, UWB2ProRole role, uint8_t value)
{
    if (!validApp(app, role, value) || (app != UWB2ProApp::Ranging && role == UWB2ProRole::Tag))
        return fail(UWB2ProError::InvalidArgument, "APP ARGUMENT");
    if (comm_ == UWB2ProComm::I2C) {
        const uint8_t fields[] = {raw(app), raw(role), value};
        if (!writeRegister(0x00, fields, 1) || !writeRegister(0x01, fields + 1, 1)) return false;
        const uint8_t reg = role == UWB2ProRole::Tag ? 0x02 : 0x03;
        return writeRegister(reg, fields + 2, 1);
    }
    char command[64];
    if (app == UWB2ProApp::Ranging)
        snprintf(command, sizeof(command), "AT+APP=RANGING,%s,%u", role == UWB2ProRole::Tag ? "TAG" : "ANCHOR", value);
    else
        snprintf(command, sizeof(command), "AT+APP=%s,ANCHOR,%c", kApps[raw(app)], "OXYZ"[value - 1]);
    return at(command);
}

bool M5UnitUWB2Pro::setLayout(UWB2ProApp app, uint8_t x, uint8_t y, uint8_t z)
{
    if ((app != UWB2ProApp::Pos2D && app != UWB2ProApp::Pos3D) || x < 1 || x > 9 || y < 1 || y > 9 ||
        (app == UWB2ProApp::Pos3D && (z < 1 || z > 9)))
        return fail(UWB2ProError::InvalidArgument, "LAYOUT ARGUMENT");
    if (comm_ == UWB2ProComm::I2C) {
        const uint8_t appValue = raw(app), role = raw(UWB2ProRole::Tag), axes[] = {x, y, z};
        return writeRegister(0x00, &appValue, 1) && writeRegister(0x01, &role, 1) &&
               writeRegister(0x05, axes, app == UWB2ProApp::Pos2D ? 2 : 3);
    }
    char command[64];
    if (app == UWB2ProApp::Pos2D)
        snprintf(command, sizeof(command), "AT+APP=POS2D,TAG,%uM,%uM", x, y);
    else
        snprintf(command, sizeof(command), "AT+APP=POS3D,TAG,%uM,%uM,%uM", x, y, z);
    return at(command);
}

bool M5UnitUWB2Pro::setGroup(uint8_t group)
{
    return group >= 1 && group <= 9 ? setNumber(0x13, "AT+GROUP=", group)
                                    : fail(UWB2ProError::InvalidArgument, "GROUP RANGE");
}

bool M5UnitUWB2Pro::setDataRate(UWB2ProDataRate rate)
{
    if (raw(rate) > 1) return fail(UWB2ProError::InvalidArgument, "RATE RANGE");
    return setNumber(0x14, "AT+RATE=", raw(rate), rate == UWB2ProDataRate::Rate850K ? "850K" : "6.8M");
}

bool M5UnitUWB2Pro::setComm(UWB2ProComm comm)
{
    if (comm != UWB2ProComm::UART && comm != UWB2ProComm::I2C)
        return fail(UWB2ProError::Unsupported, "USB TRANSPORT UNSUPPORTED");
    if (!setNumber(0x10, "AT+COMM=", raw(comm), kComms[raw(comm)])) return false;
    stagedComm_ = comm;
    return true;
}

bool M5UnitUWB2Pro::setLED(UWB2ProLED mode)
{
    return raw(mode) <= 2 ? setNumber(0x12, "AT+LED=", raw(mode)) : fail(UWB2ProError::InvalidArgument, "LED RANGE");
}

bool M5UnitUWB2Pro::setBootApp(bool app)
{
    return setNumber(0x11, "AT+BOOT=", app ? 1 : 0, app ? "APP" : "SETUP");
}

bool M5UnitUWB2Pro::setI2CAddress(uint8_t address)
{
    if (comm_ != UWB2ProComm::I2C) return fail(UWB2ProError::Unsupported, "ADDRESS IS I2C ONLY");
    if (address < 0x08 || address > 0x77) return fail(UWB2ProError::InvalidArgument, "ADDRESS RANGE");
    if (!writeRegister(0xFF, &address, 1)) return false;
    stagedAddress_ = address;
    return true;
}

bool M5UnitUWB2Pro::save()
{
    return control("AT+SAVE", 1);
}

bool M5UnitUWB2Pro::cancel()
{
    if (!control("AT+SAVE=CANCEL", 2)) return false;
    stagedComm_    = comm_;
    stagedAddress_ = address_;
    return true;
}

bool M5UnitUWB2Pro::restore()
{
    return control("AT+RESTORE", 3);
}

bool M5UnitUWB2Pro::readData(const UWB2ProConfig& config, UWB2ProData& output)
{
    const bool positioningTag = config.app != UWB2ProApp::Ranging && config.role == UWB2ProRole::Tag;
    if (!validApp(config.app, config.role, config.value) ||
        (positioningTag &&
         (config.value != config.xMeters || config.xMeters < 1 || config.xMeters > 9 || config.yMeters < 1 ||
          config.yMeters > 9 || (config.app == UWB2ProApp::Pos3D && (config.zMeters < 1 || config.zMeters > 9)))))
        return fail(UWB2ProError::InvalidArgument, "ACTIVE CONFIG");
    // Publish only after every requested record and runtime-context check succeeds.
    UWB2ProData data{};
    if (comm_ == UWB2ProComm::UART) {
        char line[256];
        const unsigned count = config.role == UWB2ProRole::Anchor  ? 1
                               : config.app == UWB2ProApp::Ranging ? config.value
                               : config.app == UWB2ProApp::Pos2D   ? 3
                                                                   : 4;
        // An aggregate STATUS is the worst state in the group. It cannot tell
        // whether a numeric value belongs to a valid or stale Anchor sample.
        const bool selectedQueries = config.role == UWB2ProRole::Tag && count > 1;
        for (unsigned index = 0; index < (selectedQueries ? count : 1); ++index) {
            char command[20] = "AT+DIST?";
            if (selectedQueries) {
                if (config.app == UWB2ProApp::Ranging)
                    snprintf(command, sizeof(command), "AT+DIST=%u?", index + 1);
                else
                    snprintf(command, sizeof(command), "AT+DIST=%c?", "OXYZ"[index]);
            }
            if (!at(command, "+DIST:", line, sizeof(line))) return false;
            if (!parseDistances(line, config, data, selectedQueries ? static_cast<int>(index) : -1))
                return fail(UWB2ProError::Protocol, "DIST FORMAT");
        }
        if (config.app != UWB2ProApp::Ranging && config.role == UWB2ProRole::Tag) {
            if (!at("AT+POS?", "+POS:", line, sizeof(line))) {
                if (errorCode_ != UWB2ProError::RemoteError) return false;
                if (!strcmp(errorText_, "ERROR:TIMEOUT"))
                    data.positionState = UWB2ProState::Timeout;
                else if (!strcmp(errorText_, "ERROR:NO_DATA"))
                    data.positionState = UWB2ProState::NoData;
                else if (!strcmp(errorText_, "ERROR:STATE"))
                    data.positionState = UWB2ProState::Error;
                else
                    return false;
                clearError();
            } else if (!parsePosition(line, config, data))
                return fail(UWB2ProError::Protocol, "POS FORMAT");
        }
    } else {
        // Registers 0x20-0x2F identify the applied configuration and its generation.
        // Read them around the data registers to reject samples from a context change.
        uint8_t context[16];
        if (!readRegister(0x20, context, sizeof(context))) return false;
        const unsigned expectedCount = config.role == UWB2ProRole::Anchor  ? 1
                                       : config.app == UWB2ProApp::Ranging ? config.value
                                       : config.app == UWB2ProApp::Pos2D   ? 3
                                                                           : 4;
        if (context[0] != 1 || context[1] > 1 || context[7])
            return fail(UWB2ProError::Protocol, "ACTIVE CONTEXT FORMAT");
        if (context[2] != raw(config.app) || context[3] != raw(config.role) || context[4] != config.group ||
            context[5] != expectedCount || context[6] != (config.role == UWB2ProRole::Anchor ? config.value : 1) ||
            context[12] != (positioningTag ? config.xMeters : 0) ||
            context[13] != (positioningTag ? config.yMeters : 0) ||
            context[14] != (positioningTag && config.app == UWB2ProApp::Pos3D ? config.zMeters : 0) ||
            context[15] != raw(config.dataRate))
            return fail(UWB2ProError::ActiveConfigChanged, "ACTIVE CONFIG CHANGED");

        const unsigned count = expectedCount;
        for (unsigned index = 0; index < count; ++index) {
            const unsigned id = config.role == UWB2ProRole::Anchor ? config.value - 1 : index;
            uint8_t bytes[4];
            if (!readRegister(static_cast<uint8_t>(0x40 + id * 4), bytes, sizeof(bytes))) return false;
            if (bytes[2] > 3) return fail(UWB2ProError::Protocol, "DATA STATE");
            // Distances are unsigned, little-endian centimetres; state validates them.
            const uint16_t centimeters = static_cast<uint16_t>(bytes[0] | bytes[1] << 8);
            data.anchors[id].state     = static_cast<UWB2ProState>(bytes[2]);
            if (data.anchors[id].state == UWB2ProState::Valid) {
                if (centimeters > 10000) return fail(UWB2ProError::Protocol, "DISTANCE RANGE");
                data.anchors[id].centimeters = centimeters;
            }
        }
        if (positioningTag) {
            uint8_t bytes[7];
            if (!readRegister(0x70, bytes, sizeof(bytes))) return false;
            if (bytes[6] > 3) return fail(UWB2ProError::Protocol, "POSITION STATE");
            data.positionState = static_cast<UWB2ProState>(bytes[6]);
            if (data.positionState == UWB2ProState::Valid) {
                // Coordinates use signed, little-endian 16-bit centimetres.
                data.xCentimeters = static_cast<int16_t>(bytes[0] | bytes[1] << 8);
                data.yCentimeters = static_cast<int16_t>(bytes[2] | bytes[3] << 8);
                data.zCentimeters = static_cast<int16_t>(bytes[4] | bytes[5] << 8);
                if (data.xCentimeters == INT16_MIN || data.yCentimeters == INT16_MIN ||
                    (config.app == UWB2ProApp::Pos3D && data.zCentimeters == INT16_MIN))
                    return fail(UWB2ProError::Protocol, "POSITION RANGE");
            }
        }
        uint8_t after[16];
        if (!readRegister(0x20, after, sizeof(after))) return false;
        if (memcmp(context, after, sizeof(context)))
            return fail(UWB2ProError::ActiveContextChanged, "ACTIVE CONTEXT CHANGED");
    }
    output = data;
    clearError();
    return true;
}
