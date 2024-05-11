#include "VmTaskList.hpp"

#include <QtGui/QResizeEvent>

#include "VirtualMachine.hpp"

VmTaskList::VmTaskList(const QList<Task*> &tasks, QWidget *parent)
    : QDialog(parent), m_tasks(tasks)
{
    setLayout(m_layout);
    setWindowFlag(Qt::FramelessWindowHint);
    installEventFilter(this);
    
    m_layout->addWidget(m_scrollArea);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea->setWidget(m_scrollWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollWidget->setLayout(m_scrollLayout);
    m_scrollLayout->setContentsMargins(0, 0, 0, 0);


    for(auto &task : m_tasks) {
        VmTaskItem* item = new VmTaskItem(task, this);
        m_items += item;
        m_scrollLayout->addWidget(item);
    }
}

bool VmTaskList::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::WindowDeactivate) {
        hide();
        return true;
    }

    return QDialog::eventFilter(object, event);
}

VmTaskItem::VmTaskItem(const Task *task, QWidget *parent)
    : QFrame(parent), m_task(task)
{
    setLayout(m_layout);
    
    m_label->setWordWrap(true);
    m_label->setText(m_task->description);

    m_layout->addWidget(m_label);
    m_layout->addWidget(m_progressBar);

    updateTaskProgress();
}

void VmTaskItem::updateTaskProgress() {
    float maxProgress = 0;

    int subtasksCount = 0;
    int doneSubtasks = 0;
    for(auto &path : m_task->taskPaths) {
        float progress = 0;
        int doneCounter = 0;
        for(auto subtask : path)
            doneCounter += subtask->done;

        progress = (float)doneCounter / (float)path.count();
        if(progress > maxProgress){
            maxProgress = progress;
            subtasksCount = path.count();
            doneSubtasks = doneCounter;
        }
    }
    
    if(subtasksCount == 0)
        subtasksCount = 1;
    m_progressBar->setRange(0, subtasksCount);
    m_progressBar->setValue(doneSubtasks);
}

void VmTaskList::updateTasksProgress() {
    for(auto &taskItem : m_items)
        taskItem->updateTaskProgress();
}
