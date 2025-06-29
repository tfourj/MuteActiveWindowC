// Global variables
var lastKnownTargetDir = "";

function Controller() {
    console.log("Controller initialized");
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
    checkAndFixTargetDirectory();
    hookIntoUIWidgets();
    
    // Disable the target directory line edit
    var currentPage = gui.currentPageWidget();
    if (currentPage && currentPage.TargetDirectoryLineEdit) {
        currentPage.TargetDirectoryLineEdit.enabled = false;
    }
}

// Initial check when script loads
console.log("Script loaded");
checkAndFixTargetDirectory(); 