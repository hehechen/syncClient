QT += core
QT -= gui

CONFIG += c++11
TARGET = syncClient
CONFIG += console
CONFIG -= app_bundle

LIBS +=  -L/usr/lib -lprotobuf -lcrypt -lpthread    \
         -L/home/chen/Desktop/work/build/release-install/lib -lmuduo_net -lmuduo_base   \
         -lz
INCLUDEPATH += /usr/local/include/google/protobuf   \
                /home/chen/Desktop/work/build/release-install/include

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
    codec.cpp \
    protobuf/filesync.pb.cc \
    TimeStamp.cpp \
    TimerHeap.cpp \
    test/timerheaptest.cpp \
    parseconfig.cpp \
    str_tool.cpp

HEADERS += \
    common.h \
    socket.h \
    sysutil.h \
    threadpool.h \
    mutexlock.h \
    mutexlockguard.h \
    condition.h \
    eventloop.h \
    codec.h \
    protobuf/filesync.pb.h \
    TimeStamp.h \
    TimerHeap.h \
    parseconfig.h \
    str_tool.h

DISTFILES += \
    protobuf/filesync.proto
