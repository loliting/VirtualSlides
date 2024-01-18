#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <QApplication>

class Application : public QApplication
{
Q_OBJECT

public:
    
    static Application* Instance();
    static Application* Instance(int &argc, char* argv[]);
    
    static void CleanUp();
private:
    Application(int &argc, char *argv[]) : QApplication(argc, argv) { }
    ~Application() override { }

    static Application *m_instance;
};

#endif // APPLICATION_HPP