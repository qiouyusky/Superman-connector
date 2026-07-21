#include "mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QKeyEvent>
#include <windows.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_locked(false), m_clicking(false),
      m_hotkeyId(1001), m_hotkeyMod(MOD_CONTROL), m_hotkeyVk(VK_F1),
      m_hotkeyDisplay("Ctrl+F1")
{
    setWindowTitle("鼠标连点器 - 录制/回放");
    resize(450, 350);

    m_clicker = new Clicker(this);
    m_recorder = new Recorder();
    m_recorder->moveToThread(&m_recordThread);
    connect(&m_recordThread, &QThread::finished, m_recorder, &QObject::deleteLater);
    m_recordThread.start();

    // --- 连点设置 ---
    QGroupBox *clickGroup = new QGroupBox("连点设置");
    QFormLayout *form = new QFormLayout;

    m_btnSelect = new QComboBox;
    m_btnSelect->addItem("鼠标左键", Clicker::LeftButton);
    m_btnSelect->addItem("鼠标右键", Clicker::RightButton);

    m_intervalSpin = new QSpinBox;
    m_intervalSpin->setRange(1, 60000);
    m_intervalSpin->setValue(100);
    m_intervalSpin->setSuffix(" 毫秒");

    // 热键设置
    QHBoxLayout *hotkeyLayout = new QHBoxLayout;
    m_hotkeyEdit = new QLineEdit(m_hotkeyDisplay);
    m_hotkeyEdit->setReadOnly(true);
    m_setHotkeyBtn = new QPushButton("设置热键");
    hotkeyLayout->addWidget(m_hotkeyEdit);
    hotkeyLayout->addWidget(m_setHotkeyBtn);

    form->addRow("点击按钮:", m_btnSelect);
    form->addRow("点击间隔:", m_intervalSpin);
    form->addRow("启动热键:", hotkeyLayout);

    // 锁定与手动控制
    m_lockCheck = new QCheckBox("锁定（仅热键启停）");
    m_toggleBtn = new QPushButton("开始连点");
    m_statusLabel = new QLabel("状态：已停止");

    QHBoxLayout *ctrlLayout = new QHBoxLayout;
    ctrlLayout->addWidget(m_lockCheck);
    ctrlLayout->addWidget(m_toggleBtn);
    ctrlLayout->addWidget(m_statusLabel);

    QVBoxLayout *clickLayout = new QVBoxLayout;
    clickLayout->addLayout(form);
    clickLayout->addLayout(ctrlLayout);
    clickGroup->setLayout(clickLayout);

    // --- 录制回放 ---
    QGroupBox *recordGroup = new QGroupBox("录制与回放");
    m_recordBtn = new QPushButton("开始录制");
    m_stopRecordBtn = new QPushButton("停止录制");
    m_stopRecordBtn->setEnabled(false);
    m_saveBtn = new QPushButton("保存录制");
    m_saveBtn->setEnabled(false);
    m_loadBtn = new QPushButton("加载录制文件");
    m_playBtn = new QPushButton("播放");
    m_playBtn->setEnabled(false);
    m_stopPlayBtn = new QPushButton("停止播放");
    m_stopPlayBtn->setEnabled(false);

    QHBoxLayout *recRow1 = new QHBoxLayout;
    recRow1->addWidget(m_recordBtn);
    recRow1->addWidget(m_stopRecordBtn);
    recRow1->addWidget(m_saveBtn);
    QHBoxLayout *recRow2 = new QHBoxLayout;
    recRow2->addWidget(m_loadBtn);
    recRow2->addWidget(m_playBtn);
    recRow2->addWidget(m_stopPlayBtn);

    QVBoxLayout *recLayout = new QVBoxLayout;
    recLayout->addLayout(recRow1);
    recLayout->addLayout(recRow2);
    recordGroup->setLayout(recLayout);

    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(clickGroup);
    mainLayout->addWidget(recordGroup);
    QWidget *central = new QWidget;
    central->setLayout(mainLayout);
    setCentralWidget(central);

    // 信号槽
    connect(m_setHotkeyBtn, &QPushButton::clicked, this, &MainWindow::onSetHotkey);
    connect(m_lockCheck, &QCheckBox::toggled, this, &MainWindow::onLockToggled);
    connect(m_toggleBtn, &QPushButton::clicked, this, &MainWindow::toggleClicking);
    connect(m_recordBtn, &QPushButton::clicked, this, &MainWindow::onStartRecord);
    connect(m_stopRecordBtn, &QPushButton::clicked, this, &MainWindow::onStopRecord);
    connect(m_saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveRecord);
    connect(m_loadBtn, &QPushButton::clicked, this, &MainWindow::onLoadRecord);
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::onPlayback);
    connect(m_stopPlayBtn, &QPushButton::clicked, this, &MainWindow::onStopPlayback);

    // 连点器信号
    connect(m_clicker, &Clicker::started, this, [this](){
        m_clicking = true;
        m_statusLabel->setText("状态：连点中...");
        m_toggleBtn->setText("停止连点");
    });
    connect(m_clicker, &Clicker::stopped, this, [this](){
        m_clicking = false;
        m_statusLabel->setText("状态：已停止");
        m_toggleBtn->setText("开始连点");
    });

    registerHotKey();
}

MainWindow::~MainWindow() {
    unregisterHotKey();
    m_recordThread.quit();
    m_recordThread.wait();
    delete m_clicker;
}

// 处理全局热键消息
bool MainWindow::nativeEvent(const QByteArray &, void *message, long *result) {
    MSG *msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        if (msg->wParam == m_hotkeyId && m_locked) {
            toggleClicking();
        }
        return true;
    }
    return false;
}

void MainWindow::registerHotKey() {
    unregisterHotKey();
    RegisterHotKey(HWND(winId()), m_hotkeyId, m_hotkeyMod, m_hotkeyVk);
}

void MainWindow::unregisterHotKey() {
    UnregisterHotKey(HWND(winId()), m_hotkeyId);
}

void MainWindow::onSetHotkey() {
    // 简易热键捕获：弹出小窗口等待按键组合
    class HotkeyDialog : public QDialog {
    public:
        int mod, vk;
        QString text;
        HotkeyDialog(QWidget *parent) : QDialog(parent), mod(0), vk(0) {
            setWindowTitle("按下组合键...");
            setFixedSize(250, 80);
            QLabel *label = new QLabel("请按下组合键（如Ctrl+F1）", this);
            label->setAlignment(Qt::AlignCenter);
            QVBoxLayout *lay = new QVBoxLayout(this);
            lay->addWidget(label);
            setFocus();
        }
    protected:
        void keyPressEvent(QKeyEvent *e) override {
            Qt::KeyboardModifiers mods = e->modifiers();
            int qtKey = e->key();
            // 转换为Windows虚拟键码（部分映射）
            mod = 0;
            if (mods & Qt::ControlModifier) mod |= MOD_CONTROL;
            if (mods & Qt::AltModifier)     mod |= MOD_ALT;
            if (mods & Qt::ShiftModifier)   mod |= MOD_SHIFT;
            if (mods & Qt::MetaModifier)    mod |= MOD_WIN;

            vk = 0;
            if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
                vk = VK_F1 + (qtKey - Qt::Key_F1);
            else if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
                vk = 'A' + (qtKey - Qt::Key_A);
            else if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
                vk = '0' + (qtKey - Qt::Key_0);
            else if (qtKey == Qt::Key_Space) vk = VK_SPACE;
            else if (qtKey == Qt::Key_Tab)   vk = VK_TAB;
            else if (qtKey == Qt::Key_Enter || qtKey == Qt::Key_Return) vk = VK_RETURN;
            else {
                QDialog::keyPressEvent(e);
                return;
            }

            // 生成显示文本
            QStringList parts;
            if (mod & MOD_CONTROL) parts << "Ctrl";
            if (mod & MOD_ALT)     parts << "Alt";
            if (mod & MOD_SHIFT)   parts << "Shift";
            if (mod & MOD_WIN)     parts << "Win";
            parts << QKeySequence(qtKey).toString();
            text = parts.join('+');
            accept();
        }
    };
    HotkeyDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_hotkeyMod = dlg.mod;
        m_hotkeyVk = dlg.vk;
        m_hotkeyDisplay = dlg.text;
        m_hotkeyEdit->setText(m_hotkeyDisplay);
        registerHotKey();
    }
}

void MainWindow::onLockToggled(bool checked) {
    m_locked = checked;
    m_toggleBtn->setEnabled(!checked);
    if (checked && m_clicking) {
        // 锁定时不改变连点状态，但禁用按钮
    }
    updateUIState();
}

void MainWindow::toggleClicking() {
    if (m_clicking) {
        m_clicker->stop();
    } else {
        m_clicker->setButton(static_cast<Clicker::MouseButton>(m_btnSelect->currentData().toInt()));
        m_clicker->setInterval(m_intervalSpin->value());
        m_clicker->start();
    }
}

void MainWindow::onStartRecord() {
    QMetaObject::invokeMethod(m_recorder, "startRecording", Qt::QueuedConnection);
    m_recordBtn->setEnabled(false);
    m_stopRecordBtn->setEnabled(true);
    m_saveBtn->setEnabled(false);
    m_playBtn->setEnabled(false);
    m_loadBtn->setEnabled(false);
    m_statusLabel->setText("状态：正在录制...");
}

void MainWindow::onStopRecord() {
    QMetaObject::invokeMethod(m_recorder, "stopRecording", Qt::QueuedConnection);
    m_recordBtn->setEnabled(true);
    m_stopRecordBtn->setEnabled(false);
    m_saveBtn->setEnabled(true);
    m_loadBtn->setEnabled(true);
    m_statusLabel->setText("状态：录制已停止");
}

void MainWindow::onSaveRecord() {
    QString fileName = QFileDialog::getSaveFileName(this, "保存录制文件", "", "记录文件 (*.rec)");
    if (!fileName.isEmpty()) {
        QMetaObject::invokeMethod(m_recorder, "saveToFile", Qt::BlockingQueuedConnection,
                                  Q_ARG(QString, fileName));
        m_recordFileName = fileName;
        m_playBtn->setEnabled(true);
    }
}

void MainWindow::onLoadRecord() {
    QString fileName = QFileDialog::getOpenFileName(this, "加载录制文件", "", "记录文件 (*.rec)");
    if (!fileName.isEmpty()) {
        QMetaObject::invokeMethod(m_recorder, "loadFromFile", Qt::BlockingQueuedConnection,
                                  Q_ARG(QString, fileName));
        m_recordFileName = fileName;
        m_playBtn->setEnabled(true);
    }
}

void MainWindow::onPlayback() {
    QMetaObject::invokeMethod(m_recorder, "startPlayback", Qt::QueuedConnection);
    m_playBtn->setEnabled(false);
    m_stopPlayBtn->setEnabled(true);
    m_recordBtn->setEnabled(false);
    m_loadBtn->setEnabled(false);
    m_statusLabel->setText("状态：正在回放...");
    connect(m_recorder, &Recorder::playbackFinished, this, [this](){
        m_playBtn->setEnabled(true);
        m_stopPlayBtn->setEnabled(false);
        m_recordBtn->setEnabled(true);
        m_loadBtn->setEnabled(true);
        m_statusLabel->setText("状态：回放完成");
    }, Qt::UniqueConnection);
}

void MainWindow::onStopPlayback() {
    QMetaObject::invokeMethod(m_recorder, "stopPlayback", Qt::QueuedConnection);
    m_playBtn->setEnabled(true);
    m_stopPlayBtn->setEnabled(false);
    m_recordBtn->setEnabled(true);
    m_loadBtn->setEnabled(true);
    m_statusLabel->setText("状态：回放已停止");
}

void MainWindow::updateUIState() {
    m_toggleBtn->setEnabled(!m_locked);
}