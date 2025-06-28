#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include "config.h"

class SettingsManager : public QObject {
    Q_OBJECT

public:
    static SettingsManager& instance();
    
    // Initialize settings with defaults
    void initializeDefaults();
    
    // Check if this is first run
    bool isFirstRun() const;
    
    // Get current settings
    QString getHotkey() const;
    bool getMainProcessOnly() const;
    bool getStartupEnabled() const;
    bool getStartupMinimized() const;
    bool getCloseToTray() const;
    bool getDarkMode() const;
    QStringList getExcludedDevices() const;
    QStringList getExcludedProcesses() const;
    
    // Set settings
    void setHotkey(const QString& hotkey);
    void setMainProcessOnly(bool enabled);
    void setStartupEnabled(bool enabled);
    void setStartupMinimized(bool enabled);
    void setCloseToTray(bool enabled);
    void setDarkMode(bool enabled);
    void setExcludedDevices(const QStringList& devices);
    void addExcludedDevice(const QString& device);
    void removeExcludedDevice(const QString& device);
    void setExcludedProcesses(const QStringList& processes);
    
    // Registry operations
    void setupStartupRegistry();
    
    // Save all settings
    void save();
    
    // Get config instance for direct access if needed
    Config& getConfig() { return config_; }

signals:
    void settingsChanged();

private:
    SettingsManager();
    ~SettingsManager() = default;
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;
    
    void migrateSettings();
    
    Config& config_;
}; 