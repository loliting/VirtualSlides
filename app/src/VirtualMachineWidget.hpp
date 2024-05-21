#ifndef VIRTUALMACHINEWIDGET_HPP
#define VIRTUALMACHINEWIDGET_HPP

#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QSpacerItem>

#include <qtermwidget6/qtermwidget.h>

#include "VirtualMachine.hpp"

class VmTaskList;

class VirtualMachineWidget : public QWidget
{
    Q_OBJECT
public:
    VirtualMachineWidget(VirtualMachine* vm, QWidget* parent = nullptr);
    ~VirtualMachineWidget();
private:
    VirtualMachine* m_vm = nullptr;

    QLabel* m_title = new QLabel(this);
    QSpacerItem* m_titleSpacer = new QSpacerItem(60, 10, QSizePolicy::MinimumExpanding);
    QGridLayout* m_layout = new QGridLayout(this);
    
    QPushButton* m_startButton = new QPushButton(QIcon(":/icons/start.png"), "Start", this);
    QPushButton* m_stopButton = new QPushButton(QIcon(":/icons/stop.png"), "Stop", this);
    QPushButton* m_restartButton = new QPushButton(QIcon(":/icons/restart.png"), "Restart", this);
    QPushButton* m_tasksButton = nullptr;

    QTermWidget* m_terminal = new QTermWidget(0, nullptr);

    QAction* m_terminalPasteAction = nullptr;
    QAction* m_terminalCopyAction = nullptr;

    QAction* m_terminalZoomOutAction = nullptr;
    QAction* m_terminalZoomInAction = nullptr;

    QAction* m_terminalSearchAction = nullptr;

    VmTaskList* m_vmTaskList = nullptr;
private slots:
    void handleNetworkChanged();
    void handleVmStopped();
    void handleVmStarted();
    void displayTaskList();
public slots:
    void startVm();
    void stopVm();
    void restartVm();
};

#endif // VIRTUALMACHINEWIDGET