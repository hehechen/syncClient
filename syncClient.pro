QT += core
QT -= gui

CONFIG += c++11
TARGET = syncClient
CONFIG += console
CONFIG -= app_bundle

LIBS +=  -L/usr/lib -lprotobuf -lcrypt -lpthread
INCLUDEPATH += /usr/local/include

TEMPLATE = app

SOURCES += main.cpp \
    socket.cpp \
    sysutil.cpp \
    test/socketTest.cpp \
    test/filerecvtest.cpp \
    threadpool.cpp \
    test/threadpooltest.cpp \
    eventloop.cpp \
    test/inotifytest.cpp \
    test/prototest.cpp \
    protobuf/filesync.init.pb.cc

HEADERS += \
    common.h \
    socket.h \
    sysutil.h \
    threadpool.h \
    mutexlock.h \
    mutexlockguard.h \
    condition.h \
    eventloop.h \
    protobuf/sync.init.pb.h \
    protobuf/filesync.init.pb.h
