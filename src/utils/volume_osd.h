#pragma once
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>

class VolumeOSD : public QWidget {
    Q_OBJECT

public:
    static VolumeOSD& instance();
    void showVolumeOSD(const QString& processName, float volumePercent);
    void setCustomPosition(int x, int y);

private:
    VolumeOSD();
    ~VolumeOSD() = default;
    VolumeOSD(const VolumeOSD&) = delete;
    VolumeOSD& operator=(const VolumeOSD&) = delete;
    
    void setupUI();
    void hideAfterDelay();
    
    QLabel* contentLabel_;
    QTimer* hideTimer_;
    
    static VolumeOSD* instance_;
};

