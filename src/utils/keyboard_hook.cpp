#include "keyboard_hook.h"
#include "logger.h"

#include <QStringList>

KeyboardHook* KeyboardHook::instance_ = nullptr;

namespace {
bool isRawWindowsToken(const QString& token) {
    return token.startsWith("VK_", Qt::CaseInsensitive) || token.startsWith("0x", Qt::CaseInsensitive);
}
}

KeyboardHook::KeyboardHook()
    : hookHandle_(nullptr),
      leftCtrlPressed_(false),
      rightCtrlPressed_(false),
      leftAltPressed_(false),
      rightAltPressed_(false),
      leftShiftPressed_(false),
      rightShiftPressed_(false),
      leftWinPressed_(false),
      rightWinPressed_(false) {
    instance_ = this;
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
        const DWORD error = GetLastError();
        Logger::log(QString("Failed to install keyboard hook. Error: %1 (0x%2)")
                        .arg(error)
                        .arg(error, 0, 16));
        return false;
    }

    Logger::log("Keyboard hook installed successfully");
    return true;
}

void KeyboardHook::uninstallHook() {
    if (!hookHandle_) {
        return;
    }

    Logger::log("Uninstalling keyboard hook");
    UnhookWindowsHookEx(hookHandle_);
    hookHandle_ = nullptr;
}

KeyboardHook::HotkeyData KeyboardHook::parseHotkey(const QString& hotkey, QString* errorMessage) {
    HotkeyData data;
    const QString trimmed = hotkey.trimmed();
    if (trimmed.isEmpty()) {
        if (errorMessage) {
            errorMessage->clear();
        }
        return data;
    }

    const QStringList tokens = trimmed.split('+', Qt::KeepEmptyParts);
    if (tokens.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "Hotkey cannot be empty.";
        }
        return data;
    }

    QString normalizedPrimaryKey;
    bool hasPrimaryKey = false;
    bool hasAltModifier = false;
    for (const QString& rawToken : tokens) {
        const QString token = rawToken.trimmed();
        if (token.isEmpty()) {
            if (errorMessage) {
                *errorMessage = "Hotkey contains an empty key segment.";
            }
            return HotkeyData();
        }

        const QString lowerToken = token.toLower();
        if (lowerToken == "ctrl" || lowerToken == "control") {
            if (data.modifiers & MOD_CONTROL) {
                if (errorMessage) {
                    *errorMessage = "Ctrl is specified more than once.";
                }
                return HotkeyData();
            }
            data.modifiers |= MOD_CONTROL;
            continue;
        }

        if (lowerToken == "shift") {
            if (data.modifiers & MOD_SHIFT) {
                if (errorMessage) {
                    *errorMessage = "Shift is specified more than once.";
                }
                return HotkeyData();
            }
            data.modifiers |= MOD_SHIFT;
            continue;
        }

        if (lowerToken == "win" || lowerToken == "meta") {
            if (data.modifiers & MOD_WIN) {
                if (errorMessage) {
                    *errorMessage = "Win is specified more than once.";
                }
                return HotkeyData();
            }
            data.modifiers |= MOD_WIN;
            continue;
        }

        if (lowerToken == "alt" || lowerToken == "lalt" || lowerToken == "leftalt" ||
            lowerToken == "ralt" || lowerToken == "rightalt") {
            if (hasAltModifier) {
                if (errorMessage) {
                    *errorMessage = "Alt is specified more than once.";
                }
                return HotkeyData();
            }

            data.modifiers |= MOD_ALT;
            hasAltModifier = true;
            if (lowerToken == "alt") {
                data.altRequirement = AltRequirement::Any;
            } else if (lowerToken == "lalt" || lowerToken == "leftalt") {
                data.altRequirement = AltRequirement::Left;
            } else {
                data.altRequirement = AltRequirement::Right;
            }
            continue;
        }

        if (isRawWindowsToken(token)) {
            if (errorMessage) {
                *errorMessage = QString("Unsupported key token '%1'. Use names like Alt, LAlt, RAlt, Ctrl, Win, F1, or M.")
                                    .arg(token);
            }
            return HotkeyData();
        }

        if (hasPrimaryKey) {
            if (errorMessage) {
                *errorMessage = "Hotkey must contain exactly one non-modifier key.";
            }
            return HotkeyData();
        }

        QString normalizedToken;
        QString keyError;
        data.virtualKey = parsePrimaryKeyToken(token, &normalizedToken, &keyError);
        if (data.virtualKey == 0) {
            if (errorMessage) {
                *errorMessage = keyError;
            }
            return HotkeyData();
        }

        normalizedPrimaryKey = normalizedToken;
        hasPrimaryKey = true;
    }

    if (!hasPrimaryKey) {
        if (errorMessage) {
            *errorMessage = "Hotkey must include a non-modifier key.";
        }
        return HotkeyData();
    }

    QStringList modifierTokens;
    if (data.modifiers & MOD_CONTROL) {
        modifierTokens << "Ctrl";
    }
    if (data.modifiers & MOD_ALT) {
        switch (data.altRequirement) {
            case AltRequirement::Any:
                modifierTokens << "Alt";
                break;
            case AltRequirement::Left:
                modifierTokens << "LAlt";
                break;
            case AltRequirement::Right:
                modifierTokens << "RAlt";
                break;
            case AltRequirement::None:
                break;
        }
    }
    if (data.modifiers & MOD_SHIFT) {
        modifierTokens << "Shift";
    }
    if (data.modifiers & MOD_WIN) {
        modifierTokens << "Win";
    }
    modifierTokens << normalizedPrimaryKey;
    data.normalizedText = modifierTokens.join('+');
    data.isValid = true;
    if (errorMessage) {
        errorMessage->clear();
    }
    return data;
}

void KeyboardHook::setHotkey(const QString& hotkey) {
    hotkeyData_ = parseHotkey(hotkey);
    if (hotkeyData_.isValid) {
        Logger::log(QString("Hook hotkey set: %1 (VK: 0x%2, Modifiers: %3)")
                        .arg(hotkeyData_.normalizedText)
                        .arg(hotkeyData_.virtualKey, 0, 16)
                        .arg(describeModifiers(hotkeyData_)));
    } else {
        Logger::log(QString("Invalid hotkey for hook: %1").arg(hotkey));
    }
}

void KeyboardHook::setVolumeUpHotkey(const QString& hotkey) {
    volumeUpHotkeyData_ = parseHotkey(hotkey);
    if (volumeUpHotkeyData_.isValid) {
        Logger::log(QString("Volume up hotkey set: %1 (VK: 0x%2, Modifiers: %3)")
                        .arg(volumeUpHotkeyData_.normalizedText)
                        .arg(volumeUpHotkeyData_.virtualKey, 0, 16)
                        .arg(describeModifiers(volumeUpHotkeyData_)));
    } else {
        Logger::log(QString("Invalid volume up hotkey: %1").arg(hotkey));
    }
}

void KeyboardHook::setVolumeDownHotkey(const QString& hotkey) {
    volumeDownHotkeyData_ = parseHotkey(hotkey);
    if (volumeDownHotkeyData_.isValid) {
        Logger::log(QString("Volume down hotkey set: %1 (VK: 0x%2, Modifiers: %3)")
                        .arg(volumeDownHotkeyData_.normalizedText)
                        .arg(volumeDownHotkeyData_.virtualKey, 0, 16)
                        .arg(describeModifiers(volumeDownHotkeyData_)));
    } else {
        Logger::log(QString("Invalid volume down hotkey: %1").arg(hotkey));
    }
}

void KeyboardHook::clearVolumeHotkeys() {
    volumeUpHotkeyData_ = HotkeyData();
    volumeDownHotkeyData_ = HotkeyData();
    Logger::log("Volume hotkeys cleared");
}

void KeyboardHook::setAdminRestartHotkey(const QString& hotkey) {
    adminRestartHotkeyData_ = parseHotkey(hotkey);
    if (adminRestartHotkeyData_.isValid) {
        Logger::log(QString("Admin restart hotkey set: %1 (VK: 0x%2, Modifiers: %3)")
                        .arg(adminRestartHotkeyData_.normalizedText)
                        .arg(adminRestartHotkeyData_.virtualKey, 0, 16)
                        .arg(describeModifiers(adminRestartHotkeyData_)));
    } else {
        Logger::log(QString("Invalid admin restart hotkey: %1").arg(hotkey));
    }
}

void KeyboardHook::clearAdminRestartHotkey() {
    adminRestartHotkeyData_ = HotkeyData();
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
        const auto* keyboardData = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        const bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        const bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

        if (isKeyDown || isKeyUp) {
            updateModifierState(keyboardData->vkCode, keyboardData->scanCode, keyboardData->flags, isKeyDown);

            if (isKeyDown) {
                if (hotkeyData_.isValid && isHotkeyMatch(keyboardData->vkCode, hotkeyData_)) {
                    Logger::log("Hook detected main hotkey match");
                    emit hotkeyTriggered();
                } else if (volumeUpHotkeyData_.isValid && isHotkeyMatch(keyboardData->vkCode, volumeUpHotkeyData_)) {
                    Logger::log("Hook detected volume up hotkey match");
                    emit volumeUpTriggered();
                } else if (volumeDownHotkeyData_.isValid && isHotkeyMatch(keyboardData->vkCode, volumeDownHotkeyData_)) {
                    Logger::log("Hook detected volume down hotkey match");
                    emit volumeDownTriggered();
                } else if (adminRestartHotkeyData_.isValid && isHotkeyMatch(keyboardData->vkCode, adminRestartHotkeyData_)) {
                    Logger::log("Hook detected admin restart hotkey match");
                    emit adminRestartTriggered();
                }
            }
        }
    }

    return CallNextHookEx(hookHandle_, nCode, wParam, lParam);
}

int KeyboardHook::parsePrimaryKeyToken(const QString& token, QString* normalizedToken, QString* errorMessage) {
    const QString upperToken = token.trimmed().toUpper();
    if (upperToken.size() == 1 && upperToken[0].isLetter()) {
        if (normalizedToken) {
            *normalizedToken = upperToken;
        }
        return upperToken[0].unicode();
    }

    if (upperToken.size() == 1 && upperToken[0].isDigit()) {
        if (normalizedToken) {
            *normalizedToken = upperToken;
        }
        return upperToken[0].unicode();
    }

    if (upperToken.startsWith('F')) {
        bool ok = false;
        const int functionKey = upperToken.mid(1).toInt(&ok);
        if (ok && functionKey >= 1 && functionKey <= 24) {
            if (normalizedToken) {
                *normalizedToken = QString("F%1").arg(functionKey);
            }
            return VK_F1 + (functionKey - 1);
        }
    }

    struct NamedKey {
        const char* token;
        int vk;
        const char* normalized;
    };

    static const NamedKey namedKeys[] = {
        {"SPACE", VK_SPACE, "Space"},
        {"TAB", VK_TAB, "Tab"},
        {"RETURN", VK_RETURN, "Return"},
        {"ENTER", VK_RETURN, "Return"},
        {"ESCAPE", VK_ESCAPE, "Escape"},
        {"ESC", VK_ESCAPE, "Escape"},
        {"BACKSPACE", VK_BACK, "Backspace"},
        {"DELETE", VK_DELETE, "Delete"},
        {"DEL", VK_DELETE, "Delete"},
        {"INSERT", VK_INSERT, "Insert"},
        {"INS", VK_INSERT, "Insert"},
        {"HOME", VK_HOME, "Home"},
        {"END", VK_END, "End"},
        {"PAGEUP", VK_PRIOR, "PageUp"},
        {"PGUP", VK_PRIOR, "PageUp"},
        {"PAGEDOWN", VK_NEXT, "PageDown"},
        {"PGDOWN", VK_NEXT, "PageDown"},
        {"LEFT", VK_LEFT, "Left"},
        {"RIGHT", VK_RIGHT, "Right"},
        {"UP", VK_UP, "Up"},
        {"DOWN", VK_DOWN, "Down"},
    };

    for (const NamedKey& namedKey : namedKeys) {
        if (upperToken == QLatin1String(namedKey.token)) {
            if (normalizedToken) {
                *normalizedToken = QLatin1String(namedKey.normalized);
            }
            return namedKey.vk;
        }
    }

    if (errorMessage) {
        *errorMessage = QString("Unsupported key token '%1'.").arg(token);
    }
    return 0;
}

QString KeyboardHook::describeModifiers(const HotkeyData& data) {
    QStringList modifierList;
    if (data.modifiers & MOD_CONTROL) {
        modifierList << "CTRL";
    }
    if (data.modifiers & MOD_ALT) {
        switch (data.altRequirement) {
            case AltRequirement::Any:
                modifierList << "ALT";
                break;
            case AltRequirement::Left:
                modifierList << "LALT";
                break;
            case AltRequirement::Right:
                modifierList << "RALT";
                break;
            case AltRequirement::None:
                break;
        }
    }
    if (data.modifiers & MOD_SHIFT) {
        modifierList << "SHIFT";
    }
    if (data.modifiers & MOD_WIN) {
        modifierList << "WIN";
    }
    return modifierList.join(' ');
}

void KeyboardHook::updateModifierState(DWORD vkCode, DWORD scanCode, DWORD flags, bool isKeyDown) {
    DWORD resolvedVk = vkCode;
    if (vkCode == VK_SHIFT) {
        resolvedVk = MapVirtualKey(scanCode, MAPVK_VSC_TO_VK_EX);
    } else if (vkCode == VK_CONTROL) {
        resolvedVk = (flags & LLKHF_EXTENDED) ? VK_RCONTROL : VK_LCONTROL;
    } else if (vkCode == VK_MENU) {
        resolvedVk = (flags & LLKHF_EXTENDED) ? VK_RMENU : VK_LMENU;
    } else if (vkCode == VK_LWIN || vkCode == VK_RWIN) {
        resolvedVk = vkCode;
    }

    switch (resolvedVk) {
        case VK_LCONTROL:
            leftCtrlPressed_ = isKeyDown;
            break;
        case VK_RCONTROL:
            rightCtrlPressed_ = isKeyDown;
            break;
        case VK_LMENU:
            leftAltPressed_ = isKeyDown;
            break;
        case VK_RMENU:
            rightAltPressed_ = isKeyDown;
            break;
        case VK_LSHIFT:
            leftShiftPressed_ = isKeyDown;
            break;
        case VK_RSHIFT:
            rightShiftPressed_ = isKeyDown;
            break;
        case VK_LWIN:
            leftWinPressed_ = isKeyDown;
            break;
        case VK_RWIN:
            rightWinPressed_ = isKeyDown;
            break;
        default:
            break;
    }
}

bool KeyboardHook::isHotkeyMatch(DWORD vkCode, const HotkeyData& data) const {
    if (!data.isValid || vkCode != static_cast<DWORD>(data.virtualKey)) {
        return false;
    }

    const bool altPressed = leftAltPressed_ || rightAltPressed_;
    const bool altGrActive = rightAltPressed_ && leftCtrlPressed_ && !rightCtrlPressed_;
    const bool explicitCtrlPressed = rightCtrlPressed_ || (leftCtrlPressed_ && !altGrActive);
    const bool shiftPressed = leftShiftPressed_ || rightShiftPressed_;
    const bool winPressed = leftWinPressed_ || rightWinPressed_;

    const bool ctrlRequired = (data.modifiers & MOD_CONTROL) != 0;
    const bool altGrSatisfiesCtrl = altGrActive &&
        (data.altRequirement == AltRequirement::Right || data.altRequirement == AltRequirement::Any);
    const bool ctrlOk = ctrlRequired ? (explicitCtrlPressed || altGrSatisfiesCtrl) : !explicitCtrlPressed;
    const bool shiftOk = (data.modifiers & MOD_SHIFT) ? shiftPressed : !shiftPressed;
    const bool winOk = (data.modifiers & MOD_WIN) ? winPressed : !winPressed;

    bool altOk = false;
    switch (data.altRequirement) {
        case AltRequirement::None:
            altOk = !altPressed;
            break;
        case AltRequirement::Any:
            altOk = altPressed;
            break;
        case AltRequirement::Left:
            altOk = leftAltPressed_ && !rightAltPressed_;
            break;
        case AltRequirement::Right:
            altOk = rightAltPressed_ && !leftAltPressed_;
            break;
    }

    return ctrlOk && altOk && shiftOk && winOk;
}
