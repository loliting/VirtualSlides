#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <QApplication>
#include <QtCore/QSharedMemory>

#include "PresentationWindow.hpp"

class Application : public QApplication
{
Q_OBJECT

public:
    static Application* Instance();
    static Application* Instance(int &argc, char* argv[]);

    PresentationWindow* addWindow(QString presentationPath);
    
    static void CleanUp();
private:
    Application(int &argc, char *argv[]) : QApplication(argc, argv) { }
    ~Application() override { }

    static Application *m_instance;
    /* Used to achieve single app instance at max */
    static QSharedMemory *m_sharedMem;
};

#endif // APPLICATION_HPP