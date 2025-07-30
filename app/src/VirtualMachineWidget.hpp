#ifndef VIRTUALMACHINEWIDGET_HPP
#define VIRTUALMACHINEWIDGET_HPP

#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QSpacerItem>

#include "VirtualMachine.hpp"

class VmTaskList;
class QTermWidget;

class TerminalEventFilter : public QObject
{
    Q_OBJECT
public:
    TerminalEventFilter(VirtualMachineWidget* w, QTermWidget* term);
protected:
    bool eventFilter(QObject *obj, QEvent *e) override;
private:
    VirtualMachineWidget* m_vmWidget;
    QTermWidget* m_termWidget;
};

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
    QPushButton* m_tasksButton = new QPushButton(QIcon("://icons/task-list.svg"), "", this);

    QTermWidget* m_terminal = nullptr;

    QAction* m_terminalPasteAction = nullptr;
    QAction* m_terminalCopyAction = nullptr;

    QAction* m_terminalZoomOutAction = nullptr;
    QAction* m_terminalZoomInAction = nullptr;

    QAction* m_terminalSearchAction = nullptr;

    VmTaskList* m_vmTaskList = nullptr;
    TerminalEventFilter* m_termEventFilter = nullptr;
private slots:
    void handleNetworkChanged();
    void handleVmStopped();
    void handleVmStarted();
    void displayTaskList();
    
    void initTerm(bool shouldStart);
    void destoryTerm();
public slots:
    void registerSize();

    void startVm();
    void stopVm();
    void restartVm();
};

#endif // VIRTUALMACHINEWIDGET