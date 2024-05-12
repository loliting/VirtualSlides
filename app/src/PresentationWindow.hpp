#ifndef PRESENTATIONWINDOW_HPP
#define PRESENTATIONWINDOW_HPP

#include <QtWidgets/QMainWindow>
#include <QtGui/QAction>

#include "Presentation.hpp"

class PresentationWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit PresentationWindow(Presentation* presentation);
    ~PresentationWindow();

    void showFullScreen();
private:
    Presentation* m_presentation = nullptr;

    quint64 m_currentSlideIndex = 0;
    QAction* m_nextSlideAction = new QAction();
    QAction* m_previousSlideAction = new QAction();
public slots:
    void nextSlide();
    void previousSlide();
protected:
    void resizeEvent(QResizeEvent *event) override;
};

#endif // PRESENTATIONWINDOW_HPP