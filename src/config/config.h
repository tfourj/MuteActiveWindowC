#pragma once
#include <QString>
#include <QStringList>
#include <QSettings>

class Config {
public:
    static Config& instance();
    
    // Hotkey settings
    QString getHotkey() const;
    void setHotkey(const QString& hotkey);
    
    // Device exclusion settings
    QStringList getExcludedDevices() const;
    void setExcludedDevices(const QStringList& devices);
    void addExcludedDevice(const QString& device);
    void removeExcludedDevice(const QString& device);
    bool isDeviceExcluded(const QString& device) const;
    
    // Process exclusion settings
    QStringList getExcludedProcesses() const;
    void setExcludedProcesses(const QStringList& processes);
    bool isProcessExcluded(const QString& process) const;
    
    // Main process only setting
    bool getMainProcessOnly() const;
    void setMainProcessOnly(bool enabled);
    
    // Startup behavior settings
    bool getStartupEnabled() const;
    void setStartupEnabled(bool enabled);
    bool getStartupMinimized() const;
    void setStartupMinimized(bool enabled);
    
    // Tray behavior settings
    bool getCloseToTray() const;
    void setCloseToTray(bool enabled);
    
    // Dark mode setting
    bool getDarkMode() const;
    void setDarkMode(bool enabled);
    
    // Check if setting exists
    bool contains(const QString& key) const;
    
    // Save all settings
    void save();
    
private:
    Config();
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    QSettings settings_;
}; 
