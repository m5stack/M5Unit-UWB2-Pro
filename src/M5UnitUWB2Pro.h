/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology Co., Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file M5UnitUWB2Pro.h
 * @brief UART/I2C host API for Unit UWB2-Pro factory firmware v0.1.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

class HardwareSerial;
class TwoWire;

static constexpr uint8_t M5_UNIT_UWB2_PRO_DEFAULT_I2C_ADDRESS = 0x64;

enum class UWB2ProComm : uint8_t { I2C = 0, UART = 1, USB = 2 };
enum class UWB2ProApp : uint8_t { Ranging = 0, Pos2D = 1, Pos3D = 2 };
enum class UWB2ProRole : uint8_t { Tag = 0, Anchor = 1 };
enum class UWB2ProState : uint8_t { NoData = 0, Valid = 1, Timeout = 2, Error = 3 };
enum class UWB2ProLED : uint8_t { Off = 0, On = 1, Auto = 2 };
enum class UWB2ProDataRate : uint8_t { Rate850K = 0, Rate6M8 = 1 };

enum class UWB2ProError : uint8_t {
    None,
    NotConnected,
    InvalidArgument,
    Unsupported,
    Timeout,
    Transport,
    Protocol,
    RemoteError,
    WrongBoard,
    ActiveConfigChanged,
    ActiveContextChanged,
};

/** Editable Unit configuration; use an applied snapshot when reading measurements. */
struct UWB2ProConfig {
    UWB2ProApp app   = UWB2ProApp::Ranging;
    UWB2ProRole role = UWB2ProRole::Tag;
    // Ranging: Anchor count for a Tag, or Anchor ID for an Anchor (1-9).
    // Positioning Anchor: 1=O, 2=X, 3=Y, 4=Z; a positioning Tag's value mirrors xMeters.
    uint8_t value            = 1;
    UWB2ProComm comm         = UWB2ProComm::UART;
    bool bootApp             = false;
    UWB2ProLED led           = UWB2ProLED::Auto;
    uint8_t group            = 1;
    bool valid               = false;
    UWB2ProDataRate dataRate = UWB2ProDataRate::Rate850K;
    // Positioning Tag layout in whole metres (1-9); unused axes are zero.
    uint8_t xMeters    = 0;
    uint8_t yMeters    = 0;
    uint8_t zMeters    = 0;
    uint8_t i2cAddress = 0;  // Zero when the current transport cannot query it.
};

/** Distance in centimetres. Use the number only when state is Valid. */
struct UWB2ProDistance {
    int32_t centimeters = INT32_MIN;
    UWB2ProState state  = UWB2ProState::NoData;
};

/** Decoded measurements; missing numbers use INT32_MIN or INT16_MIN. */
struct UWB2ProData {
    // Ranging IDs 1-9 map to indices 0-8; positioning uses O/X/Y/Z at 0-3.
    UWB2ProDistance anchors[9];
    // Signed coordinates in centimetres; positionState governs all three axes.
    int16_t xCentimeters       = INT16_MIN;
    int16_t yCentimeters       = INT16_MIN;
    int16_t zCentimeters       = INT16_MIN;
    UWB2ProState positionState = UWB2ProState::NoData;
};

enum class UWB2ProIOResult : uint8_t { Ok, Timeout, Failure };

/**
 * Custom transport callbacks. exchange() returns a bounded, null-terminated
 * response including its terminal OK/ERROR line. I2C reads use repeated START;
 * register reads/writes transfer exactly 1-16 bytes. Callbacks must honor timeouts.
 * After a successful attachment, end() invokes destroy(context) when supplied.
 */
struct UWB2ProIO {
    void* context                                                              = nullptr;
    UWB2ProIOResult (*exchange)(void*, const char*, char*, size_t, uint32_t)   = nullptr;
    UWB2ProIOResult (*read)(void*, uint8_t, uint8_t*, size_t, uint32_t)        = nullptr;
    UWB2ProIOResult (*write)(void*, uint8_t, const uint8_t*, size_t, uint32_t) = nullptr;
    void (*delayMs)(void*, uint32_t)                                           = nullptr;
    void (*destroy)(void*)                                                     = nullptr;

    UWB2ProIO() = default;
    UWB2ProIO(void* ioContext, UWB2ProIOResult (*ioExchange)(void*, const char*, char*, size_t, uint32_t),
              UWB2ProIOResult (*ioRead)(void*, uint8_t, uint8_t*, size_t, uint32_t),
              UWB2ProIOResult (*ioWrite)(void*, uint8_t, const uint8_t*, size_t, uint32_t),
              void (*ioDelayMs)(void*, uint32_t), void (*ioDestroy)(void*))
        : context(ioContext), exchange(ioExchange), read(ioRead), write(ioWrite), delayMs(ioDelayMs), destroy(ioDestroy)
    {
    }
};

/**
 * Owns one UART or I2C connection. Read/query methods leave caller outputs
 * unchanged on failure. Inspect lastErrorCode()/lastError() after a false result.
 */
class M5UnitUWB2Pro {
public:
    M5UnitUWB2Pro();
    ~M5UnitUWB2Pro();
    M5UnitUWB2Pro(const M5UnitUWB2Pro&)            = delete;
    M5UnitUWB2Pro& operator=(const M5UnitUWB2Pro&) = delete;

    // Starts and owns a UART at 115200-8N-1; pins are host RX/TX, timeout is ms.
    bool beginUART(HardwareSerial& serial, int8_t rxPin, int8_t txPin, uint32_t timeoutMs = 800);

    // Starts and owns TwoWire. Address is 7-bit (0x08-0x77), clockHz is
    // 100000 or 400000 Hz, and timeoutMs is the per-operation timeout.
    // Current ESP32 cores also get a bounded 12 ms clock-stretch allowance.
    bool beginI2C(TwoWire& wire, int8_t sdaPin, int8_t sclPin, uint8_t address = M5_UNIT_UWB2_PRO_DEFAULT_I2C_ADDRESS,
                  uint32_t clockHz = 100000, uint32_t timeoutMs = 800);

    // Attaches a custom/shared transport. The address is required for I2C.
    // The caller retains responsibility for callback context on attachment failure.
    bool beginWithIO(UWB2ProComm comm, const UWB2ProIO& io, uint8_t address = M5_UNIT_UWB2_PRO_DEFAULT_I2C_ADDRESS,
                     uint32_t timeoutMs = 800);
    // Releases the connection, including the supplied transport destroy callback.
    void end();

    // Tests communication with AT (UART) or the version register (I2C).
    // Does not identify a model; select a known compatible endpoint before probing.
    bool probe();
    // Optional model query: UART returns the reported name; I2C maps BOARD 0/1
    // to UWB2-Pro/UWB Angle-Pro. String sizes include the terminating null character.
    bool getBoard(char* output, size_t size);
    bool getVersion(char* output, size_t size);
    // UART only; output needs at least 18 bytes for XX:XX:XX:XX:XX:XX and null.
    bool getMAC(char* output, size_t size);
    // Returns the editable draft, which may differ from the running configuration.
    bool getConfig(UWB2ProConfig& output);
    // Read the actual APP/SETUP state without changing the draft or saved BOOT.
    // A running application may still report NoData, Timeout, or Error measurements.
    bool getAppRunning(bool& output);

    // Stage Ranging Tag/Anchor (value 1-9) or positioning Anchor (O/X/Y[/Z]).
    // Configure positioning Tags through setLayout().
    bool setApp(UWB2ProApp app, UWB2ProRole role, uint8_t value);
    // Stage a full positioning Tag layout. Axes are 1-9 metres; POS2D ignores Z.
    bool setLayout(UWB2ProApp app, uint8_t xMeters, uint8_t yMeters, uint8_t zMeters = 0);
    // Stage the radio group, in the range 1-9.
    bool setGroup(uint8_t group);
    bool setDataRate(UWB2ProDataRate rate);
    bool setComm(UWB2ProComm comm);  // Only UART and I2C are accepted.
    // LED changes immediately; save() persists it and cancel() restores it.
    bool setLED(UWB2ProLED mode);
    // Select automatic application startup (true) or setup mode (false) on boot.
    bool setBootApp(bool app);
    bool setI2CAddress(uint8_t address);  // I2C transport only; SAVE applies it.

    // Apply and persist the draft. After an interface/address change, call end(),
    // begin on the selected endpoint, and probe() to confirm the new connection.
    bool save();
    // Reload the saved draft, discarding unsaved changes, including LED mode.
    bool cancel();
    // Restore factory defaults (UART, address 0x64, setup mode); reconnect afterward.
    bool restore();
    // Supply a known applied configuration, not an unsaved draft. On a context
    // mismatch/change, refresh the applied settings and retry; output is unchanged.
    bool readData(const UWB2ProConfig& active, UWB2ProData& output);

    UWB2ProComm transport() const
    {
        return comm_;
    }
    uint8_t address() const
    {
        return address_;
    }
    UWB2ProError lastErrorCode() const
    {
        return errorCode_;
    }
    const char* lastError() const
    {
        return errorText_;
    }

private:
    bool fail(UWB2ProError code, const char* text);
    bool at(const char* command, const char* prefix = nullptr, char* value = nullptr, size_t size = 0);
    bool readRegister(uint8_t reg, uint8_t* data, size_t size);
    bool writeRegister(uint8_t reg, const uint8_t* data, size_t size, bool handover = false);
    bool setNumber(uint8_t reg, const char* atPrefix, uint8_t value, const char* atValue = nullptr);
    bool control(const char* atCommand, uint8_t i2cCommand);
    void clearError();

    UWB2ProComm comm_       = UWB2ProComm::UART;
    UWB2ProComm stagedComm_ = UWB2ProComm::UART;
    uint8_t address_        = 0;
    uint8_t stagedAddress_  = 0;
    uint32_t timeoutMs_     = 800;
    UWB2ProIO io_{};
    bool connected_         = false;
    UWB2ProError errorCode_ = UWB2ProError::NotConnected;
    char errorText_[64]     = "NOT CONNECTED";
};
