// Global variables
var lastKnownTargetDir = "";

function Controller() {
    console.log("Controller initialized");
}

// Function to read installation path from registry
function getPathFromRegistry() {
    try {
        console.log("=== getPathFromRegistry called ===");
        
        // Open registry key
        var registryKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
        var keyExists = installer.execute("reg", ["query", registryKey, "/v", "MuteActiveWindow"]);
        
        if (keyExists) {
            // Get the registry value data
            var regOutput = installer.execute("reg", ["query", registryKey, "/v", "MuteActiveWindow"]);
            if (regOutput) {
                console.log("Registry output type: " + typeof regOutput);
                console.log("Registry output: " + regOutput);
                
                // Convert to string if it's not already
                var outputStr = "";
                if (typeof regOutput === "string") {
                    outputStr = regOutput;
                } else if (regOutput && regOutput.toString) {
                    outputStr = regOutput.toString();
                } else {
                    console.log("Cannot convert registry output to string");
                    return "";
                }
                
                // Parse the REG_SZ value from output which is in format:
                // HKEY_CURRENT_USER\...\Run    MuteActiveWindow    REG_SZ    C:\Path\To\App\MuteActiveWindowC.exe
                var lines = outputStr.split("\r\n");
                console.log("Split into " + lines.length + " lines");
                
                for (var i = 0; i < lines.length; i++) {
                    var line = lines[i].trim();
                    console.log("Processing line " + i + ": " + line);
                    if (line.indexOf("REG_SZ") !== -1) {
                        // Extract path after REG_SZ
                        var parts = line.split("REG_SZ");
                        if (parts.length > 1) {
                            var path = parts[1].trim();
                            console.log("Found path: " + path);
                            
                            // Check if the executable exists
                            var fileExists = installer.fileExists(path);
                            console.log("File exists check: " + fileExists);
                            
                            if (fileExists) {
                                // Remove executable name to get directory
                                var lastBackslash = path.lastIndexOf("\\");
                                if (lastBackslash !== -1) {
                                    var dir = path.substring(0, lastBackslash);
                                    console.log("Extracted directory: " + dir);
                                    return dir;
                                }
                            } else {
                                console.log("Executable not found at: " + path);
                            }
                        }
                    }
                }
            }
        }
        
        console.log("No existing installation found in registry or executable not found");
        return "";
        
    } catch (e) {
        console.log("Error in getPathFromRegistry: " + e.message);
        return "";
    }
}

// Function to set target directory
function setTargetDirectory(dir) {
    console.log("Setting target directory to: " + dir);
    
    // Set the target directory in the installer
    installer.setValue("TargetDir", dir);
    
    // Update the UI if we can access it
    var currentPage = gui.currentPageWidget();
    if (currentPage && currentPage.TargetDirectoryLineEdit) {
        currentPage.TargetDirectoryLineEdit.text = dir;
        lastKnownTargetDir = dir;
    }
}

// Simple function to check and fix target directory
function checkAndFixTargetDirectory() {
    try {
        console.log("=== checkAndFixTargetDirectory called ===");
        
        // Get the current page widget
        var currentPage = gui.currentPageWidget();
        if (!currentPage) {
            console.log("No current page widget found");
            return false;
        }
        
        // Use hardcoded widget name
        var targetDir = currentPage.TargetDirectoryLineEdit.text;
        console.log("Using widget text TargetDir: '" + targetDir + "'");
        
        // Always log what we're checking
        if (targetDir !== lastKnownTargetDir) {
            console.log("TargetDir CHANGED from '" + lastKnownTargetDir + "' to '" + targetDir + "'");
            lastKnownTargetDir = targetDir;
        }
        
        // If target directory doesn't end with our app folder, add it
        if (targetDir && !targetDir.endsWith("/MuteActiveWindowC") && !targetDir.endsWith("\\MuteActiveWindowC")) {
            var newTargetDir = targetDir + "\\MuteActiveWindowC";
            console.log("APPENDING: " + targetDir + " -> " + newTargetDir);
            
            // Set both the installer value and update the widget
            installer.setValue("TargetDir", newTargetDir);
            currentPage.TargetDirectoryLineEdit.text = newTargetDir;
            
            lastKnownTargetDir = newTargetDir;
            console.log("SUCCESS: Target directory fixed to: " + newTargetDir);
            return true;
        } else {
            console.log("No fix needed - already ends with MuteActiveWindowC");
        }
        
        return false;
    } catch (e) {
        console.log("Error in checkAndFixTargetDirectory: " + e.message);
        return false;
    }
}

// Function to find and hook into UI widgets
function hookIntoUIWidgets() {
    try {
        console.log("Searching for UI widgets...");
        
        // Try to access the current page widget directly
        var currentPage = gui.currentPageWidget();
        if (currentPage) {
            console.log("Found current page widget");
            
            // Look for all properties that might change
            for (var prop in currentPage) {
                if (typeof currentPage[prop] === "object" && currentPage[prop] != null) {
                    // console.log("Current page has property: " + prop);
                    
                    // Hook into any widget that might change
                    var widget = currentPage[prop];
                    if (widget && widget.textChanged) {
                        widget.textChanged.connect(function() {
                            console.log("Widget text changed: " + prop);
                            // Check target directory when any widget changes
                            checkAndFixTargetDirectory();
                        });
                        console.log("Connected to " + prop + ".textChanged signal");
                    }
                }
            }
        }
        
    } catch (e) {
        console.log("Error in hookIntoUIWidgets: " + e.message);
    }
}

Controller.prototype.TargetDirectoryPageCallback = function() {
    console.log("TargetDirectoryPageCallback");
    
    // Disable the target directory line edit
    var currentPage = gui.currentPageWidget();
    if (currentPage && currentPage.TargetDirectoryLineEdit) {
        currentPage.TargetDirectoryLineEdit.enabled = false;
    }
    
    // First try to get path from registry
    var registryPath = getPathFromRegistry();
    if (registryPath) {
        console.log("Found previous installation at: " + registryPath);
        
        // Show prompt asking user if they want to use the found path
        var message = "A previous installation of MuteActiveWindowC was found at:\n\n" + 
                     registryPath + "\n\n" +
                     "Would you like to install to this location?";
        
        var result = QMessageBox.question(null, "Previous Installation Found", message,
                                        QMessageBox.Yes | QMessageBox.No);
        
        if (result === QMessageBox.Yes) {
            console.log("User chose to use previous installation path");
            setTargetDirectory(registryPath);
            return;
        } else {
            console.log("User chose not to use previous installation path");
        }
    }
    
    // If no registry path found or user declined, check and fix target directory
    checkAndFixTargetDirectory();
    hookIntoUIWidgets();
}

// Initial check when script loads
console.log("Script loaded");
checkAndFixTargetDirectory(); 