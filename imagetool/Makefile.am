AUTOMAKE_OPTIONS = foreign
SUBDIRS = ../imagecore
bin_PROGRAMS = imagetool
imagetool_CPPFLAGS = -I./ -I../ -I../imagecore/ -I../thirdparty/
imagetool_SOURCES = main.cpp commands/command.cpp commands/resize.cpp commands/identify.cpp commands/pad.cpp commands/clean.cpp commands/colors.cpp commands/frames.cpp
imagetool_LDADD = ../imagecore/libimagecore.la
