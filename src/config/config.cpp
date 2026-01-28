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
    QStringList excludedProcesses = getExcludedProcesses();
    
    // Check exact match first
    if (excludedProcesses.contains(process, Qt::CaseInsensitive)) {
        return true;
    }
    
    // Check normalized match (remove .exe extension if present)
    QString normalizedProcess = process;
    if (normalizedProcess.endsWith(".exe", Qt::CaseInsensitive)) {
        normalizedProcess = normalizedProcess.left(normalizedProcess.length() - 4);
    }
    
    // Check if normalized process name is in the exclusion list
    for (const QString& excluded : excludedProcesses) {
        QString normalizedExcluded = excluded;
        if (normalizedExcluded.endsWith(".exe", Qt::CaseInsensitive)) {
            normalizedExcluded = normalizedExcluded.left(normalizedExcluded.length() - 4);
        }
        
        if (normalizedProcess.compare(normalizedExcluded, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    
    return false;
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

bool Config::getShowNotifications() const {
    return settings_.value("showNotifications", true).toBool(); // Default to true (notifications on)
}

void Config::setShowNotifications(bool enabled) {
    settings_.setValue("showNotifications", enabled);
    Logger::log(QString("Show notifications setting saved: %1").arg(enabled ? "enabled" : "disabled"));
}

bool Config::getAutoUpdateCheck() const {
    return settings_.value("autoUpdateCheck", true).toBool(); // Default to true (auto-update on)
}

void Config::setAutoUpdateCheck(bool enabled) {
    settings_.setValue("autoUpdateCheck", enabled);
    Logger::log(QString("Auto-update check setting saved: %1").arg(enabled ? "enabled" : "disabled"));
}

bool Config::getUseHook() const {
    return settings_.value("useHook", false).toBool(); // Default to false (normal hotkey registration)
}

void Config::setUseHook(bool enabled) {
    settings_.setValue("useHook", enabled);
    Logger::log(QString("Use hook setting saved: %1").arg(enabled ? "enabled" : "disabled"));
}

bool Config::getAdminRestartHotkeyEnabled() const {
    return settings_.value("adminRestartHotkeyEnabled", false).toBool();
}

void Config::setAdminRestartHotkeyEnabled(bool enabled) {
    settings_.setValue("adminRestartHotkeyEnabled", enabled);
    Logger::log(QString("Admin restart hotkey enabled setting saved: %1").arg(enabled ? "enabled" : "disabled"));
}

QString Config::getAdminRestartHotkey() const {
    return settings_.value("adminRestartHotkey", "").toString();
}

void Config::setAdminRestartHotkey(const QString& hotkey) {
    settings_.setValue("adminRestartHotkey", hotkey);
    Logger::log(QString("Admin restart hotkey saved: %1").arg(hotkey));
}

bool Config::getVolumeControlEnabled() const {
    return settings_.value("volumeControlEnabled", false).toBool();
}

void Config::setVolumeControlEnabled(bool enabled) {
    settings_.setValue("volumeControlEnabled", enabled);
    Logger::log(QString("Volume control enabled setting saved: %1").arg(enabled ? "enabled" : "disabled"));
}

QString Config::getVolumeUpHotkey() const {
    return settings_.value("volumeUpHotkey", "").toString();
}

void Config::setVolumeUpHotkey(const QString& hotkey) {
    settings_.setValue("volumeUpHotkey", hotkey);
    Logger::log(QString("Volume up hotkey saved: %1").arg(hotkey));
}

QString Config::getVolumeDownHotkey() const {
    return settings_.value("volumeDownHotkey", "").toString();
}

void Config::setVolumeDownHotkey(const QString& hotkey) {
    settings_.setValue("volumeDownHotkey", hotkey);
    Logger::log(QString("Volume down hotkey saved: %1").arg(hotkey));
}

float Config::getVolumeStepPercent() const {
    return settings_.value("volumeStepPercent", 5.0).toFloat();
}

void Config::setVolumeStepPercent(float stepPercent) {
    settings_.setValue("volumeStepPercent", stepPercent);
    Logger::log(QString("Volume step percent saved: %1").arg(stepPercent));
}

bool Config::getVolumeControlShowOSD() const {
    return settings_.value("volumeControlShowOSD", true).toBool();
}

void Config::setVolumeControlShowOSD(bool enabled) {
    settings_.setValue("volumeControlShowOSD", enabled);
    Logger::log(QString("Volume control show OSD setting saved: %1").arg(enabled ? "enabled" : "disabled"));
}

QString Config::getVolumeOSDPosition() const {
    return settings_.value("volumeOSDPosition", "Center").toString();
}

void Config::setVolumeOSDPosition(const QString& position) {
    settings_.setValue("volumeOSDPosition", position);
    Logger::log(QString("Volume OSD position saved: %1").arg(position));
}

int Config::getVolumeOSDCustomX() const {
    return settings_.value("volumeOSDCustomX", -1).toInt();
}

void Config::setVolumeOSDCustomX(int x) {
    settings_.setValue("volumeOSDCustomX", x);
    Logger::log(QString("Volume OSD custom X saved: %1").arg(x));
}

int Config::getVolumeOSDCustomY() const {
    return settings_.value("volumeOSDCustomY", -1).toInt();
}

void Config::setVolumeOSDCustomY(int y) {
    settings_.setValue("volumeOSDCustomY", y);
    Logger::log(QString("Volume OSD custom Y saved: %1").arg(y));
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
    Logger::log(QString("  Show notifications: %1").arg(getShowNotifications() ? "enabled" : "disabled"));
    Logger::log(QString("  Auto-update check: %1").arg(getAutoUpdateCheck() ? "enabled" : "disabled"));
    Logger::log(QString("  Use hook: %1").arg(getUseHook() ? "enabled" : "disabled"));
    Logger::log(QString("  Admin restart hotkey enabled: %1").arg(getAdminRestartHotkeyEnabled() ? "enabled" : "disabled"));
    Logger::log(QString("  Admin restart hotkey: %1").arg(getAdminRestartHotkey()));
    Logger::log(QString("  Volume control enabled: %1").arg(getVolumeControlEnabled() ? "enabled" : "disabled"));
    Logger::log(QString("  Volume up hotkey: %1").arg(getVolumeUpHotkey()));
    Logger::log(QString("  Volume down hotkey: %1").arg(getVolumeDownHotkey()));
    Logger::log(QString("  Volume step percent: %1").arg(getVolumeStepPercent()));
    Logger::log(QString("  Excluded devices: %1").arg(getExcludedDevices().join(", ")));
    Logger::log(QString("  Excluded processes: %1").arg(getExcludedProcesses().join(", ")));
}

bool Config::contains(const QString& key) const {
    return settings_.contains(key);
} 
