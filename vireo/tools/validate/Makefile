MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
SRCROOT := $(patsubst %/,%,$(dir $(MAKEFILE_PATH)))
OS=$(shell uname -s | tr '[A-Z]' '[a-z]')
VIREO_DIR=$(SRCROOT)/../..
IMAGECORE_DIR=$(VIREO_DIR)/../imagecore
IMAGECORE_POSTFIX=
ifeq ($(DEBUG), 1)
	IMAGECORE_POSTFIX=_d
endif

CXXFLAGS+=-std=gnu++1y -DTESTING
LDFLAGS= #empty
ifeq ("$(OS)", "linux")
	LDFLAGS+=-static-libgcc -static-libstdc++
endif

DEBUG?=0
ifeq ($(DEBUG), 1)
	CXXFLAGS+=-O0 -g -DDEBUG=1
	CONFIGURATION_DIR=$(VIREO_DIR)/build/debug
else
	CXXFLAGS+=-Os -DNDEBUG=1
	CONFIGURATION_DIR=$(VIREO_DIR)/build/release
endif

THIRD_PARTY_DIR=$(VIREO_DIR)/../thirdparty
ifeq ("$(OS)", "darwin")
	THIRD_PARTY_LIB_DIR=$(THIRD_PARTY_DIR)/lib-mac
else
	THIRD_PARTY_LIB_DIR=$(THIRD_PARTY_DIR)/lib-linux-pic
endif
LIBS=$(THIRD_PARTY_LIB_DIR)/libswscale.a \
	$(THIRD_PARTY_LIB_DIR)/libavformat.a \
	$(THIRD_PARTY_LIB_DIR)/libavcodec.a \
	$(THIRD_PARTY_LIB_DIR)/libavutil.a \
	$(THIRD_PARTY_LIB_DIR)/liblsmash.a \
	$(THIRD_PARTY_LIB_DIR)/libfdk-aac.a \
	$(THIRD_PARTY_LIB_DIR)/libpng.a \
	$(THIRD_PARTY_LIB_DIR)/libturbojpeg.a \
	$(THIRD_PARTY_LIB_DIR)/libtiff.a \
	$(THIRD_PARTY_LIB_DIR)/liblcms2.a \
	$(THIRD_PARTY_LIB_DIR)/libz.a \
	$(THIRD_PARTY_LIB_DIR)/libturbojpeg.a \
	$(THIRD_PARTY_LIB_DIR)/libwebp.a \
	$(THIRD_PARTY_LIB_DIR)/libx264.a \
	$(THIRD_PARTY_LIB_DIR)/libvorbis.a \
	$(THIRD_PARTY_LIB_DIR)/libvorbisenc.a \
	$(THIRD_PARTY_LIB_DIR)/libogg.a \
	$(THIRD_PARTY_LIB_DIR)/libwebm.a \
	-lpthread
ifeq ("$(OS)", "darwin")
	LIBS+=-liconv
else
	LIBS+=-lrt
endif

BUILD_DIR=$(CONFIGURATION_DIR)/tools/validate

INCLUDES=-I$(SRCROOT) -I$(VIREO_DIR)/.. -I$(THIRD_PARTY_DIR)/include

.PHONY: all clean

all: $(CONFIGURATION_DIR)/validate

clean:
	$(MAKE) -C $(VIREO_DIR) clean
	@rm -rf $(VIREO_DIR)/build/*

force_look:
	true

checkdir: $(BUILD_DIR)

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/main.o: main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $? -o $@

$(BUILD_DIR)/test_common.o: ../../tests/test_common.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $? -o $@

$(IMAGECORE_DIR)/libimagecore$(IMAGECORE_POSTFIX).a: force_look
	env CFLAGS=-fPIC CPPFLAGS=-fPIC $(MAKE) -C $(IMAGECORE_DIR)

$(CONFIGURATION_DIR)/libvireo.a: force_look
	$(MAKE) -C $(VIREO_DIR) static

$(CONFIGURATION_DIR)/validate: checkdir $(CONFIGURATION_DIR)/libvireo.a $(IMAGECORE_DIR)/libimagecore$(IMAGECORE_POSTFIX).a $(BUILD_DIR)/test_common.o $(BUILD_DIR)/main.o
	$(CXX) -o $@ $(BUILD_DIR)/main.o $(BUILD_DIR)/test_common.o $(CONFIGURATION_DIR)/libvireo.a $(IMAGECORE_DIR)/libimagecore$(IMAGECORE_POSTFIX).a $(LIBS) $(LDFLAGS)
	strip -x $@
