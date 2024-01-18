#include "PresentationWindow.hpp"

PresentationWindow* PresentationWindow::m_instance = nullptr;

PresentationWindow* PresentationWindow::Instance(){
    if(m_instance == nullptr)
        m_instance = new PresentationWindow();
    return m_instance;
}

void PresentationWindow::CleanUp(){
    delete m_instance;
    m_instance = nullptr;
}

PresentationWindow::PresentationWindow() : QMainWindow() {
    setWindowTitle("Virtual Slides");
}
