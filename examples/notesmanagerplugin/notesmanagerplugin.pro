TEMPLATE = lib
CONFIG += plugin
INCLUDEPATH += ../../src/serviceframework
HEADERS += notesmanagerplugin.h \
           notesmanager.h
SOURCES += notesmanagerplugin.cpp \
           notesmanager.cpp
QT += sql
TARGET = serviceframework_notesmanagerplugin
DESTDIR = .

include(../examples.pri)
CONFIG += mobility
MOBILITY = serviceframework

symbian {
    load(data_caging_paths)
    pluginDep.sources = serviceframework_notesmanagerplugin.dll
    pluginDep.path = $$QT_PLUGINS_BASE_DIR    
    DEPLOYMENT += pluginDep

    TARGET.EPOCALLOWDLLDATA = 1
    TARGET.CAPABILITY = ALL -TCB
}

xml.path = $$DESTDIR/xmldata
xml.files = notesmanagerservice.xml
xml.CONFIG = no_link no_dependencies explicit_dependencies no_build combine ignore_no_exist no_clean
INSTALLS += xml
build_pass:ALL_DEPS+=install_xml