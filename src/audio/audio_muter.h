#pragma once
#include "logger.h"
#include <Windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <string>

class AudioMuter {
public:
    AudioMuter();
    ~AudioMuter();

    // Toggle mute state for all sessions whose executable name matches targetExeName (case-insensitive)
    // Returns number of sessions toggled
    int toggleMuteByExeName(const QString& targetExeName);
    
    // Toggle mute state for specific PID
    // Returns number of sessions toggled
    int toggleMuteByPID(DWORD targetPID);

private:
    // Toggle all sessions on `device` matching targetExeName
    int toggleOnDevice(IMMDevice *device, const QString& targetExeName);
    
    // Toggle all sessions on `device` matching targetPID
    int toggleOnDeviceByPID(IMMDevice *device, DWORD targetPID);

    IMMDeviceEnumerator *enumerator_;
};
