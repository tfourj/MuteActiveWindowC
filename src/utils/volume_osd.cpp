#include "volume_osd.h"
#include "logger.h"
#include <QScreen>
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QPainter>

VolumeOSD* VolumeOSD::instance_ = nullptr;

VolumeOSD::VolumeOSD() : contentLabel_(nullptr), hideTimer_(nullptr) {
    instance_ = this;
    
    // Set window flags for overlay
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::X11BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    
    setupUI();
    
    // Create hide timer
    hideTimer_ = new QTimer(this);
    hideTimer_->setSingleShot(true);
    connect(hideTimer_, &QTimer::timeout, this, &VolumeOSD::hideAfterDelay);
}

VolumeOSD& VolumeOSD::instance() {
    static VolumeOSD instance;
    return instance;
}

void VolumeOSD::setupUI() {
    // Enable clipping to show cutoff effect - children will be clipped to widget bounds
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    
    // Single label for "processname: volume%" format
    // Size will be adjusted dynamically in showVolumeOSD
    contentLabel_ = new QLabel(this);
    contentLabel_->setAlignment(Qt::AlignCenter);
    contentLabel_->setTextFormat(Qt::RichText); // Enable HTML formatting for colored volume
    contentLabel_->setStyleSheet(
        "QLabel {"
        "  background-color: rgba(40, 40, 40, 220);"
        "  color: white;"
        "  font-size: 18px;"
        "  font-weight: bold;"
        "  padding: 8px 15px;"
        "  border-radius: 4px;"
        "}"
    );
}

void VolumeOSD::showVolumeOSD(const QString& processName, float volumePercent) {
    // Process name - remove .exe extension
    QString processDisplayName = processName;
    if (processDisplayName.endsWith(".exe", Qt::CaseInsensitive)) {
        processDisplayName = processDisplayName.left(processDisplayName.length() - 4);
    }
    
    // Truncate process name if too long (max ~20 chars to leave room for ": XX%")
    const int maxProcessNameLength = 20;
    if (processDisplayName.length() > maxProcessNameLength) {
        processDisplayName = processDisplayName.left(maxProcessNameLength - 3) + "...";
    }
    
    // Format as "processname: volume%" with volume in green
    int volumeValue = qRound(volumePercent * 100.0f);
    QString fullText = QString("%1: <span style='color: #4CAF50;'>%2%</span>").arg(processDisplayName).arg(volumeValue);
    contentLabel_->setText(fullText);
    
    // Calculate text width and adjust widget size dynamically
    contentLabel_->adjustSize();
    QSize textSize = contentLabel_->sizeHint();
    
    // Padding: equal left and right padding on widget, right extends for cutoff
    int widgetLeftPadding = 10; // Visible left padding from widget edge
    int widgetRightPadding = 10; // Visible right padding (before cutoff extension)
    int rightExtension = 40; // Extra space for percent cutoff on the right
    int minWidth = 180;
    
    // Widget width = text width + left padding + right padding + space for cutoff
    int calculatedWidth = qMax(minWidth, textSize.width() + widgetLeftPadding + widgetRightPadding + rightExtension);
    int widgetWidth = calculatedWidth;
    int widgetHeight = 50;
    
    // Set widget size dynamically
    setFixedSize(widgetWidth, widgetHeight);
    
    // Position label: start at widgetLeftPadding, extend beyond widget for cutoff
    int labelX = widgetLeftPadding;
    // Label width extends beyond widget: text + both paddings + right extension
    int labelWidth = textSize.width() + widgetLeftPadding + widgetRightPadding + rightExtension;
    int labelY = (widgetHeight - textSize.height()) / 2;
    contentLabel_->setGeometry(labelX, labelY, labelWidth, textSize.height());
    
    // Position will be set by MainWindow before showing
    
    // Show with fade-in
    setWindowOpacity(0.0);
    show();
    raise();
    
    // Fade in animation
    QPropertyAnimation* fadeIn = new QPropertyAnimation(this, "windowOpacity");
    fadeIn->setDuration(200);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(0.95);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
    
    // Hide after delay
    hideTimer_->stop();
    hideTimer_->start(2000); // Show for 2 seconds
    
    Logger::log(QString("Volume OSD shown: %1 at %2%").arg(processName).arg(qRound(volumePercent * 100.0f)));
}

void VolumeOSD::setCustomPosition(int x, int y) {
    QScreen* screen = QApplication::primaryScreen();
    if (!screen) {
        return;
    }
    
    QRect screenGeometry = screen->geometry();
    
    if (x < 0 || y < 0) {
        // Calculate position based on saved setting (will be called from MainWindow)
        // For now, default to center
        QPoint center = screenGeometry.center();
        move(center.x() - width() / 2, center.y() - height() / 2);
    } else {
        // Use custom position directly
        move(x, y);
    }
}

void VolumeOSD::hideAfterDelay() {
    // Fade out animation
    QPropertyAnimation* fadeOut = new QPropertyAnimation(this, "windowOpacity");
    fadeOut->setDuration(200);
    fadeOut->setStartValue(windowOpacity());
    fadeOut->setEndValue(0.0);
    connect(fadeOut, &QPropertyAnimation::finished, this, &QWidget::hide);
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

