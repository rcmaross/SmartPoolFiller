#pragma once

#include <SPI.h>
#include <SD.h>

class StorageDisk {
protected:
    const char* mount_point;
    const int sd_cs_pin;
    bool card_is_present;

    // 🚀 THE FIX: Defined the siloed application directory variable here!
    const char* app_dir = "/SmartPoolFiller";

    // Helper utility to safely combine our silo folder with the dynamic yearly filename
    void getCsvFilePath(int rtc_year, char* dest_buf, size_t buf_size) {
        // Updated to use app_dir instead of mount_point!
        snprintf(dest_buf, buf_size, "%s/history_%04d.csv", app_dir, rtc_year);
    }

public:
    StorageDisk() 
        : mount_point("/sd"), sd_cs_pin(4), card_is_present(false) {}

    StorageDisk(const char* mountPoint, int csPin) 
        : mount_point(mountPoint), sd_cs_pin(csPin), card_is_present(false) {}

    // ─────────────────────────────────────────────────────────────────
    // 1. HARDWARE STORAGE INITIALIZATION
    // ─────────────────────────────────────────────────────────────────
    bool initMicroSDCard() {
        Serial.printf("[STORAGE] Mounting MicroSD slot under path: \"%s\"\n", mount_point);
        card_is_present = SD.begin(sd_cs_pin, SPI, 40000000, mount_point, 5, true);

        if (!card_is_present) {
            Serial.println("[ERROR] MicroSD hardware mount failed!");
            return false;
        }

        Serial.println("[STORAGE] Native storage interface mounted and stable.");

        // 🚀 Ensure our isolated folder actually exists on the card at bootup!
        if (!SD.exists(app_dir)) {
            if (SD.mkdir(app_dir)) {
                Serial.printf("[STORAGE] Created dedicated application directory: %s\n", app_dir);
            } else {
                Serial.printf("[ERROR] Failed to create application directory: %s\n", app_dir);
                return false;
            }
        }
        return true;
    }

    bool isPresent() const {
        return card_is_present;
    }

    // ─────────────────────────────────────────────────────────────────
    // 2. THE SILOED STORAGE CLEANER (Wipes only our custom folder files)
    // ─────────────────────────────────────────────────────────────────
    bool eraseAppDirectory() {
        if (!card_is_present) {
            Serial.println("[ERROR] Aborting erase sequence: Storage offline.");
            return false;
        }

        Serial.printf("[STORAGE] Sweeping all files within application folder: %s\n", app_dir);

        File dir = SD.open(app_dir);
        if (!dir || !dir.isDirectory()) {
            Serial.printf("[ERROR] Failed to open application directory path: %s\n", app_dir);
            return false;
        }

        File file = dir.openNextFile();
        while (file) {
            if (file.isDirectory()) {
                file.close();
                file = dir.openNextFile();
                continue;
            }

            // 1. Grab the absolute path string token natively
            const char* full_path = file.path();

            Serial.printf("[STORAGE] Purging application file asset: %s\n", full_path);

            // 2. Close the file handler FIRST to unlock the sector descriptors before deleting!
            file.close();

            // 3. Delete the file using its path while the directory iterator remains stable
            if (!SD.remove(full_path)) {
                Serial.printf("[WARNING] Failed to remove asset: %s\n", full_path);
            }

            // 4. Safely advance the folder tracking cursor to the next item
            file = dir.openNextFile();
        }
        dir.close();

        Serial.println("[STORAGE] Application subdirectory wiped successfully.");
        return true;
    }

    float getCardSizeGB() const {
        if (!card_is_present) return 0.0f;
        uint64_t total_bytes = SD.totalBytes();
        return (float)total_bytes / (1024.0f * 1024.0f * 1024.0f);
    }

    float getCardFreeSpaceGB() const {
        if (!card_is_present) return 0.0f;
        uint64_t free_bytes = SD.totalBytes() - SD.usedBytes();
        return (float)free_bytes / (1024.0f * 1024.0f * 1024.0f);
    }

    // ─────────────────────────────────────────────────────────────────
    // 3. THE SAFE SELF-ROTATING DATA ROW APPENDER
    // ─────────────────────────────────────────────────────────────────
    void logHourlyRowToSD(int rtc_year, const char* timestamp, int system_id, float median_depth, float instant_depth, int valve_mins, int command_state) {
        if (!card_is_present) {
            Serial.println("Aborting write sequence: Storage hardware offline.");
            return;
        }

        char path_buf[64] = {0};
        getCsvFilePath(rtc_year, path_buf, sizeof(path_buf));

        // 🚀 STEP 1: Update column labels to reflect the integer state parameter
        if (!SD.exists(path_buf)) {
            File initFile = SD.open(path_buf, FILE_WRITE);
            if (initFile) {
                initFile.println("Timestamp,SystemID,MedianDepth_in,InstantDepth_in,ValveRun_mins,CommandState");
                initFile.close();
                Serial.printf("[STORAGE] Created fresh rotated log file structure: %s\n", path_buf);
            }
        }

        File logFile = SD.open(path_buf, FILE_APPEND);
        if (!logFile) {
            Serial.printf("[ERROR] Failed to open tracking file at path: %s\n", path_buf);
            return;
        }

        // 🚀 STEP 2: Log the command state as a lean integer variable (%d)
        logFile.printf("%s,%d,%.2f,%.2f,%d,%d\n", 
                        timestamp, system_id, median_depth, instant_depth, valve_mins, command_state);
        logFile.flush();
        logFile.close();

        Serial.printf("[STORAGE] (%d) Row committed with command state integer to: %s\n", rtc_year, path_buf);
    }
};
