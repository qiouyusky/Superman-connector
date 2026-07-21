#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include "clicker.h"
#include "recorder.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QCheckBox;
class QSpinBox;
class QComboBox;
class QLineEdit;
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

private slots:
    void onSetHotkey();
    void onLockToggled(bool checked);
    void toggleClicking();
    void onStartRecord();
    void onStopRecord();
    void onSaveRecord();
    void onLoadRecord();
    void onPlayback();
    void onStopPlayback();

private:
    void updateUIState();
    void registerHotKey();
    void unregisterHotKey();

    Clicker *m_clicker;
    Recorder *m_recorder;
    QThread m_recordThread;
    bool m_locked;
    bool m_clicking;
    int m_hotkeyId;          // 热键ID
    int m_hotkeyMod;         // 修饰键
    int m_hotkeyVk;          // 虚拟键码
    QString m_hotkeyDisplay; // 显示文本

    // UI组件
    QComboBox *m_btnSelect;  // 左键/右键
    QSpinBox *m_intervalSpin;// 间隔(ms)
    QPushButton *m_setHotkeyBtn;
    QLineEdit *m_hotkeyEdit; // 显示热键
    QCheckBox *m_lockCheck;
    QPushButton *m_toggleBtn;// 开始/停止连点（用于非锁定状态）
    QLabel *m_statusLabel;

    // 录制回放
    QPushButton *m_recordBtn;
    QPushButton *m_stopRecordBtn;
    QPushButton *m_saveBtn;
    QPushButton *m_loadBtn;
    QPushButton *m_playBtn;
    QPushButton *m_stopPlayBtn;
    QString m_recordFileName;
};

#endif // MAINWINDOW_H