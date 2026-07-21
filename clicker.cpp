#include "clicker.h"

Clicker::Clicker(QObject *parent)
    : QObject(parent), m_button(LeftButton), m_interval(100)
{
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &Clicker::performClick);
}

Clicker::~Clicker() {
    stop();
}

void Clicker::setButton(MouseButton btn) {
    m_button = btn;
}

void Clicker::setInterval(int ms) {
    m_interval = ms;
    m_timer->setInterval(ms);
}

void Clicker::start() {
    m_timer->start(m_interval);
    emit started();
}

void Clicker::stop() {
    m_timer->stop();
    emit stopped();
}

void Clicker::performClick() {
    INPUT input[2] = {};
    DWORD downFlag, upFlag;
    if (m_button == LeftButton) {
        downFlag = MOUSEEVENTF_LEFTDOWN;
        upFlag   = MOUSEEVENTF_LEFTUP;
    } else {
        downFlag = MOUSEEVENTF_RIGHTDOWN;
        upFlag   = MOUSEEVENTF_RIGHTUP;
    }

    // 按下
    input[0].type = INPUT_MOUSE;
    input[0].mi.dwFlags = downFlag;
    // 释放
    input[1].type = INPUT_MOUSE;
    input[1].mi.dwFlags = upFlag;

    SendInput(2, input, sizeof(INPUT));
}