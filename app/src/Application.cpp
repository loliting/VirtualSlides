#include "Application.hpp"

#include <cassert>

#include <QtWidgets/QMessageBox>

#include "Config.hpp"

Application* Application::m_instance = nullptr;
QSharedMemory* Application::m_sharedMem = nullptr;

Application* Application::Instance() {
    assert(m_instance != nullptr);
    return m_instance;
}

Application* Application::Instance(int &argc, char* argv[]) {
    assert(m_instance == nullptr);
    m_instance = new Application(argc, argv);
    try{
        Config::Initializate();
    }
    catch(ConfigException &e){
        QMessageBox msgBox(QMessageBox::Critical, "Virtual Slides", e.cause(), QMessageBox::Ok);
        msgBox.exec();

        delete m_instance;
        m_instance = nullptr;
        return nullptr;
    }

    m_sharedMem = new QSharedMemory("virtual-slides.pid");
    m_sharedMem->attach(QSharedMemory::ReadOnly);
    m_sharedMem->detach();
    if(m_sharedMem->attach(QSharedMemory::ReadOnly) || !m_sharedMem->create(1)) {
        m_sharedMem->detach();
        QMessageBox msgBox(
            QMessageBox::Critical,
            "Virtual Slides",
            "Failed to acquire lock on virtual-slides.pid\n\nIs another instance running?",
            QMessageBox::Ok
        );
        msgBox.exec();

        delete m_instance;
        delete m_sharedMem;
        m_instance = nullptr;
        m_sharedMem = nullptr;
        return nullptr;
    }

    return m_instance;
}

void Application::CleanUp() {
    Config::CleanUp();
    delete m_instance;
    m_sharedMem->detach();
    delete m_sharedMem;
    m_instance = nullptr;
    m_sharedMem = nullptr;
}

PresentationWindow* Application::addWindow(QString presentationPath){
    Presentation* presentation;
    try {
        presentation = new Presentation(presentationPath);
    }
    catch(PresentationException &e){
        QMessageBox msgBox(QMessageBox::Critical, "Virtual Slides", e.cause(), QMessageBox::Ok);
        msgBox.exec();
        return nullptr;
    }

    PresentationWindow* win = new PresentationWindow(presentation);
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->showFullScreen();

    return win;
}