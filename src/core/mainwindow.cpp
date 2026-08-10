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
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QItemSelectionModel>
#include <algorithm>
#include <shellapi.h>
#include "update_manager.h"
#include "keyboard_hook.h"
#include "volume_osd.h"

static const QString VERSION = QString(APP_VERSION);

MainWindow* MainWindow::clickDetectionInstance_ = nullptr;

static bool isRunningAsAdmin() {
#ifdef Q_OS_WIN
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }
    
    TOKEN_ELEVATION elevation = {};
    DWORD size = 0;
    bool elevated = false;
    if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
        elevated = (elevation.TokenIsElevated != 0);
    }
    CloseHandle(token);
    return elevated;
#else
    return false;
#endif
}

static QString quoteWindowsArgument(const QString& arg) {
    if (arg.isEmpty()) {
        return "\"\"";
    }
    
    QString escaped = arg;
    escaped.replace("\"", "\\\"");
    
    if (escaped.contains(' ') || escaped.contains('\t') || escaped.contains('"')) {
        return QString("\"%1\"").arg(escaped);
    }
    
    return escaped;
}

static QString joinWindowsArguments(const QStringList& args) {
    QStringList quoted;
    quoted.reserve(args.size());
    for (const QString& arg : args) {
        quoted.append(quoteWindowsArgument(arg));
    }
    return quoted.join(' ');
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), settingsManager_(SettingsManager::instance()), trayIcon_(nullptr), trayMenu_(nullptr), mouseHookHandle_(nullptr), clickDetectionTimer_(nullptr), waitingForClick_(false), clickDetectionMessageBox_(nullptr), clickDetectionMessageBoxHandle_(nullptr) {
    Logger::log("=== MainWindow Constructor ===");
    ui->setupUi(this);
    if (isRunningAsAdmin()) {
        setWindowTitle("MuteActiveWindow (Administrator)");
    }

    ui->excludedProcessesTable->verticalHeader()->setVisible(false);
    ui->excludedProcessesTable->horizontalHeader()->setStretchLastSection(true);

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
    connect(ui->addProcessButton, &QPushButton::clicked, this, &MainWindow::addManualProcess);
    connect(ui->addCurrentProcessButton, &QPushButton::clicked, this, &MainWindow::addCurrentProcess);
    connect(ui->removeProcessButton, &QPushButton::clicked, this, &MainWindow::removeSelectedProcess);
    connect(ui->clearProcessesButton, &QPushButton::clicked, this, &MainWindow::clearProcesses);
    connect(ui->saveProcessesButton, &QPushButton::clicked, this, &MainWindow::saveProcesses);
    connect(ui->checkForUpdatesButton, &QPushButton::clicked, this, &MainWindow::checkForUpdates);
    connect(ui->hotkeyInfoButton, &QPushButton::clicked, this, &MainWindow::showHotkeyInfo);
    connect(ui->applyVolumeControlButton, &QPushButton::clicked, this, &MainWindow::applyVolumeControlSettings);
    connect(ui->setOSDPositionToCursorButton, &QPushButton::clicked, this, &MainWindow::setOSDPositionToCursor);
    
    // Connect settings checkboxes to auto-save
    connect(ui->startupCheck, &QCheckBox::toggled, this, &MainWindow::saveSettings);
    connect(ui->startupMinimizedCheck, &QCheckBox::toggled, this, &MainWindow::saveSettings);
    connect(ui->closeToTrayCheck, &QCheckBox::toggled, this, &MainWindow::saveSettings);
    connect(ui->showNotificationsCheck, &QCheckBox::toggled, this, &MainWindow::saveSettings);
    connect(ui->autoUpdateCheckBox, &QCheckBox::toggled, this, &MainWindow::saveSettings);
    connect(ui->updateCheckTimeoutCheckBox, &QCheckBox::toggled, this, [this](bool enabled) {
        ui->updateCheckTimeoutSpinBox->setEnabled(enabled);
        saveSettings();
    });
    connect(ui->updateCheckTimeoutSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::saveSettings);
    connect(ui->darkModeCheck, &QCheckBox::toggled, this, &MainWindow::onDarkModeChanged);
    connect(ui->adminRestartHotkeyEnabledCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        ui->adminRestartHotkeyEdit->setEnabled(enabled);
    });
    
    // Connect keyboard hook signals
    connect(&KeyboardHook::instance(), &KeyboardHook::hotkeyTriggered, this, &MainWindow::onHotkeyTriggered);
    connect(&KeyboardHook::instance(), &KeyboardHook::volumeUpTriggered, this, &MainWindow::onVolumeUpTriggered);
    connect(&KeyboardHook::instance(), &KeyboardHook::volumeDownTriggered, this, &MainWindow::onVolumeDownTriggered);
    connect(&KeyboardHook::instance(), &KeyboardHook::adminRestartTriggered, this, &MainWindow::onAdminRestartTriggered);
    
    // Connect volume control checkboxes
    connect(ui->volumeControlEnabledCheck, &QCheckBox::toggled, this, &MainWindow::onVolumeControlEnabledChanged);
    connect(ui->volumeControlShowOSDCheck, &QCheckBox::toggled, this, &MainWindow::saveSettings);
    
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
    // Cleanup click detection if active - must be first to prevent race condition
    waitingForClick_ = false;
    if (clickDetectionInstance_ == this) {
        clickDetectionInstance_ = nullptr;
    }
    cleanupClickDetection();
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
    currentHotkey_ = settingsManager_.getHotkey();
    const KeyboardHook::HotkeyData mainHotkeyData = KeyboardHook::parseHotkey(currentHotkey_);
    if (mainHotkeyData.isValid) {
        currentHotkey_ = mainHotkeyData.normalizedText;
    }
    ui->hotkeyEdit->setText(currentHotkey_);
    Logger::log(QString("Loaded hotkey from settings: '%1'").arg(currentHotkey_));
    
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

    bool updateCheckTimeoutEnabled = settingsManager_.getUpdateCheckTimeoutEnabled();
    ui->updateCheckTimeoutCheckBox->setChecked(updateCheckTimeoutEnabled);
    ui->updateCheckTimeoutSpinBox->setEnabled(updateCheckTimeoutEnabled);
    ui->updateCheckTimeoutSpinBox->setValue(settingsManager_.getUpdateCheckTimeoutMinutes());
    Logger::log(QString("Loaded update check timeout: %1, %2 minutes")
                .arg(updateCheckTimeoutEnabled ? "enabled" : "disabled")
                .arg(settingsManager_.getUpdateCheckTimeoutMinutes()));
    
    bool adminRestartEnabled = settingsManager_.getAdminRestartHotkeyEnabled();
    ui->adminRestartHotkeyEnabledCheck->setChecked(adminRestartEnabled);
    ui->adminRestartHotkeyEdit->setEnabled(adminRestartEnabled);
    Logger::log(QString("Loaded admin restart hotkey enabled setting: %1").arg(adminRestartEnabled ? "enabled" : "disabled"));
    
    // Load dark mode setting
    bool darkMode = settingsManager_.getDarkMode();
    ui->darkModeCheck->setChecked(darkMode);
    Logger::log(QString("Loaded dark mode setting: %1").arg(darkMode ? "enabled" : "disabled"));
    
    // Apply theme based on setting
    ThemeManager::instance().applyTheme(darkMode);
    
    // Load excluded processes
    QStringList excludedProcesses = settingsManager_.getExcludedProcesses();
    populateExcludedProcessesTable(excludedProcesses);
    Logger::log(QString("Loaded excluded processes: %1").arg(excludedProcesses.join(", ")));
    
    // Load volume control settings
    bool volumeControlEnabled = settingsManager_.getVolumeControlEnabled();
    ui->volumeControlEnabledCheck->setChecked(volumeControlEnabled);
    Logger::log(QString("Loaded volume control enabled setting: %1").arg(volumeControlEnabled ? "enabled" : "disabled"));
    
    // Show/hide volume control tab based on setting
    int tabIndex = ui->tabWidget->indexOf(ui->volumeControlTab);
    if (tabIndex >= 0) {
        ui->tabWidget->setTabVisible(tabIndex, volumeControlEnabled);
    }
    
    volumeUpHotkey_ = settingsManager_.getVolumeUpHotkey();
    const KeyboardHook::HotkeyData volumeUpData = KeyboardHook::parseHotkey(volumeUpHotkey_);
    if (volumeUpData.isValid) {
        volumeUpHotkey_ = volumeUpData.normalizedText;
    }
    ui->volumeUpHotkeyEdit->setText(volumeUpHotkey_);
    Logger::log(QString("Loaded volume up hotkey: '%1'").arg(volumeUpHotkey_));

    volumeDownHotkey_ = settingsManager_.getVolumeDownHotkey();
    const KeyboardHook::HotkeyData volumeDownData = KeyboardHook::parseHotkey(volumeDownHotkey_);
    if (volumeDownData.isValid) {
        volumeDownHotkey_ = volumeDownData.normalizedText;
    }
    ui->volumeDownHotkeyEdit->setText(volumeDownHotkey_);
    Logger::log(QString("Loaded volume down hotkey: '%1'").arg(volumeDownHotkey_));

    adminRestartHotkey_ = settingsManager_.getAdminRestartHotkey();
    const KeyboardHook::HotkeyData adminRestartData = KeyboardHook::parseHotkey(adminRestartHotkey_);
    if (adminRestartData.isValid) {
        adminRestartHotkey_ = adminRestartData.normalizedText;
    }
    ui->adminRestartHotkeyEdit->setText(adminRestartHotkey_);
    Logger::log(QString("Loaded admin restart hotkey: '%1'").arg(adminRestartHotkey_));
    
    float volumeStepPercent = settingsManager_.getVolumeStepPercent();
    ui->volumeStepSpinBox->setValue(volumeStepPercent);
    Logger::log(QString("Loaded volume step percent: %1").arg(volumeStepPercent));
    
    bool volumeControlShowOSD = settingsManager_.getVolumeControlShowOSD();
    ui->volumeControlShowOSDCheck->setChecked(volumeControlShowOSD);
    Logger::log(QString("Loaded volume control show OSD: %1").arg(volumeControlShowOSD ? "enabled" : "disabled"));
    
    QString osdPosition = settingsManager_.getVolumeOSDPosition();
    int index = ui->volumeOSDPositionComboBox->findText(osdPosition);
    if (index >= 0) {
        ui->volumeOSDPositionComboBox->setCurrentIndex(index);
    }
    Logger::log(QString("Loaded OSD position: %1").arg(osdPosition));
    
    int customX = settingsManager_.getVolumeOSDCustomX();
    int customY = settingsManager_.getVolumeOSDCustomY();
    ui->volumeOSDCustomXSpinBox->setValue(customX);
    ui->volumeOSDCustomYSpinBox->setValue(customY);
    Logger::log(QString("Loaded OSD custom position: X=%1, Y=%2").arg(customX).arg(customY));

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
    settingsManager_.setUpdateCheckTimeoutEnabled(ui->updateCheckTimeoutCheckBox->isChecked());
    settingsManager_.setUpdateCheckTimeoutMinutes(ui->updateCheckTimeoutSpinBox->value());
    
    // Save admin restart hotkey enabled setting
    settingsManager_.setAdminRestartHotkeyEnabled(ui->adminRestartHotkeyEnabledCheck->isChecked());
    
    // Save dark mode setting
    settingsManager_.setDarkMode(ui->darkModeCheck->isChecked());
    
    // Save volume control show OSD setting
    settingsManager_.setVolumeControlShowOSD(ui->volumeControlShowOSDCheck->isChecked());

    // Save excluded processes
    QStringList excludedProcesses = collectExcludedProcesses();
    settingsManager_.setExcludedProcesses(excludedProcesses);
    
    // Save all settings (this also handles registry updates)
    settingsManager_.save();
    
    // Show status message
    statusBar()->showMessage("Settings saved", 2000);
}

void MainWindow::applySettings() {
    Logger::log("=== Applying Settings ===");
    unregisterHotkey();
    
    const QString keyText = ui->hotkeyEdit->text().trimmed();
    Logger::log(QString("Raw key text: '%1'").arg(keyText));

    if (keyText.isEmpty()) {
        Logger::log("No hotkey specified");
        QMessageBox::warning(this, "Invalid Hotkey", "Please enter a valid hotkey.");
        return;
    }

    if (!parseAndNormalizeHotkey(keyText, "hotkey", false, &currentHotkey_)) {
        return;
    }
    settingsManager_.setHotkey(currentHotkey_);
    ui->hotkeyEdit->setText(currentHotkey_);
    Logger::log(QString("Saving normalized hotkey to settings: '%1'").arg(currentHotkey_));

    const bool adminRestartEnabled = ui->adminRestartHotkeyEnabledCheck->isChecked();
    const QString adminRestartKeyText = ui->adminRestartHotkeyEdit->text().trimmed();
    if (adminRestartEnabled && adminRestartKeyText.isEmpty()) {
        Logger::log("Admin restart hotkey enabled but no hotkey specified");
        QMessageBox::warning(this, "Invalid Hotkey", "Please enter a valid admin restart hotkey or disable the setting.");
        return;
    }

    if (!parseAndNormalizeHotkey(adminRestartKeyText, "admin restart hotkey", true, &adminRestartHotkey_)) {
        return;
    }
    settingsManager_.setAdminRestartHotkeyEnabled(adminRestartEnabled);
    settingsManager_.setAdminRestartHotkey(adminRestartEnabled ? adminRestartHotkey_ : QString());
    ui->adminRestartHotkeyEdit->setText(adminRestartHotkey_);

    // Save other settings
    saveSettings();

    registerHotkey();

    // Show success message
    QMessageBox::information(this, "Settings Saved", 
        QString("Hotkey '%1' has been saved successfully!\n\n"
                "PID-based muting: %2\n"
                "Excluded devices: %3")
        .arg(currentHotkey_)
        .arg(ui->mainProcessOnlyCheck->isChecked() ? "Enabled" : "Disabled")
        .arg(settingsManager_.getExcludedDevices().join(", ")));

    Logger::log(QString("Hotkey set to %1").arg(currentHotkey_));
}

void MainWindow::registerHotkey() {
    if (currentHotkey_.isEmpty()) {
        Logger::log("Cannot register hotkey: sequence is empty");
        return;
    }

    Logger::log("Registering hook-based hotkeys");
    KeyboardHook::instance().setHotkey(currentHotkey_);
    KeyboardHook::instance().clearVolumeHotkeys();

    if (settingsManager_.getVolumeControlEnabled()) {
        if (!volumeUpHotkey_.isEmpty()) {
            KeyboardHook::instance().setVolumeUpHotkey(volumeUpHotkey_);
        }
        if (!volumeDownHotkey_.isEmpty()) {
            KeyboardHook::instance().setVolumeDownHotkey(volumeDownHotkey_);
        }
    }

    if (settingsManager_.getAdminRestartHotkeyEnabled() && !adminRestartHotkey_.isEmpty()) {
        KeyboardHook::instance().setAdminRestartHotkey(adminRestartHotkey_);
    } else {
        KeyboardHook::instance().clearAdminRestartHotkey();
    }

    if (ensureKeyboardHookInstalled()) {
        Logger::log(QString("Hook hotkeys registered successfully: %1").arg(currentHotkey_));
    }
}

void MainWindow::unregisterHotkey() {
    if (KeyboardHook::instance().isHookInstalled()) {
        Logger::log("Uninstalling keyboard hook");
        KeyboardHook::instance().uninstallHook();
    }
    unregisterVolumeHotkeys();
    KeyboardHook::instance().clearAdminRestartHotkey();
}

bool MainWindow::parseAndNormalizeHotkey(const QString& input, const QString& settingName, bool allowEmpty, QString* normalizedHotkey) {
    if (allowEmpty && input.trimmed().isEmpty()) {
        normalizedHotkey->clear();
        return true;
    }

    QString errorMessage;
    const KeyboardHook::HotkeyData parsedHotkey = KeyboardHook::parseHotkey(input, &errorMessage);
    if (!parsedHotkey.isValid) {
        QMessageBox::warning(this,
                             "Invalid Hotkey",
                             QString("Invalid %1 format: %2\n\n%3")
                                 .arg(settingName)
                                 .arg(input)
                                 .arg(errorMessage.isEmpty() ? "Use formats like Ctrl+F1, Alt+M, LAlt+M, or Win+F16." : errorMessage));
        return false;
    }

    *normalizedHotkey = parsedHotkey.normalizedText;
    return true;
}

bool MainWindow::ensureKeyboardHookInstalled() {
    if (KeyboardHook::instance().installHook()) {
        return true;
    }

    QMessageBox::warning(this,
                         "Hotkeys Inactive",
                         "Failed to install the keyboard hook. Hotkey settings were saved, but hotkeys are currently inactive.");
    return false;
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
    simulateHotkeyInSelectedApp();
}

void MainWindow::simulateHotkeyInSelectedApp() {
    Logger::log("=== Simulate Hotkey in Selected App ===");
    
    // Show process selection dialog
    ProcessSelectionDialog dialog(ProcessSelectionDialog::SimulationMode, this);
    if (dialog.exec() != QDialog::Accepted) {
        Logger::log("Process selection cancelled by user");
        return;
    }
    
    QString selectedProcess = dialog.getSelectedProcess();
    DWORD selectedPID = dialog.getSelectedPID();
    
    if (selectedProcess.isEmpty() || selectedPID == 0) {
        Logger::log("No valid process selected");
        return;
    }
    
    Logger::log(QString("User selected process: %1 (PID: %2)").arg(selectedProcess).arg(selectedPID));
    
    // Find the main window of the selected process (more flexible approach)
    struct WindowCandidateData {
        DWORD pid;
        quintptr bestWindow;
        int bestScore;
    };

    HWND targetWindow = nullptr;
    WindowCandidateData data = {selectedPID, 0, 0};
    
    // First pass: look for the best window
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        WindowCandidateData* dataArray = reinterpret_cast<WindowCandidateData*>(lParam);
        DWORD targetPID = dataArray->pid;
        DWORD windowPID = 0;
        
        GetWindowThreadProcessId(hwnd, &windowPID);
        
        if (windowPID == targetPID) {
            if (IsWindowVisible(hwnd)) {
                char title[256] = {0};
                int titleLen = GetWindowTextA(hwnd, title, sizeof(title));
                
                // Calculate window "score" for best match
                int score = 0;
                
                // Prefer windows with titles
                if (titleLen > 0) score += 10;
                
                // Prefer main windows (no owner)
                if (GetWindow(hwnd, GW_OWNER) == nullptr) score += 20;
                
                // Prefer windows that are not minimized
                if (!IsIconic(hwnd)) score += 5;
                
                // Prefer larger windows
                RECT rect;
                if (GetWindowRect(hwnd, &rect)) {
                    int area = (rect.right - rect.left) * (rect.bottom - rect.top);
                    if (area > 10000) score += 3; // Decent size window
                }
                
                // Check window class for common app window types
                char className[256] = {0};
                if (GetClassNameA(hwnd, className, sizeof(className)) > 0) {
                    QString classStr = QString::fromLatin1(className);
                    if (classStr.contains("Chrome", Qt::CaseInsensitive) ||
                        classStr.contains("Mozilla", Qt::CaseInsensitive) ||
                        classStr.contains("Spotify", Qt::CaseInsensitive) ||
                        classStr.contains("Qt", Qt::CaseInsensitive) ||
                        classStr.contains("Application", Qt::CaseInsensitive)) {
                        score += 15;
                    }
                }
                
                // Update if this is the best window so far
                if (score > dataArray->bestScore) {
                    dataArray->bestWindow = reinterpret_cast<quintptr>(hwnd);
                    dataArray->bestScore = score;
                }
            }
        }
        return TRUE; // Continue enumeration to find the best window
    }, reinterpret_cast<LPARAM>(&data));
    
    targetWindow = reinterpret_cast<HWND>(data.bestWindow);
    
    if (!targetWindow) {
        Logger::log(QString("Could not find main window for process %1 (PID: %2)").arg(selectedProcess).arg(selectedPID));
        return;
    }
    
    // Log details about the selected window
    char windowTitle[256] = {0};
    char className[256] = {0};
    GetWindowTextA(targetWindow, windowTitle, sizeof(windowTitle));
    GetClassNameA(targetWindow, className, sizeof(className));
    Logger::log(QString("Selected window: '%1' (Class: %2, Score: %3)")
               .arg(QString::fromLatin1(windowTitle))
               .arg(QString::fromLatin1(className))
               .arg(data.bestScore));
    
    // Focus the target application
    Logger::log(QString("Focusing window: 0x%1").arg(reinterpret_cast<quintptr>(targetWindow), 0, 16));
    
    // Use a more robust method to bring window to foreground
    if (IsIconic(targetWindow)) {
        ShowWindow(targetWindow, SW_RESTORE);
    }
    
    SetForegroundWindow(targetWindow);
    BringWindowToTop(targetWindow);
    
    // Wait a bit for the window to be focused
    Sleep(200);
    
    // Verify the window is now in foreground
    HWND currentFG = GetForegroundWindow();
    if (currentFG == targetWindow) {
        Logger::log("Successfully focused target application");
        
        // Now simulate the hotkey on this specific application
        Logger::log("Simulating hotkey on focused application");
        onHotkeyTriggered();
    } else {
        Logger::log(QString("Failed to focus target application. Current FG: 0x%1, Target: 0x%2")
                   .arg(reinterpret_cast<quintptr>(currentFG), 0, 16)
                   .arg(reinterpret_cast<quintptr>(targetWindow), 0, 16));
    }
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

void MainWindow::populateExcludedProcessesTable(const QStringList& processes) {
    ui->excludedProcessesTable->setRowCount(0);

    QStringList added;
    added.reserve(processes.size());

    for (const QString& process : processes) {
        QString trimmed = process.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        if (trimmed.endsWith(".exe", Qt::CaseInsensitive)) {
            trimmed.chop(4);
        }

        bool exists = false;
        for (const QString& existing : added) {
            if (existing.compare(trimmed, Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }

        if (exists) {
            continue;
        }

        addProcessRow(trimmed);
        added.append(trimmed);
    }
}

void MainWindow::addProcessRow(const QString& processName) {
    int row = ui->excludedProcessesTable->rowCount();
    ui->excludedProcessesTable->insertRow(row);

    auto *item = new QTableWidgetItem(processName);
    item->setFlags((item->flags() | Qt::ItemIsEditable) & ~Qt::ItemIsUserCheckable);
    ui->excludedProcessesTable->setItem(row, 0, item);
}

QStringList MainWindow::collectExcludedProcesses() const {
    QStringList processes;

    for (int row = 0; row < ui->excludedProcessesTable->rowCount(); ++row) {
        QTableWidgetItem *item = ui->excludedProcessesTable->item(row, 0);
        if (!item) {
            continue;
        }

        QString name = item->text().trimmed();
        if (name.isEmpty()) {
            continue;
        }

        if (name.endsWith(".exe", Qt::CaseInsensitive)) {
            name.chop(4);
        }

        bool duplicate = false;
        for (const QString& existing : processes) {
            if (existing.compare(name, Qt::CaseInsensitive) == 0) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            processes.append(name);
        }
    }

    return processes;
}

void MainWindow::addManualProcess() {
    bool ok = false;
    QString processName = QInputDialog::getText(
        this,
        "Add Process",
        "Process name (without .exe):",
        QLineEdit::Normal,
        "",
        &ok);

    if (!ok) {
        return;
    }

    processName = processName.trimmed();
    if (processName.endsWith(".exe", Qt::CaseInsensitive)) {
        processName.chop(4);
    }

    if (processName.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a valid process name.");
        return;
    }

    QStringList excludedProcesses = collectExcludedProcesses();
    if (excludedProcesses.contains(processName, Qt::CaseInsensitive)) {
        QMessageBox::information(this, "Already Excluded",
            QString("Process '%1' is already in the exclusion list.").arg(processName));
        return;
    }

    addProcessRow(processName);
    QStringList updated = collectExcludedProcesses();
    settingsManager_.setExcludedProcesses(updated);

    QMessageBox::information(this, "Process Added",
        QString("Process '%1' has been added to exclusions.").arg(processName));

    Logger::log(QString("Added process to exclusions manually: %1").arg(processName));
}

void MainWindow::addCurrentProcess() {
    ProcessSelectionDialog dialog(ProcessSelectionDialog::ExclusionMode, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        QString processName = dialog.getSelectedProcess().trimmed();
        
        if (processName.isEmpty()) {
            QMessageBox::warning(this, "No Selection", "Please select a process from the list.");
            return;
        }

        if (processName.endsWith(".exe", Qt::CaseInsensitive)) {
            processName.chop(4);
        }
        
        // Get current excluded processes
        QStringList excludedProcesses = collectExcludedProcesses();
        
        // Check if already in the list
        if (excludedProcesses.contains(processName, Qt::CaseInsensitive)) {
            QMessageBox::information(this, "Already Excluded", 
                QString("Process '%1' is already in the exclusion list.").arg(processName));
            return;
        }
        
        // Add to the list and update settings/UI
        addProcessRow(processName);
        QStringList updated = collectExcludedProcesses();
        settingsManager_.setExcludedProcesses(updated);
        
        QMessageBox::information(this, "Process Added", 
            QString("Process '%1' has been added to exclusions.").arg(processName));
        
        Logger::log(QString("Added process to exclusions: %1").arg(processName));
    }
}

void MainWindow::removeSelectedProcess() {
    QItemSelectionModel *selectionModel = ui->excludedProcessesTable->selectionModel();
    if (!selectionModel) {
        return;
    }

    QModelIndexList selectedRows = selectionModel->selectedRows();
    if (selectedRows.isEmpty()) {
        QMessageBox::information(this, "No Selection", "Please select a process to remove.");
        return;
    }

    QStringList removedProcesses;
    removedProcesses.reserve(selectedRows.size());

    for (const QModelIndex& index : selectedRows) {
        QTableWidgetItem *item = ui->excludedProcessesTable->item(index.row(), 0);
        if (item) {
            QString name = item->text().trimmed();
            if (!name.isEmpty()) {
                removedProcesses.append(name);
            }
        }
    }

    std::sort(selectedRows.begin(), selectedRows.end(), [](const QModelIndex& a, const QModelIndex& b) {
        return a.row() > b.row();
    });

    for (const QModelIndex& index : selectedRows) {
        ui->excludedProcessesTable->removeRow(index.row());
    }

    QStringList updated = collectExcludedProcesses();
    settingsManager_.setExcludedProcesses(updated);

    statusBar()->showMessage(
        removedProcesses.size() == 1
            ? QString("Removed '%1' from exclusions").arg(removedProcesses.first())
            : QString("Removed %1 processes from exclusions").arg(removedProcesses.size()),
        2000);

    if (!removedProcesses.isEmpty()) {
        Logger::log(QString("Removed processes from exclusions: %1").arg(removedProcesses.join(", ")));
    }
}

void MainWindow::clearProcesses() {
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Clear All Processes", 
        "Are you sure you want to clear all excluded processes?", 
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        settingsManager_.setExcludedProcesses(QStringList());
        ui->excludedProcessesTable->setRowCount(0);
        saveSettings();
        
        QMessageBox::information(this, "Processes Cleared", 
            "All excluded processes have been cleared.");
        
        Logger::log("Cleared all excluded processes");
    }
}

void MainWindow::saveProcesses() {
    QStringList excludedProcesses = collectExcludedProcesses();
    
    // Save to settings
    settingsManager_.setExcludedProcesses(excludedProcesses);

    // Refresh table to normalise any edits (trim, remove duplicates)
    populateExcludedProcessesTable(excludedProcesses);
    
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
        "<li><b>Alt</b> - Either Alt key</li>"
        "<li><b>LAlt</b> or <b>LeftAlt</b> - Left Alt only</li>"
        "<li><b>RAlt</b> or <b>RightAlt</b> - Right Alt only</li>"
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
        "<li><b>Side-specific Alt:</b> LAlt+M, RAlt+M</li>"
        "<li><b>Two modifiers:</b> Ctrl+Alt+F1, Shift+Win+A, Ctrl+Win+F2</li>"
        "<li><b>Three modifiers:</b> Ctrl+Alt+Shift+F1, Ctrl+Win+Alt+F2</li>"
        "<li><b>All modifiers:</b> Ctrl+Alt+Shift+Win+F1</li>"
        "</ul>"
        
        "<h4>Tips:</h4>"
        "<ul>"
        "<li>Hotkeys always use a low-level keyboard hook</li>"
        "<li>Admin restart hotkey will relaunch the app with UAC prompt</li>"
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

void MainWindow::applyVolumeControlSettings() {
    Logger::log("=== Applying Volume Control Settings ===");
    unregisterVolumeHotkeys();
    
    bool enabled = ui->volumeControlEnabledCheck->isChecked();
    settingsManager_.setVolumeControlEnabled(enabled);
    
    float stepPercent = ui->volumeStepSpinBox->value();
    settingsManager_.setVolumeStepPercent(stepPercent);
    
    bool showOSD = ui->volumeControlShowOSDCheck->isChecked();
    settingsManager_.setVolumeControlShowOSD(showOSD);
    
    QString osdPosition = ui->volumeOSDPositionComboBox->currentText();
    settingsManager_.setVolumeOSDPosition(osdPosition);
    
    int customX = ui->volumeOSDCustomXSpinBox->value();
    int customY = ui->volumeOSDCustomYSpinBox->value();
    settingsManager_.setVolumeOSDCustomX(customX);
    settingsManager_.setVolumeOSDCustomY(customY);
    
    const QString volumeUpKeyText = ui->volumeUpHotkeyEdit->text().trimmed();
    if (!parseAndNormalizeHotkey(volumeUpKeyText, "volume up hotkey", true, &volumeUpHotkey_)) {
        return;
    }

    const QString volumeDownKeyText = ui->volumeDownHotkeyEdit->text().trimmed();
    if (!parseAndNormalizeHotkey(volumeDownKeyText, "volume down hotkey", true, &volumeDownHotkey_)) {
        return;
    }

    ui->volumeUpHotkeyEdit->setText(volumeUpHotkey_);
    ui->volumeDownHotkeyEdit->setText(volumeDownHotkey_);
    settingsManager_.setVolumeUpHotkey(volumeUpHotkey_);
    settingsManager_.setVolumeDownHotkey(volumeDownHotkey_);
    
    settingsManager_.save();
    
    // Register volume hotkeys if enabled
    if (enabled && (!volumeUpHotkey_.isEmpty() || !volumeDownHotkey_.isEmpty())) {
        registerVolumeHotkeys();
    }
    
    QMessageBox::information(this, "Volume Control Settings Saved", 
        QString("Volume control settings saved successfully!\n\n"
                "Enabled: %1\n"
                "Volume Up Hotkey: %2\n"
                "Volume Down Hotkey: %3\n"
                "Volume Step: %4%")
        .arg(enabled ? "Yes" : "No")
        .arg(volumeUpHotkey_.isEmpty() ? "Not set" : volumeUpHotkey_)
        .arg(volumeDownHotkey_.isEmpty() ? "Not set" : volumeDownHotkey_)
        .arg(stepPercent));
    
    Logger::log(QString("Volume control settings saved: enabled=%1, up=%2, down=%3, step=%4%")
                .arg(enabled)
                .arg(volumeUpHotkey_)
                .arg(volumeDownHotkey_)
                .arg(stepPercent));
}

void MainWindow::onVolumeControlEnabledChanged() {
    bool enabled = ui->volumeControlEnabledCheck->isChecked();
    settingsManager_.setVolumeControlEnabled(enabled);
    
    // Show/hide volume control tab based on checkbox state
    int tabIndex = ui->tabWidget->indexOf(ui->volumeControlTab);
    if (tabIndex >= 0) {
        ui->tabWidget->setTabVisible(tabIndex, enabled);
    }
    
    if (enabled) {
        registerVolumeHotkeys();
    } else {
        unregisterVolumeHotkeys();
    }
    
    Logger::log(QString("Volume control enabled changed to: %1").arg(enabled ? "enabled" : "disabled"));
}

void MainWindow::onVolumeUpTriggered() {
    Logger::log("=== Volume Up Hotkey Triggered ===");
    
    if (!settingsManager_.getVolumeControlEnabled()) {
        Logger::log("Volume control is disabled, ignoring");
        return;
    }
    
    // Debounce: Check if enough time has passed since last adjustment
    if (lastVolumeAdjustTime_.isValid() && lastVolumeAdjustTime_.elapsed() < VOLUME_DEBOUNCE_MS) {
        Logger::log(QString("Volume adjustment debounced: only %1ms since last adjustment (need %2ms)").arg(lastVolumeAdjustTime_.elapsed()).arg(VOLUME_DEBOUNCE_MS));
        return;
    }
    
    // Start or restart the timer
    lastVolumeAdjustTime_.restart();
    
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
    
    QString targetExe = getMainProcessName(pid);
    Logger::log(QString("Volume up pressed. Target executable: %1 (PID: %2)").arg(targetExe).arg(pid));
    
    float stepPercent = settingsManager_.getVolumeStepPercent();
    int n = 0;
    
    // Check if PID-based muting is enabled (reuse the same setting)
    if (ui->mainProcessOnlyCheck->isChecked()) {
        Logger::log("PID-based mode: Adjusting volume for specific PID");
        n = muter_.increaseVolumeByPID(pid, stepPercent);
        if (n == 0) {
            Logger::log("No audio sessions found for specific PID, falling back to executable-based");
            n = muter_.increaseVolumeByExeName(targetExe, stepPercent);
        }
    } else {
        Logger::log("Executable-based mode: Adjusting volume for all processes with same executable name");
        n = muter_.increaseVolumeByExeName(targetExe, stepPercent);
    }
    
    Logger::log(QString("Volume increased for %1 sessions").arg(n));
    
    // Show OSD if enabled
    if (settingsManager_.getVolumeControlShowOSD() && n > 0) {
        float currentVolume = -1.0f;
        if (ui->mainProcessOnlyCheck->isChecked()) {
            currentVolume = muter_.getVolumeByPID(pid);
            if (currentVolume < 0.0f) {
                currentVolume = muter_.getVolumeByExeName(targetExe);
            }
        } else {
            currentVolume = muter_.getVolumeByExeName(targetExe);
        }
        
        if (currentVolume >= 0.0f) {
            positionVolumeOSD();
            VolumeOSD::instance().showVolumeOSD(targetExe, currentVolume);
        }
    }
}

void MainWindow::onVolumeDownTriggered() {
    Logger::log("=== Volume Down Hotkey Triggered ===");
    
    if (!settingsManager_.getVolumeControlEnabled()) {
        Logger::log("Volume control is disabled, ignoring");
        return;
    }
    
    // Debounce: Check if enough time has passed since last adjustment
    if (lastVolumeAdjustTime_.isValid() && lastVolumeAdjustTime_.elapsed() < VOLUME_DEBOUNCE_MS) {
        Logger::log(QString("Volume adjustment debounced: only %1ms since last adjustment (need %2ms)").arg(lastVolumeAdjustTime_.elapsed()).arg(VOLUME_DEBOUNCE_MS));
        return;
    }
    
    // Start or restart the timer
    lastVolumeAdjustTime_.restart();
    
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
    
    QString targetExe = getMainProcessName(pid);
    Logger::log(QString("Volume down pressed. Target executable: %1 (PID: %2)").arg(targetExe).arg(pid));
    
    float stepPercent = settingsManager_.getVolumeStepPercent();
    int n = 0;
    
    // Check if PID-based muting is enabled (reuse the same setting)
    if (ui->mainProcessOnlyCheck->isChecked()) {
        Logger::log("PID-based mode: Adjusting volume for specific PID");
        n = muter_.decreaseVolumeByPID(pid, stepPercent);
        if (n == 0) {
            Logger::log("No audio sessions found for specific PID, falling back to executable-based");
            n = muter_.decreaseVolumeByExeName(targetExe, stepPercent);
        }
    } else {
        Logger::log("Executable-based mode: Adjusting volume for all processes with same executable name");
        n = muter_.decreaseVolumeByExeName(targetExe, stepPercent);
    }
    
    Logger::log(QString("Volume decreased for %1 sessions").arg(n));
    
    // Show OSD if enabled
    if (settingsManager_.getVolumeControlShowOSD() && n > 0) {
        float currentVolume = -1.0f;
        if (ui->mainProcessOnlyCheck->isChecked()) {
            currentVolume = muter_.getVolumeByPID(pid);
            if (currentVolume < 0.0f) {
                currentVolume = muter_.getVolumeByExeName(targetExe);
            }
        } else {
            currentVolume = muter_.getVolumeByExeName(targetExe);
        }
        
        if (currentVolume >= 0.0f) {
            positionVolumeOSD();
            VolumeOSD::instance().showVolumeOSD(targetExe, currentVolume);
        }
    }
}

void MainWindow::onAdminRestartTriggered() {
    Logger::log("=== Admin Restart Hotkey Triggered ===");
    
#ifdef Q_OS_WIN
    if (isRunningAsAdmin()) {
        Logger::log("Already running as administrator, skipping restart");
        QMessageBox::information(this, "Already Running as Administrator",
            "This app is already running with administrator privileges.");
        return;
    }
    
    QString appPath = QCoreApplication::applicationFilePath();
    QString appDir = QCoreApplication::applicationDirPath();
    QString currentDir = QDir::currentPath();
    appPath = QDir::toNativeSeparators(appPath);
    appDir = QDir::toNativeSeparators(appDir);
    currentDir = QDir::toNativeSeparators(currentDir);
    
    QStringList args = QCoreApplication::arguments();
    if (!args.isEmpty()) {
        args.removeFirst();
    }
    if (!args.contains("--admin-restart")) {
        args.append("--admin-restart");
    }
    
    QString params = joinWindowsArguments(args);
    std::wstring wAppPath = appPath.toStdWString();
    std::wstring wParams = params.toStdWString();
    
    Logger::log(QString("Attempting to restart as admin: %1 %2").arg(appPath, params));
    Logger::log(QString("Admin restart working dir: %1 (current dir: %2)").arg(appDir, currentDir));
    
    HINSTANCE result = ShellExecuteW(
        nullptr,
        L"runas",
        wAppPath.c_str(),
        params.isEmpty() ? nullptr : wParams.c_str(),
        appDir.toStdWString().c_str(),
        SW_SHOWNORMAL
    );
    
    if (reinterpret_cast<uintptr_t>(result) > 32) {
        Logger::log("Admin restart launch succeeded, exiting current instance");
        QCoreApplication::quit();
        return;
    }
    
    DWORD error = GetLastError();
    Logger::log(QString("Admin restart failed. Error: %1 (0x%2)").arg(error).arg(error, 0, 16));
    if (error == ERROR_CANCELLED || reinterpret_cast<uintptr_t>(result) == SE_ERR_ACCESSDENIED) {
        QMessageBox::warning(this, "Elevation Canceled",
            "Administrator restart was canceled or denied.");
    } else {
        QMessageBox::warning(this, "Elevation Failed",
            QString("Failed to restart with administrator privileges. Error: %1").arg(error));
    }
#else
    Logger::log("Admin restart hotkey not supported on this platform");
    QMessageBox::information(this, "Not Supported",
        "Admin restart is only supported on Windows.");
#endif
}

void MainWindow::registerVolumeHotkeys() {
    Logger::log("Registering hook-based volume hotkeys");
    KeyboardHook::instance().clearVolumeHotkeys();

    if (!volumeUpHotkey_.isEmpty()) {
        KeyboardHook::instance().setVolumeUpHotkey(volumeUpHotkey_);
    }
    if (!volumeDownHotkey_.isEmpty()) {
        KeyboardHook::instance().setVolumeDownHotkey(volumeDownHotkey_);
    }

    if (!volumeUpHotkey_.isEmpty() || !volumeDownHotkey_.isEmpty()) {
        if (ensureKeyboardHookInstalled()) {
            Logger::log("Volume hotkeys registered successfully");
        }
    }
}

void MainWindow::unregisterVolumeHotkeys() {
    KeyboardHook::instance().clearVolumeHotkeys();
    Logger::log("Volume hotkeys unregistered");
}

void MainWindow::positionVolumeOSD() {
    QString position = settingsManager_.getVolumeOSDPosition();
    QScreen* screen = QApplication::primaryScreen();
    if (!screen) {
        return;
    }
    
    QRect screenGeometry = screen->geometry();
    int x = -1, y = -1;
    
    if (position == "Custom") {
        x = settingsManager_.getVolumeOSDCustomX();
        y = settingsManager_.getVolumeOSDCustomY();
        // -1 is used as sentinel value to indicate "not set"
        // Other values (including negative for multi-monitor) are valid
        if (x != -1 && y != -1) {
            VolumeOSD::instance().setCustomPosition(x, y);
            return;
        }
        // Fall through to center if custom not set
        position = "Center";
    }
    
    // OSD size is dynamic, use default size for positioning calculations
    int osdWidth = 250;
    int osdHeight = 50;
    
    if (position == "Center") {
        QPoint center = screenGeometry.center();
        x = center.x() - osdWidth / 2;
        y = center.y() - osdHeight / 2;
    } else if (position == "Top Left") {
        x = 20;
        y = 20;
    } else if (position == "Top Right") {
        x = screenGeometry.width() - osdWidth - 20;
        y = 20;
    } else if (position == "Bottom Left") {
        x = 20;
        y = screenGeometry.height() - osdHeight - 20;
    } else if (position == "Bottom Right") {
        x = screenGeometry.width() - osdWidth - 20;
        y = screenGeometry.height() - osdHeight - 20;
    } else {
        // Default to center
        QPoint center = screenGeometry.center();
        x = center.x() - osdWidth / 2;
        y = center.y() - osdHeight / 2;
    }
    
    VolumeOSD::instance().setCustomPosition(x, y);
}

LRESULT CALLBACK MainWindow::mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && clickDetectionInstance_) {
        // Check if waiting for click - use only thread-safe members (no Qt widget access)
        // Only detect clicks while waitingForClick_ is true (set/unset on main thread)
        if (clickDetectionInstance_->waitingForClick_ && wParam == WM_LBUTTONDOWN) {
            MSLLHOOKSTRUCT* mouseData = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
            int x = mouseData->pt.x;
            int y = mouseData->pt.y;
            
            // Check if the click is on the message box window itself - if so, ignore it
            // This prevents detecting clicks on the Cancel button
            // Use the cached window handle (set on main thread) to avoid thread safety issues
            HWND msgBoxHandle = clickDetectionInstance_->clickDetectionMessageBoxHandle_;
            if (msgBoxHandle) {
                HWND clickedWindow = WindowFromPoint(mouseData->pt);
                if (clickedWindow) {
                    // Check if clicked window is the message box or any of its child windows
                    HWND parentWindow = clickedWindow;
                    while (parentWindow && parentWindow != GetDesktopWindow()) {
                        if (parentWindow == msgBoxHandle) {
                            // Click is on the message box, ignore it and let it process normally
                            return CallNextHookEx(nullptr, nCode, wParam, lParam);
                        }
                        parentWindow = GetParent(parentWindow);
                    }
                }
            }
            
            // Store instance pointer locally (hook callback runs on different thread)
            MainWindow* instance = clickDetectionInstance_;
            
            // Set waiting flag to false to prevent duplicate detections
            instance->waitingForClick_ = false;
            
            // Cleanup hook (do this from hook thread, but UI operations must be on main thread)
            // Don't log here - log messages from hook thread may not appear immediately
            HHOOK hookToUnhook = instance->mouseHookHandle_;
            if (hookToUnhook) {
                UnhookWindowsHookEx(hookToUnhook);
                instance->mouseHookHandle_ = nullptr;
            }
            
            // Post event to main thread to handle UI operations
            // Use Qt::QueuedConnection to safely handle if MainWindow is destroyed before this executes
            QMetaObject::invokeMethod(instance, "onMouseClickDetected", 
                                      Qt::QueuedConnection,
                                      Q_ARG(int, x), 
                                      Q_ARG(int, y));
            
            // Suppress the click from being processed
            return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void MainWindow::cleanupClickDetection() {
    waitingForClick_ = false;
    
    if (mouseHookHandle_) {
        UnhookWindowsHookEx(mouseHookHandle_);
        mouseHookHandle_ = nullptr;
        Logger::log("Mouse hook uninstalled in cleanupClickDetection");
    }
    
    if (clickDetectionTimer_) {
        clickDetectionTimer_->stop();
        clickDetectionTimer_->deleteLater();
        clickDetectionTimer_ = nullptr;
    }
    
    // Safely clean up message box - disconnect first, then close, then delete
    if (clickDetectionMessageBox_) {
        // Disconnect any signals to prevent callbacks during deletion
        clickDetectionMessageBox_->disconnect();
        
        // Close the message box if it's still visible
        if (clickDetectionMessageBox_->isVisible()) {
            clickDetectionMessageBox_->hide();
        }
        
        // Schedule for deletion (Qt will delete it when safe)
        clickDetectionMessageBox_->deleteLater();
        clickDetectionMessageBox_ = nullptr;
    }
    
    clickDetectionMessageBoxHandle_ = nullptr;
    clickDetectionInstance_ = nullptr;
}

void MainWindow::onMouseClickDetected(int x, int y) {
    // This is called on the main thread via QMetaObject::invokeMethod
    // Now we can safely use Qt UI components
    
    // Stop waiting for clicks
    waitingForClick_ = false;
    
    // Unhook if still hooked (might already be unhooked by hook callback)
    if (mouseHookHandle_) {
        UnhookWindowsHookEx(mouseHookHandle_);
        mouseHookHandle_ = nullptr;
        Logger::log("Mouse hook uninstalled after click detection");
    } else {
        Logger::log("Mouse hook already uninstalled (was unhooked in callback)");
    }
    
    // Clean up message box safely
    if (clickDetectionMessageBox_) {
        // Disconnect signals first
        clickDetectionMessageBox_->disconnect();
        
        // Hide the message box
        if (clickDetectionMessageBox_->isVisible()) {
            clickDetectionMessageBox_->hide();
        }
        
        // Delete later (Qt will handle it safely)
        clickDetectionMessageBox_->deleteLater();
        clickDetectionMessageBox_ = nullptr;
    }
    
    clickDetectionMessageBoxHandle_ = nullptr;
    clickDetectionInstance_ = nullptr;
    
    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Set OSD Position",
        QString("Set OSD position to coordinates:\n\nX: %1\nY: %2\n\nDo you want to set this position?")
            .arg(x).arg(y),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        ui->volumeOSDCustomXSpinBox->setValue(x);
        ui->volumeOSDCustomYSpinBox->setValue(y);
        ui->volumeOSDPositionComboBox->setCurrentText("Custom");
        Logger::log(QString("OSD custom position set to clicked position: X=%1, Y=%2").arg(x).arg(y));
    } else {
        Logger::log("OSD position setting cancelled by user");
    }
}


void MainWindow::setOSDPositionToCursor() {
    // Cleanup any existing click detection (ensure timer is fully cleaned up)
    cleanupClickDetection();
    
    // Wait a moment for any pending deleteLater() operations to complete
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    
    // Set instance for static callback
    clickDetectionInstance_ = this;
    waitingForClick_ = true;
    
    // Install mouse hook BEFORE showing message box
    mouseHookHandle_ = SetWindowsHookEx(WH_MOUSE_LL, mouseHookProc, GetModuleHandle(nullptr), 0);
    
    if (!mouseHookHandle_) {
        DWORD error = GetLastError();
        Logger::log(QString("Failed to install mouse hook. Error: %1 (0x%2)").arg(error).arg(error, 0, 16));
        QMessageBox::warning(this, "Error", "Failed to install mouse hook. Please try again.");
        waitingForClick_ = false;
        clickDetectionInstance_ = nullptr;
        return;
    }
    
    Logger::log("Mouse hook installed for click detection");
    
    // Create and show message box with Cancel button (non-blocking)
    clickDetectionMessageBox_ = new QMessageBox(this);
    clickDetectionMessageBox_->setWindowTitle("Click Detection");
    clickDetectionMessageBox_->setText("Click Detection Active");
    clickDetectionMessageBox_->setInformativeText("Click anywhere on the screen where you want the OSD to appear.\n\nClick Cancel to abort.");
    clickDetectionMessageBox_->setStandardButtons(QMessageBox::Cancel);
    clickDetectionMessageBox_->setModal(true);
    
    // Connect Cancel button to cleanup
    connect(clickDetectionMessageBox_, &QMessageBox::rejected, this, [this]() {
        Logger::log("Click detection cancelled by user");
        
        // Stop waiting and unhook
        waitingForClick_ = false;
        
        if (mouseHookHandle_) {
            UnhookWindowsHookEx(mouseHookHandle_);
            mouseHookHandle_ = nullptr;
            Logger::log("Mouse hook uninstalled after cancel");
        }
        
        // Clean up message box safely
        if (clickDetectionMessageBox_) {
            clickDetectionMessageBox_->disconnect();
            clickDetectionMessageBox_->hide();
            clickDetectionMessageBox_->deleteLater();
            clickDetectionMessageBox_ = nullptr;
        }
        
        clickDetectionMessageBoxHandle_ = nullptr;
        clickDetectionInstance_ = nullptr;
    });
    
    // Show the message box (non-blocking)
    clickDetectionMessageBox_->show();
    clickDetectionMessageBox_->raise();
    clickDetectionMessageBox_->activateWindow();
    
    // Cache the window handle on the main thread for safe access from hook callback
    // This must be done after showing the window for the handle to be valid
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    clickDetectionMessageBoxHandle_ = (HWND)clickDetectionMessageBox_->winId();
    
    Logger::log(QString("Click detection started - waiting for left click while message box is visible (handle: 0x%1)").arg(reinterpret_cast<quintptr>(clickDetectionMessageBoxHandle_), 0, 16));
}
