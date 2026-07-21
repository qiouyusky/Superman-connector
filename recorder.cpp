#include "recorder.h"
#include <QFile>
#include <QDataStream>
#include <QThread>
#include <QCoreApplication>
#include <QDebug>
#include <chrono>

Recorder *Recorder::s_instance = nullptr;

Recorder::Recorder(QObject *parent)
    : QObject(parent), m_mouseHook(nullptr), m_keyHook(nullptr),
      m_recording(false), m_playing(false), m_stopPlay(false)
{
    s_instance = this;
}

Recorder::~Recorder() {
    stopRecording();
    stopPlayback();
    s_instance = nullptr;
}

void Recorder::startRecording() {
    if (m_recording) return;
    m_actions.clear();
    m_startTick = GetTickCount();
    m_recording = true;

    // 设置低级鼠标钩子
    m_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, mouseHookProc, GetModuleHandle(nullptr), 0);
    m_keyHook = SetWindowsHookEx(WH_KEYBOARD_LL, keyHookProc, GetModuleHandle(nullptr), 0);
}

void Recorder::stopRecording() {
    if (!m_recording) return;
    m_recording = false;
    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
    }
    if (m_keyHook) {
        UnhookWindowsHookEx(m_keyHook);
        m_keyHook = nullptr;
    }
}

LRESULT CALLBACK Recorder::mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_instance && s_instance->m_recording) {
        MSLLHOOKSTRUCT *p = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        DWORD now = GetTickCount();
        Action act;
        act.time = now - s_instance->m_startTick;
        switch (wParam) {
        case WM_MOUSEMOVE:
            act.type = Action::MouseMove;
            act.data.mouseMove.x = p->pt.x;
            act.data.mouseMove.y = p->pt.y;
            s_instance->recordAction(act);
            break;
        case WM_LBUTTONDOWN:
            act.type = Action::MouseDown; act.data.mouseBtn.button = 0;
            s_instance->recordAction(act);
            break;
        case WM_LBUTTONUP:
            act.type = Action::MouseUp; act.data.mouseBtn.button = 0;
            s_instance->recordAction(act);
            break;
        case WM_RBUTTONDOWN:
            act.type = Action::MouseDown; act.data.mouseBtn.button = 1;
            s_instance->recordAction(act);
            break;
        case WM_RBUTTONUP:
            act.type = Action::MouseUp; act.data.mouseBtn.button = 1;
            s_instance->recordAction(act);
            break;
        case WM_MBUTTONDOWN:
            act.type = Action::MouseDown; act.data.mouseBtn.button = 2;
            s_instance->recordAction(act);
            break;
        case WM_MBUTTONUP:
            act.type = Action::MouseUp; act.data.mouseBtn.button = 2;
            s_instance->recordAction(act);
            break;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK Recorder::keyHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_instance && s_instance->m_recording) {
        KBDLLHOOKSTRUCT *p = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        DWORD now = GetTickCount();
        Action act;
        act.time = now - s_instance->m_startTick;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            act.type = Action::KeyDown;
            act.data.key.vkCode = p->vkCode;
            s_instance->recordAction(act);
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            act.type = Action::KeyUp;
            act.data.key.vkCode = p->vkCode;
            s_instance->recordAction(act);
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void Recorder::recordAction(const Action &act) {
    // 简单过滤掉过密的鼠标移动（每像素都记录会数据爆炸）
    if (act.type == Action::MouseMove) {
        if (!m_actions.isEmpty()) {
            const Action &last = m_actions.last();
            if (last.type == Action::MouseMove &&
                abs(last.data.mouseMove.x - act.data.mouseMove.x) < 2 &&
                abs(last.data.mouseMove.y - act.data.mouseMove.y) < 2 &&
                act.time - last.time < 10)
                return; // 忽略微小移动
        }
    }
    m_actions.append(act);
}

void Recorder::saveToFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return;
    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_5_15);
    out << (quint32)m_actions.size();
    for (const Action &act : m_actions) {
        out << act.time << (qint32)act.type;
        switch (act.type) {
        case Action::MouseMove:
            out << act.data.mouseMove.x << act.data.mouseMove.y;
            break;
        case Action::MouseDown:
        case Action::MouseUp:
            out << act.data.mouseBtn.button;
            break;
        case Action::KeyDown:
        case Action::KeyUp:
            out << (quint32)act.data.key.vkCode;
            break;
        }
    }
}

void Recorder::loadFromFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_5_15);
    quint32 size;
    in >> size;
    m_actions.clear();
    m_actions.reserve(size);
    for (quint32 i = 0; i < size; ++i) {
        Action act;
        qint32 type;
        in >> act.time >> type;
        act.type = static_cast<Action::Type>(type);
        switch (act.type) {
        case Action::MouseMove:
            in >> act.data.mouseMove.x >> act.data.mouseMove.y;
            break;
        case Action::MouseDown:
        case Action::MouseUp:
            in >> act.data.mouseBtn.button;
            break;
        case Action::KeyDown:
        case Action::KeyUp: {
            quint32 vk;
            in >> vk;
            act.data.key.vkCode = vk;
            break;
        }
        }
        m_actions.append(act);
    }
}

void Recorder::startPlayback() {
    if (m_playing || m_actions.isEmpty()) return;
    m_playing = true;
    m_stopPlay = false;
    // 在独立线程中运行回放，避免阻塞UI
    QtConcurrent::run([this]() { playbackLoop(); });
}

void Recorder::stopPlayback() {
    m_stopPlay = true;
}

void Recorder::playbackLoop() {
    if (m_actions.isEmpty()) {
        m_playing = false;
        emit playbackFinished();
        return;
    }
    DWORD startTime = GetTickCount();
    for (const Action &act : m_actions) {
        if (m_stopPlay) break;
        // 等待到正确的时间点
        DWORD elapsed = GetTickCount() - startTime;
        if (act.time > elapsed) {
            QThread::msleep(act.time - elapsed);
        }
        if (m_stopPlay) break;

        INPUT input = {};
        switch (act.type) {
        case Action::MouseMove: {
            // 根据屏幕绝对坐标计算（需转换为0-65535）
            int screenW = GetSystemMetrics(SM_CXSCREEN);
            int screenH = GetSystemMetrics(SM_CYSCREEN);
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
            input.mi.dx = (act.data.mouseMove.x * 65535) / screenW;
            input.mi.dy = (act.data.mouseMove.y * 65535) / screenH;
            SendInput(1, &input, sizeof(INPUT));
            break;
        }
        case Action::MouseDown:
        case Action::MouseUp: {
            input.type = INPUT_MOUSE;
            DWORD flag = 0;
            switch (act.data.mouseBtn.button) {
            case 0: flag = (act.type == Action::MouseDown) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP; break;
            case 1: flag = (act.type == Action::MouseDown) ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP; break;
            case 2: flag = (act.type == Action::MouseDown) ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP; break;
            }
            input.mi.dwFlags = flag;
            SendInput(1, &input, sizeof(INPUT));
            break;
        }
        case Action::KeyDown:
        case Action::KeyUp: {
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = static_cast<WORD>(act.data.key.vkCode);
            if (act.type == Action::KeyUp)
                input.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &input, sizeof(INPUT));
            break;
        }
        }
    }
    m_playing = false;
    emit playbackFinished();
}