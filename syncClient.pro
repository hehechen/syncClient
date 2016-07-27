QT += core
QT -= gui

CONFIG += c++11

TARGET = syncClient
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    socket.cpp \
    sysutil.cpp \
    test/socketTest.cpp \
    test/filerecvtest.cpp \
    threadpool.cpp \
    test/threadpooltest.cpp

HEADERS += \
    common.h \
    socket.h \
    sysutil.h \
    threadpool.h \
    mutexlock.h \
    mutexlockguard.h \
    condition.h
