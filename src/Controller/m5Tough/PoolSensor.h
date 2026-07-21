#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h> // 🚀 RESTORED: Official Adafruit driver abstraction
#include "SystemState.h"

// --- The Core Abstract Sensor Interface ---
class PoolSensor {
public:
    virtual ~PoolSensor() {}
    
    virtual void begin() = 0;
    virtual void update() = 0;
    virtual float getVoltage() = 0;
    virtual bool isHardwarePresent() = 0;
    virtual bool isFaulted() = 0;

    // Static helper method allows main.ino to safely check the I2C bus 
    // without leaking dangling driver structures into global scope
    static bool probeHardware(TwoWire &wire) {
        Adafruit_ADS1115 verificationAds;
        // Returns true if the chip successfully responds on address 0x48
        return verificationAds.begin(0x48, &wire);
    }
};

// ---------------------------------------------------------------------
// CLASS 1: Physical Hardware Layer (ADS1115 + ISO1540 Watchdog)
// ---------------------------------------------------------------------
class PhysicalAdcSensor : public PoolSensor {
private:
    Adafruit_ADS1115 _ads;     // 🚀 RESTORED: Local encapsulated library driver instance
    float    _currentVoltage;
    bool     _hwPresentAtBoot;
    bool     _isCurrentlyFaulted;
    int16_t  _lastRawAdc;
    uint8_t  _frozenCounter;
    const uint8_t _frozenLimit = 5;
    TwoWire &_wire;

public:
    PhysicalAdcSensor(TwoWire &wire) :
        _wire(wire), 
        _currentVoltage(0.0f), 
        _hwPresentAtBoot(false), 
        _isCurrentlyFaulted(false), 
        _lastRawAdc(-9999), 
        _frozenCounter(0) {}

    void begin() override {
        Serial.println("ads.begin() started");
        
        // Pass the designated I2C instance directly into the library's init pipeline
        if (!_ads.begin(0x48, &_wire)) {
            _hwPresentAtBoot = false;
            _isCurrentlyFaulted = true;
            _currentVoltage = 0.0f;
            Serial.println("ads.begin() failed - Library could not establish handshake");
        } else {
            Serial.println("ads.begin() succeeded - Hardware paired");
            
            // Enforce explicit GAIN_ONE scaling ceiling (Targets +/- 4.096V maximum range)
            _ads.setGain(GAIN_ONE); 
            Serial.println("ads.setGain configured to GAIN_ONE (+/-4.096V)");
            
            _hwPresentAtBoot = true;
            _isCurrentlyFaulted = false;
            
            // Execute the initial baseline telemetry read via standard driver calls
            int16_t primaryRead = _ads.readADC_SingleEnded(0);
            Serial.println("ads.readADC_SingleEnded(0) verified");
            
            _currentVoltage = _ads.computeVolts(primaryRead);
            Serial.println("ads.computeVolts() calculation verified");
        }
    }

    void update() override {
        // Guard check: Lock out if hardware was completely absent at power up
        if (!_hwPresentAtBoot) {
            _isCurrentlyFaulted = true;
            _currentVoltage = 0.0f;
            return;
        }

        // 1. Physical Bus Telemetry Check
        _wire.beginTransmission(0x48);
        uint8_t i2cError = _wire.endTransmission();

        // If the device on address 0x48 does not return a clean ACK (0 = Success)
        if (i2cError != 0) {
            if (!_isCurrentlyFaulted) {
                Serial.printf("[SENSOR HARDWARE FAULT] Physical I2C Bus disconnected! Wire Error Code: %d\n", i2cError);
            }
            _isCurrentlyFaulted = true;
            _currentVoltage = 0.0f; // Force a cold safe floor fallback
            return; // STOP execution immediately on this loop iteration
        }

        // 2. Data Acquisition
        int16_t raw0 = _ads.readADC_SingleEnded(0);

        // 3. Digital Output Freeze Detection Watchdog
        if (raw0 == _lastRawAdc && raw0 != 0) {
            _frozenCounter++;
        } else {
            _frozenCounter = 0;
            _lastRawAdc = raw0;
        }
        _currentVoltage = _ads.computeVolts(raw0);

        // Intercept corrupt or internal library artifact error bounds (e.g., timeouts)
        if (_currentVoltage < 0.4f || _frozenCounter >= _frozenLimit) {
            if (!_isCurrentlyFaulted) {
                Serial.printf("[SENSOR FAULT] Corrupted or frozen registration data packet received: %d\n", raw0);
            }
            _isCurrentlyFaulted = true;
            _currentVoltage = 0.0f; 
        } else {
            // Hot Recovery Path: If the hardware comes back online, recover smoothly
            if (_isCurrentlyFaulted) {
                Serial.println(F("[SENSOR RECOVERY] ADS1115 live signaling successfully re-established."));
                _isCurrentlyFaulted = false;
            }
        }
    }
    
    float getVoltage() override        { return _currentVoltage; }
    bool  isHardwarePresent() override { return _hwPresentAtBoot; }
    bool  isFaulted() override         { return _isCurrentlyFaulted; }
};

// ---------------------------------------------------------------------
// CLASS 2: Simulated Stream Layer (Serial CLI Controls)
// ---------------------------------------------------------------------
class SimulatedSerialSensor : public PoolSensor {
private:
    float _simulatedVoltage;

public:
    SimulatedSerialSensor() : _simulatedVoltage(0.0f) {}

    void begin() override {
        Serial.println(F("[SENSOR] Simulated Interface Engine Mounted. Ready for voltages:"));
        _simulatedVoltage = sysState->full_volts;
    }

    void update() override {
        if (Serial.available() > 0) {
            String inputStr = Serial.readStringUntil('\n');
            inputStr.trim();
            if (inputStr.length() > 0) {
                _simulatedVoltage = inputStr.toFloat();
                Serial.printf("[SIMULATOR] Terminal voltage input verified: %.3f V\n", _simulatedVoltage);
            }
        }
    }

    float getVoltage() override        { return _simulatedVoltage; }
    bool  isHardwarePresent() override { return true; }
    bool  isFaulted() override         { return false; } 
};
