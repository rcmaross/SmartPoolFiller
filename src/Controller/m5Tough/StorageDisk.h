#pragma once
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <vector>

class StorageDisk {
private:
    // ─────────────────────────────────────────────────────────────────
    // Private RAII Scoped Mount Engine
    // ─────────────────────────────────────────────────────────────────
    class ScopedMount {
    private:
        bool _mounted = false;
        int _cs_pin;
        const char* _mount_point;
    public:
        // Constructor: Automatically cleans stale flags and locks the hardware at 4MHz
        ScopedMount(int csPin, const char* mountPoint) : _cs_pin(csPin), _mount_point(mountPoint) {
            SD.end();
            delay(5);
            // Initializing with format_if_empty=true, max_files=5 to match your boot config
            _mounted = SD.begin(_cs_pin, SPI, 4000000, _mount_point, 5, true);
        }

        // Destructor: Automatically triggers when the block or function scope exits
        ~ScopedMount() {
            SD.end();
        }

        bool isReady() const { return _mounted; }
    };

protected:
    const char* mount_point;
    const int sd_cs_pin;
    bool card_is_present; 
    const char* app_dir = "/SmartPoolFiller";
    inline static StorageDisk *_instance = nullptr;
    // Helper utility to combine our folder with the dynamic filename
    void getCsvFilePath(int rtc_year, char* dest_buf, size_t buf_size) {
        snprintf(dest_buf, buf_size, "%s/history_%04d.csv", app_dir, rtc_year);
    }

public:
    StorageDisk() : mount_point("/sd"), sd_cs_pin(4), card_is_present(false) {
        if (_instance) assert(false);
        _instance = this;
    }

    inline static StorageDisk* getInstance() {
        return _instance;
    }

    class ActiveFileStream {
    private:
        StorageDisk::ScopedMount _mount; // 🚀 Holds the SPI bus open first!
        File _file;
    public:
        // Constructor: Automatically mounts the hardware and opens the path target
        ActiveFileStream(int csPin, const char* mountPoint, const String& fullPath, const char* mode = FILE_READ) 
            : _mount(csPin, mountPoint) {
            if (_mount.isReady()) {
                _file = SD.open(fullPath, mode);
            }
        }
        
        // Destructor: Triggers automatically when this stream object goes out of scope!
        // Closes the file handle, then _mount destructor automatically fires SD.end().
        ~ActiveFileStream() {
            if (_file) _file.close();
        }

        bool isOpen() const { return (_file == true); }
        File& getFile() { return _file; }
    };

    // Checks for containment safely by spinning up a brief scoped mount pass
    bool fileExistsInApp(const String& filename) const {
        ScopedMount mount(sd_cs_pin, mount_point);
        if (!mount.isReady()) return false;
        
        char fullPath[64] = {0};
        snprintf(fullPath, sizeof(fullPath), "%s/%s", app_dir, filename.c_str());
        return SD.exists(fullPath);
    } 

    // Factory method to generate a live, self-managed Directory file loop stream
    ActiveFileStream openAppDirectoryStream() {
        return ActiveFileStream(sd_cs_pin, mount_point, String(app_dir), FILE_READ);
    }

    // Factory method to generate a live, self-managed File reading stream
    ActiveFileStream openFileReadStream(const String& filename) {
        char fullPath[64] = {0};
        snprintf(fullPath, sizeof(fullPath), "%s/%s", app_dir, filename.c_str());
        return ActiveFileStream(sd_cs_pin, mount_point, String(fullPath), FILE_READ);
    }

    bool initMicroSDCard() {
        Serial.printf("[STORAGE] Mounting MicroSD slot under path: \"%s\"\n", mount_point);
        
        // Scope block to verify and initialize directories
        {
            ScopedMount mount(sd_cs_pin, mount_point);
            card_is_present = mount.isReady();
            
            if (!card_is_present) {
                Serial.println("[ERROR] MicroSD hardware mount failed at boot!");
                return false;
            }

            if (!SD.exists(app_dir)) {
                if (SD.mkdir(app_dir)) {
                    Serial.printf("[STORAGE] Created dedicated application directory: %s\n", app_dir);
                } else {
                    Serial.printf("[ERROR] Failed to create application directory: %s\n", app_dir);
                    card_is_present = false;
                    return false;
                }
            }
        }
        
        Serial.println("[STORAGE] Native storage interface verified and isolated.");
        return true;
    }

    bool isPresent() const { 
        return card_is_present; 
    }

    float getCardSizeGB() const {
        ScopedMount mount(sd_cs_pin, mount_point);
        if (!mount.isReady()) return 0.0f;
        
        uint64_t total_bytes = SD.totalBytes();
        return (float)total_bytes / (1024.0f * 1024.0f * 1024.0f);
    } // 🚀 Auto-unmounts SD upon leaving

    float getCardFreeSpaceGB() const {
        ScopedMount mount(sd_cs_pin, mount_point);
        if (!mount.isReady()) return 0.0f;
        
        uint64_t free_bytes = SD.totalBytes() - SD.usedBytes();
        return (float)free_bytes / (1024.0f * 1024.0f * 1024.0f);
    }

    bool removeFileFromApp(const String& filename) {
        // Automatically wake up and mount the card using our safe scoped manager pattern
        ScopedMount mount(sd_cs_pin, mount_point);
        if (!mount.isReady()) return false;
        
        char fullPath[64] = {0};
        snprintf(fullPath, sizeof(fullPath), "%s/%s", app_dir, filename.c_str());
        
        if (SD.exists(fullPath)) {
            return SD.remove(fullPath);
        }
        return false;
    }
    
    bool eraseAppDirectory() {
        ScopedMount mount(sd_cs_pin, mount_point);
        if (!mount.isReady()) {
            Serial.println("[ERROR] Aborting erase sequence: Hardware unreachable.");
            return false;
        }

        Serial.printf("[STORAGE] Sweeping all files within application folder: %s\n", app_dir);
        File dir = SD.open(app_dir);
        if (!dir || !dir.isDirectory()) {
            Serial.printf("[ERROR] Failed to open application directory path: %s\n", app_dir);
            return false;
        }

        std::vector<String> filesToDelete;

        // Phase 1: Collect absolute path handles safely
        File file = dir.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                filesToDelete.push_back(String(file.path()));
            }
            file.close(); 
            file = dir.openNextFile();
        }
        dir.close(); // 🚀 Unlock sector table before erasing

        // Phase 2: Loop across the collected strings to delete files safely
        bool overallSuccess = true;
        for (const String& full_path : filesToDelete) {
            Serial.printf("[STORAGE] Purging application file asset: %s\n", full_path.c_str());
            if (!SD.remove(full_path.c_str())) {
                Serial.printf("[WARNING] Failed to remove asset: %s\n", full_path.c_str());
                overallSuccess = false;
            }
        }

        if (overallSuccess) {
            Serial.println("[STORAGE] Application subdirectory wiped successfully.");
        }
        return overallSuccess;
    }

    void logHourlyRowToSD(int rtc_year, const char* timestamp, int system_id, float median_depth, float instant_depth, int valve_mins, int command_state) {
        ScopedMount mount(sd_cs_pin, mount_point);
        if (!mount.isReady()) {
            Serial.println("[STORAGE ERROR] Card remains thermally locked out. Skipping hour.");
            return; 
        }

        char path_buf[64] = {0};
        getCsvFilePath(rtc_year, path_buf, sizeof(path_buf));

        bool fileExists = SD.exists(path_buf);
        File logFile = SD.open(path_buf, FILE_APPEND);
        if (!logFile) {
            Serial.printf("[ERROR] Failed to open tracking file at path: %s\n", path_buf);
            return;
        }

        if (!fileExists) {
            logFile.println("Timestamp,SystemID,MedianDepth_in,InstantDepth_in,ValveRun_mins,CommandState");
            Serial.printf("[STORAGE] Created fresh rotated log file structure: %s\n", path_buf);
        }

        logFile.printf("%s,%d,%.2f,%.2f,%d,%d\n", timestamp, system_id, median_depth, instant_depth, valve_mins, command_state);
        logFile.flush();
        logFile.close();

        Serial.printf("[STORAGE] (%d) Row committed with command state integer to: %s\n", rtc_year, path_buf);
    } 
};
