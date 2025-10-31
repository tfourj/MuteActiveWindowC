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

    // Increase volume for all sessions whose executable name matches targetExeName (case-insensitive)
    // stepPercent: volume step as percentage (e.g., 5.0 for 5%)
    // Returns number of sessions adjusted
    int increaseVolumeByExeName(const QString& targetExeName, float stepPercent);
    
    // Decrease volume for all sessions whose executable name matches targetExeName (case-insensitive)
    // stepPercent: volume step as percentage (e.g., 5.0 for 5%)
    // Returns number of sessions adjusted
    int decreaseVolumeByExeName(const QString& targetExeName, float stepPercent);
    
    // Increase volume for specific PID
    // stepPercent: volume step as percentage (e.g., 5.0 for 5%)
    // Returns number of sessions adjusted
    int increaseVolumeByPID(DWORD targetPID, float stepPercent);
    
    // Decrease volume for specific PID
    // stepPercent: volume step as percentage (e.g., 5.0 for 5%)
    // Returns number of sessions adjusted
    int decreaseVolumeByPID(DWORD targetPID, float stepPercent);
    
    // Get current volume for process by executable name
    // Returns average volume (0.0-1.0) of all matching sessions, or -1.0 if not found
    float getVolumeByExeName(const QString& targetExeName);
    
    // Get current volume for process by PID
    // Returns average volume (0.0-1.0) of all matching sessions, or -1.0 if not found
    float getVolumeByPID(DWORD targetPID);

private:
    // Toggle all sessions on `device` matching targetExeName
    int toggleOnDevice(IMMDevice *device, const QString& targetExeName);
    
    // Toggle all sessions on `device` matching targetPID
    int toggleOnDeviceByPID(IMMDevice *device, DWORD targetPID);
    
    // Adjust volume on all sessions on `device` matching targetExeName
    // stepPercent: volume step as percentage (positive for increase, negative for decrease)
    int adjustVolumeOnDevice(IMMDevice *device, const QString& targetExeName, float stepPercent);
    
    // Adjust volume on all sessions on `device` matching targetPID
    // stepPercent: volume step as percentage (positive for increase, negative for decrease)
    int adjustVolumeOnDeviceByPID(IMMDevice *device, DWORD targetPID, float stepPercent);
    
    // Helper: Adjust volume on a single session
    // Returns new volume (0.0-1.0) on success, -1.0 on failure
    float adjustVolumeOnSession(ISimpleAudioVolume *vol, float stepPercent);

    IMMDeviceEnumerator *enumerator_;
};
