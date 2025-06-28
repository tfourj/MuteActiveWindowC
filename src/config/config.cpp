#include "config.h"
#include "logger.h"

Config::Config() : settings_("TfourJ", "MuteActiveWindow") {
}

Config& Config::instance() {
    static Config instance;
    return instance;
}

QString Config::getHotkey() const {
    return settings_.value("hotkey", "F1").toString();
}

void Config::setHotkey(const QString& hotkey) {
    settings_.setValue("hotkey", hotkey);
    Logger::log(QString("Hotkey saved: %1").arg(hotkey));
}

QStringList Config::getExcludedDevices() const {
    return settings_.value("excludedDevices", QStringList()).toStringList();
}

void Config::setExcludedDevices(const QStringList& devices) {
    settings_.setValue("excludedDevices", devices);
    Logger::log(QString("Excluded devices saved: %1").arg(devices.join(", ")));
}

void Config::addExcludedDevice(const QString& device) {
    QStringList devices = getExcludedDevices();
    if (!devices.contains(device)) {
        devices.append(device);
        setExcludedDevices(devices);
        Logger::log(QString("Added excluded device: %1").arg(device));
    }
}

void Config::removeExcludedDevice(const QString& device) {
    QStringList devices = getExcludedDevices();
    if (devices.removeOne(device)) {
        setExcludedDevices(devices);
        Logger::log(QString("Removed excluded device: %1").arg(device));
    }
}

bool Config::isDeviceExcluded(const QString& device) const {
    return getExcludedDevices().contains(device, Qt::CaseInsensitive);
}

QStringList Config::getExcludedProcesses() const {
    return settings_.value("excludedProcesses", QStringList()).toStringList();
}

void Config::setExcludedProcesses(const QStringList& processes) {
    settings_.setValue("excludedProcesses", processes);
    Logger::log(QString("Excluded processes saved: %1").arg(processes.join(", ")));
}

bool Config::isProcessExcluded(const QString& process) const {
    return getExcludedProcesses().contains(process, Qt::CaseInsensitive);
}

bool Config::getMainProcessOnly() const {
    return settings_.value("mainProcessOnly", false).toBool();
}

void Config::setMainProcessOnly(bool enabled) {
    settings_.setValue("mainProcessOnly", enabled);
    Logger::log(QString("Main process only setting saved: %1").arg(enabled ? "enabled" : "disabled"));
}

bool Config::getStartupEnabled() const {
    return settings_.value("startupEnabled", false).toBool();
}

void Config::setStartupEnabled(bool enabled) {
    settings_.setValue("startupEnabled", enabled);
    Logger::log(QString("Startup enabled setting saved: %1").arg(enabled ? "enabled" : "disabled"));
}

bool Config::getStartupMinimized() const {
    return settings_.value("startupMinimized", false).toBool();
}

void Config::setStartupMinimized(bool enabled) {
    settings_.setValue("startupMinimized", enabled);
    Logger::log(QString("Startup minimized setting saved: %1").arg(enabled ? "enabled" : "disabled"));
}

bool Config::getCloseToTray() const {
    return settings_.value("closeToTray", true).toBool();
}

void Config::setCloseToTray(bool enabled) {
    settings_.setValue("closeToTray", enabled);
    Logger::log(QString("Close to tray setting saved: %1").arg(enabled ? "enabled" : "disabled"));
}

bool Config::getDarkMode() const {
    return settings_.value("darkMode", true).toBool(); // Default to true (dark mode on)
}

void Config::setDarkMode(bool enabled) {
    settings_.setValue("darkMode", enabled);
    Logger::log(QString("Dark mode setting saved: %1").arg(enabled ? "enabled" : "disabled"));
}

void Config::save() {
    settings_.sync();
    Logger::log("All settings saved to registry");
    
    // Log all current settings for debugging
    Logger::log(QString("Current settings:"));
    Logger::log(QString("  Hotkey: %1").arg(getHotkey()));
    Logger::log(QString("  Main process only: %1").arg(getMainProcessOnly() ? "true" : "false"));
    Logger::log(QString("  Startup enabled: %1").arg(getStartupEnabled() ? "true" : "false"));
    Logger::log(QString("  Startup minimized: %1").arg(getStartupMinimized() ? "true" : "false"));
    Logger::log(QString("  Close to tray: %1").arg(getCloseToTray() ? "true" : "false"));
    Logger::log(QString("  Dark mode: %1").arg(getDarkMode() ? "enabled" : "disabled"));
    Logger::log(QString("  Excluded devices: %1").arg(getExcludedDevices().join(", ")));
    Logger::log(QString("  Excluded processes: %1").arg(getExcludedProcesses().join(", ")));
}

bool Config::contains(const QString& key) const {
    return settings_.contains(key);
} 
