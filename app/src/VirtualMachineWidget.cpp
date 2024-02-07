#include "VirtualMachineWidget.hpp"

#include <QtCore/QDebug>

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
    m_stopButton->setEnabled(false);
    m_restartButton->setEnabled(false);
    m_startButton->setEnabled(false);

    m_vm->start();
}

void VirtualMachineWidget::stopVm(){
    m_stopButton->setEnabled(false);
    m_restartButton->setEnabled(false);
    m_startButton->setEnabled(false);

    m_vm->stop();
}

void VirtualMachineWidget::restartVm(){
    stopVm();
    startVm();
}

void VirtualMachineWidget::handleNetworkChanged() {
    if(m_vm->net()){
        m_title->setText(m_vm->net()->id() + ": " + m_vm->id());
    }
    else{
        m_title->setText(m_vm->id());
    }
}