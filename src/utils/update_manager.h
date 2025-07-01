#pragma once

#include <QObject>
#include <QString>
#include <QProcess>

class UpdateManager : public QObject {
    Q_OBJECT

public:
    static UpdateManager& instance();
    
    // Check if update checker is available
    bool isUpdateCheckerAvailable() const;
    
    // Get the path to the update checker
    QString getUpdateCheckerPath() const;
    
    // Launch the update checker
    void checkForUpdates();
    
    // Check availability and return the result
    bool checkAvailability();

private:
    UpdateManager();
    ~UpdateManager() = default;
    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;
    
    QString updateCheckerPath_;
    bool updateCheckerAvailable_;
    
    void updateAvailability();
}; 