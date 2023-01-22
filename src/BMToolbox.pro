#-------------------------------------------------
#
# Project created by QtCreator 2012-10-29T13:26:50
#
#-------------------------------------------------

QT       += core gui network xml sql qtsingleapplication

TARGET = BMToolbox
TEMPLATE = app

TRANSLATIONS = bmtoolbox_en.ts \
    bmtoolbox_de.ts

SOURCES += main.cpp\
        mainwindow.cpp \
    bmtoolboxapp.cpp \
    aboutdialog.cpp \
    clientapi.cpp \
    smsmanager.cpp \
    addressbook.cpp \
    webdisksync.cpp \
    webdiskconflictdialog.cpp \
    sendfaxdialog.cpp \
    welcomewizard.cpp \
    updatenotification.cpp

HEADERS  += mainwindow.h \
    bmtoolboxapp.h \
    aboutdialog.h \
    clientapi.h \
    smsmanager.h \
    addressbook.h \
    webdisksync.h \
    webdiskconflictdialog.h \
    sendfaxdialog.h \
    welcomewizard.h \
    updatenotification.h \
    macauth.h

FORMS    += mainwindow.ui \
    aboutdialog.ui \
    smsmanager.ui \
    addressbook.ui \
    webdiskconflictdialog.ui \
    sendfaxdialog.ui \
    welcomewizard.ui \
    updatenotification.ui

RESOURCES += \
    resource.qrc

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/libharu/ -llibhpdf -lshell32 -lQtSingleApplication
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/libharu/ -llibhpdf -lshell32 -lQtSingleApplicationd

macx {
    SOURCES += macauth.cpp
    OBJECTIVE_SOURCES += macloginitemsmanager.mm
    HEADERS += macloginitemsmanager.h macauth.h
    LIBS += -framework CoreFoundation -framework ApplicationServices -framework Foundation -framework Security -lcrypto
    QMAKE_INFO_PLIST = BMToolboxInfo.plist

    MAC_RES.files = res/license.html res/app-ico.png res/newmail.wav res/branding.ini bmtoolbox_de.qm bmtoolbox_en.qm mac/updater/updater
    MAC_RES.path = Contents/Resources

    PRINTER_RES.files = mac/drv/BMFaxPrint.ppd
    PRINTER_RES.path = Contents/Resources/faxdrv

    QMAKE_BUNDLE_DATA += MAC_RES PRINTER_RES
}

win32 {
    RC_FILE = resource.rc
    LIBS += -llibeay32
}

INCLUDEPATH += $$PWD/libharu/include
DEPENDPATH += $$PWD/libharu/include

OTHER_FILES += \
    resource.rc \
    BMToolboxInfo.plist
