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
    
    :: Remove all DLLs except the 3 essential Qt6 ones
    echo Removing all DLLs except Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll...
    
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
    
    :: Remove ALL other DLLs
    for %%f in (*.dll) do (
        if not "%%f"=="Qt6Core.dll" if not "%%f"=="Qt6Gui.dll" if not "%%f"=="Qt6Widgets.dll" (
            echo Removing: %%f
            del /q "%%f"
        )
    )
    
    echo.
    echo Removing all subfolders except platforms...
    
    :: Remove all subfolders except platforms
    for /d %%d in (*) do (
        if not "%%d"=="platforms" (
            echo Removing folder: %%d
            rmdir /s /q "%%d"
        ) else (
            echo Keeping folder: %%d
        )
    )
    
    :: Remove other common unwanted files
    echo.
    echo Removing debug and build artifacts...
    if exist "*.pdb" del /q "*.pdb"
    if exist "*.ilk" del /q "*.ilk"
    if exist "*.qrc" del /q "*.qrc"
    if exist "*.exp" del /q "*.exp"
    if exist "*.lib" del /q "*.lib"
    if exist "*.obj" del /q "*.obj"
    if exist "*.o" del /q "*.o"
    
    echo.
    echo Release directory cleaned successfully!
    echo Kept: MuteActiveWindowC.exe, Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll, platforms folder
    echo Removed: All other DLLs, all other subfolders, and build artifacts
    
) else (
    echo Error: release directory not found: %RELEASE_DIR%
    echo Make sure the path is correct and the directory exists.
)

pause 