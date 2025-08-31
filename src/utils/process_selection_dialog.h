#pragma once
#include <QDialog>
#include <QListWidget>
#include <QMap>
#include <Windows.h>

class ProcessSelectionDialog : public QDialog {
    Q_OBJECT

public:
    enum Mode {
        ExclusionMode,
        SimulationMode
    };

    ProcessSelectionDialog(Mode mode = ExclusionMode, QWidget *parent = nullptr);
    QString getSelectedProcess() const;
    DWORD getSelectedPID() const;

private slots:
    void refreshProcesses();

private:
    QListWidget *processList;
    Mode dialogMode_;
}; 