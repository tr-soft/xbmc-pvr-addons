#
# Makefile for the PVR demo add-on for XBMC PVR
#
# See the README for copyright information and
# how to reach the author.
#

ADDONBINNAME    = Kodi_SledovaniTV
ADDONNAME       = pvr.sledovanitv
LIBNAME         = libpvrsledovanitv-addon
lib_LTLIBRARIES = libpvrsledovanitv-addon.la

LIBS            = @abs_top_srcdir@/lib/tinyxml/libtinyxml.la @abs_top_srcdir@/lib/jsoncpp/libjsoncpp.la

include ../Makefile.include.am

INCLUDES+=-I@abs_top_srcdir@/lib/jsoncpp/include

libpvrsledovanitv_addon_la_SOURCES = src/client.cpp \
	src/PVRData.cpp \
	src/utils.cpp \
	src/DialogLogin.cpp \
	src/DialogOK.cpp \
	src/uri.cpp
libpvrsledovanitv_addon_la_LDFLAGS = @TARGET_LDFLAGS@

