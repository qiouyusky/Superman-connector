QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
TARGET = MouseClicker
TEMPLATE = app
SOURCES += main.cpp \
           mainwindow.cpp \
           clicker.cpp \
           recorder.cpp
HEADERS += mainwindow.h \
           clicker.h \
           recorder.h
LIBS += -luser32 -lgdi32