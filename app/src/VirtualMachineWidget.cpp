#include "VirtualMachineWidget.hpp"

VirtualMachineWidget::VirtualMachineWidget(VirtualMachine* vm)
    : QWidget()
{
    assert(vm);

    m_vm = vm;
    handleNetworkChanged();

    m_terminal->setColorScheme("WhiteOnBlack");
    
    connect(m_startButton, SIGNAL(clicked(bool)),
        this, SLOT(startVm(void)));
    connect(m_stopButton, SIGNAL(clicked(bool)),
        this, SLOT(stopVm(void)));
    connect(m_restartButton, SIGNAL(clicked(bool)),
        this, SLOT(restartVm(void)));
    connect(m_vm, SIGNAL(networkChanged(void)),
        this, SLOT(handleNetworkChanged(void)));
    connect(m_terminal, SIGNAL(finished(void)),
        this, SLOT(stopVm(void)));

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

VirtualMachineWidget::~VirtualMachineWidget(){
    m_terminal->close();
    m_layout->deleteLater();
    m_startButton->deleteLater();
    m_stopButton->deleteLater();
    m_restartButton->deleteLater();
    m_title->deleteLater();
    m_terminal->deleteLater();
}

void VirtualMachineWidget::startVm(){
    m_terminal->setShellProgram("qemu-system-x86_64");
    m_terminal->setArgs(m_args);
    m_terminal->startShellProgram();
    m_stopButton->setEnabled(true);
    m_restartButton->setEnabled(true);
    m_startButton->setEnabled(false);
    m_isRunning = true;
}

void VirtualMachineWidget::stopVm(){
    disconnect(m_terminal, SIGNAL(finished(void)), 0, 0);
    delete m_terminal;

    m_isRunning = false;
    m_terminal = new QTermWidget(0, this);
    m_terminal->setColorScheme("WhiteOnBlack");
    m_layout->addWidget(m_terminal, 1, 0, 1, 5);

    m_stopButton->setEnabled(false);
    m_restartButton->setEnabled(false);
    m_startButton->setEnabled(true);

    connect(m_terminal, SIGNAL(finished(void)),
        this, SLOT(stopVm(void)));
}

void VirtualMachineWidget::restartVm(){
    stopVm();
    startVm();
}

void VirtualMachineWidget::handleNetworkChanged() {
    m_args = m_vm->getArgs();

    if(m_vm->net()){
        m_title->setText(m_vm->net()->id() + ": " + m_vm->id());
    }
    else{
        m_title->setText(m_vm->id());
    }
}