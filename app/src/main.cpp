#include "Application.hpp"
#include "PresentationWindow.hpp"

int main(int argc, char* argv[]){
    Application* app = Application::Instance(argc, argv);
    PresentationWindow* win = PresentationWindow::Instance();


    win->showFullScreen();

    int ret = app->exec();
    Application::CleanUp();
    return ret;
}