#include "process_selection_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMap>
#include <Windows.h>
#include <TlHelp32.h>

ProcessSelectionDialog::ProcessSelectionDialog(Mode mode, QWidget *parent) 
    : QDialog(parent), dialogMode_(mode) {
    
    if (mode == SimulationMode) {
        setWindowTitle("Select Application for Hotkey Simulation");
    } else {
        setWindowTitle("Select Process to Exclude");
    }
    setModal(true);
    resize(400, 300);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    QLabel *label;
    if (mode == SimulationMode) {
        label = new QLabel("Select an application to focus and simulate hotkey:", this);
    } else {
        label = new QLabel("Select a process to add to exclusions:", this);
    }
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

DWORD ProcessSelectionDialog::getSelectedPID() const {
    QListWidgetItem *item = processList->currentItem();
    return item ? item->data(Qt::UserRole + 1).toUInt() : 0;
}

void ProcessSelectionDialog::refreshProcesses() {
    processList->clear();
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    
    QMap<QString, DWORD> uniqueProcesses; // Map to store unique process names with their main PID
    
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            QString processName = QString::fromWCharArray(pe32.szExeFile);
            
            // Remove .exe extension for display
            if (processName.endsWith(".exe", Qt::CaseInsensitive)) {
                processName = processName.left(processName.length() - 4);
            }
            
            // Skip system processes and processes without visible windows
            if (processName.isEmpty() || 
                processName.compare("csrss", Qt::CaseInsensitive) == 0 ||
                processName.compare("winlogon", Qt::CaseInsensitive) == 0 ||
                processName.compare("services", Qt::CaseInsensitive) == 0 ||
                processName.compare("lsass", Qt::CaseInsensitive) == 0 ||
                processName.compare("svchost", Qt::CaseInsensitive) == 0 ||
                processName.compare("smss", Qt::CaseInsensitive) == 0) {
                continue;
            }
            
            // Only add if we haven't seen this process name before, or if this PID has visible windows
            if (!uniqueProcesses.contains(processName)) {
                // Check if this process has visible windows
                bool hasVisibleWindows = false;
                EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                    DWORD* data = reinterpret_cast<DWORD*>(lParam);
                    DWORD targetPID = data[0];
                    DWORD windowPID = 0;
                    
                    GetWindowThreadProcessId(hwnd, &windowPID);
                    
                    if (windowPID == targetPID && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
                        char title[256];
                        if (GetWindowTextA(hwnd, title, sizeof(title)) > 0) {
                            data[1] = 1; // Mark as having visible windows
                            return FALSE;
                        }
                    }
                    return TRUE;
                }, reinterpret_cast<LPARAM>(&pe32.th32ProcessID));
                
                DWORD checkData[2] = {pe32.th32ProcessID, 0};
                EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                    DWORD* data = reinterpret_cast<DWORD*>(lParam);
                    DWORD targetPID = data[0];
                    DWORD windowPID = 0;
                    
                    GetWindowThreadProcessId(hwnd, &windowPID);
                    
                    if (windowPID == targetPID && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
                        char title[256];
                        if (GetWindowTextA(hwnd, title, sizeof(title)) > 0) {
                            data[1] = 1;
                            return FALSE;
                        }
                    }
                    return TRUE;
                }, reinterpret_cast<LPARAM>(checkData));
                
                if (checkData[1] == 1) {
                    uniqueProcesses[processName] = pe32.th32ProcessID;
                }
            }
            
        } while (Process32NextW(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    
    // Add unique processes to the list
    for (auto it = uniqueProcesses.begin(); it != uniqueProcesses.end(); ++it) {
        QString processName = it.key();
        DWORD pid = it.value();
        
        QListWidgetItem *item = new QListWidgetItem(processName);
        item->setData(Qt::UserRole, processName); // Store process name
        item->setData(Qt::UserRole + 1, static_cast<uint>(pid)); // Store PID for focusing
        processList->addItem(item);
    }
    
    // Sort the list alphabetically
    processList->sortItems();
} 