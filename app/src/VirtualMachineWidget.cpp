#include "VirtualMachineWidget.hpp"

#include <QtCore/QDebug>

#include "Application.hpp"
#include "Network.hpp"
#include "VmTaskList.hpp"

TerminalEventFilter::TerminalEventFilter(VirtualMachineWidget* w, QTermWidget* term)
    : QObject(w), m_vmWidget(w), m_termWidget(term)
{

}

bool TerminalEventFilter::eventFilter(QObject *obj, QEvent *e) {
    if(obj == m_termWidget && e->type() == QEvent::Resize) {
        QEvent *event = new QEvent(QEvent::User);
        QCoreApplication::postEvent(obj, event);
    }
    else if(obj == m_termWidget && e->type() == QEvent::User) {
        m_vmWidget->registerSize();
        return true;
    }

    return QObject::eventFilter(obj, e);
}

VirtualMachineWidget::VirtualMachineWidget(VirtualMachine* vm, QWidget* parent)
    : QWidget(parent)
{
    assert(vm);

    m_vm = vm;
    handleNetworkChanged();

    m_terminal->setColorScheme("WhiteOnBlack");
    m_terminal->setAutoClose(false);
    m_terminal->setConfirmMultilinePaste(false);
    m_terminal->setTerminalSizeHint(false);
    
    connect(m_startButton, &QPushButton::clicked,
        this, &VirtualMachineWidget::startVm);
    connect(m_stopButton, &QPushButton::clicked,
        this, &VirtualMachineWidget::stopVm);
    connect(m_restartButton, &QPushButton::clicked,
        this, &VirtualMachineWidget::restartVm);
    connect(m_vm, &VirtualMachine::networkChanged,
        this, &VirtualMachineWidget::handleNetworkChanged);
    connect(m_vm, &VirtualMachine::vmStarted,
        this, &VirtualMachineWidget::handleVmStarted);
    connect(m_vm, &VirtualMachine::vmStopped,
        this, &VirtualMachineWidget::handleVmStopped);
    
    setLayout(m_layout);
    m_layout->addWidget(m_title, 0, 0);
    m_layout->addItem(m_titleSpacer, 0, 1);
    m_layout->addWidget(m_startButton, 0, 2);
    m_layout->addWidget(m_restartButton, 0, 3);
    m_layout->addWidget(m_stopButton, 0, 4);
    m_layout->addWidget(m_terminal, 1, 0, 1, 6);

    m_stopButton->setEnabled(false);
    m_restartButton->setEnabled(false);

    if(m_vm->m_tasks.count() > 0) {
        m_tasksButton = new QPushButton(QIcon(":/icons/task-list.png"), "", this);

        connect(m_tasksButton, &QPushButton::clicked,
            this, &VirtualMachineWidget::displayTaskList);

        m_layout->addWidget(m_tasksButton, 0, 5);
        m_tasksButton->setMaximumWidth(m_tasksButton->height());
        m_vmTaskList = new VmTaskList(m_vm->m_tasks, this);
        m_vmTaskList->setMaximumSize(256, 368);
        m_vmTaskList->setMinimumSize(256, 0);
    }
}

VirtualMachineWidget::~VirtualMachineWidget() {
    if(m_layout)
        m_layout->deleteLater();
    if(m_startButton)
        m_startButton->deleteLater();
    if(m_stopButton)
        m_stopButton->deleteLater();
    if(m_restartButton)
        m_restartButton->deleteLater();
    if(m_title)
        m_title->deleteLater();
    if(m_terminalCopyAction)
        m_terminalCopyAction->deleteLater();
    if(m_terminalPasteAction)
        m_terminalPasteAction->deleteLater();
    if(m_terminalZoomInAction)
        m_terminalZoomInAction->deleteLater();
    if(m_terminalZoomOutAction)
        m_terminalZoomOutAction->deleteLater();
    if(m_terminalSearchAction)
        m_terminalSearchAction->deleteLater();
    if(m_termEventFilter) {
        m_termEventFilter->deleteLater();
        m_termEventFilter = nullptr;
    }
    if(m_terminal) {
        m_terminal->close();
        m_terminal->deleteLater();
    }
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
    m_terminal->setVisible(false);
    m_terminal->deleteLater();

    m_terminal = new QTermWidget(0, this);
    m_terminal->setColorScheme("WhiteOnBlack");
    m_terminal->setAutoClose(false);
    m_layout->addWidget(m_terminal, 1, 0, 1, 6);
    m_terminal->setShellProgram(Application::applicationDirPath() +  "/vs-sock-stdio-connector");
    m_terminal->setArgs(QStringList(m_vm->serverName()));
    
    if(m_terminalCopyAction) {
        m_terminalCopyAction->deleteLater();
        m_terminalCopyAction = nullptr;
    }
    if(m_terminalPasteAction) {
        m_terminalPasteAction->deleteLater();
        m_terminalPasteAction = nullptr;
    }

    if(m_terminalZoomInAction) {
        m_terminalZoomInAction->deleteLater();
        m_terminalZoomInAction = nullptr;
    }
    if(m_terminalZoomOutAction) {
        m_terminalZoomOutAction->deleteLater();
        m_terminalZoomOutAction = nullptr;
    }

    if(m_termEventFilter) {
        m_termEventFilter->deleteLater();
        m_termEventFilter = nullptr;
    }

    m_terminalCopyAction = new QAction("Copy");
    m_terminalPasteAction = new QAction("Paste");

    m_terminalZoomInAction = new QAction("Zoom In");
    m_terminalZoomOutAction = new QAction("Zoom Out");
    m_terminalSearchAction = new QAction("Find");

    m_terminalCopyAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
    m_terminalPasteAction->setShortcut(QKeySequence("Ctrl+Shift+V"));
    
    m_terminalZoomInAction->setShortcut(QKeySequence("Ctrl++"));
    m_terminalZoomOutAction->setShortcut(QKeySequence("Ctrl+-"));

    m_terminalSearchAction->setShortcut(QKeySequence("Ctrl+F"));

    m_terminalZoomInAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_terminalZoomOutAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_terminalCopyAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_terminalPasteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_terminalSearchAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    m_terminal->addAction(m_terminalCopyAction);
    m_terminal->addAction(m_terminalPasteAction);

    m_terminal->addAction(m_terminalSearchAction);

    m_terminal->addAction(m_terminalZoomInAction);
    m_terminal->addAction(m_terminalZoomOutAction);

    connect(m_terminalCopyAction, &QAction::triggered,
        m_terminal, &QTermWidget::copyClipboard
    );
    connect(m_terminalPasteAction, &QAction::triggered,
        m_terminal, &QTermWidget::pasteClipboard
    );

    connect(m_terminalZoomInAction, &QAction::triggered,
        this, [this] {
            this->m_terminal->zoomIn();
            this->registerSize();
        }
    );
    connect(m_terminalZoomOutAction, &QAction::triggered,
        this, [this] {
            this->m_terminal->zoomOut();
            this->registerSize();
        }
    );

    connect(m_terminalSearchAction, &QAction::triggered,
        m_terminal, &QTermWidget::toggleShowSearchBar
    );


    m_terminal->setConfirmMultilinePaste(false);

    m_terminal->setContextMenuPolicy(Qt::ActionsContextMenu);

    m_terminal->startShellProgram();
    
    m_termEventFilter = new TerminalEventFilter(this, m_terminal);
    m_terminal->installEventFilter(m_termEventFilter);
    registerSize();
}

void VirtualMachineWidget::displayTaskList() {
    m_vmTaskList->move(mapToGlobal(QPoint(m_tasksButton->pos().x(),
        m_tasksButton->pos().y() + m_tasksButton->height()))
    );

    m_vmTaskList->updateTasksProgress();
    
    m_vmTaskList->show();
}

void VirtualMachineWidget::registerSize() {
    if(!m_vm || !m_terminal)
        return;
    
    m_vm->registerWidget(this, QSize(m_terminal->screenColumnsCount(),
        m_terminal->screenLinesCount()
    ));
}