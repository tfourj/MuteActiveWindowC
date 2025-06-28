#pragma once
#include <QDialog>
#include <QListWidget>

class ProcessSelectionDialog : public QDialog {
    Q_OBJECT

public:
    ProcessSelectionDialog(QWidget *parent = nullptr);
    QString getSelectedProcess() const;

private slots:
    void refreshProcesses();

private:
    QListWidget *processList;
}; 