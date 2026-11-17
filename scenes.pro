QT += core gui qml quick quickcontrols2 network concurrent

CONFIG += c++17 release
TARGET = scenes
TEMPLATE = app

HEADERS += \
    src/appicons.h \
    src/backend.h \
    src/catalog.h \
    src/cli.h \
    src/hypr.h \
    src/hyprevents.h \
    src/plan.h \
    src/restorer.h \
    src/scene.h \
    src/scenestore.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/appicons.cpp \
    src/backend.cpp \
    src/catalog.cpp \
    src/cli.cpp \
    src/hypr.cpp \
    src/hyprevents.cpp \
    src/main.cpp \
    src/plan.cpp \
    src/restorer.cpp \
    src/scene.cpp \
    src/scenestore.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc

# `qmake6 PREFIX=$HOME/.local && make install` puts it in the launcher without root.
isEmpty(PREFIX): PREFIX = /usr
target.path = $$PREFIX/bin
desktop.files = pkgbuild/scenes.desktop
desktop.path = $$PREFIX/share/applications
icon.files = pkgbuild/scenes.svg
icon.path = $$PREFIX/share/icons/hicolor/scalable/apps
INSTALLS += target desktop icon
