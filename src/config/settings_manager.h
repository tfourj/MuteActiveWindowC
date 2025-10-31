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
    bool getShowNotifications() const;
    QStringList getExcludedDevices() const;
    QStringList getExcludedProcesses() const;
    bool getAutoUpdateCheck() const;
    bool getUseHook() const;
    bool getVolumeControlEnabled() const;
    QString getVolumeUpHotkey() const;
    QString getVolumeDownHotkey() const;
    float getVolumeStepPercent() const;
    bool getVolumeControlShowOSD() const;
    QString getVolumeOSDPosition() const;
    int getVolumeOSDCustomX() const;
    int getVolumeOSDCustomY() const;
    
    // Set settings
    void setHotkey(const QString& hotkey);
    void setMainProcessOnly(bool enabled);
    void setStartupEnabled(bool enabled);
    void setStartupMinimized(bool enabled);
    void setCloseToTray(bool enabled);
    void setDarkMode(bool enabled);
    void setShowNotifications(bool enabled);
    void setAutoUpdateCheck(bool enabled);
    void setUseHook(bool enabled);
    void setVolumeControlEnabled(bool enabled);
    void setVolumeUpHotkey(const QString& hotkey);
    void setVolumeDownHotkey(const QString& hotkey);
    void setVolumeStepPercent(float stepPercent);
    void setVolumeControlShowOSD(bool enabled);
    void setVolumeOSDPosition(const QString& position);
    void setVolumeOSDCustomX(int x);
    void setVolumeOSDCustomY(int y);
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