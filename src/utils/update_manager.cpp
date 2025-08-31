#include "update_manager.h"
#include "logger.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QWidget>
#include <QNetworkRequest>
#include <QUrl>
#include <QDesktopServices>
#include <QVersionNumber>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QProgressDialog>

UpdateManager::UpdateManager() : updateCheckerAvailable_(false), networkManager_(nullptr), currentReply_(nullptr) {
    updateAvailability();
    
    // Initialize network manager
    networkManager_ = new QNetworkAccessManager(this);
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

QString UpdateManager::getCurrentVersion() const {
    // Get version from compiled-in constant
    return QString(APP_VERSION);
}

bool UpdateManager::isNewerVersion(const QString& remoteVersion, const QString& currentVersion) const {
    QVersionNumber remote = QVersionNumber::fromString(remoteVersion);
    QVersionNumber current = QVersionNumber::fromString(currentVersion);
    
    if (remote.isNull() || current.isNull()) {
        Logger::log(QString("Failed to parse versions for comparison: remote=%1, current=%2").arg(remoteVersion, currentVersion));
        return false;
    }
    
    bool isNewer = QVersionNumber::compare(remote, current) > 0;
    Logger::log(QString("Version comparison: remote=%1, current=%2, isNewer=%3").arg(remoteVersion, currentVersion).arg(isNewer ? "true" : "false"));
    
    return isNewer;
}

void UpdateManager::checkForUpdates(bool triggeredByUser) {
    Logger::log("=== Check for Updates Requested ===");
    userInitiated_ = triggeredByUser;
    // First, try to check online version
    checkOnlineVersion();
}

void UpdateManager::showUpdateCheckError(const QString& reason) {
    Logger::log("Showing update check error dialog: " + reason);
    QMessageBox::warning(nullptr, "Update Check Failed", QString("Failed to check for updates.\nReason: %1").arg(reason));
}

void UpdateManager::checkOnlineVersion() {
    Logger::log("Checking online version from GitHub API");
    
    if (!networkManager_) {
        Logger::log("Network manager not initialized, showing update check error");
        showUpdateCheckError("Network manager not initialized");
        return;
    }
    
    // Cancel any existing request
    if (currentReply_) {
        currentReply_->abort();
        currentReply_->deleteLater();
        currentReply_ = nullptr;
    }
    
    QNetworkRequest request(QUrl("https://api.github.com/repos/tfourj/MuteActiveWindowC/releases/latest"));
    request.setRawHeader("User-Agent", "MuteActiveWindowC-UpdateChecker");
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    currentReply_ = networkManager_->get(request);
    connect(currentReply_, &QNetworkReply::finished, this, &UpdateManager::onVersionCheckFinished);
    
    Logger::log("GitHub API version check request sent");
}

void UpdateManager::onVersionCheckFinished() {
    if (!currentReply_) {
        Logger::log("No current reply in onVersionCheckFinished");
        showUpdateCheckError("No network reply");
        return;
    }
    
    if (currentReply_->error() != QNetworkReply::NoError) {
        QString errorStr = currentReply_->errorString();
        Logger::log(QString("Online version check failed: %1").arg(errorStr));
        currentReply_->deleteLater();
        currentReply_ = nullptr;
        showUpdateCheckError(errorStr);
        return;
    }
    
    QByteArray data = currentReply_->readAll();
    currentReply_->deleteLater();
    currentReply_ = nullptr;
    
    Logger::log(QString("Received JSON data (%1 bytes)").arg(data.size()));
    
    if (data.isEmpty()) {
        Logger::log("Received empty JSON data, showing update check error");
        showUpdateCheckError("Received empty JSON data");
        return;
    }
    
    parseGitHubApiResponse(data);
}

void UpdateManager::parseGitHubApiResponse(const QByteArray& jsonData) {
    Logger::log("Parsing GitHub API JSON response");
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &error);
    
    if (error.error != QJsonParseError::NoError) {
        QString errorStr = error.errorString();
        Logger::log(QString("JSON parsing error: %1").arg(errorStr));
        showUpdateCheckError(QString("JSON parsing error: %1").arg(errorStr));
        return;
    }
    
    if (!doc.isObject()) {
        Logger::log("JSON response is not an object");
        showUpdateCheckError("Invalid JSON response format");
        return;
    }
    
    QJsonObject release = doc.object();
    
    // Get tag_name (version)
    QString remoteVersion = release["tag_name"].toString();
    if (remoteVersion.isEmpty()) {
        Logger::log("No tag_name found in GitHub API response");
        showUpdateCheckError("No version found in GitHub API response");
        return;
    }
    
    // Remove 'v' prefix if present
    if (remoteVersion.startsWith('v', Qt::CaseInsensitive)) {
        remoteVersion = remoteVersion.mid(1);
    }
    
    Logger::log(QString("Found version in GitHub API: %1").arg(remoteVersion));
    
    // Find the installer download URL
    QString downloadUrl;
    QJsonArray assets = release["assets"].toArray();
    
    // First look for MuteActiveWindowC-Setup.exe (future asset name)
    for (const QJsonValue& asset : assets) {
        QJsonObject assetObj = asset.toObject();
        QString name = assetObj["name"].toString();
        if (name == "MuteActiveWindowC-Setup.exe") {
            downloadUrl = assetObj["browser_download_url"].toString();
            Logger::log(QString("Found MuteActiveWindowC-Setup.exe: %1").arg(downloadUrl));
            break;
        }
    }
    
    // Fallback to MuteActiveWindowC-Online-Installer.exe (current asset name)
    if (downloadUrl.isEmpty()) {
        for (const QJsonValue& asset : assets) {
            QJsonObject assetObj = asset.toObject();
            QString name = assetObj["name"].toString();
            if (name == "MuteActiveWindowC-Online-Installer.exe") {
                downloadUrl = assetObj["browser_download_url"].toString();
                Logger::log(QString("Found MuteActiveWindowC-Online-Installer.exe as fallback: %1").arg(downloadUrl));
                break;
            }
        }
    }
    
    if (downloadUrl.isEmpty()) {
        Logger::log("No suitable installer found in GitHub release assets");
        showUpdateCheckError("No installer found in GitHub release assets");
        return;
    }
    
    QString currentVersion = getCurrentVersion();
    Logger::log(QString("Version comparison: current=%1, remote=%2").arg(currentVersion, remoteVersion));
    
    if (isNewerVersion(remoteVersion, currentVersion)) {
        Logger::log("Update is available");
        showUpdatePrompt(remoteVersion, downloadUrl);
    } else {
        Logger::log("No update available");
        if (userInitiated_) {
            QMessageBox::information(nullptr, "No Updates Available", 
                QString("You are running the latest version v%1\n\nVersion v%2 is the latest available on GitHub.").arg(currentVersion, remoteVersion));
        }
    }
}

void UpdateManager::showUpdatePrompt(const QString& newVersion, const QString& downloadUrl) {
    QString currentVersion = getCurrentVersion();
    
    QMessageBox msgBox;
    msgBox.setWindowTitle("Update Available");
    msgBox.setText(QString("A new version of MuteActiveWindowC is available!"));
    msgBox.setInformativeText(QString("Current version: %1\nNew version: %2\n\nThe installer will be downloaded and run automatically.\nWould you like to update now?")
                             .arg(currentVersion, newVersion));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.setIcon(QMessageBox::Information);
    
    int result = msgBox.exec();
    
    if (result == QMessageBox::Yes) {
        Logger::log("User chose to update, starting download");
        pendingVersion_ = newVersion;
        downloadInstaller(downloadUrl);
    } else {
        Logger::log("User chose not to update");
    }
}

void UpdateManager::fallbackToConfigureOrGitHub() {
    Logger::log("Using fallback update method");
    
    if (!updateCheckerAvailable_) {
        Logger::log("Configure.exe not available, prompting user to open GitHub releases");
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr,
            "Manual Update Required",
            "The automatic updater is not available.\n\nWould you like to open the GitHub releases page to download the latest version?",
            QMessageBox::Yes | QMessageBox::No
        );
        if (reply == QMessageBox::Yes) {
            openGitHubReleases();
        } else {
            Logger::log("User declined to open GitHub releases page");
        }
        return;
    }
    
    // Launch configure.exe with --su argument (original logic)
    QProcess* updateProcess = new QProcess(this);
    
    // Connect signals to handle process completion
    connect(updateProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [ updateProcess](int exitCode, QProcess::ExitStatus exitStatus) {
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
            [ updateProcess](QProcess::ProcessError error) {
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

void UpdateManager::openGitHubReleases() {
    Logger::log("Opening GitHub releases page");
    
    QString url = "https://github.com/tfourj/MuteActiveWindowC/releases";
    bool success = QDesktopServices::openUrl(QUrl(url));
    
    if (success) {
        Logger::log("Successfully opened GitHub releases page");
    } else {
        Logger::log("Failed to open GitHub releases page");
        QMessageBox::warning(nullptr, "Manual Update Required", 
            QString("Failed to open GitHub releases page.\n\nPlease visit the following URL to download the latest version:\n\n%1").arg(url));
    }
}

void UpdateManager::downloadInstaller(const QString& downloadUrl) {
    Logger::log(QString("Starting installer download from: %1").arg(downloadUrl));
    
    // Cancel any existing request
    if (currentReply_) {
        currentReply_->abort();
        currentReply_->deleteLater();
        currentReply_ = nullptr;
    }
    
    // Create temp directory for download
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tempDir);
    
    // Set download path
    downloadedInstallerPath_ = tempDir + "/MuteActiveWindowC-Setup.exe";
    
    // Remove existing file if it exists
    if (QFile::exists(downloadedInstallerPath_)) {
        QFile::remove(downloadedInstallerPath_);
    }
    
    // Create network request
    QUrl url(downloadUrl);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "MuteActiveWindowC-UpdateChecker");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    // Start download
    currentReply_ = networkManager_->get(request);
    connect(currentReply_, &QNetworkReply::finished, this, &UpdateManager::onDownloadFinished);
    connect(currentReply_, &QNetworkReply::downloadProgress, this, &UpdateManager::onDownloadProgress);
    
    Logger::log(QString("Download started, saving to: %1").arg(downloadedInstallerPath_));
}

void UpdateManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (bytesTotal > 0) {
        int percentage = static_cast<int>((bytesReceived * 100) / bytesTotal);
        Logger::log(QString("Download progress: %1% (%2/%3 bytes)").arg(percentage).arg(bytesReceived).arg(bytesTotal));
    }
}

void UpdateManager::onDownloadFinished() {
    if (!currentReply_) {
        Logger::log("No current reply in onDownloadFinished");
        showUpdateCheckError("Download failed: No network reply");
        return;
    }
    
    if (currentReply_->error() != QNetworkReply::NoError) {
        QString errorStr = currentReply_->errorString();
        Logger::log(QString("Download failed: %1").arg(errorStr));
        currentReply_->deleteLater();
        currentReply_ = nullptr;
        showUpdateCheckError(QString("Download failed: %1").arg(errorStr));
        return;
    }
    
    // Save downloaded data to file
    QByteArray data = currentReply_->readAll();
    currentReply_->deleteLater();
    currentReply_ = nullptr;
    
    Logger::log(QString("Download completed, received %1 bytes").arg(data.size()));
    
    QFile file(downloadedInstallerPath_);
    if (!file.open(QIODevice::WriteOnly)) {
        QString error = file.errorString();
        Logger::log(QString("Failed to open file for writing: %1").arg(error));
        showUpdateCheckError(QString("Failed to save installer: %1").arg(error));
        return;
    }
    
    qint64 bytesWritten = file.write(data);
    file.close();
    
    if (bytesWritten != data.size()) {
        Logger::log("Failed to write all data to file");
        showUpdateCheckError("Failed to save installer completely");
        return;
    }
    
    Logger::log(QString("Installer saved successfully: %1").arg(downloadedInstallerPath_));
    
    // Run the installer
    runInstaller();
}

void UpdateManager::runInstaller() {
    if (downloadedInstallerPath_.isEmpty() || !QFile::exists(downloadedInstallerPath_)) {
        Logger::log("Downloaded installer not found");
        showUpdateCheckError("Downloaded installer not found");
        return;
    }
    
    Logger::log(QString("Running installer: %1").arg(downloadedInstallerPath_));
    
    // Create process to run installer
    QProcess* installerProcess = new QProcess(this);
    
    // Connect signals to handle process completion
    connect(installerProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this, installerProcess](int exitCode, QProcess::ExitStatus exitStatus) {
        Logger::log(QString("Installer process finished with exit code: %1").arg(exitCode));
        
        if (exitStatus == QProcess::CrashExit) {
            Logger::log("Installer process crashed");
            QMessageBox::warning(nullptr, "Installer Error", 
                "The installer process crashed unexpectedly.");
        } else if (exitCode == 0) {
            Logger::log("Installer completed successfully");
            QMessageBox::information(nullptr, "Update Started", 
                QString("The installer for version %1 has been launched.\n\nThe current application may close during the update process.").arg(pendingVersion_));
        } else {
            Logger::log(QString("Installer completed with non-zero exit code: %1").arg(exitCode));
        }
        
        // Clean up downloaded file
        if (QFile::exists(downloadedInstallerPath_)) {
            QFile::remove(downloadedInstallerPath_);
            Logger::log("Cleaned up downloaded installer file");
        }
        
        installerProcess->deleteLater();
    });
    
    // Connect error signal
    connect(installerProcess, &QProcess::errorOccurred,
            [this, installerProcess](QProcess::ProcessError error) {
        Logger::log(QString("Installer process error: %1").arg(error));
        QMessageBox::warning(nullptr, "Installer Error", 
            QString("Failed to launch the installer: %1").arg(installerProcess->errorString()));
        
        // Clean up downloaded file
        if (QFile::exists(downloadedInstallerPath_)) {
            QFile::remove(downloadedInstallerPath_);
            Logger::log("Cleaned up downloaded installer file after error");
        }
        
        installerProcess->deleteLater();
    });
    
    // Start the installer process
    installerProcess->start(downloadedInstallerPath_);
    
    if (!installerProcess->waitForStarted(5000)) {
        Logger::log("Failed to start installer process");
        QMessageBox::warning(nullptr, "Installer Error", 
            QString("Failed to start the installer: %1").arg(installerProcess->errorString()));
        
        // Clean up downloaded file
        if (QFile::exists(downloadedInstallerPath_)) {
            QFile::remove(downloadedInstallerPath_);
            Logger::log("Cleaned up downloaded installer file after start failure");
        }
        
        installerProcess->deleteLater();
        return;
    }
    
    Logger::log("Installer process started successfully");
} 
