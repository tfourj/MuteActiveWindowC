#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logger.h"
#include "process_selection_dialog.h"
#include <Windows.h>
#include <QSettings>
#include <QLineEdit>
#include <QProcess>
#include <QFileInfo>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QInputDialog>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <comdef.h>
#include <mmdeviceapi.h>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <TlHelp32.h>
#include <Psapi.h>
#include "theme_manager.h"
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QClipboard>
#include <QSizePolicy>
#include "update_manager.h"
#include "keyboard_hook.h"

static const QString VERSION = QString(APP_VERSION);
static constexpr int HOTKEY_ID = 0xBEEF;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), hotkeyId_(HOTKEY_ID), settingsManager_(SettingsManager::instance()), trayIcon_(nullptr), trayMenu_(nullptr) {
    Logger::log("=== MainWindow Constructor ===");
    ui->setupUi(this);

    Logger::init(QCoreApplication::applicationDirPath() + "/app.log");
    Logger::log("=== App started ===");

    // Initialize settings manager (handles defaults and migration)
    settingsManager_.initializeDefaults();

    // Load settings into UI
    loadSettings();

    // Connect signals
    connect(ui->applyButton, &QPushButton::clicked, this, &MainWindow::applySettings);
    connect(ui->testButton, &QPushButton::clicked, this, &MainWindow::testHotkey);
    connect(ui->addDeviceButton, &QPushButton::clicked, this, &MainWindow::addExcludedDevice);
    connect(ui->removeDeviceButton, &QPushButton::clicked, this, &MainWindow::removeExcludedDevice);
    connect(ui->refreshDevicesButton, &QPushButton::clicked, this, &MainWindow::refreshDevices);
    connect(ui->openApplicationFolderButton, &QPushButton::clicked, this, &MainWindow::openApplicationFolder);
    connect(ui->copyRegistryPathButton, &QPushButton::clicked, this, &MainWindow::copyRegistryPath);
    connect(ui->addCurrentProcessButton, &QPushButton::clicked, this, &MainWindow::addCurrentProcess);
    connect(ui->clearProcessesButton, &QPushButton::clicked, this, &MainWindow::clearProcesses);
    connect(ui->saveProcessesButton, &QPushButton::clicked, this, &MainWindow::saveProcesses);
    connect(ui->checkForUpdatesButton, &QPushButton::clicked, this, &MainWindow::checkForUpdates);
    connect(ui->hotkeyInfoButton, &QPushButton::clicked, this, &MainWindow::showHotkeyInfo);
    
    // Connect settings checkboxes to auto-save
    connect(ui->startupCheck, &QCheckBox::toggled, this, &MainWindow::saveSettings);
    connect(ui->startupMinimizedCheck, &QCheckBox::toggled, this, &MainWindow::saveSettings);
    connect(ui->closeToTrayCheck, &QCheckBox::toggled, this, &MainWindow::saveSettings);
    connect(ui->showNotificationsCheck, &QCheckBox::toggled, this, &MainWindow::saveSettings);
    connect(ui->autoUpdateCheckBox, &QCheckBox::toggled, this, &MainWindow::saveSettings);
    connect(ui->darkModeCheck, &QCheckBox::toggled, this, &MainWindow::onDarkModeChanged);
    connect(ui->useHookCheck, &QCheckBox::toggled, this, &MainWindow::onUseHookChanged);
    
    // Connect keyboard hook signal
    connect(&KeyboardHook::instance(), &KeyboardHook::hotkeyTriggered, this, &MainWindow::onHotkeyTriggered);
    
    // Setup system tray
    setupSystemTray();
    
    // Add labels to status bar
    // Calculate spacing for author label to match right-side spacing
    QLabel* tempVersionLabel = new QLabel(QString("v%1").arg(VERSION));
    tempVersionLabel->setStyleSheet("color: gray; font-size: 10px; margin-right: 8px;");
    tempVersionLabel->adjustSize();
    int rightSpacing = tempVersionLabel->width() / 2;
    delete tempVersionLabel;
    
    // Create author label on left side with matching spacing
    QLabel* authorLabel = new QLabel("by TfourJ");
    authorLabel->setStyleSheet(QString("color: gray; font-size: 10px; margin-left: %1px;").arg(rightSpacing));
    statusBar()->addWidget(authorLabel);
    
    // Create spacer widget for middle
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    statusBar()->addWidget(spacer);
    
    // Create permanent widgets for status bar (right side)
    QLabel* versionLabel = new QLabel(QString("v%1").arg(VERSION));
    versionLabel->setStyleSheet("color: gray; font-size: 10px; margin-right: 8px;");
    statusBar()->addPermanentWidget(versionLabel);
    
    // Ensure tray icon is visible if we're starting minimized
    if (ui->startupMinimizedCheck->isChecked() && trayIcon_) {
        trayIcon_->show();
    }
    
    // Populate device list
    populateDeviceList();
    
    // Update checker button is always visible - it will check online first, then fallback to configure.exe or GitHub
    ui->checkForUpdatesButton->setVisible(true);
    
    Logger::log("Registering initial hotkey...");
    registerHotkey();
    
    // Check if we should start minimized
    if (ui->startupMinimizedCheck->isChecked()) {
        // Don't show the window at all if startup minimized is enabled
        Logger::log("Application started minimized - window not shown");
    } else {
        // Only show the window if startup minimized is disabled
        show();
        Logger::log("Application started normally - window shown");
    }
    
    // Check for updates on startup if enabled (delay slightly to ensure window is ready)
    if (settingsManager_.getAutoUpdateCheck()) {
        Logger::log("Performing startup update check");
        UpdateManager::instance().checkForUpdates(false);
    }
    
    Logger::log("MainWindow constructor completed");
}

MainWindow::~MainWindow() {
    unregisterHotkey();
    
    // Clean up system tray
    if (trayIcon_) {
        trayIcon_->hide();
        delete trayIcon_;
    }
    if (trayMenu_) {
        delete trayMenu_;
    }
    
    delete ui;
}

void MainWindow::loadSettings() {
    Logger::log("=== Loading Settings ===");
    
    // Load hotkey
    QString hotkey = settingsManager_.getHotkey();
    currentSeq_ = QKeySequence::fromString(hotkey);
    
    // Convert "Meta" back to "Win" for user-friendly display
    QString displayText = currentSeq_.toString();
    displayText.replace("Meta+", "Win+", Qt::CaseInsensitive);
    displayText.replace("+Meta", "+Win", Qt::CaseInsensitive);
    if (displayText == "Meta") {
        displayText = "Win";
    }
    
    ui->hotkeyEdit->setText(displayText);
    Logger::log(QString("Loaded hotkey from settings: '%1' (displayed as: '%2')").arg(currentSeq_.toString()).arg(displayText));
    
    // Load main process only setting
    bool mainProcessOnly = settingsManager_.getMainProcessOnly();
    ui->mainProcessOnlyCheck->setChecked(mainProcessOnly);
    Logger::log(QString("Loaded PID-based muting setting: %1").arg(mainProcessOnly ? "enabled" : "disabled"));
    
    // Load startup behavior settings
    bool startupEnabled = settingsManager_.getStartupEnabled();
    ui->startupCheck->setChecked(startupEnabled);
    Logger::log(QString("Loaded startup enabled setting: %1").arg(startupEnabled ? "enabled" : "disabled"));
    
    bool startupMinimized = settingsManager_.getStartupMinimized();
    ui->startupMinimizedCheck->setChecked(startupMinimized);
    Logger::log(QString("Loaded startup minimized setting: %1").arg(startupMinimized ? "enabled" : "disabled"));
    
    // Load tray behavior settings
    bool closeToTray = settingsManager_.getCloseToTray();
    ui->closeToTrayCheck->setChecked(closeToTray);
    Logger::log(QString("Loaded close to tray setting: %1").arg(closeToTray ? "enabled" : "disabled"));
    
    // Load notification settings
    bool showNotifications = settingsManager_.getShowNotifications();
    ui->showNotificationsCheck->setChecked(showNotifications);
    Logger::log(QString("Loaded show notifications setting: %1").arg(showNotifications ? "enabled" : "disabled"));
    
    // Load auto-update check setting
    bool autoUpdateCheck = settingsManager_.getAutoUpdateCheck();
    ui->autoUpdateCheckBox->setChecked(autoUpdateCheck);
    Logger::log(QString("Loaded auto-update check setting: %1").arg(autoUpdateCheck ? "enabled" : "disabled"));
    
    // Load use hook setting
    bool useHook = settingsManager_.getUseHook();
    ui->useHookCheck->setChecked(useHook);
    Logger::log(QString("Loaded use hook setting: %1").arg(useHook ? "enabled" : "disabled"));
    
    // Load dark mode setting
    bool darkMode = settingsManager_.getDarkMode();
    ui->darkModeCheck->setChecked(darkMode);
    Logger::log(QString("Loaded dark mode setting: %1").arg(darkMode ? "enabled" : "disabled"));
    
    // Apply theme based on setting
    ThemeManager::instance().applyTheme(darkMode);
    
    // Load excluded processes
    QStringList excludedProcesses = settingsManager_.getExcludedProcesses();
    ui->excludedProcessesEdit->setPlainText(excludedProcesses.join("\n"));
    Logger::log(QString("Loaded excluded processes: %1").arg(excludedProcesses.join(", ")));
    
    Logger::log("=== Settings Loading Complete ===");
}

void MainWindow::saveSettings() {
    // Don't save hotkey here - it should only be saved through applySettings()
    // This function is called when checkboxes change, and we shouldn't save
    // potentially unprocessed hotkey text from the UI field
    
    // Save main process only setting
    settingsManager_.setMainProcessOnly(ui->mainProcessOnlyCheck->isChecked());
    
    // Save startup behavior settings
    settingsManager_.setStartupEnabled(ui->startupCheck->isChecked());
    settingsManager_.setStartupMinimized(ui->startupMinimizedCheck->isChecked());
    
    // Save tray behavior settings
    settingsManager_.setCloseToTray(ui->closeToTrayCheck->isChecked());
    
    // Save notification settings
    settingsManager_.setShowNotifications(ui->showNotificationsCheck->isChecked());
    
    // Save auto-update check setting
    settingsManager_.setAutoUpdateCheck(ui->autoUpdateCheckBox->isChecked());
    
    // Save use hook setting
    settingsManager_.setUseHook(ui->useHookCheck->isChecked());
    
    // Save dark mode setting
    settingsManager_.setDarkMode(ui->darkModeCheck->isChecked());
    
    // Save excluded processes
    QStringList excludedProcesses = ui->excludedProcessesEdit->toPlainText().split("\n", Qt::SkipEmptyParts);
    settingsManager_.setExcludedProcesses(excludedProcesses);
    
    // Save all settings (this also handles registry updates)
    settingsManager_.save();
    
    // Show status message
    statusBar()->showMessage("Settings saved", 2000);
}

void MainWindow::applySettings() {
    Logger::log("=== Applying Settings ===");
    unregisterHotkey();
    
    QString keyText = ui->hotkeyEdit->text().trimmed();
    Logger::log(QString("Raw key text: '%1'").arg(keyText));
    
    if (keyText.isEmpty()) {
        Logger::log("No hotkey specified");
        QMessageBox::warning(this, "Invalid Hotkey", "Please enter a valid hotkey.");
        return;
    }
    
    // Preprocess the key text to convert "Win" to "Meta" for Qt compatibility
    QString processedKeyText = keyText;
    processedKeyText.replace("Win+", "Meta+", Qt::CaseInsensitive);
    processedKeyText.replace("+Win", "+Meta", Qt::CaseInsensitive);
    if (processedKeyText == "Win") {
        processedKeyText = "Meta";
    }
    
    currentSeq_ = QKeySequence::fromString(processedKeyText);
    Logger::log(QString("Original key text: '%1'").arg(keyText));
    if (processedKeyText != keyText) {
        Logger::log(QString("Processed key text: '%1'").arg(processedKeyText));
    }
    Logger::log(QString("Parsed key sequence: '%1' (count: %2)").arg(currentSeq_.toString()).arg(currentSeq_.count()));
    
    if (currentSeq_.isEmpty()) {
        Logger::log(QString("Invalid key sequence: %1").arg(keyText));
        QMessageBox::warning(this, "Invalid Hotkey", QString("Invalid hotkey format: %1\n\nPlease use format like: Ctrl+F1, Alt+M, Shift+F2").arg(keyText));
        return;
    }
    
    // Log the individual keys in the sequence
    for (int i = 0; i < currentSeq_.count(); ++i) {
        QKeyCombination key = currentSeq_[i];
        int keyValue = key.toCombined();
        Logger::log(QString("Key %1: 0x%2").arg(i).arg(keyValue, 0, 16));
        
        // Debug modifier detection
        QString modifiers;
        if (keyValue & Qt::ControlModifier) modifiers += "Ctrl ";
        if (keyValue & Qt::AltModifier) modifiers += "Alt ";
        if (keyValue & Qt::ShiftModifier) modifiers += "Shift ";
        if (keyValue & Qt::MetaModifier) modifiers += "Meta(Win) ";
        Logger::log(QString("Key %1 modifiers: %2").arg(i).arg(modifiers.trimmed()));
        
        int baseKey = keyValue & ~Qt::KeyboardModifierMask;
        Logger::log(QString("Key %1 base key: 0x%2").arg(i).arg(baseKey, 0, 16));
    }
    
    // Save the processed hotkey to settings
    QString validHotkeyString = currentSeq_.toString();
    settingsManager_.setHotkey(validHotkeyString);
    Logger::log(QString("Saving processed hotkey to settings: '%1'").arg(validHotkeyString));
    
    // Save other settings
    saveSettings();
    
    registerHotkey();
    
    // Show success message
    QMessageBox::information(this, "Settings Saved", 
        QString("Hotkey '%1' has been saved successfully!\n\n"
                "PID-based muting: %2\n"
                "Excluded devices: %3")
        .arg(currentSeq_.toString())
        .arg(ui->mainProcessOnlyCheck->isChecked() ? "Enabled" : "Disabled")
        .arg(settingsManager_.getExcludedDevices().join(", ")));
    
    Logger::log(QString("Hotkey set to %1").arg(currentSeq_.toString()));
}

void MainWindow::registerHotkey() {
    if (currentSeq_.isEmpty()) {
        Logger::log("Cannot register hotkey: sequence is empty");
        return;
    }
    
    // Check if we should use hook-based detection
    bool useHook = settingsManager_.getUseHook();
    
    if (useHook) {
        Logger::log("Using hook-based hotkey detection");
        KeyboardHook::instance().setHotkey(currentSeq_);
        if (KeyboardHook::instance().installHook()) {
            Logger::log(QString("Hotkey hook registered successfully: %1").arg(currentSeq_.toString()));
        } else {
            Logger::log("Failed to install keyboard hook, falling back to RegisterHotKey");
            registerHotkeyNormal();
        }
    } else {
        Logger::log("Using normal RegisterHotKey hotkey detection");
        registerHotkeyNormal();
    }
}

void MainWindow::registerHotkeyNormal() {
    // Check if window handle is valid
    HWND hwnd = (HWND)winId();
    if (!hwnd) {
        Logger::log("Cannot register hotkey: invalid window handle");
        return;
    }
    
    Logger::log(QString("Window handle: 0x%1").arg((quintptr)hwnd, 0, 16));
    
    // Get the first key from the sequence
    QKeyCombination key = currentSeq_[0];
    if (key.toCombined() == 0) {
        Logger::log("Cannot register hotkey: invalid key code");
        return;
    }
    
    Logger::log(QString("Registering hotkey. Raw key: 0x%1").arg(key.toCombined(), 0, 16));
    
    // Extract modifiers manually
    int mods = 0;
    int keyValue = key.toCombined();
    if (keyValue & Qt::ShiftModifier) {
        mods |= MOD_SHIFT;
        Logger::log("Shift modifier detected");
    }
    if (keyValue & Qt::ControlModifier) {
        mods |= MOD_CONTROL;
        Logger::log("Control modifier detected");
    }
    if (keyValue & Qt::AltModifier) {
        mods |= MOD_ALT;
        Logger::log("Alt modifier detected");
    }
    if (keyValue & Qt::MetaModifier) {
        mods |= MOD_WIN;
        Logger::log("Win modifier detected");
    }
    
    // Get the actual key code without modifiers
    int vk = keyValue & ~Qt::KeyboardModifierMask;
    Logger::log(QString("Key code without modifiers: 0x%1").arg(vk, 0, 16));
    
    // Map Qt key to Windows virtual key code
    int winVk = 0;
    if (vk >= Qt::Key_A && vk <= Qt::Key_Z) {
        winVk = 'A' + (vk - Qt::Key_A);
        Logger::log(QString("Mapped letter key: %1 -> 0x%2").arg(QChar(vk)).arg(winVk, 0, 16));
    } else if (vk >= Qt::Key_0 && vk <= Qt::Key_9) {
        winVk = '0' + (vk - Qt::Key_0);
        Logger::log(QString("Mapped number key: %1 -> 0x%2").arg(vk - Qt::Key_0).arg(winVk, 0, 16));
    } else if (vk >= Qt::Key_F1 && vk <= Qt::Key_F24) {
        winVk = VK_F1 + (vk - Qt::Key_F1);
        Logger::log(QString("Mapped function key: F%1 -> 0x%2").arg(vk - Qt::Key_F1 + 1).arg(winVk, 0, 16));
    } else {
        // For other keys, try to map them
        switch (vk) {
            case Qt::Key_Space: winVk = VK_SPACE; break;
            case Qt::Key_Tab: winVk = VK_TAB; break;
            case Qt::Key_Return: winVk = VK_RETURN; break;
            case Qt::Key_Escape: winVk = VK_ESCAPE; break;
            case Qt::Key_Backspace: winVk = VK_BACK; break;
            case Qt::Key_Delete: winVk = VK_DELETE; break;
            case Qt::Key_Insert: winVk = VK_INSERT; break;
            case Qt::Key_Home: winVk = VK_HOME; break;
            case Qt::Key_End: winVk = VK_END; break;
            case Qt::Key_PageUp: winVk = VK_PRIOR; break;
            case Qt::Key_PageDown: winVk = VK_NEXT; break;
            default: 
                winVk = vk; 
                Logger::log(QString("Using raw key code: 0x%1").arg(winVk, 0, 16));
                break;
        }
        if (winVk != vk) {
            Logger::log(QString("Mapped special key: 0x%1 -> 0x%2").arg(vk, 0, 16).arg(winVk, 0, 16));
        }
    }
    
    if (winVk == 0) {
        Logger::log(QString("Failed to map key: 0x%1").arg(vk, 0, 16));
        return;
    }
    
    Logger::log(QString("Final registration: mods=0x%1, vk=0x%2, hotkeyId=%3").arg(mods, 0, 16).arg(winVk, 0, 16).arg(hotkeyId_));
    
    if (!RegisterHotKey(hwnd, hotkeyId_, mods, winVk)) {
        DWORD error = GetLastError();
        Logger::log(QString("Failed to register hotkey. Error: %1 (0x%2)").arg(error).arg(error, 0, 16));
        
        // Try alternative approach - use scan code
        int scanCode = MapVirtualKeyW(winVk, MAPVK_VK_TO_VSC);
        Logger::log(QString("Trying with scan code: 0x%1").arg(scanCode, 0, 16));
        
        if (!RegisterHotKey(hwnd, hotkeyId_, mods, scanCode)) {
            error = GetLastError();
            Logger::log(QString("Failed to register hotkey with scan code. Error: %1 (0x%2)").arg(error).arg(error, 0, 16));
        } else {
            Logger::log(QString("Hotkey registered successfully with scan code: %1").arg(currentSeq_.toString()));
        }
    } else {
        Logger::log(QString("Hotkey registered successfully: %1").arg(currentSeq_.toString()));
    }
}

void MainWindow::unregisterHotkey() {
    // Uninstall keyboard hook if it was installed
    if (KeyboardHook::instance().isHookInstalled()) {
        Logger::log("Uninstalling keyboard hook");
        KeyboardHook::instance().uninstallHook();
    }
    
    // Also try to unregister normal hotkey in case it was registered
    if (!UnregisterHotKey((HWND)winId(), hotkeyId_)) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) { // This error is expected if hotkey wasn't registered
            Logger::log(QString("Failed to unregister hotkey: %1").arg(error));
        }
    }
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        
        // Log all Windows messages for debugging
        if (msg->message == WM_HOTKEY) {
            Logger::log(QString("WM_HOTKEY received: wParam=0x%1, lParam=0x%2").arg(msg->wParam, 0, 16).arg(msg->lParam, 0, 16));
            
            if (msg->wParam == hotkeyId_) {
                Logger::log("Hotkey ID matches! Triggering onHotkeyTriggered()");
                onHotkeyTriggered();
                *result = 0;
                return true;
            } else {
                Logger::log(QString("Hotkey ID mismatch: expected %1, got %2").arg(hotkeyId_).arg(msg->wParam));
            }
        }
        
        // Log other interesting messages
        if (msg->message == WM_KEYDOWN || msg->message == WM_KEYUP) {
            Logger::log(QString("Key event: message=0x%1, wParam=0x%2, lParam=0x%3").arg(msg->message, 0, 16).arg(msg->wParam, 0, 16).arg(msg->lParam, 0, 16));
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

QString MainWindow::getMainProcessName(DWORD pid) {
    QString exeName = "(unknown)";
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (h != nullptr) {
        WCHAR buf[MAX_PATH];
        DWORD len = MAX_PATH;
        if (QueryFullProcessImageNameW(h, 0, buf, &len)) {
            exeName = QFileInfo(QString::fromWCharArray(buf)).fileName();
            
            // Check if this is ApplicationFrameHost.exe (UWP app)
            if (exeName == "ApplicationFrameHost.exe") {
                Logger::log("Detected ApplicationFrameHost.exe, attempting to get UWP app name");
                // Try to get the actual UWP app name
                QString uwpAppName = getUWPAppName(pid);
                if (!uwpAppName.isEmpty()) {
                    exeName = uwpAppName;
                    Logger::log(QString("UWP app detected, using app name: %1").arg(exeName));
                } else {
                    Logger::log("Failed to get UWP app name, keeping ApplicationFrameHost.exe");
                }
            }
        }
        CloseHandle(h);
    }
    return exeName;
}

QString MainWindow::getUWPAppName(DWORD pid) {
    QString appName;
    
    // Get the foreground window
    HWND fgWindow = GetForegroundWindow();
    if (!fgWindow) {
        Logger::log("Failed to get foreground window for UWP detection");
        return appName;
    }
    
    Logger::log(QString("Foreground window: 0x%1").arg((quintptr)fgWindow, 0, 16));
    
    // Enumerate child windows to find the actual UWP app process
    Logger::log("Enumerating child windows to find UWP app process");
    
    // Callback function to enumerate child windows
    struct EnumData {
        DWORD mainPid;
        QString* result;
    };
    
    auto enumChildProc = [](HWND hwnd, LPARAM lParam) -> BOOL {
        EnumData* data = reinterpret_cast<EnumData*>(lParam);
        
        DWORD childPid = 0;
        if (GetWindowThreadProcessId(hwnd, &childPid)) {
            if (childPid != data->mainPid) {
                HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, childPid);
                if (h != nullptr) {
                    WCHAR buf[MAX_PATH];
                    DWORD len = MAX_PATH;
                    if (QueryFullProcessImageNameW(h, 0, buf, &len)) {
                        QString childExeName = QFileInfo(QString::fromWCharArray(buf)).fileName();
                        Logger::log(QString("Child window PID: %1, EXE: %2").arg(childPid).arg(childExeName));
                        
                        // If this is not ApplicationFrameHost and not a system process, use it
                        if (childExeName != "ApplicationFrameHost.exe" && 
                            !childExeName.contains("System") && 
                            !childExeName.contains("svchost") &&
                            !childExeName.contains("explorer")) {
                            *data->result = childExeName;
                            Logger::log(QString("Found UWP app process: %1").arg(*data->result));
                            CloseHandle(h);
                            return FALSE; // Stop enumeration
                        }
                    }
                    CloseHandle(h);
                }
            }
        }
        return TRUE; // Continue enumeration
    };
    
    EnumData enumData = { pid, &appName };
    EnumChildWindows(fgWindow, enumChildProc, reinterpret_cast<LPARAM>(&enumData));
    
    if (appName.isEmpty()) {
        Logger::log("No suitable child window process found");
    }
    
    return appName;
}

void MainWindow::onHotkeyTriggered() {
    Logger::log("=== Hotkey Triggered ===");
    HWND fg = GetForegroundWindow();
    if (!fg) {
        Logger::log("Failed to get foreground window");
        return;
    }
    
    DWORD pid = 0;
    if (!GetWindowThreadProcessId(fg, &pid)) {
        Logger::log("Failed to get process ID");
        return;
    }

    // Get the executable name of the foreground window
    QString targetExe = getMainProcessName(pid);
    Logger::log(QString("Hotkey pressed. Target executable: %1 (PID: %2)").arg(targetExe).arg(pid));

    int n = 0;
    
    // Check if PID-based muting is enabled
    if (ui->mainProcessOnlyCheck->isChecked()) {
        Logger::log("PID-based muting mode: Trying to mute specific PID only");
        n = muter_.toggleMuteByPID(pid); // Don't include related processes
        
        // If no sessions were found for the specific PID, fall back to executable-based muting
        if (n == 0) {
            Logger::log("No audio sessions found for specific PID, falling back to executable-based muting");
            n = muter_.toggleMuteByExeName(targetExe);
        }
    } else {
        Logger::log("Executable-based muting mode: Muting all processes with same executable name");
        n = muter_.toggleMuteByExeName(targetExe);
    }
    
    Logger::log(QString("Sessions toggled: %1").arg(n));
}

void MainWindow::testHotkey() {
    Logger::log("=== Test Hotkey Button Pressed ===");
    onHotkeyTriggered();
}

void MainWindow::populateDeviceList() {
    // Populate all devices list
    ui->allDevicesList->clear();
    QStringList allDevices = getAvailableAudioDevices();
    for (const QString& device : allDevices) {
        ui->allDevicesList->addItem(device);
    }
    
    // Populate excluded devices list
    ui->excludedDevicesList->clear();
    QStringList excludedDevices = settingsManager_.getExcludedDevices();
    for (const QString& device : excludedDevices) {
        ui->excludedDevicesList->addItem(device);
    }
}

QStringList MainWindow::getAvailableAudioDevices() {
    QStringList devices;
    
    // Initialize COM if needed
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        Logger::log("Failed to initialize COM");
        return devices;
    }
    
    // Create device enumerator
    CComPtr<IMMDeviceEnumerator> enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, 
                         IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) {
        Logger::log("Failed to create device enumerator");
        CoUninitialize();
        return devices;
    }
    
    // Enumerate audio endpoints
    CComPtr<IMMDeviceCollection> devs;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devs);
    if (FAILED(hr)) {
        Logger::log("Failed to enumerate audio endpoints");
        CoUninitialize();
        return devices;
    }
    
    UINT n;
    devs->GetCount(&n);
    Logger::log(QString("Found %1 active audio render devices").arg(n));
    
    for (UINT i = 0; i < n; ++i) {
        CComPtr<IMMDevice> dev;
        if (SUCCEEDED(devs->Item(i, &dev))) {
            // Get device friendly name
            CComPtr<IPropertyStore> props;
            if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props))) {
                PROPVARIANT var; 
                PropVariantInit(&var);
                if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var))) {
                    QString deviceName = QString::fromWCharArray(var.pwszVal);
                    devices.append(deviceName);
                    Logger::log(QString("Found device: %1").arg(deviceName));
                }
                PropVariantClear(&var);
            }
        }
    }
    
    CoUninitialize();
    return devices;
}

void MainWindow::addExcludedDevice() {
    QListWidgetItem* currentItem = ui->allDevicesList->currentItem();
    if (currentItem) {
        QString device = currentItem->text();
        settingsManager_.addExcludedDevice(device);
        populateDeviceList();
        saveSettings();
        QMessageBox::information(this, "Device Added", 
            QString("Device '%1' has been added to exclusions.").arg(device));
    } else {
        QMessageBox::information(this, "No Selection", 
            "Please select a device from the 'All Available Audio Devices' list to add to exclusions.");
    }
}

void MainWindow::removeExcludedDevice() {
    QListWidgetItem* currentItem = ui->excludedDevicesList->currentItem();
    if (currentItem) {
        QString device = currentItem->text();
        settingsManager_.removeExcludedDevice(device);
        populateDeviceList();
        saveSettings();
        QMessageBox::information(this, "Device Removed", 
            QString("Device '%1' has been removed from exclusions.").arg(device));
    } else {
        QMessageBox::information(this, "No Selection", 
            "Please select a device from the 'Excluded Devices' list to remove.");
    }
}

void MainWindow::refreshDevices() {
    populateDeviceList();
    QMessageBox::information(this, "Devices Refreshed", 
        "Device lists have been refreshed with current audio devices.");
}

void MainWindow::openApplicationFolder() {
    QString appDir = QCoreApplication::applicationDirPath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(appDir));
    Logger::log(QString("Opened application folder: %1").arg(appDir));
}

void MainWindow::copyRegistryPath() {
    // Copy the registry path to clipboard
    QString registryPath = "HKEY_CURRENT_USER\\SOFTWARE\\TfourJ\\MuteActiveWindow";
    QApplication::clipboard()->setText(registryPath);
    
    // Show confirmation message
    QMessageBox::information(this, "Registry Path Copied", 
        QString("Registry path copied to clipboard:\n\n%1\n\nYou can now paste this path in regedit to navigate to the settings.").arg(registryPath));
    
    Logger::log(QString("Registry path copied to clipboard: %1").arg(registryPath));
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (ui->closeToTrayCheck->isChecked() && trayIcon_ && trayIcon_->isSystemTrayAvailable()) {
        // Hide the window instead of closing
        hide();
        
        // Ensure tray icon is visible
        if (!trayIcon_->isVisible()) {
            trayIcon_->show();
        }
        
        // Show a notification that the app is still running
        if (ui->showNotificationsCheck->isChecked()) {
            trayIcon_->showMessage("MuteActiveWindow", "Application minimized to system tray", 
                                  QSystemTrayIcon::Information, 2000);
        }
        
        event->ignore();
        Logger::log("Window closed to tray");
    } else {
        // Check if close to tray is enabled but tray is not available
        if (ui->closeToTrayCheck->isChecked() && (!trayIcon_ || !trayIcon_->isSystemTrayAvailable())) {
            QMessageBox::StandardButton reply = QMessageBox::question(this, "Close Application", 
                "Close to tray is enabled but system tray is not available.\n\nDo you want to quit the application?", 
                QMessageBox::Yes | QMessageBox::No);
            
            if (reply == QMessageBox::No) {
                event->ignore();
                return;
            }
            
        } else {
            // Normal close behavior - show confirmation dialog
            QMessageBox::StandardButton reply = QMessageBox::question(this, "Quit Application", 
                "Are you sure you want to quit MuteActiveWindow?", 
                QMessageBox::Yes | QMessageBox::No);
            
            if (reply == QMessageBox::No) {
                event->ignore();
                return;
            }
        }
        
        // Normal close behavior
        QApplication::quit();
        Logger::log("Application closing normally");
    }
}

void MainWindow::setupSystemTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        Logger::log("System tray is not available");
        return;
    }
    
    // Create tray menu
    trayMenu_ = new QMenu(this);
    
    QAction* showAction = new QAction("Show Window", this);
    connect(showAction, &QAction::triggered, this, &MainWindow::showMainWindow);
    trayMenu_->addAction(showAction);
    
    trayMenu_->addSeparator();
    
    QAction* quitAction = new QAction("Quit", this);
    connect(quitAction, &QAction::triggered, this, &MainWindow::quitApplication);
    trayMenu_->addAction(quitAction);
    
    // Create tray icon
    trayIcon_ = new QSystemTrayIcon(this);
    
    // Set the tray icon - try to use the same icon as the application
    QIcon appIcon = QApplication::windowIcon();
    if (!appIcon.isNull()) {
        trayIcon_->setIcon(appIcon);
    } else {
        // Fallback to default icon
        trayIcon_->setIcon(QIcon(":/src/assets/maw.png"));
    }
    
    trayIcon_->setToolTip("MuteActiveWindow");
    trayIcon_->setContextMenu(trayMenu_);
    
    // Connect double-click to show window
    connect(trayIcon_, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            showMainWindow();
        }
    });
    
    // Show the tray icon
    if (!trayIcon_->isVisible()) {
        trayIcon_->show();
        Logger::log("System tray icon created and shown");
    } else {
        Logger::log("System tray icon already visible");
    }
}

void MainWindow::showMainWindow() {
    if (isVisible()) {
        hide();
        Logger::log("Main window hidden from tray");
    } else {
        show();
        raise();
        activateWindow();
        Logger::log("Main window shown from tray");
    }
}

void MainWindow::quitApplication() {
    Logger::log("Quitting application from tray");
    QApplication::quit();
}

void MainWindow::addCurrentProcess() {
    ProcessSelectionDialog dialog(this);
    
    if (dialog.exec() == QDialog::Accepted) {
        QString processName = dialog.getSelectedProcess();
        
        if (processName.isEmpty()) {
            QMessageBox::warning(this, "No Selection", "Please select a process from the list.");
            return;
        }
        
        // Get current excluded processes
        QStringList excludedProcesses = settingsManager_.getExcludedProcesses();
        
        // Check if already in the list
        if (excludedProcesses.contains(processName, Qt::CaseInsensitive)) {
            QMessageBox::information(this, "Already Excluded", 
                QString("Process '%1' is already in the exclusion list.").arg(processName));
            return;
        }
        
        // Add to the list
        excludedProcesses.append(processName);
        settingsManager_.setExcludedProcesses(excludedProcesses);
        
        // Update the UI
        ui->excludedProcessesEdit->setPlainText(excludedProcesses.join("\n"));
        
        QMessageBox::information(this, "Process Added", 
            QString("Process '%1' has been added to exclusions.").arg(processName));
        
        Logger::log(QString("Added process to exclusions: %1").arg(processName));
    }
}

void MainWindow::clearProcesses() {
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Clear All Processes", 
        "Are you sure you want to clear all excluded processes?", 
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        settingsManager_.setExcludedProcesses(QStringList());
        ui->excludedProcessesEdit->clear();
        saveSettings();
        
        QMessageBox::information(this, "Processes Cleared", 
            "All excluded processes have been cleared.");
        
        Logger::log("Cleared all excluded processes");
    }
}

void MainWindow::saveProcesses() {
    // Get the current text from the text edit
    QStringList excludedProcesses = ui->excludedProcessesEdit->toPlainText().split("\n", Qt::SkipEmptyParts);
    
    // Save to settings
    settingsManager_.setExcludedProcesses(excludedProcesses);
    
    // Show success message
    statusBar()->showMessage("Process exclusions saved", 2000);
    
    QMessageBox::information(this, "Settings Saved", 
        QString("Process exclusions have been saved successfully!\n\n"
                "Excluded processes: %1")
        .arg(excludedProcesses.isEmpty() ? "None" : excludedProcesses.join(", ")));
    
    Logger::log(QString("Process exclusions saved: %1").arg(excludedProcesses.join(", ")));
}

void MainWindow::onDarkModeChanged() {
    bool darkMode = ui->darkModeCheck->isChecked();
    settingsManager_.setDarkMode(darkMode);
    ThemeManager::instance().applyTheme(darkMode);
    Logger::log(QString("Dark mode changed to: %1").arg(darkMode ? "enabled" : "disabled"));
}

void MainWindow::onUseHookChanged() {
    bool useHook = ui->useHookCheck->isChecked();
    settingsManager_.setUseHook(useHook);
    
    // Re-register hotkey with new setting
    Logger::log(QString("Use hook setting changed to: %1 - re-registering hotkey").arg(useHook ? "enabled" : "disabled"));
    unregisterHotkey();
    registerHotkey();
    
    Logger::log(QString("Use hook changed to: %1").arg(useHook ? "enabled" : "disabled"));
}

void MainWindow::checkForUpdates() {
    Logger::log("=== Check for Updates Button Pressed ===");
    // Use the UpdateManager to handle the update check
    UpdateManager::instance().checkForUpdates(true);
    // Show a brief status message
    statusBar()->showMessage("Update checker launched", 2000);
}

void MainWindow::showHotkeyInfo() {
    QString infoText = 
        "<h3>Hotkey Format Information</h3>"
        
        "<h4>Modifier Keys:</h4>"
        "<ul>"
        "<li><b>Ctrl</b> - Control key</li>"
        "<li><b>Alt</b> - Alt key</li>"
        "<li><b>Shift</b> - Shift key</li>"
        "<li><b>Win</b> or <b>Meta</b> - Windows key (both formats work)</li>"
        "</ul>"
        
        "<h4>Supported Keys:</h4>"
        "<ul>"
        "<li><b>Function Keys:</b> F1, F2, F3, ... F24</li>"
        "<li><b>Letters:</b> A, B, C, ... Z</li>"
        "<li><b>Numbers:</b> 0, 1, 2, ... 9</li>"
        "<li><b>Special Keys:</b> Space, Tab, Return, Escape, Backspace, Delete, Insert, Home, End, PageUp, PageDown</li>"
        "<li><b>Arrow Keys:</b> Left, Right, Up, Down</li>"
        "</ul>"
        
        "<h4>Format Examples:</h4>"
        "<ul>"
        "<li><b>Single key:</b> F1, F16, A, Space</li>"
        "<li><b>One modifier:</b> Ctrl+F1, Alt+M, Shift+A, Win+F1</li>"
        "<li><b>Two modifiers:</b> Ctrl+Alt+F1, Shift+Win+A, Ctrl+Win+F2</li>"
        "<li><b>Three modifiers:</b> Ctrl+Alt+Shift+F1, Ctrl+Win+Alt+F2</li>"
        "<li><b>All modifiers:</b> Ctrl+Alt+Shift+Win+F1</li>"
        "</ul>"
        
        "<h4>Tips:</h4>"
        "<ul>"
        "<li>Use <b>hook detection</b> if hotkeys don't work in games</li>"
        "<li>Function keys (F13-F24) are rarely used and work well</li>"
        "<li>Avoid common system shortcuts (Ctrl+C, Alt+Tab, etc.)</li>"
        "<li>Test your hotkey with the <b>Test Hotkey</b> button</li>"
        "</ul>";
    
    QMessageBox infoBox(this);
    infoBox.setWindowTitle("Hotkey Information");
    infoBox.setTextFormat(Qt::RichText);
    infoBox.setText(infoText);
    infoBox.setIcon(QMessageBox::Information);
    infoBox.setStandardButtons(QMessageBox::Ok);
    infoBox.exec();
    
    Logger::log("Displayed hotkey info dialog");
}
