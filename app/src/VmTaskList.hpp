#include <QtWidgets/QWidget>
#include <QtWidgets/QFrame>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QDialog>

class VirtualMachine;
class Task;

class VmTaskItem : public QFrame
{
    Q_OBJECT
public:
    VmTaskItem(const Task *task, QWidget *parent = nullptr);
    ~VmTaskItem() = default;

public slots:
    void updateTaskProgress();
private:
    const Task* m_task;

    QVBoxLayout* m_layout = new QVBoxLayout(this);
    QLabel* m_label = new QLabel(this);
    QProgressBar* m_progressBar = new QProgressBar(this);
};

class VmTaskList : public QDialog
{
    Q_OBJECT
public:
    VmTaskList(const QList<Task*> &tasks, QWidget *parent = nullptr);
    ~VmTaskList() = default;

public slots:
    void updateTasksProgress();
private:
    bool eventFilter(QObject *object, QEvent *event);
private:
    const QList<Task*> &m_tasks;

    QVBoxLayout* m_layout = new QVBoxLayout(this);

    QScrollArea* m_scrollArea = new QScrollArea(this);
    QWidget* m_scrollWidget = new QWidget(m_scrollArea);
    QVBoxLayout* m_scrollLayout = new QVBoxLayout(m_scrollWidget);

    QList<VmTaskItem*> m_items;
};

