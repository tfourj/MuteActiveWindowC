@echo off
setlocal

rem usage: user_post_deploy.bat "C:\path\to\DEPLOY_DIR" "VERSION"
set "DEPLOY_DIR=%~1"
set "VERSION=%~2"
if not defined DEPLOY_DIR (
  echo Usage: %~nx0 path\to\deploy_dir version
  exit /b 1
)

rem Set default version if not provided
if not defined VERSION set "VERSION=1.3.2"

set "OUTPUT_ZIP=%DEPLOY_DIR%\..\MuteActiveWindowC.zip"
echo Repacking "%DEPLOY_DIR%" into "%OUTPUT_ZIP%" with folder "MuteActiveWindowC" using WinRAR...

rem go into the deploy dir so * matches its contents
pushd "%DEPLOY_DIR%" || (echo Failed to cd "%DEPLOY_DIR%" & exit /b 1)

rem -r = recurse into subfolders
rem -afzip = create .zip archive
rem * = all files/dirs in current dir
rem -apMuteActiveWindowC\ = store files inside archive under the MuteActiveWindowC\ prefix
"C:\Program Files\WinRAR\WinRAR.exe" a -afzip -r "%OUTPUT_ZIP%" * -apMuteActiveWindowC\

popd

rem Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"

rem Version is now passed as second argument

rem Call create_installer.bat from the same directory as this script
echo Calling installer creation script...
call "%SCRIPT_DIR%create_installer.bat" "%DEPLOY_DIR%" "%VERSION%"

echo Done! Created "%OUTPUT_ZIP%"
endlocal