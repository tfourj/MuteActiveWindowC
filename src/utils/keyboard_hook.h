#pragma once
#include <QObject>
#include <QKeySequence>
#include <Windows.h>

class KeyboardHook : public QObject {
    Q_OBJECT

public:
    static KeyboardHook& instance();
    
    // Install/uninstall the hook
    bool installHook();
    void uninstallHook();
    
    // Set the hotkey to monitor
    void setHotkey(const QKeySequence& sequence);
    
    // Set volume control hotkeys
    void setVolumeUpHotkey(const QKeySequence& sequence);
    void setVolumeDownHotkey(const QKeySequence& sequence);
    void clearVolumeHotkeys();
    
    // Check if hook is installed
    bool isHookInstalled() const { return hookHandle_ != nullptr; }

signals:
    void hotkeyTriggered();
    void volumeUpTriggered();
    void volumeDownTriggered();

private:
    KeyboardHook();
    ~KeyboardHook();
    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;
    
    // Static hook procedure
    static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam);
    
    // Instance method for processing hook messages
    LRESULT processHook(int nCode, WPARAM wParam, LPARAM lParam);
    
    // Convert QKeySequence to Windows virtual key codes
    struct HotkeyData {
        int virtualKey;
        int modifiers;  // Combination of MOD_ALT, MOD_CONTROL, MOD_SHIFT, MOD_WIN
        bool isValid;
    };
    HotkeyData convertKeySequence(const QKeySequence& sequence);
    
    // Check if current key state matches our hotkey
    bool isHotkeyMatch(int vk, bool isKeyDown, const HotkeyData& data);
    
    HHOOK hookHandle_;
    QKeySequence currentHotkey_;
    HotkeyData hotkeyData_;
    
    // Volume control hotkeys
    QKeySequence volumeUpHotkey_;
    QKeySequence volumeDownHotkey_;
    HotkeyData volumeUpHotkeyData_;
    HotkeyData volumeDownHotkeyData_;
    
    // Track modifier states
    bool ctrlPressed_;
    bool altPressed_;
    bool shiftPressed_;
    bool winPressed_;
    
    static KeyboardHook* instance_;
};
