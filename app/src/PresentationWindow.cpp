#include "PresentationWindow.hpp"

#include <QtGui/QResizeEvent>

PresentationWindow::PresentationWindow(Presentation* presentation)
    : QMainWindow(), m_presentation(presentation)
{
    presentation->m_slides[0]->setParent(this);
    presentation->m_slides[0]->setFixedSize(size());
    presentation->m_slides[0]->show();

    setWindowTitle("Virtual Slides - " + m_presentation->m_title);

    m_nextSlideAction->setShortcut(QKeySequence(Qt::Key_Right));
    m_previousSlideAction->setShortcut(QKeySequence(Qt::Key_Left));

    connect(m_nextSlideAction, SIGNAL(triggered(void)), this, SLOT(nextSlide(void)));
    connect(m_previousSlideAction, SIGNAL(triggered(void)), this, SLOT(previousSlide(void)));

    addAction(m_nextSlideAction);
    addAction(m_previousSlideAction);
}

PresentationWindow::~PresentationWindow(){
    delete m_presentation;
    delete m_nextSlideAction;
    delete m_previousSlideAction;
}

void PresentationWindow::resizeEvent(QResizeEvent *e) {
    m_presentation->m_slides[m_currentSlideIndex]->setFixedSize(e->size());

    QMainWindow::resizeEvent(e);
}

void PresentationWindow::nextSlide() {
    if(m_currentSlideIndex == m_presentation->m_slides.count() - 1)
        return;
    m_presentation->m_slides[m_currentSlideIndex + 1]->setParent(this);
    m_presentation->m_slides[m_currentSlideIndex + 1]->setFixedSize(size());
    m_presentation->m_slides[m_currentSlideIndex + 1]->show();
    m_presentation->m_slides[m_currentSlideIndex]->hide();

    ++m_currentSlideIndex;
}

void PresentationWindow::previousSlide() {
    if(m_currentSlideIndex == 0)
        return;

    m_presentation->m_slides[m_currentSlideIndex - 1]->setParent(this);
    m_presentation->m_slides[m_currentSlideIndex - 1]->setFixedSize(size());
    m_presentation->m_slides[m_currentSlideIndex - 1]->show();
    m_presentation->m_slides[m_currentSlideIndex]->hide();

    --m_currentSlideIndex;
}

void PresentationWindow::showFullScreen() {
    QScreen *screen = QGuiApplication::primaryScreen();
    move(screen->geometry().x(), screen->geometry().y());
    resize(screen->geometry().width(), screen->geometry().height());
    
    QMainWindow::showFullScreen();
}