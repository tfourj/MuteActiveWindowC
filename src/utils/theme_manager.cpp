#include "theme_manager.h"
#include "logger.h"
#include <QApplication>
#include <QStyleFactory>
#include <QColor>

ThemeManager::ThemeManager() : isDarkMode_(false) {
    setupDarkPalette();
    setupLightPalette();
}

ThemeManager& ThemeManager::instance() {
    static ThemeManager instance;
    return instance;
}

void ThemeManager::setupDarkPalette() {
    darkPalette_.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette_.setColor(QPalette::WindowText, QColor(255, 255, 255));
    darkPalette_.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette_.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette_.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    darkPalette_.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
    darkPalette_.setColor(QPalette::Text, QColor(255, 255, 255));
    darkPalette_.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette_.setColor(QPalette::ButtonText, QColor(255, 255, 255));
    darkPalette_.setColor(QPalette::BrightText, QColor(255, 0, 0));
    darkPalette_.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette_.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette_.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    
    // Additional colors for better dark theme
    darkPalette_.setColor(QPalette::Light, QColor(80, 80, 80));
    darkPalette_.setColor(QPalette::Midlight, QColor(65, 65, 65));
    darkPalette_.setColor(QPalette::Dark, QColor(35, 35, 35));
    darkPalette_.setColor(QPalette::Mid, QColor(45, 45, 45));
    darkPalette_.setColor(QPalette::Shadow, QColor(20, 20, 20));
    darkPalette_.setColor(QPalette::PlaceholderText, QColor(150, 150, 150));
}

void ThemeManager::setupLightPalette() {
    lightPalette_.setColor(QPalette::Window, QColor(240, 240, 240));
    lightPalette_.setColor(QPalette::WindowText, QColor(0, 0, 0));
    lightPalette_.setColor(QPalette::Base, QColor(255, 255, 255));
    lightPalette_.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
    lightPalette_.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    lightPalette_.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
    lightPalette_.setColor(QPalette::Text, QColor(0, 0, 0));
    lightPalette_.setColor(QPalette::Button, QColor(240, 240, 240));
    lightPalette_.setColor(QPalette::ButtonText, QColor(0, 0, 0));
    lightPalette_.setColor(QPalette::BrightText, QColor(255, 0, 0));
    lightPalette_.setColor(QPalette::Link, QColor(0, 0, 255));
    lightPalette_.setColor(QPalette::Highlight, QColor(42, 130, 218));
    lightPalette_.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    
    // Additional colors for better light theme
    lightPalette_.setColor(QPalette::Light, QColor(255, 255, 255));
    lightPalette_.setColor(QPalette::Midlight, QColor(245, 245, 245));
    lightPalette_.setColor(QPalette::Dark, QColor(180, 180, 180));
    lightPalette_.setColor(QPalette::Mid, QColor(200, 200, 200));
    lightPalette_.setColor(QPalette::Shadow, QColor(120, 120, 120));
    lightPalette_.setColor(QPalette::PlaceholderText, QColor(100, 100, 100));
}

void ThemeManager::applyDarkTheme() {
    QApplication::setPalette(darkPalette_);
    isDarkMode_ = true;
    Logger::log("Dark theme applied");
}

void ThemeManager::applyLightTheme() {
    QApplication::setPalette(lightPalette_);
    isDarkMode_ = false;
    Logger::log("Light theme applied");
}

void ThemeManager::applyTheme(bool darkMode) {
    if (darkMode) {
        applyDarkTheme();
    } else {
        applyLightTheme();
    }
}

bool ThemeManager::isDarkMode() const {
    return isDarkMode_;
} 