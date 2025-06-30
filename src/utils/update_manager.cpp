#include "update_manager.h"
#include "logger.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QWidget>

UpdateManager::UpdateManager() : updateCheckerAvailable_(false) {
    updateAvailability();
}

UpdateManager& UpdateManager::instance() {
    static UpdateManager instance;
    return instance;
}

bool UpdateManager::isUpdateCheckerAvailable() const {
    return updateCheckerAvailable_;
}

QString UpdateManager::getUpdateCheckerPath() const {
    return updateCheckerPath_;
}

bool UpdateManager::checkAvailability() {
    updateAvailability();
    return updateCheckerAvailable_;
}

void UpdateManager::updateAvailability() {
    // Get the application directory path
    QString appDir = QCoreApplication::applicationDirPath();
    updateCheckerPath_ = appDir + "/configure.exe";
    
    // Check if configure.exe exists
    QFileInfo configureFile(updateCheckerPath_);
    updateCheckerAvailable_ = configureFile.exists();
    
    Logger::log(QString("Update checker availability check: %1 (path: %2)")
                .arg(updateCheckerAvailable_ ? "found" : "not found")
                .arg(updateCheckerPath_));
}

void UpdateManager::checkForUpdates() {
    Logger::log("=== Check for Updates Requested ===");
    
    if (!updateCheckerAvailable_) {
        Logger::log("Update checker not available");
        QMessageBox::warning(nullptr, "Update Checker Not Found", 
            QString("The update checker (configure.exe) was not found in the application directory:\n\n%1\n\nPlease ensure the update checker is installed in the same folder as the application.")
            .arg(QCoreApplication::applicationDirPath()));
        return;
    }
    
    // Launch configure.exe with --su argument
    QProcess* updateProcess = new QProcess(this);
    
    // Connect signals to handle process completion
    connect(updateProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this, updateProcess](int exitCode, QProcess::ExitStatus exitStatus) {
        Logger::log(QString("Update checker process finished with exit code: %1").arg(exitCode));
        
        if (exitStatus == QProcess::CrashExit) {
            Logger::log("Update checker process crashed");
            QMessageBox::warning(nullptr, "Update Checker Error", 
                "The update checker process crashed unexpectedly.");
        }
        
        updateProcess->deleteLater();
    });
    
    // Connect error signal
    connect(updateProcess, &QProcess::errorOccurred,
            [this, updateProcess](QProcess::ProcessError error) {
        Logger::log(QString("Update checker process error: %1").arg(error));
        QMessageBox::warning(nullptr, "Update Checker Error", 
            QString("Failed to launch the update checker: %1").arg(updateProcess->errorString()));
        updateProcess->deleteLater();
    });
    
    // Start the process
    QStringList arguments;
    arguments << "--su";
    
    Logger::log(QString("Launching configure.exe with arguments: %1").arg(arguments.join(" ")));
    
    updateProcess->start(updateCheckerPath_, arguments);
    
    if (!updateProcess->waitForStarted(5000)) {
        Logger::log("Failed to start update checker process");
        QMessageBox::warning(nullptr, "Update Checker Error", 
            QString("Failed to start the update checker: %1").arg(updateProcess->errorString()));
        updateProcess->deleteLater();
        return;
    }
    
    Logger::log("Update checker process started successfully");
} 