# MuteActiveWindowC Installer

This directory contains the Qt Installer Framework (IFW) configuration for creating Windows installers.

## Directory Structure

```
installer/
├── config.xml              
├── package.xml             
├── controllerscript.qs     # Custom installer behavior script
├── create_installer.bat    # Installer creation script
├── packages/               # Temporary package structure (created during build)
├── release/                # Output directory for installers
└── README.md               # This file
```

## How It Works

The installer creation process:

1. **Temporary Structure Creation**: The script creates temporary `packages/com.muteactivewindowc/data` and `packages/com.muteactivewindowc/meta` folders
2. **File Copying**: Only essential files are copied to the data folder:
   - MuteActiveWindowC.exe
   - Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll
   - platforms folder
3. **Temporary File Generation**: 
   - Creates temporary config.xml with version replaced
   - Creates temporary package.xml with version and current date
4. **Installer Creation**: Qt IFW creates the installer in `installer/release/`
5. **Cleanup**: All temporary folders and files are removed

## Automatic Installation

The installer is automatically created when you build the project in Release mode. The process:

1. Build the project in Release mode
2. `windeployqt` copies Qt dependencies
3. `clean_release.bat` removes unnecessary files
4. `installer/create_installer.bat` creates the installer

## Manual Installation

If you need to create the installer manually:

1. Ensure you have Qt Installer Framework installed
2. Build the project in Release mode
3. Run `installer/create_installer.bat [release_path] [version]`

## Qt Installer Framework

The installer automatically detects Qt IFW in standard locations:
- `C:\Qt\Tools\QtInstallerFramework\4.x\bin\`

If not found, you'll be prompted to enter the path manually.

## Output

The installer will be created as `MuteActiveWindowC-[version]-Setup.exe` in the `installer/release/` directory.

## Customization

- Edit `config.xml` to modify installer appearance and behavior
- Edit `package.xml` to change package information and component selection
- Edit `controllerscript.qs` to customize installer behavior:
- Edit `packages/com.muteactivewindowc/meta/installscript.qs` to add custom installation logic 