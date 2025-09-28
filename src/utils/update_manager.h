#pragma once

#include <QObject>
#include <QString>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>

class QProgressDialog;

class UpdateManager : public QObject {
    Q_OBJECT

public:
    static UpdateManager& instance();
    
    // Check if update checker is available
    bool isUpdateCheckerAvailable() const;
    
    // Get the path to the update checker
    QString getUpdateCheckerPath() const;
    
    // Launch the update checker (now checks online first)
    void checkForUpdates(bool triggeredByUser);
    
    // Check availability and return the result
    bool checkAvailability();

private slots:
    void onVersionCheckFinished();
    void onDownloadFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadCanceled();

private:
    UpdateManager();
    ~UpdateManager() = default;
    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;
    
    QString updateCheckerPath_;
    bool updateCheckerAvailable_;
    QNetworkAccessManager* networkManager_;
    QNetworkReply* currentReply_;
    QString downloadedInstallerPath_;
    QString pendingVersion_;
    QProgressDialog* progressDialog_;
    
    void updateAvailability();
    void checkOnlineVersion();
    void parseGitHubApiResponse(const QByteArray& jsonData);
    void downloadInstaller(const QString& downloadUrl);
    void runInstaller();
    QString getCurrentVersion() const;
    bool isNewerVersion(const QString& remoteVersion, const QString& currentVersion) const;
    void showUpdatePrompt(const QString& newVersion, const QString& downloadUrl);
    void fallbackToConfigureOrGitHub();
    void openGitHubReleases();
    void showUpdateCheckError(const QString& reason);
    QString formatFileSize(qint64 bytes) const;
    bool userInitiated_ = false;
}; 