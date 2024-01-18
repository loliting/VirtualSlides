#ifndef PRESENTATIONWINDOW_HPP
#define PRESENTATIONWINDOW_HPP

#include <QtWidgets/QMainWindow>

class PresentationWindow : public QMainWindow
{
    Q_OBJECT
public:
    static PresentationWindow* Instance();
    static void CleanUp();

private:
    PresentationWindow();
    ~PresentationWindow() override { };

    static PresentationWindow* m_instance;
};

#endif // PRESENTATIONWINDOW_HPP