@echo off
echo Cleaning release directory...

if "%1"=="" (
    echo Error: No release directory specified!
    echo Usage: clean_release.bat [release_directory_path]
    echo Example: clean_release.bat "C:\path\to\release"
    pause
    exit /b 1
)

set RELEASE_DIR=%1
echo Target directory: %RELEASE_DIR%

if exist "%RELEASE_DIR%" (
    cd /d "%RELEASE_DIR%"
    
    :: Check if MuteActiveWindowC.exe exists
    if not exist "MuteActiveWindowC.exe" (
        echo Error: MuteActiveWindowC.exe not found in %RELEASE_DIR%
        echo Skipping cleanup - only run when executable is present.
        pause
        exit /b 1
    )
    
    echo MuteActiveWindowC.exe found - proceeding with cleanup...
    echo.
    
    :: Remove all DLLs except the 4 essential Qt6 ones (now including Qt6Network.dll)
    echo Removing all DLLs except Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll, Qt6Network.dll...
    
    :: Keep essential Qt6 DLLs
    if exist "Qt6Core.dll" (
        echo Keeping Qt6Core.dll
    ) else (
        echo Warning: Qt6Core.dll not found
    )
    
    if exist "Qt6Gui.dll" (
        echo Keeping Qt6Gui.dll
    ) else (
        echo Warning: Qt6Gui.dll not found
    )
    
    if exist "Qt6Widgets.dll" (
        echo Keeping Qt6Widgets.dll
    ) else (
        echo Warning: Qt6Widgets.dll not found
    )
    
    if exist "Qt6Network.dll" (
        echo Keeping Qt6Network.dll
    ) else (
        echo Warning: Qt6Network.dll not found
    )
    
    :: Remove ALL other DLLs
    for %%f in (*.dll) do (
        if not "%%f"=="Qt6Core.dll" if not "%%f"=="Qt6Gui.dll" if not "%%f"=="Qt6Widgets.dll" if not "%%f"=="Qt6Network.dll" (
            echo Removing: %%f
            del /q "%%f"
        )
    )
    
    :: Remove ALL files except the defined DLLs and EXEs
    echo Removing all files except MuteActiveWindowC.exe, Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll, Qt6Network.dll...
    for %%f in (*) do (
        if /I not "%%f"=="MuteActiveWindowC.exe" if /I not "%%f"=="Qt6Core.dll" if /I not "%%f"=="Qt6Gui.dll" if /I not "%%f"=="Qt6Widgets.dll" if /I not "%%f"=="Qt6Network.dll" if not exist "%%f\" (
            echo Removing file: %%f
            del /q "%%f"
        )
    )
    
    echo.
    echo Removing all subfolders except platforms and tls...
    
    :: Remove all subfolders except platforms and tls
    for /d %%d in (*) do (
        if not "%%d"=="platforms" if not "%%d"=="tls" (
            echo Removing folder: %%d
            rmdir /s /q "%%d"
        ) else (
            echo Keeping folder: %%d
        )
    )
    
    echo.
    echo Release directory cleaned successfully!
    echo Kept: MuteActiveWindowC.exe, Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll, Qt6Network.dll, platforms folder, tls folder
    echo Removed: All other files and all other subfolders
    
) else (
    echo Error: release directory not found: %RELEASE_DIR%
    echo Make sure the path is correct and the directory exists.
)

pause 