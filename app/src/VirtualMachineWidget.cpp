#include "VirtualMachineWidget.hpp"

#include <QtCore/QDebug>

#include "Application.hpp"

VirtualMachineWidget::VirtualMachineWidget(VirtualMachine* vm, QWidget* parent)
    : QWidget(parent)
{
    assert(vm);

    m_vm = vm;
    handleNetworkChanged();

    m_terminal->setColorScheme("WhiteOnBlack");
    m_terminal->setAutoClose(false);
    
    connect(m_startButton, SIGNAL(clicked(bool)),
        this, SLOT(startVm(void)));
    connect(m_stopButton, SIGNAL(clicked(bool)),
        this, SLOT(stopVm(void)));
    connect(m_restartButton, SIGNAL(clicked(bool)),
        this, SLOT(restartVm(void)));
    connect(m_vm, SIGNAL(networkChanged(void)),
        this, SLOT(handleNetworkChanged(void)));
    connect(m_vm, SIGNAL(vmStarted(void)),
        this, SLOT(handleVmStarted(void)));
    connect(m_vm, SIGNAL(vmStopped(void)),
        this, SLOT(handleVmStopped(void)));
    
    setLayout(m_layout);
    m_layout->addWidget(m_title, 0, 0);
    m_layout->addItem(m_titleSpacer, 0, 1);
    m_layout->addWidget(m_startButton, 0, 2);
    m_layout->addWidget(m_restartButton, 0, 3);
    m_layout->addWidget(m_stopButton, 0, 4);
    m_layout->addWidget(m_terminal, 1, 0, 1, 5);

    m_stopButton->setEnabled(false);
    m_restartButton->setEnabled(false);
}

VirtualMachineWidget::~VirtualMachineWidget() {
    m_terminal->close();
    m_layout->deleteLater();
    m_startButton->deleteLater();
    m_stopButton->deleteLater();
    m_restartButton->deleteLater();
    m_title->deleteLater();
    m_terminal->deleteLater();
}

void VirtualMachineWidget::startVm() {
    m_stopButton->setEnabled(false);
    m_restartButton->setEnabled(false);
    m_startButton->setEnabled(false);

    m_vm->start();
}

void VirtualMachineWidget::stopVm() {
    m_stopButton->setEnabled(false);
    m_restartButton->setEnabled(false);
    m_startButton->setEnabled(false);

    m_vm->stop();
}

void VirtualMachineWidget::restartVm() {
    m_stopButton->setEnabled(false);
    m_restartButton->setEnabled(false);
    m_startButton->setEnabled(false);

    m_vm->restart();
}

void VirtualMachineWidget::handleNetworkChanged() {
    QString title = "";
    if(m_vm->net())
        title += m_vm->net()->id() + ": ";
    title += m_vm->id();
    #if DEBUG_BUILD
    title += " (cid: " + QString::number(m_vm->cid()) + ")";
    #endif
    m_title->setText(title);
    setWindowTitle(m_title->text());
}

void VirtualMachineWidget::handleVmStopped() {
    m_stopButton->setEnabled(false);
    m_restartButton->setEnabled(false);
    m_startButton->setEnabled(true);
}

void VirtualMachineWidget::handleVmStarted() {
    m_stopButton->setEnabled(true);
    m_restartButton->setEnabled(true);
    m_startButton->setEnabled(false);

    m_terminal->close();
    m_terminal->deleteLater();

    m_terminal = new QTermWidget(0, this);
    m_terminal->setColorScheme("WhiteOnBlack");
    m_terminal->setAutoClose(false);
    m_layout->addWidget(m_terminal, 1, 0, 1, 5);
    m_terminal->setShellProgram(Application::applicationDirPath() +  "/vs-sock-stdio-connector");
    m_terminal->setArgs(QStringList(m_vm->serverName()));
    m_terminal->startShellProgram();
}