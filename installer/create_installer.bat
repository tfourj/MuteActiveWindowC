@echo off
setlocal enabledelayedexpansion

:: 1) Get release directory and version from arguments
if "%~1"=="" (
    echo ERROR: No release directory specified!
    echo Usage: create_installer.bat [release_directory_path] [version]
    echo Example: create_installer.bat "C:\path\to\release" "1.0.0"
    goto :EOF
)
set RELEASE_DIR=%~1

if "%~2"=="" (
    echo ERROR: No version specified!
    echo Usage: create_installer.bat [release_directory_path] [version]
    echo Example: create_installer.bat "C:\path\to\release" "1.0.0"
    goto :EOF
)
set VERSION=%~2

:: 2) Auto-detect Qt IFW bin folder
set IFW_BIN=
for %%i in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
    if exist "%%i:\Qt\Tools\QtInstallerFramework\4.10\bin\repogen.exe" (
        set IFW_BIN=%%i:\Qt\Tools\QtInstallerFramework\4.10\bin
        goto :found_ifw
    )
    if exist "%%i:\Qt\Tools\QtInstallerFramework\4.9\bin\repogen.exe" (
        set IFW_BIN=%%i:\Qt\Tools\QtInstallerFramework\4.9\bin
        goto :found_ifw
    )
    if exist "%%i:\Qt\Tools\QtInstallerFramework\4.8\bin\repogen.exe" (
        set IFW_BIN=%%i:\Qt\Tools\QtInstallerFramework\4.8\bin
        goto :found_ifw
    )
    if exist "%%i:\Qt\Tools\QtInstallerFramework\4.7\bin\repogen.exe" (
        set IFW_BIN=%%i:\Qt\Tools\QtInstallerFramework\4.7\bin
        goto :found_ifw
    )
    if exist "%%i:\Qt\Tools\QtInstallerFramework\4.6\bin\repogen.exe" (
        set IFW_BIN=%%i:\Qt\Tools\QtInstallerFramework\4.6\bin
        goto :found_ifw
    )
    if exist "%%i:\Qt\Tools\QtInstallerFramework\4.5\bin\repogen.exe" (
        set IFW_BIN=%%i:\Qt\Tools\QtInstallerFramework\4.5\bin
        goto :found_ifw
    )
    if exist "%%i:\Qt\Tools\QtInstallerFramework\4.4\bin\repogen.exe" (
        set IFW_BIN=%%i:\Qt\Tools\QtInstallerFramework\4.4\bin
        goto :found_ifw
    )
    if exist "%%i:\Qt\Tools\QtInstallerFramework\4.3\bin\repogen.exe" (
        set IFW_BIN=%%i:\Qt\Tools\QtInstallerFramework\4.3\bin
        goto :found_ifw
    )
    if exist "%%i:\Qt\Tools\QtInstallerFramework\4.2\bin\repogen.exe" (
        set IFW_BIN=%%i:\Qt\Tools\QtInstallerFramework\4.2\bin
        goto :found_ifw
    )
    if exist "%%i:\Qt\Tools\QtInstallerFramework\4.1\bin\repogen.exe" (
        set IFW_BIN=%%i:\Qt\Tools\QtInstallerFramework\4.1\bin
        goto :found_ifw
    )
    if exist "%%i:\Qt\Tools\QtInstallerFramework\4.0\bin\repogen.exe" (
        set IFW_BIN=%%i:\Qt\Tools\QtInstallerFramework\4.0\bin
        goto :found_ifw
    )
)

:found_ifw
if "!IFW_BIN!"=="" (
    echo ERROR: Qt IFW not found. Please install Qt Installer Framework.
    echo Expected locations: C:\Qt\Tools\QtInstallerFramework\4.x\bin\
    echo.
    set /p IFW_BIN="Enter Qt IFW bin folder manually: "
)

if not exist "!IFW_BIN!\repogen.exe" (
    echo ERROR: repogen.exe not found in "!IFW_BIN!".
    goto :EOF
)
if not exist "!IFW_BIN!\binarycreator.exe" (
    echo ERROR: binarycreator.exe not found in "!IFW_BIN!".
    goto :EOF
)
if not exist "!IFW_BIN!\installerbase.exe" (
    echo ERROR: installerbase.exe not found in "!IFW_BIN!".
    goto :EOF
)

echo Found Qt IFW at: !IFW_BIN!
echo Using release directory: %RELEASE_DIR%
echo Using version: %VERSION%

:: 3) Project-specific paths
set INSTALLER_ROOT=%~dp0
set PACKAGES_DIR=%INSTALLER_ROOT%packages
set CONFIG_FILE=%INSTALLER_ROOT%config.xml
set PACKAGE_XML=%INSTALLER_ROOT%package.xml
set REPOSITORY_OUTPUT_DIR=%INSTALLER_ROOT%repository
set RELEASE_OUTPUT_DIR=%INSTALLER_ROOT%release
set ONLINE_INSTALLER_NAME=MuteActiveWindowC-Online-Installer.exe

:: 4) Check if release files exist
if not exist "%RELEASE_DIR%\MuteActiveWindowC.exe" (
    echo ERROR: Release build not found at "%RELEASE_DIR%\MuteActiveWindowC.exe"
    echo Please build the project in Release mode first.
    goto :EOF
)

:: 5) Get current date for release date
for /f %%i in ('powershell -Command "Get-Date -Format 'yyyy-MM-dd'"') do set "RELEASE_DATE=%%i"
echo Release date: %RELEASE_DATE%

:: 6) Create temporary package structure
echo.
echo Creating temporary package structure...
set TEMP_PACKAGE_DIR=%PACKAGES_DIR%\com.muteactivewindowc
set DATA_DIR=%TEMP_PACKAGE_DIR%\data
set META_DIR=%TEMP_PACKAGE_DIR%\meta

:: Clean and create directories
if exist "%TEMP_PACKAGE_DIR%" rmdir /s /q "%TEMP_PACKAGE_DIR%"
mkdir "%DATA_DIR%"
mkdir "%META_DIR%"

:: 7) Copy only essential files to installer data directory
echo Copying essential files to installer data directory...

:: Copy executable
copy "%RELEASE_DIR%\MuteActiveWindowC.exe" "%DATA_DIR%\" >nul

:: Copy essential Qt6 DLLs
if exist "%RELEASE_DIR%\Qt6Core.dll" copy "%RELEASE_DIR%\Qt6Core.dll" "%DATA_DIR%\" >nul
if exist "%RELEASE_DIR%\Qt6Gui.dll" copy "%RELEASE_DIR%\Qt6Gui.dll" "%DATA_DIR%\" >nul
if exist "%RELEASE_DIR%\Qt6Widgets.dll" copy "%RELEASE_DIR%\Qt6Widgets.dll" "%DATA_DIR%\" >nul

:: Copy platforms folder
if exist "%RELEASE_DIR%\platforms" (
    xcopy "%RELEASE_DIR%\platforms" "%DATA_DIR%\platforms" /E /I /Y >nul
)

:: Copy control script to meta directory
if exist "%INSTALLER_ROOT%controllerscript.qs" (
    copy "%INSTALLER_ROOT%controllerscript.qs" "%TEMP_PACKAGE_DIR%\" >nul
    echo Control script copied: controllerscript.qs
) else (
    echo WARNING: controllerscript.qs not found in installer directory
)

echo Essential files copied: MuteActiveWindowC.exe, Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll, platforms folder

:: 8) Create temporary config and package files with version and date
echo Creating temporary config and package files...

:: Create temporary config.xml
powershell -Command "(Get-Content '%CONFIG_FILE%') -replace '@VERSION@', '%VERSION%' | Set-Content '%TEMP_PACKAGE_DIR%\temp_config.xml'"

:: Create temporary package.xml
powershell -Command "(Get-Content '%PACKAGE_XML%') -replace '@VERSION@', '%VERSION%' -replace '@RELEASE_DATE@', '%RELEASE_DATE%' | Set-Content '%META_DIR%\package.xml'"

:: 9) Create output directories
if not exist "%REPOSITORY_OUTPUT_DIR%" mkdir "%REPOSITORY_OUTPUT_DIR%"
if not exist "%RELEASE_OUTPUT_DIR%" mkdir "%RELEASE_OUTPUT_DIR%"

:: 10) Run repogen to create online repository
echo.
echo Running repogen to create online repository...
"!IFW_BIN!\repogen.exe" ^
  --packages "%PACKAGES_DIR%" ^
  "%REPOSITORY_OUTPUT_DIR%"

if exist "%REPOSITORY_OUTPUT_DIR%\Updates.xml" (
    echo.
    echo SUCCESS: Created online repository in "%REPOSITORY_OUTPUT_DIR%".
    echo Repository files:
    dir "%REPOSITORY_OUTPUT_DIR%"
) else (
    echo.
    echo ERROR: Repository not created.
    goto :cleanup
)

:: 11) Run binarycreator to create online installer
echo.
echo Running binarycreator to create online installer...
"!IFW_BIN!\binarycreator.exe" ^
  --online-only ^
  -t "!IFW_BIN!\installerbase.exe" ^
  -p "%PACKAGES_DIR%" ^
  -c "%TEMP_PACKAGE_DIR%\temp_config.xml" ^
  "%RELEASE_OUTPUT_DIR%\%ONLINE_INSTALLER_NAME%"

if exist "%RELEASE_OUTPUT_DIR%\%ONLINE_INSTALLER_NAME%" (
    echo.
    echo SUCCESS: Created online installer "%RELEASE_OUTPUT_DIR%\%ONLINE_INSTALLER_NAME%".
    echo Online installer size: 
    for %%A in ("%RELEASE_OUTPUT_DIR%\%ONLINE_INSTALLER_NAME%") do echo %%~zA bytes
    echo.
    echo To deploy online updates:
    echo 1. Upload the contents of %REPOSITORY_OUTPUT_DIR% to your web server
    echo 2. Update the repository URL in config.xml if needed
    echo 3. Distribute the online installer: %RELEASE_OUTPUT_DIR%\%ONLINE_INSTALLER_NAME%
    echo 4. Users can now install and update automatically
) else (
    echo.
    echo ERROR: Online installer not created.
)

:cleanup
:: 12) Clean up temporary package structure
echo.
echo Cleaning up temporary files...
if exist "%TEMP_PACKAGE_DIR%" rmdir /s /q "%TEMP_PACKAGE_DIR%"

endlocal 