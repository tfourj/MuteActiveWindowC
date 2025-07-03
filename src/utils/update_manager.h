#pragma once

#include <QObject>
#include <QString>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QXmlStreamReader>

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

private:
    UpdateManager();
    ~UpdateManager() = default;
    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;
    
    QString updateCheckerPath_;
    bool updateCheckerAvailable_;
    QNetworkAccessManager* networkManager_;
    QNetworkReply* currentReply_;
    
    void updateAvailability();
    void checkOnlineVersion();
    void parseVersionXml(const QByteArray& xmlData);
    QString getCurrentVersion() const;
    bool isNewerVersion(const QString& remoteVersion, const QString& currentVersion) const;
    void showUpdatePrompt(const QString& newVersion);
    void fallbackToConfigureOrGitHub();
    void openGitHubReleases();
    void showUpdateCheckError(const QString& reason);
    bool userInitiated_ = false;
}; 