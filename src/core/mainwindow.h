#pragma once
#include <QMainWindow>
#include <QKeySequence>
#include <QListWidget>
#include <QInputDialog>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTimer>
#include <QElapsedTimer>
#include <atlbase.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include "audio_muter.h"
#include "settings_manager.h"
#include "theme_manager.h"
#include "update_manager.h"
#include "process_selection_dialog.h"
#include "ui_mainwindow.h"
#include "keyboard_hook.h"
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
    void simulateHotkeyInSelectedApp();
    void addExcludedDevice();
    void removeExcludedDevice();
    void refreshDevices();
    void loadSettings();
    void saveSettings();
    void openApplicationFolder();
    void copyRegistryPath();
    void showMainWindow();
    void quitApplication();
    void addManualProcess();
    void addCurrentProcess();
    void removeSelectedProcess();
    void clearProcesses();
    void saveProcesses();
    void onDarkModeChanged();
    void onUseHookChanged();
    void checkForUpdates();
    void showHotkeyInfo();
    void applyVolumeControlSettings();
    void onVolumeControlEnabledChanged();
    void onVolumeUpTriggered();
    void onVolumeDownTriggered();
    void onAdminRestartTriggered();
    void setOSDPositionToCursor();
    void onMouseClickDetected(int x, int y);

private:
    void registerHotkey();
    void registerHotkeyNormal();
    void registerAdminRestartHotkeyNormal(const QKeySequence& sequence, int hotkeyId);
    void unregisterHotkey();
    void onHotkeyTriggered();
    void registerVolumeHotkeys();
    void unregisterVolumeHotkeys();
    void registerVolumeHotkeyNormal(const QKeySequence& sequence, int hotkeyId);
    void positionVolumeOSD();
    void populateDeviceList();
    QString getMainProcessName(DWORD pid);
    QString getUWPAppName(DWORD pid);
    QStringList getAvailableAudioDevices();
    void setupSystemTray();
    void populateExcludedProcessesTable(const QStringList& processes);
    void addProcessRow(const QString& processName);
    QStringList collectExcludedProcesses() const;

    Ui::MainWindow *ui;
    AudioMuter muter_;
    int hotkeyId_;
    QKeySequence currentSeq_;
    int volumeUpHotkeyId_;
    int volumeDownHotkeyId_;
    int adminRestartHotkeyId_;
    QKeySequence volumeUpSeq_;
    QKeySequence volumeDownSeq_;
    QKeySequence adminRestartSeq_;
    SettingsManager& settingsManager_;
    QSystemTrayIcon* trayIcon_;
    QMenu* trayMenu_;
    
    // For mouse click detection
    HHOOK mouseHookHandle_;
    QTimer* clickDetectionTimer_;
    bool waitingForClick_;
    QMessageBox* clickDetectionMessageBox_;
    HWND clickDetectionMessageBoxHandle_;
    static MainWindow* clickDetectionInstance_;
    static LRESULT CALLBACK mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    void cleanupClickDetection();
    
    // For volume adjustment debouncing
    QElapsedTimer lastVolumeAdjustTime_;
    static const int VOLUME_DEBOUNCE_MS = 150; // Debounce delay in milliseconds
};
