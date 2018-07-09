QT += quick
CONFIG += c++14

include(matrix/libqmatrixclient/libqmatrixclient.pri)

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += main.cpp \
    matrix/controller.cpp \
    matrix/roomlistmodel.cpp \
    matrix/imageprovider.cpp \
    matrix/messageeventmodel.cpp \
    matrix/imageproviderconnection.cpp

RESOURCES += \
    res.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    ChatForm.qml \
    LoginForm.qml \
    main.qml \
    Home.qml \
    Login.qml \
    ImageStatus.qml \
    ButtonDelegate.qml \
    SideNav.qml \
    RoomListForm.qml \
    RoomDetailForm.qml \
    Room.qml \
    Setting.qml \
    qml/js/md.js

HEADERS += \
    matrix/controller.h \
    matrix/roomlistmodel.h \
    matrix/imageprovider.h \
    matrix/messageeventmodel.h \
    matrix/imageproviderconnection.h
