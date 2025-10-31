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
    // Widget size - but label will extend beyond for cutoff effect
    setFixedSize(250, 60);
    // Enable clipping to show cutoff effect - children will be clipped to widget bounds
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    
    // Single label for "processname: volume%" format
    contentLabel_ = new QLabel(this);
    // Position it so it extends beyond the widget bounds (partially cut off) - extends to 290px but widget is only 250px
    contentLabel_->setGeometry(10, 10, 290, 45);
    contentLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    contentLabel_->setTextFormat(Qt::RichText); // Enable HTML formatting for colored volume
    contentLabel_->setStyleSheet(
        "QLabel {"
        "  background-color: rgba(40, 40, 40, 220);"
        "  color: white;"
        "  font-size: 18px;"
        "  font-weight: bold;"
        "  padding: 10px 20px;"
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

