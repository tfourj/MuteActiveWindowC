#include "audio_muter.h"
#include "logger.h"
#include "config.h"
#include <atlbase.h>       // CComPtr
#include <functiondiscoverykeys_devpkey.h>
#include <QString>
#include <QFileInfo>

AudioMuter::AudioMuter() : enumerator_(nullptr) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    CoCreateInstance(__uuidof(MMDeviceEnumerator),
                     nullptr,
                     CLSCTX_ALL,
                     IID_PPV_ARGS(&enumerator_));
}

AudioMuter::~AudioMuter() {
    if (enumerator_) enumerator_->Release();
    CoUninitialize();
}

int AudioMuter::toggleOnDevice(IMMDevice *device, const QString& targetExeName) {
    Logger::log("=== toggleOnDevice called ===");
    
    CComPtr<IAudioSessionManager2> mgr2;
    if (FAILED(device->Activate(__uuidof(IAudioSessionManager2),
                                CLSCTX_ALL,
                                nullptr,
                                (void**)&mgr2))) {
        Logger::log("Failed to activate IAudioSessionManager2");
        return 0;
    }
    Logger::log("IAudioSessionManager2 activated successfully");
    
    CComPtr<IAudioSessionEnumerator> sessEnum;
    if (FAILED(mgr2->GetSessionEnumerator(&sessEnum))) {
        Logger::log("Failed to get session enumerator");
        return 0;
    }
    Logger::log("Session enumerator obtained successfully");

    int count = 0;
    int n;
    sessEnum->GetCount(&n);
    Logger::log(QString("Total audio sessions on device: %1").arg(n));
    
    for (int i = 0; i < n; ++i) {
        CComPtr<IAudioSessionControl> ctl;
        if (FAILED(sessEnum->GetSession(i, &ctl))) {
            Logger::log(QString("Failed to get session %1").arg(i));
            continue;
        }

        CComPtr<IAudioSessionControl2> ctl2;
        if (FAILED(ctl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctl2))) {
            Logger::log(QString("Failed to get IAudioSessionControl2 for session %1").arg(i));
            continue;
        }

        DWORD pid = 0;
        if (FAILED(ctl2->GetProcessId(&pid))) {
            Logger::log(QString("Failed to get process ID for session %1").arg(i));
            continue;
        }
        
        // Get process name
        QString exeName = "(unknown)";
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc) {
            WCHAR buf[MAX_PATH];
            DWORD len = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, buf, &len)) {
                exeName = QFileInfo(QString::fromWCharArray(buf)).fileName();
            }
            CloseHandle(hProc);
        }
        
        Logger::log(QString("Session %1: PID=%2, EXE=%3, Target EXE=%4").arg(i).arg(pid).arg(exeName, targetExeName));
        
        // Check if this process is in the exclusion list
        QString processNameWithoutExt = exeName;
        if (processNameWithoutExt.endsWith(".exe", Qt::CaseInsensitive)) {
            processNameWithoutExt = processNameWithoutExt.left(processNameWithoutExt.length() - 4);
        }
        
        if (Config::instance().isProcessExcluded(processNameWithoutExt)) {
            Logger::log(QString("Session %1: Process '%2' is in exclusion list, skipping").arg(i).arg(processNameWithoutExt));
            continue;
        }
        
        if (exeName.compare(targetExeName, Qt::CaseInsensitive) != 0) {
            Logger::log(QString("Session %1: Executable mismatch, skipping").arg(i));
            continue;
        }

        Logger::log(QString("Session %1: Executable match found! Checking mute state...").arg(i));

        CComPtr<ISimpleAudioVolume> vol;
        if (FAILED(ctl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&vol))) {
            Logger::log(QString("Failed to get ISimpleAudioVolume for session %1").arg(i));
            continue;
        }
        
        if (vol) {
            BOOL isMuted = FALSE;
            if (SUCCEEDED(vol->GetMute(&isMuted))) {
                Logger::log(QString("Session %1: Current mute state: %2").arg(i).arg(isMuted ? "Muted" : "Not muted"));
                
                // Toggle the mute state
                BOOL newMuteState = !isMuted;
                if (SUCCEEDED(vol->SetMute(newMuteState, nullptr))) {
                    ++count;
                    Logger::log(QString("Session %1: Successfully toggled mute to %2 for session pid=%3").arg(i).arg(newMuteState ? "Muted" : "Unmuted").arg(pid));
                } else {
                    Logger::log(QString("Session %1: Failed to toggle mute for session pid=%2").arg(i).arg(pid));
                }
            } else {
                Logger::log(QString("Session %1: Failed to get mute state for session pid=%2").arg(i).arg(pid));
            }
        } else {
            Logger::log(QString("Session %1: ISimpleAudioVolume is null").arg(i));
        }
    }
    
    Logger::log(QString("toggleOnDevice completed: %1 sessions toggled").arg(count));
    return count;
}

int AudioMuter::toggleMuteByExeName(const QString& targetExeName) {
    Logger::log(QString("=== toggleMuteByExeName called with target EXE: %1 ===").arg(targetExeName));
    
    if (!enumerator_) {
        Logger::log("Audio enumerator is null!");
        return 0;
    }
    
    CComPtr<IMMDeviceCollection> devs;
    HRESULT hr = enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devs);
    if (FAILED(hr)) {
        Logger::log(QString("Failed to enumerate audio endpoints. HRESULT: 0x%1").arg(hr, 0, 16));
        return 0;
    }
    Logger::log("Audio endpoints enumerated successfully");

    int total = 0;
    UINT n;
    devs->GetCount(&n);
    Logger::log(QString("Found %1 active audio render devices").arg(n));
    
    for (UINT i = 0; i < n; ++i) {
        CComPtr<IMMDevice> dev;
        if (FAILED(devs->Item(i, &dev))) {
            Logger::log(QString("Failed to get device %1").arg(i));
            continue;
        }

        // Get device friendly name
        QString deviceName = "(unknown)";
        CComPtr<IPropertyStore> props;
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props))) {
            PROPVARIANT var; 
            PropVariantInit(&var);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var))) {
                deviceName = QString::fromWCharArray(var.pwszVal);
            }
            PropVariantClear(&var);
        }
        
        Logger::log(QString("Scanning device %1: %2").arg(i).arg(deviceName));
        
        // Check if device is excluded
        if (Config::instance().isDeviceExcluded(deviceName)) {
            Logger::log(QString("Device %1 (%2) is excluded, skipping").arg(i).arg(deviceName));
            continue;
        }
        
        int deviceToggled = toggleOnDevice(dev, targetExeName);
        total += deviceToggled;
        Logger::log(QString("Device %1: %2 sessions toggled").arg(i).arg(deviceToggled));
    }
    
    Logger::log(QString("=== toggleMuteByExeName completed: Total sessions toggled for exe=%1: %2 ===")
                    .arg(targetExeName)
                    .arg(total)
                );
    return total;
}

int AudioMuter::toggleMuteByPID(DWORD targetPID) {
    Logger::log(QString("=== toggleMuteByPID called with target PID: %1 ===").arg(targetPID));
    
    if (!enumerator_) {
        Logger::log("Audio enumerator is null!");
        return 0;
    }
    
    // Get the executable name for logging
    QString exeName = "(unknown)";
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, targetPID);
    if (hProc) {
        WCHAR buf[MAX_PATH];
        DWORD len = MAX_PATH;
        if (QueryFullProcessImageNameW(hProc, 0, buf, &len)) {
            exeName = QFileInfo(QString::fromWCharArray(buf)).fileName();
        }
        CloseHandle(hProc);
    }
    
    Logger::log(QString("Target process: %1 (PID: %2)").arg(exeName).arg(targetPID));
    
    CComPtr<IMMDeviceCollection> devs;
    HRESULT hr = enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devs);
    if (FAILED(hr)) {
        Logger::log(QString("Failed to enumerate audio endpoints. HRESULT: 0x%1").arg(hr, 0, 16));
        return 0;
    }
    Logger::log("Audio endpoints enumerated successfully");

    int total = 0;
    UINT n;
    devs->GetCount(&n);
    Logger::log(QString("Found %1 active audio render devices").arg(n));
    
    for (UINT i = 0; i < n; ++i) {
        CComPtr<IMMDevice> dev;
        if (FAILED(devs->Item(i, &dev))) {
            Logger::log(QString("Failed to get device %1").arg(i));
            continue;
        }

        // Get device friendly name
        QString deviceName = "(unknown)";
        CComPtr<IPropertyStore> props;
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props))) {
            PROPVARIANT var; 
            PropVariantInit(&var);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var))) {
                deviceName = QString::fromWCharArray(var.pwszVal);
            }
            PropVariantClear(&var);
        }
        
        Logger::log(QString("Scanning device %1: %2").arg(i).arg(deviceName));
        
        // Check if device is excluded
        if (Config::instance().isDeviceExcluded(deviceName)) {
            Logger::log(QString("Device %1 (%2) is excluded, skipping").arg(i).arg(deviceName));
            continue;
        }
        
        int deviceToggled = toggleOnDeviceByPID(dev, targetPID);
        total += deviceToggled;
        Logger::log(QString("Device %1: %2 sessions toggled").arg(i).arg(deviceToggled));
    }
    
    Logger::log(QString("=== toggleMuteByPID completed: Total sessions toggled for PID=%1: %2 ===")
                    .arg(targetPID)
                    .arg(total)
                );
    return total;
}

int AudioMuter::toggleOnDeviceByPID(IMMDevice *device, DWORD targetPID) {
    Logger::log("=== toggleOnDeviceByPID called ===");
    
    CComPtr<IAudioSessionManager2> mgr2;
    if (FAILED(device->Activate(__uuidof(IAudioSessionManager2),
                                CLSCTX_ALL,
                                nullptr,
                                (void**)&mgr2))) {
        Logger::log("Failed to activate IAudioSessionManager2");
        return 0;
    }
    Logger::log("IAudioSessionManager2 activated successfully");
    
    CComPtr<IAudioSessionEnumerator> sessEnum;
    if (FAILED(mgr2->GetSessionEnumerator(&sessEnum))) {
        Logger::log("Failed to get session enumerator");
        return 0;
    }
    Logger::log("Session enumerator obtained successfully");

    int count = 0;
    int n;
    sessEnum->GetCount(&n);
    Logger::log(QString("Total audio sessions on device: %1").arg(n));
    
    for (int i = 0; i < n; ++i) {
        CComPtr<IAudioSessionControl> ctl;
        if (FAILED(sessEnum->GetSession(i, &ctl))) {
            Logger::log(QString("Failed to get session %1").arg(i));
            continue;
        }

        CComPtr<IAudioSessionControl2> ctl2;
        if (FAILED(ctl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctl2))) {
            Logger::log(QString("Failed to get IAudioSessionControl2 for session %1").arg(i));
            continue;
        }

        DWORD pid = 0;
        if (FAILED(ctl2->GetProcessId(&pid))) {
            Logger::log(QString("Failed to get process ID for session %1").arg(i));
            continue;
        }
        
        // Get process name
        QString exeName = "(unknown)";
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc) {
            WCHAR buf[MAX_PATH];
            DWORD len = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, buf, &len)) {
                exeName = QFileInfo(QString::fromWCharArray(buf)).fileName();
            }
            CloseHandle(hProc);
        }
        
        Logger::log(QString("Session %1: PID=%2, EXE=%3").arg(i).arg(pid).arg(exeName));
        
        // Check if this process is in the exclusion list
        QString processNameWithoutExt = exeName;
        if (processNameWithoutExt.endsWith(".exe", Qt::CaseInsensitive)) {
            processNameWithoutExt = processNameWithoutExt.left(processNameWithoutExt.length() - 4);
        }
        
        if (Config::instance().isProcessExcluded(processNameWithoutExt)) {
            Logger::log(QString("Session %1: Process '%2' is in exclusion list, skipping").arg(i).arg(processNameWithoutExt));
            continue;
        }
        
        // Check if this PID matches our target PID
        if (pid != targetPID) {
            Logger::log(QString("Session %1: PID %2 not in target list, skipping").arg(i).arg(pid));
            continue;
        }

        Logger::log(QString("Session %1: PID match found! Checking mute state...").arg(i));

        CComPtr<ISimpleAudioVolume> vol;
        if (FAILED(ctl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&vol))) {
            Logger::log(QString("Failed to get ISimpleAudioVolume for session %1").arg(i));
            continue;
        }
        
        if (vol) {
            BOOL isMuted = FALSE;
            if (SUCCEEDED(vol->GetMute(&isMuted))) {
                Logger::log(QString("Session %1: Current mute state: %2").arg(i).arg(isMuted ? "Muted" : "Not muted"));
                
                // Toggle the mute state
                BOOL newMuteState = !isMuted;
                if (SUCCEEDED(vol->SetMute(newMuteState, nullptr))) {
                    ++count;
                    Logger::log(QString("Session %1: Successfully toggled mute to %2 for session pid=%3").arg(i).arg(newMuteState ? "Muted" : "Unmuted").arg(pid));
                } else {
                    Logger::log(QString("Session %1: Failed to toggle mute for session pid=%2").arg(i).arg(pid));
                }
            } else {
                Logger::log(QString("Session %1: Failed to get mute state for session pid=%2").arg(i).arg(pid));
            }
        } else {
            Logger::log(QString("Session %1: ISimpleAudioVolume is null").arg(i));
        }
    }
    
    Logger::log(QString("toggleOnDeviceByPID completed: %1 sessions toggled").arg(count));
    return count;
}

int AudioMuter::increaseVolumeByExeName(const QString& targetExeName, float stepPercent) {
    Logger::log(QString("=== increaseVolumeByExeName called with target EXE: %1, step: %2% ===").arg(targetExeName).arg(stepPercent));
    
    if (!enumerator_) {
        Logger::log("Audio enumerator is null!");
        return 0;
    }
    
    CComPtr<IMMDeviceCollection> devs;
    HRESULT hr = enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devs);
    if (FAILED(hr)) {
        Logger::log(QString("Failed to enumerate audio endpoints. HRESULT: 0x%1").arg(hr, 0, 16));
        return 0;
    }
    Logger::log("Audio endpoints enumerated successfully");

    int total = 0;
    UINT n;
    devs->GetCount(&n);
    Logger::log(QString("Found %1 active audio render devices").arg(n));
    
    for (UINT i = 0; i < n; ++i) {
        CComPtr<IMMDevice> dev;
        if (FAILED(devs->Item(i, &dev))) {
            Logger::log(QString("Failed to get device %1").arg(i));
            continue;
        }

        // Get device friendly name
        QString deviceName = "(unknown)";
        CComPtr<IPropertyStore> props;
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props))) {
            PROPVARIANT var; 
            PropVariantInit(&var);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var))) {
                deviceName = QString::fromWCharArray(var.pwszVal);
            }
            PropVariantClear(&var);
        }
        
        Logger::log(QString("Scanning device %1: %2").arg(i).arg(deviceName));
        
        // Check if device is excluded
        if (Config::instance().isDeviceExcluded(deviceName)) {
            Logger::log(QString("Device %1 (%2) is excluded, skipping").arg(i).arg(deviceName));
            continue;
        }
        
        int deviceAdjusted = adjustVolumeOnDevice(dev, targetExeName, stepPercent);
        total += deviceAdjusted;
        Logger::log(QString("Device %1: %2 sessions adjusted").arg(i).arg(deviceAdjusted));
    }
    
    Logger::log(QString("=== increaseVolumeByExeName completed: Total sessions adjusted for exe=%1: %2 ===")
                    .arg(targetExeName)
                    .arg(total)
                );
    return total;
}

int AudioMuter::decreaseVolumeByExeName(const QString& targetExeName, float stepPercent) {
    Logger::log(QString("=== decreaseVolumeByExeName called with target EXE: %1, step: %2% ===").arg(targetExeName).arg(stepPercent));
    
    if (!enumerator_) {
        Logger::log("Audio enumerator is null!");
        return 0;
    }
    
    CComPtr<IMMDeviceCollection> devs;
    HRESULT hr = enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devs);
    if (FAILED(hr)) {
        Logger::log(QString("Failed to enumerate audio endpoints. HRESULT: 0x%1").arg(hr, 0, 16));
        return 0;
    }
    Logger::log("Audio endpoints enumerated successfully");

    int total = 0;
    UINT n;
    devs->GetCount(&n);
    Logger::log(QString("Found %1 active audio render devices").arg(n));
    
    for (UINT i = 0; i < n; ++i) {
        CComPtr<IMMDevice> dev;
        if (FAILED(devs->Item(i, &dev))) {
            Logger::log(QString("Failed to get device %1").arg(i));
            continue;
        }

        // Get device friendly name
        QString deviceName = "(unknown)";
        CComPtr<IPropertyStore> props;
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props))) {
            PROPVARIANT var; 
            PropVariantInit(&var);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var))) {
                deviceName = QString::fromWCharArray(var.pwszVal);
            }
            PropVariantClear(&var);
        }
        
        Logger::log(QString("Scanning device %1: %2").arg(i).arg(deviceName));
        
        // Check if device is excluded
        if (Config::instance().isDeviceExcluded(deviceName)) {
            Logger::log(QString("Device %1 (%2) is excluded, skipping").arg(i).arg(deviceName));
            continue;
        }
        
        int deviceAdjusted = adjustVolumeOnDevice(dev, targetExeName, -stepPercent);
        total += deviceAdjusted;
        Logger::log(QString("Device %1: %2 sessions adjusted").arg(i).arg(deviceAdjusted));
    }
    
    Logger::log(QString("=== decreaseVolumeByExeName completed: Total sessions adjusted for exe=%1: %2 ===")
                    .arg(targetExeName)
                    .arg(total)
                );
    return total;
}

int AudioMuter::increaseVolumeByPID(DWORD targetPID, float stepPercent) {
    Logger::log(QString("=== increaseVolumeByPID called with target PID: %1, step: %2% ===").arg(targetPID).arg(stepPercent));
    
    if (!enumerator_) {
        Logger::log("Audio enumerator is null!");
        return 0;
    }
    
    // Get the executable name for logging
    QString exeName = "(unknown)";
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, targetPID);
    if (hProc) {
        WCHAR buf[MAX_PATH];
        DWORD len = MAX_PATH;
        if (QueryFullProcessImageNameW(hProc, 0, buf, &len)) {
            exeName = QFileInfo(QString::fromWCharArray(buf)).fileName();
        }
        CloseHandle(hProc);
    }
    
    Logger::log(QString("Target process: %1 (PID: %2)").arg(exeName).arg(targetPID));
    
    CComPtr<IMMDeviceCollection> devs;
    HRESULT hr = enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devs);
    if (FAILED(hr)) {
        Logger::log(QString("Failed to enumerate audio endpoints. HRESULT: 0x%1").arg(hr, 0, 16));
        return 0;
    }
    Logger::log("Audio endpoints enumerated successfully");

    int total = 0;
    UINT n;
    devs->GetCount(&n);
    Logger::log(QString("Found %1 active audio render devices").arg(n));
    
    for (UINT i = 0; i < n; ++i) {
        CComPtr<IMMDevice> dev;
        if (FAILED(devs->Item(i, &dev))) {
            Logger::log(QString("Failed to get device %1").arg(i));
            continue;
        }

        // Get device friendly name
        QString deviceName = "(unknown)";
        CComPtr<IPropertyStore> props;
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props))) {
            PROPVARIANT var; 
            PropVariantInit(&var);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var))) {
                deviceName = QString::fromWCharArray(var.pwszVal);
            }
            PropVariantClear(&var);
        }
        
        Logger::log(QString("Scanning device %1: %2").arg(i).arg(deviceName));
        
        // Check if device is excluded
        if (Config::instance().isDeviceExcluded(deviceName)) {
            Logger::log(QString("Device %1 (%2) is excluded, skipping").arg(i).arg(deviceName));
            continue;
        }
        
        int deviceAdjusted = adjustVolumeOnDeviceByPID(dev, targetPID, stepPercent);
        total += deviceAdjusted;
        Logger::log(QString("Device %1: %2 sessions adjusted").arg(i).arg(deviceAdjusted));
    }
    
    Logger::log(QString("=== increaseVolumeByPID completed: Total sessions adjusted for PID=%1: %2 ===")
                    .arg(targetPID)
                    .arg(total)
                );
    return total;
}

int AudioMuter::decreaseVolumeByPID(DWORD targetPID, float stepPercent) {
    Logger::log(QString("=== decreaseVolumeByPID called with target PID: %1, step: %2% ===").arg(targetPID).arg(stepPercent));
    
    if (!enumerator_) {
        Logger::log("Audio enumerator is null!");
        return 0;
    }
    
    // Get the executable name for logging
    QString exeName = "(unknown)";
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, targetPID);
    if (hProc) {
        WCHAR buf[MAX_PATH];
        DWORD len = MAX_PATH;
        if (QueryFullProcessImageNameW(hProc, 0, buf, &len)) {
            exeName = QFileInfo(QString::fromWCharArray(buf)).fileName();
        }
        CloseHandle(hProc);
    }
    
    Logger::log(QString("Target process: %1 (PID: %2)").arg(exeName).arg(targetPID));
    
    CComPtr<IMMDeviceCollection> devs;
    HRESULT hr = enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devs);
    if (FAILED(hr)) {
        Logger::log(QString("Failed to enumerate audio endpoints. HRESULT: 0x%1").arg(hr, 0, 16));
        return 0;
    }
    Logger::log("Audio endpoints enumerated successfully");

    int total = 0;
    UINT n;
    devs->GetCount(&n);
    Logger::log(QString("Found %1 active audio render devices").arg(n));
    
    for (UINT i = 0; i < n; ++i) {
        CComPtr<IMMDevice> dev;
        if (FAILED(devs->Item(i, &dev))) {
            Logger::log(QString("Failed to get device %1").arg(i));
            continue;
        }

        // Get device friendly name
        QString deviceName = "(unknown)";
        CComPtr<IPropertyStore> props;
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props))) {
            PROPVARIANT var; 
            PropVariantInit(&var);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var))) {
                deviceName = QString::fromWCharArray(var.pwszVal);
            }
            PropVariantClear(&var);
        }
        
        Logger::log(QString("Scanning device %1: %2").arg(i).arg(deviceName));
        
        // Check if device is excluded
        if (Config::instance().isDeviceExcluded(deviceName)) {
            Logger::log(QString("Device %1 (%2) is excluded, skipping").arg(i).arg(deviceName));
            continue;
        }
        
        int deviceAdjusted = adjustVolumeOnDeviceByPID(dev, targetPID, -stepPercent);
        total += deviceAdjusted;
        Logger::log(QString("Device %1: %2 sessions adjusted").arg(i).arg(deviceAdjusted));
    }
    
    Logger::log(QString("=== decreaseVolumeByPID completed: Total sessions adjusted for PID=%1: %2 ===")
                    .arg(targetPID)
                    .arg(total)
                );
    return total;
}

float AudioMuter::adjustVolumeOnSession(ISimpleAudioVolume *vol, float stepPercent) {
    if (!vol) {
        return -1.0f;
    }
    
    float currentVolume = 0.0f;
    if (FAILED(vol->GetMasterVolume(&currentVolume))) {
        return -1.0f;
    }
    
    float stepFloat = stepPercent / 100.0f;
    float newVolume = currentVolume + stepFloat;
    
    // Clamp to valid range [0.0, 1.0]
    if (newVolume > 1.0f) {
        newVolume = 1.0f;
    } else if (newVolume < 0.0f) {
        newVolume = 0.0f;
    }
    
    if (SUCCEEDED(vol->SetMasterVolume(newVolume, nullptr))) {
        Logger::log(QString("Successfully adjusted volume from %1 to %2").arg(currentVolume).arg(newVolume));
        return newVolume;
    }
    
    return -1.0f;
}

int AudioMuter::adjustVolumeOnDevice(IMMDevice *device, const QString& targetExeName, float stepPercent) {
    Logger::log("=== adjustVolumeOnDevice called ===");
    
    CComPtr<IAudioSessionManager2> mgr2;
    if (FAILED(device->Activate(__uuidof(IAudioSessionManager2),
                                CLSCTX_ALL,
                                nullptr,
                                (void**)&mgr2))) {
        Logger::log("Failed to activate IAudioSessionManager2");
        return 0;
    }
    Logger::log("IAudioSessionManager2 activated successfully");
    
    CComPtr<IAudioSessionEnumerator> sessEnum;
    if (FAILED(mgr2->GetSessionEnumerator(&sessEnum))) {
        Logger::log("Failed to get session enumerator");
        return 0;
    }
    Logger::log("Session enumerator obtained successfully");

    int count = 0;
    int n;
    sessEnum->GetCount(&n);
    Logger::log(QString("Total audio sessions on device: %1").arg(n));
    
    for (int i = 0; i < n; ++i) {
        CComPtr<IAudioSessionControl> ctl;
        if (FAILED(sessEnum->GetSession(i, &ctl))) {
            Logger::log(QString("Failed to get session %1").arg(i));
            continue;
        }

        CComPtr<IAudioSessionControl2> ctl2;
        if (FAILED(ctl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctl2))) {
            Logger::log(QString("Failed to get IAudioSessionControl2 for session %1").arg(i));
            continue;
        }

        DWORD pid = 0;
        if (FAILED(ctl2->GetProcessId(&pid))) {
            Logger::log(QString("Failed to get process ID for session %1").arg(i));
            continue;
        }
        
        // Get process name (reuse existing detection logic)
        QString exeName = "(unknown)";
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc) {
            WCHAR buf[MAX_PATH];
            DWORD len = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, buf, &len)) {
                exeName = QFileInfo(QString::fromWCharArray(buf)).fileName();
            }
            CloseHandle(hProc);
        }
        
        Logger::log(QString("Session %1: PID=%2, EXE=%3, Target EXE=%4").arg(i).arg(pid).arg(exeName, targetExeName));
        
        // Check if this process is in the exclusion list
        QString processNameWithoutExt = exeName;
        if (processNameWithoutExt.endsWith(".exe", Qt::CaseInsensitive)) {
            processNameWithoutExt = processNameWithoutExt.left(processNameWithoutExt.length() - 4);
        }
        
        if (Config::instance().isProcessExcluded(processNameWithoutExt)) {
            Logger::log(QString("Session %1: Process '%2' is in exclusion list, skipping").arg(i).arg(processNameWithoutExt));
            continue;
        }
        
        if (exeName.compare(targetExeName, Qt::CaseInsensitive) != 0) {
            Logger::log(QString("Session %1: Executable mismatch, skipping").arg(i));
            continue;
        }

        Logger::log(QString("Session %1: Executable match found! Adjusting volume...").arg(i));

        CComPtr<ISimpleAudioVolume> vol;
        if (FAILED(ctl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&vol))) {
            Logger::log(QString("Failed to get ISimpleAudioVolume for session %1").arg(i));
            continue;
        }
        
        float newVolume = adjustVolumeOnSession(vol, stepPercent);
        if (newVolume >= 0.0f) {
            ++count;
            Logger::log(QString("Session %1: Successfully adjusted volume for session pid=%2").arg(i).arg(pid));
        } else {
            Logger::log(QString("Session %1: Failed to adjust volume for session pid=%2").arg(i).arg(pid));
        }
    }
    
    Logger::log(QString("adjustVolumeOnDevice completed: %1 sessions adjusted").arg(count));
    return count;
}

int AudioMuter::adjustVolumeOnDeviceByPID(IMMDevice *device, DWORD targetPID, float stepPercent) {
    Logger::log("=== adjustVolumeOnDeviceByPID called ===");
    
    CComPtr<IAudioSessionManager2> mgr2;
    if (FAILED(device->Activate(__uuidof(IAudioSessionManager2),
                                CLSCTX_ALL,
                                nullptr,
                                (void**)&mgr2))) {
        Logger::log("Failed to activate IAudioSessionManager2");
        return 0;
    }
    Logger::log("IAudioSessionManager2 activated successfully");
    
    CComPtr<IAudioSessionEnumerator> sessEnum;
    if (FAILED(mgr2->GetSessionEnumerator(&sessEnum))) {
        Logger::log("Failed to get session enumerator");
        return 0;
    }
    Logger::log("Session enumerator obtained successfully");

    int count = 0;
    int n;
    sessEnum->GetCount(&n);
    Logger::log(QString("Total audio sessions on device: %1").arg(n));
    
    for (int i = 0; i < n; ++i) {
        CComPtr<IAudioSessionControl> ctl;
        if (FAILED(sessEnum->GetSession(i, &ctl))) {
            Logger::log(QString("Failed to get session %1").arg(i));
            continue;
        }

        CComPtr<IAudioSessionControl2> ctl2;
        if (FAILED(ctl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctl2))) {
            Logger::log(QString("Failed to get IAudioSessionControl2 for session %1").arg(i));
            continue;
        }

        DWORD pid = 0;
        if (FAILED(ctl2->GetProcessId(&pid))) {
            Logger::log(QString("Failed to get process ID for session %1").arg(i));
            continue;
        }
        
        // Get process name (reuse existing detection logic)
        QString exeName = "(unknown)";
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc) {
            WCHAR buf[MAX_PATH];
            DWORD len = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, buf, &len)) {
                exeName = QFileInfo(QString::fromWCharArray(buf)).fileName();
            }
            CloseHandle(hProc);
        }
        
        Logger::log(QString("Session %1: PID=%2, EXE=%3").arg(i).arg(pid).arg(exeName));
        
        // Check if this process is in the exclusion list
        QString processNameWithoutExt = exeName;
        if (processNameWithoutExt.endsWith(".exe", Qt::CaseInsensitive)) {
            processNameWithoutExt = processNameWithoutExt.left(processNameWithoutExt.length() - 4);
        }
        
        if (Config::instance().isProcessExcluded(processNameWithoutExt)) {
            Logger::log(QString("Session %1: Process '%2' is in exclusion list, skipping").arg(i).arg(processNameWithoutExt));
            continue;
        }
        
        // Check if this PID matches our target PID
        if (pid != targetPID) {
            Logger::log(QString("Session %1: PID %2 not in target list, skipping").arg(i).arg(pid));
            continue;
        }

        Logger::log(QString("Session %1: PID match found! Adjusting volume...").arg(i));

        CComPtr<ISimpleAudioVolume> vol;
        if (FAILED(ctl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&vol))) {
            Logger::log(QString("Failed to get ISimpleAudioVolume for session %1").arg(i));
            continue;
        }
        
        float newVolume = adjustVolumeOnSession(vol, stepPercent);
        if (newVolume >= 0.0f) {
            ++count;
            Logger::log(QString("Session %1: Successfully adjusted volume for session pid=%2").arg(i).arg(pid));
        } else {
            Logger::log(QString("Session %1: Failed to adjust volume for session pid=%2").arg(i).arg(pid));
        }
    }
    
    Logger::log(QString("adjustVolumeOnDeviceByPID completed: %1 sessions adjusted").arg(count));
    return count;
}

float AudioMuter::getVolumeByExeName(const QString& targetExeName) {
    Logger::log(QString("=== getVolumeByExeName called with target EXE: %1 ===").arg(targetExeName));
    
    if (!enumerator_) {
        Logger::log("Enumerator not initialized");
        return -1.0f;
    }
    
    CComPtr<IMMDeviceCollection> devices;
    if (FAILED(enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) {
        Logger::log("Failed to enumerate audio endpoints");
        return -1.0f;
    }
    
    UINT count = 0;
    devices->GetCount(&count);
    Logger::log(QString("Found %1 active render devices").arg(count));
    
    float totalVolume = 0.0f;
    int sessionCount = 0;
    
    for (UINT i = 0; i < count; ++i) {
        CComPtr<IMMDevice> device;
        if (FAILED(devices->Item(i, &device))) {
            continue;
        }
        
        CComPtr<IPropertyStore> props;
        if (FAILED(device->OpenPropertyStore(STGM_READ, &props))) {
            continue;
        }
        
        PROPVARIANT nameVar;
        PropVariantInit(&nameVar);
        if (FAILED(props->GetValue(PKEY_Device_FriendlyName, &nameVar))) {
            continue;
        }
        
        QString deviceName = QString::fromWCharArray(nameVar.pwszVal);
        PropVariantClear(&nameVar);
        
        if (Config::instance().isDeviceExcluded(deviceName)) {
            Logger::log(QString("Device '%1' is excluded, skipping").arg(deviceName));
            continue;
        }
        
        CComPtr<IAudioSessionManager2> mgr2;
        if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&mgr2))) {
            continue;
        }
        
        CComPtr<IAudioSessionEnumerator> sessEnum;
        if (FAILED(mgr2->GetSessionEnumerator(&sessEnum))) {
            continue;
        }
        
        int n;
        sessEnum->GetCount(&n);
        
        for (int j = 0; j < n; ++j) {
            CComPtr<IAudioSessionControl> ctl;
            if (FAILED(sessEnum->GetSession(j, &ctl))) {
                continue;
            }
            
            CComPtr<IAudioSessionControl2> ctl2;
            if (FAILED(ctl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctl2))) {
                continue;
            }
            
            DWORD pid = 0;
            if (FAILED(ctl2->GetProcessId(&pid))) {
                continue;
            }
            
            QString exeName = "(unknown)";
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                WCHAR buf[MAX_PATH];
                DWORD len = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, buf, &len)) {
                    exeName = QFileInfo(QString::fromWCharArray(buf)).fileName();
                }
                CloseHandle(hProc);
            }
            
            QString processNameWithoutExt = exeName;
            if (processNameWithoutExt.endsWith(".exe", Qt::CaseInsensitive)) {
                processNameWithoutExt = processNameWithoutExt.left(processNameWithoutExt.length() - 4);
            }
            
            if (Config::instance().isProcessExcluded(processNameWithoutExt)) {
                continue;
            }
            
            if (exeName.compare(targetExeName, Qt::CaseInsensitive) != 0) {
                continue;
            }
            
            CComPtr<ISimpleAudioVolume> vol;
            if (FAILED(ctl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&vol))) {
                continue;
            }
            
            float volume = 0.0f;
            if (SUCCEEDED(vol->GetMasterVolume(&volume))) {
                totalVolume += volume;
                ++sessionCount;
            }
        }
    }
    
    if (sessionCount > 0) {
        float avgVolume = totalVolume / sessionCount;
        Logger::log(QString("getVolumeByExeName completed: Average volume for %1 sessions = %2").arg(sessionCount).arg(avgVolume));
        return avgVolume;
    }
    
    Logger::log("getVolumeByExeName completed: No matching sessions found");
    return -1.0f;
}

float AudioMuter::getVolumeByPID(DWORD targetPID) {
    Logger::log(QString("=== getVolumeByPID called with target PID: %1 ===").arg(targetPID));
    
    if (!enumerator_) {
        Logger::log("Enumerator not initialized");
        return -1.0f;
    }
    
    CComPtr<IMMDeviceCollection> devices;
    if (FAILED(enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) {
        Logger::log("Failed to enumerate audio endpoints");
        return -1.0f;
    }
    
    UINT count = 0;
    devices->GetCount(&count);
    Logger::log(QString("Found %1 active render devices").arg(count));
    
    float totalVolume = 0.0f;
    int sessionCount = 0;
    
    for (UINT i = 0; i < count; ++i) {
        CComPtr<IMMDevice> device;
        if (FAILED(devices->Item(i, &device))) {
            continue;
        }
        
        CComPtr<IPropertyStore> props;
        if (FAILED(device->OpenPropertyStore(STGM_READ, &props))) {
            continue;
        }
        
        PROPVARIANT nameVar;
        PropVariantInit(&nameVar);
        if (FAILED(props->GetValue(PKEY_Device_FriendlyName, &nameVar))) {
            continue;
        }
        
        QString deviceName = QString::fromWCharArray(nameVar.pwszVal);
        PropVariantClear(&nameVar);
        
        if (Config::instance().isDeviceExcluded(deviceName)) {
            Logger::log(QString("Device '%1' is excluded, skipping").arg(deviceName));
            continue;
        }
        
        CComPtr<IAudioSessionManager2> mgr2;
        if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&mgr2))) {
            continue;
        }
        
        CComPtr<IAudioSessionEnumerator> sessEnum;
        if (FAILED(mgr2->GetSessionEnumerator(&sessEnum))) {
            continue;
        }
        
        int n;
        sessEnum->GetCount(&n);
        
        for (int j = 0; j < n; ++j) {
            CComPtr<IAudioSessionControl> ctl;
            if (FAILED(sessEnum->GetSession(j, &ctl))) {
                continue;
            }
            
            CComPtr<IAudioSessionControl2> ctl2;
            if (FAILED(ctl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctl2))) {
                continue;
            }
            
            DWORD pid = 0;
            if (FAILED(ctl2->GetProcessId(&pid))) {
                continue;
            }
            
            if (pid != targetPID) {
                continue;
            }
            
            QString exeName = "(unknown)";
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                WCHAR buf[MAX_PATH];
                DWORD len = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, buf, &len)) {
                    exeName = QFileInfo(QString::fromWCharArray(buf)).fileName();
                }
                CloseHandle(hProc);
            }
            
            QString processNameWithoutExt = exeName;
            if (processNameWithoutExt.endsWith(".exe", Qt::CaseInsensitive)) {
                processNameWithoutExt = processNameWithoutExt.left(processNameWithoutExt.length() - 4);
            }
            
            if (Config::instance().isProcessExcluded(processNameWithoutExt)) {
                continue;
            }
            
            CComPtr<ISimpleAudioVolume> vol;
            if (FAILED(ctl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&vol))) {
                continue;
            }
            
            float volume = 0.0f;
            if (SUCCEEDED(vol->GetMasterVolume(&volume))) {
                totalVolume += volume;
                ++sessionCount;
            }
        }
    }
    
    if (sessionCount > 0) {
        float avgVolume = totalVolume / sessionCount;
        Logger::log(QString("getVolumeByPID completed: Average volume for %1 sessions = %2").arg(sessionCount).arg(avgVolume));
        return avgVolume;
    }
    
    Logger::log("getVolumeByPID completed: No matching sessions found");
    return -1.0f;
}