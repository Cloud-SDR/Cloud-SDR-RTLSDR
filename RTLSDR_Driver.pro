#-------------------------------------------------
#
# Project created by QtCreator 2016-11-07T20:15:53
#
#-------------------------------------------------

QT       -= core gui

TARGET = CloudSDR_RTLSDR
TEMPLATE = lib

DEFINES += DRIVEREXAMPLE_LIBRARY
LIBS += -lpthread  -lrtlsdr  
DESTDIR = C:/SDRNode/addons
SOURCES += \
    entrypoint.cpp \
    jansson/dump.c \
    jansson/error.c \
    jansson/hashtable.c \
    jansson/hashtable_seed.c \
    jansson/load.c \
    jansson/memory.c \
    jansson/pack_unpack.c \
    jansson/strbuffer.c \
    jansson/strconv.c \
    jansson/utf.c \
    jansson/value.c

HEADERS +=\
    external_hardware_def.h \
    entrypoint.h \
    jansson/hashtable.h \
    jansson/jansson.h \
    jansson/jansson_config.h \
    jansson/jansson_private.h \
    jansson/lookup3.h \
    jansson/strbuffer.h \
    jansson/utf.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}
