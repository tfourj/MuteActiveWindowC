#pragma once
#include <QObject>
#include <QApplication>
#include <QPalette>

class ThemeManager : public QObject {
    Q_OBJECT

public:
    static ThemeManager& instance();
    
    // Apply dark or light theme
    void applyDarkTheme();
    void applyLightTheme();
    void applyTheme(bool darkMode);
    
    // Get current theme state
    bool isDarkMode() const;

private:
    ThemeManager();
    ~ThemeManager() = default;
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    
    void setupDarkPalette();
    void setupLightPalette();
    
    QPalette darkPalette_;
    QPalette lightPalette_;
    bool isDarkMode_;
}; 