TEMPLATE = app

QT += qml quick widgets
QT += network
QT += multimedia

SOURCES += main.cpp \
    dumpgnashprovider.cpp

RESOURCES += qml.qrc \
    img.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)

HEADERS += \
    dumpgnashprovider.h
