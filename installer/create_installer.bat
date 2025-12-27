@echo off
setlocal

REM === Accept arguments ===
set "APP_BUILD_DIR=%~1"
set "VERSION=%~2"

if not defined APP_BUILD_DIR (
    echo [ERROR] Usage: %~nx0 "path\to\deploy_dir" "version"
    pause
    exit /b 1
)

if not defined VERSION set "VERSION=1.3.2"

REM === Dynamic Paths ===
set "SCRIPT_DIR=%~dp0"
set "NSIS_SCRIPT=%SCRIPT_DIR%installer\installer.nsi"
set "INSTALLER_DIR=%SCRIPT_DIR%temp_installer"
set "MuteActiveWindowC_COPY_DIR=%INSTALLER_DIR%\MuteActiveWindowC"
echo [INFO] Using version: %VERSION%
echo [INFO] Using NSIS script: %NSIS_SCRIPT%

REM === Locate makensis.exe (PATH first, fallback to hardcoded) ===
set "MAKENSIS_PATH="

REM Try PATH
for /f "delims=" %%i in ('where makensis 2^>nul') do (
    set "MAKENSIS_PATH=%%i"
    goto :makensis_found
)

REM Fallback path
if exist "C:\Program Files (x86)\NSIS\makensis.exe" (
    set "MAKENSIS_PATH=C:\Program Files (x86)\NSIS\makensis.exe"
)

:makensis_found
if not defined MAKENSIS_PATH (
    echo [ERROR] makensis.exe not found
    echo [HINT] Install NSIS and add it to PATH
    pause
    exit /b 1
)

echo [INFO] Using makensis: %MAKENSIS_PATH%

REM === Remove old MuteActiveWindowC folder if exists ===
if exist "%MuteActiveWindowC_COPY_DIR%" (
    echo [INFO] Removing old MuteActiveWindowC folder...
    rmdir /s /q "%MuteActiveWindowC_COPY_DIR%"
)

REM === Create installer directory ===
if not exist "%INSTALLER_DIR%" mkdir "%INSTALLER_DIR%"

REM === Copy new build files ===
echo [INFO] Copying app files to installer folder...
if exist "%MuteActiveWindowC_COPY_DIR%" rmdir /s /q "%MuteActiveWindowC_COPY_DIR%"
mkdir "%MuteActiveWindowC_COPY_DIR%"
xcopy "%APP_BUILD_DIR%\*" "%MuteActiveWindowC_COPY_DIR%\" /E /I /Y
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to copy files from %APP_BUILD_DIR%
    pause
    exit /b 1
)
echo [INFO] Files copied successfully
dir "%MuteActiveWindowC_COPY_DIR%" /B

REM === Copy NSIS script and license to temp directory ===
set "TEMP_NSIS_SCRIPT=%INSTALLER_DIR%\installer.nsi"
set "LICENSE_FILE=%SCRIPT_DIR%installer\license.txt"
set "TEMP_LICENSE_FILE=%INSTALLER_DIR%\license.txt"
set "VERSION_NSH_FILE=%INSTALLER_DIR%\version.nsh"
copy "%NSIS_SCRIPT%" "%TEMP_NSIS_SCRIPT%"
copy "%LICENSE_FILE%" "%TEMP_LICENSE_FILE%"

REM === Generate version.nsh file from VERSION parameter ===
echo [INFO] Generating version.nsh with version: %VERSION%

REM Parse version string (e.g., "2.0.0" -> MAJOR=2, MINOR=0, BUILD=0)
for /f "tokens=1,2,3 delims=." %%a in ("%VERSION%") do (
    set "VERSION_MAJOR=%%a"
    set "VERSION_MINOR=%%b"
    set "VERSION_BUILD=%%c"
)

REM Handle cases where build number might be missing
if not defined VERSION_BUILD set "VERSION_BUILD=0"

echo ; Auto-generated version file > "%VERSION_NSH_FILE%"
echo ; Generated from Qt project VERSION = %VERSION% >> "%VERSION_NSH_FILE%"
echo !define VERSIONMAJOR %VERSION_MAJOR% >> "%VERSION_NSH_FILE%"
echo !define VERSIONMINOR %VERSION_MINOR% >> "%VERSION_NSH_FILE%"
echo !define VERSIONBUILD %VERSION_BUILD% >> "%VERSION_NSH_FILE%"
echo !define FULLVERSION "%VERSION%" >> "%VERSION_NSH_FILE%"

REM === Debug: Show generated version file ===
echo [DEBUG] Generated version.nsh contents:
type "%VERSION_NSH_FILE%"

REM === Debug: Check directory structure ===
echo [DEBUG] Contents of installer directory:
dir "%INSTALLER_DIR%" /B
echo [DEBUG] Contents of MuteActiveWindowC folder:
if exist "%MuteActiveWindowC_COPY_DIR%" (
    dir "%MuteActiveWindowC_COPY_DIR%" /B
) else (
    echo [ERROR] MuteActiveWindowC folder does not exist!
)

REM === Compile the NSIS script ===
echo [INFO] Compiling installer...
pushd "%INSTALLER_DIR%"
echo [DEBUG] Current directory: %CD%
echo [DEBUG] Files in current directory:
dir /B
echo [DEBUG] Checking MuteActiveWindowC folder:
if exist "MuteActiveWindowC" (
    echo [DEBUG] MuteActiveWindowC folder found - contents:
    dir "MuteActiveWindowC" /B
) else (
    echo [ERROR] MuteActiveWindowC folder NOT found in %CD%
)
"%MAKENSIS_PATH%" "installer.nsi"
set "NSIS_RESULT=%ERRORLEVEL%"
popd

REM === Check result ===
if %NSIS_RESULT% NEQ 0 (
    echo [ERROR] NSIS compilation failed!
    pause
    exit /b 1
)

REM === Move installer to build directory ===
set "OUTPUT_INSTALLER=%INSTALLER_DIR%\MuteActiveWindowC-Setup.exe"
set "FINAL_INSTALLER=%APP_BUILD_DIR%\..\MuteActiveWindowC-Setup.exe"

if exist "%OUTPUT_INSTALLER%" (
    echo [INFO] Moving installer to: %FINAL_INSTALLER%
    move "%OUTPUT_INSTALLER%" "%FINAL_INSTALLER%"
) else (
    echo [ERROR] Installer file not found at: %OUTPUT_INSTALLER%
)

REM === Clean up temporary folder ===
echo [INFO] Cleaning up temporary installer folder...
rmdir /s /q "%INSTALLER_DIR%"

if exist "%FINAL_INSTALLER%" (
    echo [SUCCESS] Installer built successfully: %FINAL_INSTALLER%
) else (
    echo [ERROR] Final installer not found!
    pause
    exit /b 1
)

echo.
endlocal