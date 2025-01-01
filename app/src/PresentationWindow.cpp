#include "PresentationWindow.hpp"

#include <QtCore/QPropertyAnimation>

#include <QtGui/QResizeEvent>

#include <QtWidgets/QGraphicsOpacityEffect>

PresentationWindow::PresentationWindow(Presentation* presentation)
    : QMainWindow(), m_presentation(presentation)
{
    presentation->m_slides[0]->setParent(this);
    presentation->m_slides[0]->setFixedSize(size());
    presentation->m_slides[0]->show();

    setWindowTitle("Virtual Slides - " + m_presentation->m_title);

    m_nextSlideAction->setShortcuts(QList<QKeySequence>() 
        << QKeySequence(Qt::Key_Right)
        << QKeySequence("Ctrl+Shift+Right")
    );
    m_previousSlideAction->setShortcuts(QList<QKeySequence>() 
        << QKeySequence(Qt::Key_Left)
        << QKeySequence("Ctrl+Shift+Left")
    );

    connect(m_nextSlideAction, &QAction::triggered, this, [this] {
        setSlide(m_currentSlideIndex + 1);
    });
    connect(m_previousSlideAction, &QAction::triggered, this, [this] {
        setSlide(m_currentSlideIndex - 1);
    });

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

void PresentationWindow::setSlide(size_t index) {
    if(m_presentation->m_slides.size() <= index)
        return;
    size_t newIndex = index;
    size_t oldIndex = m_currentSlideIndex;

    // slide set-up
    m_presentation->m_slides[newIndex]->setParent(this);
    m_presentation->m_slides[newIndex]->setFixedSize(size());
    m_presentation->m_slides[newIndex]->show();
    m_presentation->m_slides[newIndex]->raise();


    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect();
    effect->setOpacity(1.0f);
    m_presentation->m_slides[newIndex]->setGraphicsEffect(effect);

    QPropertyAnimation *a = new QPropertyAnimation(effect, "opacity");
    a->setDuration(200);
    a->setStartValue(0.0f);
    a->setEndValue(1.0f);
    a->setEasingCurve(QEasingCurve::Type::InBack);

    connect(a, &QPropertyAnimation::finished, this, [this, oldIndex]() {
        m_presentation->m_slides[oldIndex]->hide();
    });
    a->start(QPropertyAnimation::DeleteWhenStopped);


    m_currentSlideIndex = newIndex;
}

void PresentationWindow::showFullScreen() {
    QScreen *screen = QGuiApplication::primaryScreen();
    move(screen->geometry().x(), screen->geometry().y());
    resize(screen->geometry().width(), screen->geometry().height());
    
    QMainWindow::showFullScreen();
}