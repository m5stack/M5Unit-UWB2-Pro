/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology Co., Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file M5UnitUWB2ProArduino.cpp
 * @brief Arduino HardwareSerial and TwoWire transport adapters.
 */

#include "M5UnitUWB2Pro.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_idf_version.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
#include <driver/i2c_master.h>
#endif

#include <limits.h>
#include <new>
#include <string.h>

namespace {

struct UARTContext {
    HardwareSerial* serial;
};

struct I2CContext {
    TwoWire* wire;
    uint8_t address;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    i2c_master_dev_handle_t device = nullptr;
#endif
};

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
// Recent TwoWire versions expose the bus number, including custom instances.
template <typename WireType>
auto wireBusNumber(WireType& wire, int) -> decltype(wire.getBusNum())
{
    return wire.getBusNum();
}

// Earlier cores expose only the standard Wire / Wire1 instances publicly.
template <typename WireType>
int wireBusNumber(WireType& wire, long)
{
    if (&wire == &Wire) return 0;
#if SOC_I2C_NUM > 1
    if (&wire == &Wire1) return 1;
#endif
    return -1;
}

int i2cTimeoutMs(uint32_t timeoutMs)
{
    // IDF uses a signed timeout; a large unsigned value must never become -1.
    return timeoutMs > INT_MAX ? INT_MAX : static_cast<int>(timeoutMs);
}

UWB2ProIOResult i2cResult(esp_err_t result)
{
    if (result == ESP_OK) return UWB2ProIOResult::Ok;
    return result == ESP_ERR_TIMEOUT ? UWB2ProIOResult::Timeout : UWB2ProIOResult::Failure;
}
#endif

void destroyUART(void* context)
{
    auto* uart = static_cast<UARTContext*>(context);
    if (uart) {
        uart->serial->end();
        delete uart;
    }
}

void destroyI2C(void* context)
{
    auto* i2c = static_cast<I2CContext*>(context);
    if (i2c) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
        // The device must be detached before TwoWire releases its owning bus.
        if (i2c->device) i2c_master_bus_rm_device(i2c->device);
#endif
        i2c->wire->end();
        delete i2c;
    }
}

void delayCallback(void*, uint32_t milliseconds)
{
    delay(milliseconds);
}

UWB2ProIOResult uartExchange(void* context, const char* command, char* output, size_t size, uint32_t timeoutMs)
{
    auto& serial = *static_cast<UARTContext*>(context)->serial;
    if (!command || !output || size < 8 || strpbrk(command, "\r\n")) return UWB2ProIOResult::Failure;

    // Drain only the currently queued bytes. Do not wait for an idle wire.
    for (size_t count = 0; count < 4096 && serial.available(); ++count) serial.read();
#ifdef M5UNIT_UWB2PRO_TRACE
    Serial.printf("TRACE UWB TX [%s]\\r\\n\n", command);
#endif
    if (serial.print(command) != strlen(command) || serial.print("\r\n") != 2) return UWB2ProIOResult::Failure;
    serial.flush();

    size_t used            = 0;
    size_t lineStart       = 0;
    const uint32_t started = millis();
    while (static_cast<uint32_t>(millis() - started) < timeoutMs) {
        while (serial.available()) {
            const int byte = serial.read();
            if (byte < 0) break;
            if (used + 1 >= size) return UWB2ProIOResult::Failure;
            output[used++] = static_cast<char>(byte);
            output[used]   = 0;
            if (byte != '\n') continue;

            size_t lineEnd = used;
            while (lineEnd > lineStart && (output[lineEnd - 1] == '\r' || output[lineEnd - 1] == '\n')) --lineEnd;
            const size_t length = lineEnd - lineStart;
            if ((length == 2 && !strncmp(output + lineStart, "OK", 2)) ||
                (length >= 5 && !strncmp(output + lineStart, "ERROR", 5))) {
                // The firmware emits its terminal line before the command task
                // has fully yielded. A short gap prevents a back-to-back MCU
                // host from entering the next line during that handoff.
                delay(10);
#ifdef M5UNIT_UWB2PRO_TRACE
                Serial.printf("TRACE UWB RX [%s]\n", output);
#endif
                return UWB2ProIOResult::Ok;
            }
            lineStart = used;
        }
        delay(1);
    }
    return UWB2ProIOResult::Timeout;
}

UWB2ProIOResult i2cRead(void* context, uint8_t reg, uint8_t* output, size_t size, uint32_t timeoutMs)
{
    auto& i2c = *static_cast<I2CContext*>(context);
    if (!output || size < 1 || size > 16) return UWB2ProIOResult::Failure;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    if (i2c.device) {
        // One combined transfer preserves repeated START and the exact read length.
        return i2cResult(i2c_master_transmit_receive(i2c.device, &reg, 1, output, size, i2cTimeoutMs(timeoutMs)));
    }
#else
    (void)timeoutMs;
#endif
    i2c.wire->beginTransmission(i2c.address);
    // Keep the register-pointer write and data read in one repeated-START transfer.
    if (i2c.wire->write(reg) != 1 || i2c.wire->endTransmission(false) != 0) return UWB2ProIOResult::Failure;
    const size_t received = i2c.wire->requestFrom(i2c.address, size, true);
    if (received != size) {
        while (i2c.wire->available()) i2c.wire->read();
        return UWB2ProIOResult::Failure;
    }
    for (size_t index = 0; index < size; ++index) {
        if (!i2c.wire->available()) return UWB2ProIOResult::Failure;
        output[index] = static_cast<uint8_t>(i2c.wire->read());
    }
    return i2c.wire->available() ? UWB2ProIOResult::Failure : UWB2ProIOResult::Ok;
}

UWB2ProIOResult i2cWrite(void* context, uint8_t reg, const uint8_t* data, size_t size, uint32_t timeoutMs)
{
    auto& i2c = *static_cast<I2CContext*>(context);
    if (!data || size < 1 || size > 16) return UWB2ProIOResult::Failure;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    if (i2c.device) {
        uint8_t payload[17];
        payload[0] = reg;
        memcpy(payload + 1, data, size);
        return i2cResult(i2c_master_transmit(i2c.device, payload, size + 1, i2cTimeoutMs(timeoutMs)));
    }
#else
    (void)timeoutMs;
#endif
    i2c.wire->beginTransmission(i2c.address);
    if (i2c.wire->write(reg) != 1 || i2c.wire->write(data, size) != size) return UWB2ProIOResult::Failure;
    return i2c.wire->endTransmission(true) == 0 ? UWB2ProIOResult::Ok : UWB2ProIOResult::Failure;
}

}  // namespace

bool M5UnitUWB2Pro::beginUART(HardwareSerial& serial, int8_t rxPin, int8_t txPin, uint32_t timeoutMs)
{
    if (rxPin < 0 || txPin < 0 || !timeoutMs) return fail(UWB2ProError::InvalidArgument, "UART ARGUMENT");
    end();
    auto* context = new (std::nothrow) UARTContext{&serial};
    if (!context) return fail(UWB2ProError::Transport, "NO MEMORY");
    serial.begin(115200, SERIAL_8N1, rxPin, txPin);
    serial.setTimeout(timeoutMs);
    UWB2ProIO io{context, uartExchange, nullptr, nullptr, delayCallback, destroyUART};
    if (!beginWithIO(UWB2ProComm::UART, io, 0, timeoutMs)) {
        destroyUART(context);
        return false;
    }
    return true;
}

bool M5UnitUWB2Pro::beginI2C(TwoWire& wire, int8_t sdaPin, int8_t sclPin, uint8_t address, uint32_t clockHz,
                             uint32_t timeoutMs)
{
    if (sdaPin < 0 || sclPin < 0 || address < 0x08 || address > 0x77 || (clockHz != 100000 && clockHz != 400000) ||
        !timeoutMs)
        return fail(UWB2ProError::InvalidArgument, "I2C ARGUMENT");
    end();
    auto* context = new (std::nothrow) I2CContext{&wire, address};
    if (!context) return fail(UWB2ProError::Transport, "NO MEMORY");
    if (!wire.begin(sdaPin, sclPin, clockHz)) {
        delete context;
        return fail(UWB2ProError::Transport, "I2C BEGIN");
    }
    wire.setTimeOut(timeoutMs);
    wire.setClock(clockHz);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    const int busNumber = wireBusNumber(wire, 0);
    if (busNumber >= 0) {
        i2c_master_bus_handle_t bus = nullptr;
        i2c_device_config_t config  = {};
        config.dev_addr_length      = I2C_ADDR_BIT_LEN_7;
        config.device_address       = address;
        config.scl_speed_hz         = clockHz;
        // The C6 slave prepares each reply in a task and may stretch SCL while
        // ranging. The new ESP32 master defaults to only 2 ms; Wire.setTimeOut()
        // changes its software timeout, not that hardware limit. Allow 12 ms (within ESP32's
        // limit), bounded by the caller's timeout, without retrying failed transactions.
        config.scl_wait_us = timeoutMs < 12 ? timeoutMs * 1000 : 12000;
        if (i2c_master_get_bus_handle(static_cast<i2c_port_num_t>(busNumber), &bus) != ESP_OK ||
            i2c_master_bus_add_device(bus, &config, &context->device) != ESP_OK) {
            destroyI2C(context);
            return fail(UWB2ProError::Transport, "I2C DEVICE");
        }
    }
#endif
    UWB2ProIO io{context, nullptr, i2cRead, i2cWrite, delayCallback, destroyI2C};
    if (!beginWithIO(UWB2ProComm::I2C, io, address, timeoutMs)) {
        destroyI2C(context);
        return false;
    }
    return true;
}
