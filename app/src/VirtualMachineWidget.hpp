#ifndef VIRTUALMACHINEWIDGET_HPP
#define VIRTUALMACHINEWIDGET_HPP

#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QSpacerItem>

#include <qtermwidget6/qtermwidget.h>

class VirtualMachineWidget;

#include "VirtualMachine.hpp"

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
    QTermWidget* m_terminal = new QTermWidget(0, nullptr);

    QAction* m_terminalPasteAction;
    QAction* m_terminalCopyAction;

    QAction* m_terminalZoomOutAction;
    QAction* m_terminalZoomInAction;
private slots:
    void handleNetworkChanged();
    void handleVmStopped();
    void handleVmStarted();
public slots:
    void startVm();
    void stopVm();
    void restartVm();
};

#endif // VIRTUALMACHINEWIDGET