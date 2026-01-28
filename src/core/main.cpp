#include "mainwindow.h"
#include "settings_manager.h"
#include "theme_manager.h"
#include <QApplication>
#include <QIcon>
#include <QDir>
#include <QFileInfo>
#include <QStyleFactory>
#include <QSharedMemory>
#include <QMessageBox>
#include <QThread>

int main(int argc, char *argv[])
{   
    QApplication a(argc, argv);

    QSharedMemory sharedMemory("MuteActiveWindowC_SingleInstanceGuard");
    bool isAdminRestart = QCoreApplication::arguments().contains("--admin-restart");
    if (!sharedMemory.create(1)) {
        if (isAdminRestart) {
            bool created = false;
            for (int attempt = 0; attempt < 30; ++attempt) {
                QThread::msleep(100);
                if (sharedMemory.create(1)) {
                    created = true;
                    break;
                }
            }
            if (!created) {
                QMessageBox::warning(nullptr, "MuteActiveWindowC", "Another instance of MuteActiveWindowC is already running.\n\nPlease close the other instance and try again.");
                return 0;
            }
        } else {
        QMessageBox::warning(nullptr, "MuteActiveWindowC", "Another instance of MuteActiveWindowC is already running.\n\nPlease close the other instance and try again.");
        return 0;
        }
    }
    
    // Set application icon
    QString iconPath = ":/src/assets/maw.png";
    
    if (QFile::exists(iconPath)) {
        QIcon icon(iconPath);
        if (!icon.isNull()) {
            a.setWindowIcon(icon);
        }
    }
    
    // Set Fusion style for better cross-platform consistency
    #ifdef Q_OS_WIN
    a.setStyle(QStyleFactory::create("Fusion"));
    #endif
    
    // Initialize settings manager to get dark mode preference
    SettingsManager& settingsManager = SettingsManager::instance();
    settingsManager.initializeDefaults();
    
    // Apply theme based on settings
    bool darkMode = settingsManager.getDarkMode();
    ThemeManager::instance().applyTheme(darkMode);
    
    a.setQuitOnLastWindowClosed(false);
    
    MainWindow w;
    
    // Don't show immediately - let MainWindow constructor handle it
    // based on the startup minimized setting
    return a.exec();
}
