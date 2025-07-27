function Component() {
    console.log("Component initialized");
}

Component.prototype.createOperations = function() {
    console.log("Extract start");

    // Kill any running MuteActiveWindowC.exe processes before installation
    if (systemInfo.productType === "windows") {
        console.log("Attempting to kill any running MuteActiveWindowC.exe processes...");
        component.addOperation(
            "Execute",
            "cmd.exe",
            "/c", "taskkill /f /im MuteActiveWindowC.exe || exit 0"
        );
        console.log("Taskkill operation added.");
    }

    // Call the base implementation (Extract)
    component.createOperations();

    // Only on Windows do we add a Start-Menu shortcut
    if (systemInfo.productType === "windows") {
        // installer.value("TargetDir") and "StartMenuDir" give you the
        // actual paths at run-time, no more raw "@…@" macros
        var targetDir = installer.value("TargetDir");
        var startMenuDir = installer.value("StartMenuDir");

        var exePath = targetDir + "\\MuteActiveWindowC.exe";
        var shortcutPath = startMenuDir + "\\MuteActiveWindowC.lnk";

        console.log("Attempting to create Start Menu shortcut:");
        console.log("  Target: " + exePath);
        console.log("  Shortcut: " + shortcutPath);

        component.addOperation(
            "CreateShortcut",
            exePath,
            shortcutPath,
            "",  // use default icon from the EXE
            "workingDirectory=" + targetDir
        );

        console.log("CreateShortcut operation added.");
    } else {
        console.log("Skipping shortcut creation: not running on Windows.");
    }
};