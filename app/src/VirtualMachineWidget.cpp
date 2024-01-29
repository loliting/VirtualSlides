#include "VirtualMachineWidget.hpp"

#include <csignal>

VirtualMachineWidget::VirtualMachineWidget(VirtualMachine* vm)
    : QWidget()
{
    assert(vm);

    m_vm = vm;
    networkChanged();

    m_terminal->setColorScheme("WhiteOnBlack");
    
    connect(m_startButton, SIGNAL(clicked(bool)),
        this, SLOT(startVm(void)));
    connect(m_stopButton, SIGNAL(clicked(bool)),
        this, SLOT(stopVm(void)));
    connect(m_restartButton, SIGNAL(clicked(bool)),
        this, SLOT(restartVm(void)));
    connect(m_vm, SIGNAL(networkChanged(void)),
        this, SLOT(networkChanged(void)));

    setLayout(m_layout);
    m_layout->addWidget(m_title, 0, 0);
    m_layout->addItem(m_titleSpacer, 0, 1);
    m_layout->addWidget(m_startButton, 0, 2);
    m_layout->addWidget(m_restartButton, 0, 3);
    m_layout->addWidget(m_stopButton, 0, 4);
    m_layout->addWidget(m_terminal, 1, 0, 1, 5);
}

VirtualMachineWidget::~VirtualMachineWidget(){
    m_terminal->close();
    delete m_layout;
    delete m_startButton;
    delete m_stopButton;
    delete m_restartButton;
    delete m_title;
    delete m_titleSpacer;
    delete m_terminal;
}

void VirtualMachineWidget::startVm(){
    m_terminal->setShellProgram("qemu-system-x86_64");
    m_terminal->setArgs(m_args);
    m_terminal->startShellProgram();
}

void VirtualMachineWidget::stopVm(){
    int pid = m_terminal->getShellPID();
    if(pid <= 0){
        return;
    }
    kill(pid, SIGKILL);
    delete m_terminal;
    m_terminal = new QTermWidget(0, this);
    m_terminal->setColorScheme("WhiteOnBlack");
    m_layout->addWidget(m_terminal, 1, 0, 1, 5);
}

void VirtualMachineWidget::restartVm(){
    if(m_terminal->getShellPID() <= 0){
        return;
    }
    stopVm();
    startVm();
}

void VirtualMachineWidget::networkChanged(){
    m_args = m_vm->getArgs();
    if(m_vm->net()){
        m_title->setText(m_vm->net()->id() + ": " + m_vm->id());
    }
    else{
        m_title->setText(m_vm->id());
    }
}