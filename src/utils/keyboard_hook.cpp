#include "keyboard_hook.h"
#include "logger.h"
#include <QKeyCombination>

KeyboardHook* KeyboardHook::instance_ = nullptr;

KeyboardHook::KeyboardHook() 
    : hookHandle_(nullptr), 
      ctrlPressed_(false),
      altPressed_(false), 
      shiftPressed_(false),
      winPressed_(false) {
    instance_ = this;
    hotkeyData_.isValid = false;
    volumeUpHotkeyData_.isValid = false;
    volumeDownHotkeyData_.isValid = false;
    adminRestartHotkeyData_.isValid = false;
}

KeyboardHook::~KeyboardHook() {
    uninstallHook();
    instance_ = nullptr;
}

KeyboardHook& KeyboardHook::instance() {
    static KeyboardHook instance;
    return instance;
}

bool KeyboardHook::installHook() {
    if (hookHandle_) {
        Logger::log("Keyboard hook already installed");
        return true;
    }
    
    Logger::log("Installing low-level keyboard hook");
    
    hookHandle_ = SetWindowsHookEx(WH_KEYBOARD_LL, hookProc, GetModuleHandle(nullptr), 0);
    
    if (!hookHandle_) {
        DWORD error = GetLastError();
        Logger::log(QString("Failed to install keyboard hook. Error: %1 (0x%2)").arg(error).arg(error, 0, 16));
        return false;
    }
    
    Logger::log("Keyboard hook installed successfully");
    return true;
}

void KeyboardHook::uninstallHook() {
    if (hookHandle_) {
        Logger::log("Uninstalling keyboard hook");
        UnhookWindowsHookEx(hookHandle_);
        hookHandle_ = nullptr;
    }
}

void KeyboardHook::setHotkey(const QKeySequence& sequence) {
    currentHotkey_ = sequence;
    hotkeyData_ = convertKeySequence(sequence);
    
    if (hotkeyData_.isValid) {
        Logger::log(QString("Hook hotkey set: %1 (VK: 0x%2, Modifiers: 0x%3)")
                    .arg(sequence.toString())
                    .arg(hotkeyData_.virtualKey, 0, 16)
                    .arg(hotkeyData_.modifiers, 0, 16));
        
        // Log which modifiers are detected
        QString modifierList;
        if (hotkeyData_.modifiers & MOD_CONTROL) modifierList += "CTRL ";
        if (hotkeyData_.modifiers & MOD_ALT) modifierList += "ALT ";
        if (hotkeyData_.modifiers & MOD_SHIFT) modifierList += "SHIFT ";
        if (hotkeyData_.modifiers & MOD_WIN) modifierList += "WIN ";
        Logger::log(QString("Hook modifiers detected: %1").arg(modifierList.trimmed()));
    } else {
        Logger::log(QString("Invalid hotkey for hook: %1").arg(sequence.toString()));
    }
}

void KeyboardHook::setVolumeUpHotkey(const QKeySequence& sequence) {
    volumeUpHotkey_ = sequence;
    volumeUpHotkeyData_ = convertKeySequence(sequence);
    
    if (volumeUpHotkeyData_.isValid) {
        Logger::log(QString("Volume up hotkey set: %1 (VK: 0x%2, Modifiers: 0x%3)")
                    .arg(sequence.toString())
                    .arg(volumeUpHotkeyData_.virtualKey, 0, 16)
                    .arg(volumeUpHotkeyData_.modifiers, 0, 16));
    } else {
        Logger::log(QString("Invalid volume up hotkey: %1").arg(sequence.toString()));
    }
}

void KeyboardHook::setVolumeDownHotkey(const QKeySequence& sequence) {
    volumeDownHotkey_ = sequence;
    volumeDownHotkeyData_ = convertKeySequence(sequence);
    
    if (volumeDownHotkeyData_.isValid) {
        Logger::log(QString("Volume down hotkey set: %1 (VK: 0x%2, Modifiers: 0x%3)")
                    .arg(sequence.toString())
                    .arg(volumeDownHotkeyData_.virtualKey, 0, 16)
                    .arg(volumeDownHotkeyData_.modifiers, 0, 16));
    } else {
        Logger::log(QString("Invalid volume down hotkey: %1").arg(sequence.toString()));
    }
}

void KeyboardHook::clearVolumeHotkeys() {
    volumeUpHotkey_ = QKeySequence();
    volumeDownHotkey_ = QKeySequence();
    volumeUpHotkeyData_.isValid = false;
    volumeDownHotkeyData_.isValid = false;
    Logger::log("Volume hotkeys cleared");
}

void KeyboardHook::setAdminRestartHotkey(const QKeySequence& sequence) {
    adminRestartHotkey_ = sequence;
    adminRestartHotkeyData_ = convertKeySequence(sequence);
    
    if (adminRestartHotkeyData_.isValid) {
        Logger::log(QString("Admin restart hotkey set: %1 (VK: 0x%2, Modifiers: 0x%3)")
                    .arg(sequence.toString())
                    .arg(adminRestartHotkeyData_.virtualKey, 0, 16)
                    .arg(adminRestartHotkeyData_.modifiers, 0, 16));
    } else {
        Logger::log(QString("Invalid admin restart hotkey: %1").arg(sequence.toString()));
    }
}

void KeyboardHook::clearAdminRestartHotkey() {
    adminRestartHotkey_ = QKeySequence();
    adminRestartHotkeyData_.isValid = false;
    Logger::log("Admin restart hotkey cleared");
}

LRESULT CALLBACK KeyboardHook::hookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (instance_) {
        return instance_->processHook(nCode, wParam, lParam);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT KeyboardHook::processHook(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* kbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
        
        if (isKeyDown || isKeyUp) {
            int vk = kbd->vkCode;
            
            // Track modifier key states
            if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL) {
                ctrlPressed_ = isKeyDown;
            } else if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU) {
                altPressed_ = isKeyDown;
            } else if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
                shiftPressed_ = isKeyDown;
            } else if (vk == VK_LWIN || vk == VK_RWIN) {
                winPressed_ = isKeyDown;
            }
            
            // Check if this key matches any of our hotkeys (only on key down)
            if (isKeyDown) {
                // Check main hotkey
                if (hotkeyData_.isValid && isHotkeyMatch(vk, true, hotkeyData_)) {
                    Logger::log("Hook detected main hotkey match! Emitting signal");
                    emit hotkeyTriggered();
                    // Don't suppress the key - let it pass through
                }
                // Check volume up hotkey
                else if (volumeUpHotkeyData_.isValid && isHotkeyMatch(vk, true, volumeUpHotkeyData_)) {
                    Logger::log("Hook detected volume up hotkey match! Emitting signal");
                    emit volumeUpTriggered();
                    // Don't suppress the key - let it pass through
                }
                // Check volume down hotkey
                else if (volumeDownHotkeyData_.isValid && isHotkeyMatch(vk, true, volumeDownHotkeyData_)) {
                    Logger::log("Hook detected volume down hotkey match! Emitting signal");
                    emit volumeDownTriggered();
                    // Don't suppress the key - let it pass through
                }
                // Check admin restart hotkey
                else if (adminRestartHotkeyData_.isValid && isHotkeyMatch(vk, true, adminRestartHotkeyData_)) {
                    Logger::log("Hook detected admin restart hotkey match! Emitting signal");
                    emit adminRestartTriggered();
                    // Don't suppress the key - let it pass through
                }
            }
        }
    }
    
    return CallNextHookEx(hookHandle_, nCode, wParam, lParam);
}

KeyboardHook::HotkeyData KeyboardHook::convertKeySequence(const QKeySequence& sequence) {
    HotkeyData data;
    data.isValid = false;
    data.virtualKey = 0;
    data.modifiers = 0;
    
    if (sequence.isEmpty() || sequence.count() == 0) {
        return data;
    }
    
    // Get the first key combination from the sequence
    QKeyCombination key = sequence[0];
    int keyValue = key.toCombined();
    
    // Extract modifiers
    if (keyValue & Qt::ControlModifier) {
        data.modifiers |= MOD_CONTROL;
    }
    if (keyValue & Qt::AltModifier) {
        data.modifiers |= MOD_ALT;
    }
    if (keyValue & Qt::ShiftModifier) {
        data.modifiers |= MOD_SHIFT;
    }
    if (keyValue & Qt::MetaModifier) {
        data.modifiers |= MOD_WIN;
    }
    
    // Get the base key without modifiers
    int vk = keyValue & ~Qt::KeyboardModifierMask;
    
    // Map Qt key to Windows virtual key code
    if (vk >= Qt::Key_A && vk <= Qt::Key_Z) {
        data.virtualKey = 'A' + (vk - Qt::Key_A);
    } else if (vk >= Qt::Key_0 && vk <= Qt::Key_9) {
        data.virtualKey = '0' + (vk - Qt::Key_0);
    } else if (vk >= Qt::Key_F1 && vk <= Qt::Key_F24) {
        data.virtualKey = VK_F1 + (vk - Qt::Key_F1);
    } else {
        // Map special keys
        switch (vk) {
            case Qt::Key_Space: data.virtualKey = VK_SPACE; break;
            case Qt::Key_Tab: data.virtualKey = VK_TAB; break;
            case Qt::Key_Return: data.virtualKey = VK_RETURN; break;
            case Qt::Key_Escape: data.virtualKey = VK_ESCAPE; break;
            case Qt::Key_Backspace: data.virtualKey = VK_BACK; break;
            case Qt::Key_Delete: data.virtualKey = VK_DELETE; break;
            case Qt::Key_Insert: data.virtualKey = VK_INSERT; break;
            case Qt::Key_Home: data.virtualKey = VK_HOME; break;
            case Qt::Key_End: data.virtualKey = VK_END; break;
            case Qt::Key_PageUp: data.virtualKey = VK_PRIOR; break;
            case Qt::Key_PageDown: data.virtualKey = VK_NEXT; break;
            case Qt::Key_Left: data.virtualKey = VK_LEFT; break;
            case Qt::Key_Right: data.virtualKey = VK_RIGHT; break;
            case Qt::Key_Up: data.virtualKey = VK_UP; break;
            case Qt::Key_Down: data.virtualKey = VK_DOWN; break;
            default: 
                data.virtualKey = vk;
                break;
        }
    }
    
    if (data.virtualKey != 0) {
        data.isValid = true;
    }
    
    return data;
}

bool KeyboardHook::isHotkeyMatch(int vk, bool isKeyDown, const HotkeyData& data) {
    if (!data.isValid || !isKeyDown) {
        return false;
    }
    
    // Check if the main key matches
    if (vk != data.virtualKey) {
        return false;
    }
    
    // Check if modifier states match
    bool needsCtrl = (data.modifiers & MOD_CONTROL) != 0;
    bool needsAlt = (data.modifiers & MOD_ALT) != 0;
    bool needsShift = (data.modifiers & MOD_SHIFT) != 0;
    bool needsWin = (data.modifiers & MOD_WIN) != 0;
    
    bool ctrlOk = needsCtrl ? ctrlPressed_ : !ctrlPressed_;
    bool altOk = needsAlt ? altPressed_ : !altPressed_;
    bool shiftOk = needsShift ? shiftPressed_ : !shiftPressed_;
    bool winOk = needsWin ? winPressed_ : !winPressed_;
    
    return ctrlOk && altOk && shiftOk && winOk;
}
