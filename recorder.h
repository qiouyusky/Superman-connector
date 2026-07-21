#ifndef RECORDER_H
#define RECORDER_H

#include <QObject>
#include <QVector>
#include <QString>
#include <windows.h>

struct Action {
    DWORD time;      // 自录制开始的毫秒数
    enum Type { MouseMove, MouseDown, MouseUp, KeyDown, KeyUp } type;
    union {
        struct { int x, y; } mouseMove;
        struct { int button; } mouseBtn;  // 0左 1右 2中
        struct { DWORD vkCode; } key;
    } data;
};

class Recorder : public QObject {
    Q_OBJECT
public:
    explicit Recorder(QObject *parent = nullptr);
    ~Recorder();

public slots:
    void startRecording();
    void stopRecording();
    void saveToFile(const QString &filePath);
    void loadFromFile(const QString &filePath);
    void startPlayback();
    void stopPlayback();

signals:
    void playbackFinished();

private:
    static LRESULT CALLBACK mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK keyHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    void recordAction(const Action &act);
    void playbackLoop();

    HHOOK m_mouseHook;
    HHOOK m_keyHook;
    bool m_recording;
    QVector<Action> m_actions;
    DWORD m_startTick;

    bool m_playing;
    bool m_stopPlay;

    static Recorder *s_instance; // 用于回调中访问实例
};

#endif // RECORDER_H