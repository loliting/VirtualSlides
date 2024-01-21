#include <cassert>

#include "QtWidgets/QMessageBox"

#include "Application.hpp"

Application* Application::m_instance = nullptr;

Application* Application::Instance() {
    assert(m_instance != nullptr);
    return m_instance;
}

Application* Application::Instance(int &argc, char* argv[]) {
    assert(m_instance == nullptr);
    m_instance = new Application(argc, argv);
    return m_instance;
}

void Application::CleanUp() {
    delete m_instance;
    m_instance = nullptr;
}

PresentationWindow* Application::addWindow(QString presentationPath){
    Presentation* presentation;
    try{
        presentation = new Presentation(presentationPath);
    }
    catch(PresentationException &e){
        QMessageBox msgBox(QMessageBox::Critical, "Virtual Slides", e.cause(), QMessageBox::Ok);
        msgBox.exec();
        return nullptr;
    }

    PresentationWindow* win = new PresentationWindow(presentation);
    win->showFullScreen();

    return win;
}