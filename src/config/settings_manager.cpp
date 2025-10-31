#include "settings_manager.h"
#include "logger.h"
#include <Windows.h>
#include <QCoreApplication>
#include <QDir>

SettingsManager::SettingsManager() : config_(Config::instance()) {
}

SettingsManager& SettingsManager::instance() {
    static SettingsManager instance;
    return instance;
}

void SettingsManager::initializeDefaults() {
    Logger::log("=== Initializing Settings Manager ===");
    
    // Always run migration to ensure all settings exist
    migrateSettings();
    
    Logger::log("=== Settings Manager Initialization Complete ===");
}

bool SettingsManager::isFirstRun() const {
    return config_.getHotkey().isEmpty();
}

void SettingsManager::migrateSettings() {
    bool needsSave = false;
    
    // Check each setting and apply default if missing
    if (!config_.contains("hotkey")) {
        config_.setHotkey("F1");
        needsSave = true;
        Logger::log("Added missing setting 'hotkey' with default value: F1");
    }
    
    if (!config_.contains("mainProcessOnly")) {
        config_.setMainProcessOnly(false);
        needsSave = true;
        Logger::log("Added missing setting 'mainProcessOnly' with default value: false");
    }
    
    if (!config_.contains("startupEnabled")) {
        config_.setStartupEnabled(false);
        needsSave = true;
        Logger::log("Added missing setting 'startupEnabled' with default value: false");
    }
    
    if (!config_.contains("startupMinimized")) {
        config_.setStartupMinimized(false);
        needsSave = true;
        Logger::log("Added missing setting 'startupMinimized' with default value: false");
    }
    
    if (!config_.contains("closeToTray")) {
        config_.setCloseToTray(true);
        needsSave = true;
        Logger::log("Added missing setting 'closeToTray' with default value: true");
    }
    
    if (!config_.contains("excludedDevices")) {
        config_.setExcludedDevices(QStringList());
        needsSave = true;
        Logger::log("Added missing setting 'excludedDevices' with default value: empty list");
    }
    
    if (!config_.contains("excludedProcesses")) {
        config_.setExcludedProcesses(QStringList());
        needsSave = true;
        Logger::log("Added missing setting 'excludedProcesses' with default value: empty list");
    }
    
    if (!config_.contains("darkMode")) {
        config_.setDarkMode(true); // Default to dark mode enabled
        needsSave = true;
        Logger::log("Added missing setting 'darkMode' with default value: true");
    }
    
    if (!config_.contains("showNotifications")) {
        config_.setShowNotifications(true); // Default to notifications enabled
        needsSave = true;
        Logger::log("Added missing setting 'showNotifications' with default value: true");
    }
    
    if (!config_.contains("autoUpdateCheck")) {
        config_.setAutoUpdateCheck(true); // Default to auto-update enabled
        needsSave = true;
        Logger::log("Added missing setting 'autoUpdateCheck' with default value: true");
    }
    
    if (!config_.contains("useHook")) {
        config_.setUseHook(false); // Default to normal hotkey registration
        needsSave = true;
        Logger::log("Added missing setting 'useHook' with default value: false");
    }
    
    if (!config_.contains("volumeControlEnabled")) {
        config_.setVolumeControlEnabled(false); // Default to disabled
        needsSave = true;
        Logger::log("Added missing setting 'volumeControlEnabled' with default value: false");
    }
    
    if (!config_.contains("volumeUpHotkey")) {
        config_.setVolumeUpHotkey(""); // Default to empty
        needsSave = true;
        Logger::log("Added missing setting 'volumeUpHotkey' with default value: empty");
    }
    
    if (!config_.contains("volumeDownHotkey")) {
        config_.setVolumeDownHotkey(""); // Default to empty
        needsSave = true;
        Logger::log("Added missing setting 'volumeDownHotkey' with default value: empty");
    }
    
    if (!config_.contains("volumeStepPercent")) {
        config_.setVolumeStepPercent(5.0); // Default to 5%
        needsSave = true;
        Logger::log("Added missing setting 'volumeStepPercent' with default value: 5.0");
    }
    
    if (!config_.contains("volumeControlShowOSD")) {
        config_.setVolumeControlShowOSD(true); // Default to enabled
        needsSave = true;
        Logger::log("Added missing setting 'volumeControlShowOSD' with default value: true");
    }
    
    if (!config_.contains("volumeOSDPosition")) {
        config_.setVolumeOSDPosition("Center"); // Default to center
        needsSave = true;
        Logger::log("Added missing setting 'volumeOSDPosition' with default value: Center");
    }
    
    if (!config_.contains("volumeOSDCustomX")) {
        config_.setVolumeOSDCustomX(-1); // Default to -1 (not set)
        needsSave = true;
        Logger::log("Added missing setting 'volumeOSDCustomX' with default value: -1");
    }
    
    if (!config_.contains("volumeOSDCustomY")) {
        config_.setVolumeOSDCustomY(-1); // Default to -1 (not set)
        needsSave = true;
        Logger::log("Added missing setting 'volumeOSDCustomY' with default value: -1");
    }
    
    if (needsSave) {
        config_.save();
        Logger::log("Settings migration completed - new defaults applied");
    } else {
        Logger::log("Settings migration completed - all settings already exist");
    }
}

QString SettingsManager::getHotkey() const {
    return config_.getHotkey();
}

bool SettingsManager::getMainProcessOnly() const {
    return config_.getMainProcessOnly();
}

bool SettingsManager::getStartupEnabled() const {
    return config_.getStartupEnabled();
}

bool SettingsManager::getStartupMinimized() const {
    return config_.getStartupMinimized();
}

bool SettingsManager::getCloseToTray() const {
    return config_.getCloseToTray();
}

bool SettingsManager::getDarkMode() const {
    return config_.getDarkMode();
}

bool SettingsManager::getShowNotifications() const {
    return config_.getShowNotifications();
}

bool SettingsManager::getAutoUpdateCheck() const {
    return config_.getAutoUpdateCheck();
}

bool SettingsManager::getUseHook() const {
    return config_.getUseHook();
}

bool SettingsManager::getVolumeControlEnabled() const {
    return config_.getVolumeControlEnabled();
}

QString SettingsManager::getVolumeUpHotkey() const {
    return config_.getVolumeUpHotkey();
}

QString SettingsManager::getVolumeDownHotkey() const {
    return config_.getVolumeDownHotkey();
}

float SettingsManager::getVolumeStepPercent() const {
    return config_.getVolumeStepPercent();
}

bool SettingsManager::getVolumeControlShowOSD() const {
    return config_.getVolumeControlShowOSD();
}

QString SettingsManager::getVolumeOSDPosition() const {
    return config_.getVolumeOSDPosition();
}

int SettingsManager::getVolumeOSDCustomX() const {
    return config_.getVolumeOSDCustomX();
}

int SettingsManager::getVolumeOSDCustomY() const {
    return config_.getVolumeOSDCustomY();
}

QStringList SettingsManager::getExcludedDevices() const {
    return config_.getExcludedDevices();
}

QStringList SettingsManager::getExcludedProcesses() const {
    return config_.getExcludedProcesses();
}

void SettingsManager::setHotkey(const QString& hotkey) {
    config_.setHotkey(hotkey);
    emit settingsChanged();
}

void SettingsManager::setMainProcessOnly(bool enabled) {
    config_.setMainProcessOnly(enabled);
    emit settingsChanged();
}

void SettingsManager::setStartupEnabled(bool enabled) {
    config_.setStartupEnabled(enabled);
    setupStartupRegistry();
    emit settingsChanged();
}

void SettingsManager::setStartupMinimized(bool enabled) {
    config_.setStartupMinimized(enabled);
    emit settingsChanged();
}

void SettingsManager::setCloseToTray(bool enabled) {
    config_.setCloseToTray(enabled);
    emit settingsChanged();
}

void SettingsManager::setDarkMode(bool enabled) {
    config_.setDarkMode(enabled);
    emit settingsChanged();
}

void SettingsManager::setShowNotifications(bool enabled) {
    config_.setShowNotifications(enabled);
    emit settingsChanged();
}

void SettingsManager::setAutoUpdateCheck(bool enabled) {
    config_.setAutoUpdateCheck(enabled);
    emit settingsChanged();
}

void SettingsManager::setUseHook(bool enabled) {
    config_.setUseHook(enabled);
    emit settingsChanged();
}

void SettingsManager::setVolumeControlEnabled(bool enabled) {
    config_.setVolumeControlEnabled(enabled);
    emit settingsChanged();
}

void SettingsManager::setVolumeUpHotkey(const QString& hotkey) {
    config_.setVolumeUpHotkey(hotkey);
    emit settingsChanged();
}

void SettingsManager::setVolumeDownHotkey(const QString& hotkey) {
    config_.setVolumeDownHotkey(hotkey);
    emit settingsChanged();
}

void SettingsManager::setVolumeStepPercent(float stepPercent) {
    config_.setVolumeStepPercent(stepPercent);
    emit settingsChanged();
}

void SettingsManager::setVolumeControlShowOSD(bool enabled) {
    config_.setVolumeControlShowOSD(enabled);
    emit settingsChanged();
}

void SettingsManager::setVolumeOSDPosition(const QString& position) {
    config_.setVolumeOSDPosition(position);
    emit settingsChanged();
}

void SettingsManager::setVolumeOSDCustomX(int x) {
    config_.setVolumeOSDCustomX(x);
    emit settingsChanged();
}

void SettingsManager::setVolumeOSDCustomY(int y) {
    config_.setVolumeOSDCustomY(y);
    emit settingsChanged();
}

void SettingsManager::setExcludedDevices(const QStringList& devices) {
    config_.setExcludedDevices(devices);
    emit settingsChanged();
}

void SettingsManager::addExcludedDevice(const QString& device) {
    config_.addExcludedDevice(device);
    emit settingsChanged();
}

void SettingsManager::removeExcludedDevice(const QString& device) {
    config_.removeExcludedDevice(device);
    emit settingsChanged();
}

void SettingsManager::setExcludedProcesses(const QStringList& processes) {
    config_.setExcludedProcesses(processes);
    emit settingsChanged();
}

void SettingsManager::setupStartupRegistry() {
    bool startupEnabled = getStartupEnabled();
    QString appPath = QCoreApplication::applicationFilePath();
    
    // Convert to Windows path format
    appPath = QDir::toNativeSeparators(appPath);
    
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, 
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 
        0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
    
    if (result != ERROR_SUCCESS) {
        Logger::log("Failed to open registry key for startup settings");
        return;
    }
    
    if (startupEnabled) {
        // Add to startup
        std::wstring wAppPath = appPath.toStdWString();
        result = RegSetValueExW(hKey, L"MuteActiveWindow", 0, REG_SZ, 
            (const BYTE*)wAppPath.c_str(), 
            (wAppPath.length() + 1) * sizeof(wchar_t));
        
        if (result == ERROR_SUCCESS) {
            Logger::log("Successfully added to Windows startup");
        } else {
            Logger::log(QString("Failed to add to startup registry: %1").arg(result));
        }
    } else {
        // Remove from startup
        result = RegDeleteValueW(hKey, L"MuteActiveWindow");
        if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND) {
            Logger::log("Successfully removed from Windows startup");
        } else {
            Logger::log(QString("Failed to remove from startup registry: %1").arg(result));
        }
    }
    
    RegCloseKey(hKey);
}

void SettingsManager::save() {
    config_.save();
    emit settingsChanged();
} 