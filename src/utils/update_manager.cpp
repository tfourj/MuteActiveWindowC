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
    Logger::log("Checking online version from Updates.xml");
    
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
    
    QNetworkRequest request(QUrl("https://mawc.tfourj.com/repository/Updates.xml"));
    request.setRawHeader("User-Agent", "MuteActiveWindowC-UpdateChecker");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    currentReply_ = networkManager_->get(request);
    connect(currentReply_, &QNetworkReply::finished, this, &UpdateManager::onVersionCheckFinished);
    
    Logger::log("Online version check request sent");
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
    
    Logger::log(QString("Received XML data (%1 bytes)").arg(data.size()));
    
    if (data.isEmpty()) {
        Logger::log("Received empty XML data, showing update check error");
        showUpdateCheckError("Received empty XML data");
        return;
    }
    
    parseVersionXml(data);
}

void UpdateManager::parseVersionXml(const QByteArray& xmlData) {
    Logger::log("Parsing version XML data");
    
    QXmlStreamReader xml(xmlData);
    QString remoteVersion;
    bool inPackageUpdate = false;
    
    while (!xml.atEnd()) {
        xml.readNext();
        
        if (xml.isStartElement()) {
            if (xml.name().toString() == "PackageUpdate") {
                inPackageUpdate = true;
                Logger::log("Found PackageUpdate element");
            } else if (inPackageUpdate && xml.name().toString() == "Version") {
                remoteVersion = xml.readElementText().trimmed();
                Logger::log(QString("Found version in PackageUpdate: %1").arg(remoteVersion));
                break;
            }
        } else if (xml.isEndElement() && xml.name().toString() == "PackageUpdate") {
            inPackageUpdate = false;
        }
    }
    
    if (xml.hasError()) {
        QString errorStr = xml.errorString();
        Logger::log(QString("XML parsing error: %1").arg(errorStr));
        showUpdateCheckError(QString("XML parsing error: %1").arg(errorStr));
        return;
    }
    
    if (remoteVersion.isEmpty()) {
        Logger::log("No version found in XML, showing update check error");
        showUpdateCheckError("No version found in XML");
        return;
    }
    
    QString currentVersion = getCurrentVersion();
    Logger::log(QString("Version comparison: current=%1, remote=%2").arg(currentVersion, remoteVersion));
    
    if (isNewerVersion(remoteVersion, currentVersion)) {
        Logger::log("Update is available");
        showUpdatePrompt(remoteVersion);
    } else {
        Logger::log("No update available");
        if (userInitiated_) {
            QMessageBox::information(nullptr, "No Updates Available", 
                QString("You are running the latest version v%1\n\nVersion v%2 is the version available on the repository.").arg(currentVersion, remoteVersion));
        }
    }
}

void UpdateManager::showUpdatePrompt(const QString& newVersion) {
    QString currentVersion = getCurrentVersion();
    
    QMessageBox msgBox;
    msgBox.setWindowTitle("Update Available");
    msgBox.setText(QString("A new version of MuteActiveWindowC is available!"));
    msgBox.setInformativeText(QString("Current version: %1\nNew version: %2\n\nWould you like to update now?")
                             .arg(currentVersion, newVersion));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.setIcon(QMessageBox::Information);
    
    int result = msgBox.exec();
    
    if (result == QMessageBox::Yes) {
        Logger::log("User chose to update");
        fallbackToConfigureOrGitHub();
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
