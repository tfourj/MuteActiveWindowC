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
    
    // Notification settings
    bool getShowNotifications() const;
    void setShowNotifications(bool enabled);
    
    // Auto-update settings
    bool getAutoUpdateCheck() const;
    void setAutoUpdateCheck(bool enabled);
    
    // Hook-based hotkey settings
    bool getUseHook() const;
    void setUseHook(bool enabled);

    // Admin restart hotkey settings
    bool getAdminRestartHotkeyEnabled() const;
    void setAdminRestartHotkeyEnabled(bool enabled);
    QString getAdminRestartHotkey() const;
    void setAdminRestartHotkey(const QString& hotkey);
    
    // Volume control settings
    bool getVolumeControlEnabled() const;
    void setVolumeControlEnabled(bool enabled);
    QString getVolumeUpHotkey() const;
    void setVolumeUpHotkey(const QString& hotkey);
    QString getVolumeDownHotkey() const;
    void setVolumeDownHotkey(const QString& hotkey);
    float getVolumeStepPercent() const;
    void setVolumeStepPercent(float stepPercent);
    bool getVolumeControlShowOSD() const;
    void setVolumeControlShowOSD(bool enabled);
    QString getVolumeOSDPosition() const;
    void setVolumeOSDPosition(const QString& position);
    int getVolumeOSDCustomX() const;
    void setVolumeOSDCustomX(int x);
    int getVolumeOSDCustomY() const;
    void setVolumeOSDCustomY(int y);
    
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
