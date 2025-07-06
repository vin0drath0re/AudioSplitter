QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    gui/mainwindow.cpp \
    gui/components/DeviceItemWidget.cpp \
    core/relay.cpp

HEADERS += \
    gui/mainwindow.h \
    gui/components/DeviceItemWidget.h \
    core/relay.h

FORMS += \
    gui/mainwindow.ui

INCLUDEPATH += \
    core \
    gui \
    gui/components

LIBS += -lole32 -luuid -lwinmm

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
