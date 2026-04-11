#pragma once
#include <QObject>
#include <QString>
#include <Windows.h>

class KeyboardHook : public QObject {
    Q_OBJECT

public:
    enum class AltRequirement {
        None,
        Any,
        Left,
        Right
    };

    struct HotkeyData {
        int virtualKey = 0;
        int modifiers = 0;  // Combination of MOD_ALT, MOD_CONTROL, MOD_SHIFT, MOD_WIN
        AltRequirement altRequirement = AltRequirement::None;
        bool modifierOnly = false;
        bool isValid = false;
        QString normalizedText;
    };

    static KeyboardHook& instance();
    
    // Install/uninstall the hook
    bool installHook();
    void uninstallHook();
    
    // Set the hotkey to monitor
    void setHotkey(const QString& hotkey);
    
    // Set volume control hotkeys
    void setVolumeUpHotkey(const QString& hotkey);
    void setVolumeDownHotkey(const QString& hotkey);
    void clearVolumeHotkeys();

    // Set admin restart hotkey
    void setAdminRestartHotkey(const QString& hotkey);
    void clearAdminRestartHotkey();

    static HotkeyData parseHotkey(const QString& hotkey, QString* errorMessage = nullptr);
    
    // Check if hook is installed
    bool isHookInstalled() const { return hookHandle_ != nullptr; }

signals:
    void hotkeyTriggered();
    void volumeUpTriggered();
    void volumeDownTriggered();
    void adminRestartTriggered();

private:
    KeyboardHook();
    ~KeyboardHook();
    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;
    
    // Static hook procedure
    static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam);
    
    // Instance method for processing hook messages
    LRESULT processHook(int nCode, WPARAM wParam, LPARAM lParam);

    static int parsePrimaryKeyToken(const QString& token, QString* normalizedToken, QString* errorMessage);
    static QString describeModifiers(const HotkeyData& data);
    void updateModifierState(DWORD vkCode, DWORD scanCode, DWORD flags, bool isKeyDown);
    
    // Check if current key state matches our hotkey
    bool isHotkeyMatch(DWORD vkCode, const HotkeyData& data) const;
    
    HHOOK hookHandle_;
    HotkeyData hotkeyData_;
    
    // Volume control hotkeys
    HotkeyData volumeUpHotkeyData_;
    HotkeyData volumeDownHotkeyData_;

    // Admin restart hotkey
    HotkeyData adminRestartHotkeyData_;
    
    // Track modifier states
    bool leftCtrlPressed_;
    bool rightCtrlPressed_;
    bool leftAltPressed_;
    bool rightAltPressed_;
    bool leftShiftPressed_;
    bool rightShiftPressed_;
    bool leftWinPressed_;
    bool rightWinPressed_;
    
    static KeyboardHook* instance_;
};
