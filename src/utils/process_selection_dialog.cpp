#include "process_selection_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <Windows.h>
#include <TlHelp32.h>

ProcessSelectionDialog::ProcessSelectionDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Select Process to Exclude");
    setModal(true);
    resize(400, 300);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    QLabel *label = new QLabel("Select a process to add to exclusions:", this);
    mainLayout->addWidget(label);
    
    processList = new QListWidget(this);
    mainLayout->addWidget(processList);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    QPushButton *refreshButton = new QPushButton("Refresh", this);
    connect(refreshButton, &QPushButton::clicked, this, &ProcessSelectionDialog::refreshProcesses);
    buttonLayout->addWidget(refreshButton);
    
    buttonLayout->addStretch();
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttonLayout->addWidget(buttonBox);
    
    mainLayout->addLayout(buttonLayout);
    
    refreshProcesses();
}

QString ProcessSelectionDialog::getSelectedProcess() const {
    QListWidgetItem *item = processList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void ProcessSelectionDialog::refreshProcesses() {
    processList->clear();
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            QString processName = QString::fromWCharArray(pe32.szExeFile);
            
            // Remove .exe extension for display
            if (processName.endsWith(".exe", Qt::CaseInsensitive)) {
                processName = processName.left(processName.length() - 4);
            }
            
            // Add process name and PID for display
            QString displayText = QString("%1 (PID: %2)").arg(processName).arg(pe32.th32ProcessID);
            
            QListWidgetItem *item = new QListWidgetItem(displayText);
            item->setData(Qt::UserRole, processName); // Store process name without PID
            processList->addItem(item);
            
        } while (Process32NextW(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    
    // Sort the list alphabetically
    processList->sortItems();
} 