#include "Application.hpp"
#include "PresentationWindow.hpp"

#include "Presentation.hpp"

int main(int argc, char* argv[]) {
    Application* app = Application::Instance(argc, argv);
    
    if(app == nullptr || argc <= 1){
        exit(1);
    }
    
    if(app->addWindow(argv[1]) == nullptr){
        exit(1);
    }

    int ret = app->exec();
    Application::CleanUp();
    return ret;
}