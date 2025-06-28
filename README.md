<div align="center">

# MuteActiveWindow

A C++ and Qt rewrite of the AutoHotkey-based [MuteActiveWindow](https://github.com/tfourj/MuteActiveWindow) tool.

<img src="src/assets/maw.png" width="200">

</div>

## Features

- **Hotkey-based muting**: Press a customizable hotkey to mute/unmute the active window
- **PID-based muting**: Mute specific window processes for precise control
- **Executable-based muting**: Fallback to mute all processes with the same executable name
- **Dark mode support**: Modern dark theme with toggle option
- **Device exclusions**: Exclude specific audio devices from muting
- **Process exclusions**: Exclude specific processes from muting
- **System tray support**: Minimize to system tray with startup options
- **Cross-platform**: Built with Qt for Windows compatibility

## Download

Download the latest release from the [Releases](https://github.com/yourusername/MuteActiveWindow/releases) page.

## Quick Start

1. **Download and run** the application
2. **Set your hotkey** in the "Hotkey Settings" tab (e.g., Ctrl+F1, Alt+M)
3. **Configure settings** in the "Settings" tab:
   - Enable/disable dark mode
   - Set startup behavior
   - Configure tray options
4. **Apply settings** and start using your hotkey!

## Muting Modes

#### Executable-Based Muting
- Mutes all processes with the same executable name
- Good for general use cases
- Use when PID-based muting doesn't work as expected

#### PID-Based Muting
- Mutes only the specific window process
- Falls back to executable-based muting if no audio is found
- Perfect for applications with multiple windows (VLC, browsers)

## Settings

#### Hotkey Settings
- **Hotkey**: Set your preferred hotkey combination
- **PID-based muting**: Enable for precise window targeting

#### Device Management
- **Excluded devices**: Audio devices that won't be muted
- **Refresh devices**: Update the list of available audio devices

#### Process Exclusions
- **Excluded processes**: Processes that won't be muted
- **Add from running processes**: Select from currently running processes

#### Application Settings
- **Startup behavior**: Auto-start with Windows, start minimized
- **Tray behavior**: Close to system tray
- **Dark mode**: Toggle between light and dark themes
- **Files & folders**: Access application folder and registry settings

## Compilation

### Prerequisites

- **Qt Creator** with Qt 6+ 
- **MSVC 2022** compiler
- **Windows 10/11**

### Build Steps

1. **Clone the repository**:
   ```bash
   git clone https://github.com/tfourj/MuteActiveWindowC.git
   cd MuteActiveWindowC
   ```

2. **Open in Qt Creator**:
   - Open `MuteActiveWindowC.pro` in Qt Creator
   - Configure for Qt 6+ and MSVC 2022

3. **Build the project**:
   - Select Release configuration
   - Build the project (Ctrl+B)

4. **Deploy** (optional):
   - The project includes auto-deployment for Release builds
   - Uses `windeployqt` to include necessary Qt libraries

## License

This project is licensed under the **GNU General Public License v3.0** - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Built with Qt framework
- Uses Windows Core Audio APIs 