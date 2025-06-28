#pragma once
#include <QMainWindow>
#include <QKeySequence>
#include <QListWidget>
#include <QInputDialog>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QMenu>
#include <atlbase.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include "audio_muter.h"
#include "settings_manager.h"
#include "theme_manager.h"
#include "ui_mainwindow.h"
#include "process_selection_dialog.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <Windows.h>
#include <TlHelp32.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent=nullptr);
    ~MainWindow();

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void applySettings();
    void testHotkey();
    void addExcludedDevice();
    void removeExcludedDevice();
    void refreshDevices();
    void loadSettings();
    void saveSettings();
    void openApplicationFolder();
    void copyRegistryPath();
    void showMainWindow();
    void quitApplication();
    void addCurrentProcess();
    void clearProcesses();
    void saveProcesses();
    void onDarkModeChanged();

private:
    void registerHotkey();
    void unregisterHotkey();
    void onHotkeyTriggered();
    void populateDeviceList();
    QString getMainProcessName(DWORD pid);
    QString getUWPAppName(DWORD pid);
    QStringList getAvailableAudioDevices();
    void setupSystemTray();

    Ui::MainWindow *ui;
    AudioMuter muter_;
    int hotkeyId_;
    QKeySequence currentSeq_;
    SettingsManager& settingsManager_;
    QSystemTrayIcon* trayIcon_;
    QMenu* trayMenu_;
};
