#ifndef CLICKER_H
#define CLICKER_H

#include <QObject>
#include <QTimer>
#include <windows.h>

class Clicker : public QObject {
    Q_OBJECT
public:
    enum MouseButton {
        LeftButton = 0,
        RightButton = 1
    };

    explicit Clicker(QObject *parent = nullptr);
    ~Clicker();

    void setButton(MouseButton btn);
    void setInterval(int ms);

public slots:
    void start();
    void stop();

signals:
    void started();
    void stopped();

private slots:
    void performClick();

private:
    QTimer *m_timer;
    MouseButton m_button;
    int m_interval;
};

#endif // CLICKER_H