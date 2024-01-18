#include <cassert>

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

void Application::CleanUp(){
    delete m_instance;
    m_instance = nullptr;
}
