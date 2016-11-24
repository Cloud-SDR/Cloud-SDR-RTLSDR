# *
# * Adds RTLSDR Dongles capability to SDRNode
# * Copyright (C) 2016 Sylvain AZARIAN <sylvain.azarian@gmail.com>
# *
# * This program is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 2 of the License, or
# * (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

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
