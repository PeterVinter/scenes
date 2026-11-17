QT += core gui qml quick quickcontrols2 network concurrent
CONFIG += c++17 warn_on
QMAKE_CXXFLAGS += -Wall -Wextra
TARGET = scenes_screenshot
TEMPLATE = app

INCLUDEPATH += ../src
DEFINES += SRC_DIR=\\\"$$PWD/../src\\\"

HEADERS += \
    ../src/appicons.h ../src/backend.h ../src/catalog.h ../src/cli.h ../src/hypr.h ../src/hyprevents.h \
    ../src/plan.h ../src/restorer.h ../src/scene.h ../src/scenestore.h ../src/theme.h ../src/translator.h
SOURCES += \
    screenshot.cpp \
    ../src/appicons.cpp ../src/backend.cpp ../src/catalog.cpp ../src/cli.cpp ../src/hypr.cpp \
    ../src/hyprevents.cpp ../src/plan.cpp ../src/restorer.cpp ../src/scene.cpp ../src/scenestore.cpp \
    ../src/theme.cpp ../src/translator.cpp
RESOURCES += ../src/resources.qrc
