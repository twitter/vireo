include $(CLEAR_VARS)

LOCAL_MODULE := libvireo

LOCAL_PATH := $(BASE_PATH)
LOCAL_CPPFLAGS += -ffunction-sections -fdata-sections -fvisibility=hidden
LOCAL_CPPFLAGS += -std=c++1y -fno-exceptions -fno-rtti
LOCAL_CPPFLAGS += -DNDEBUG=1 -DLSMASH_DEMUXER_ENABLED -DANDROID -DTESTING -DTWITTER_INTERNAL

THIRDPARTY_PATH = $(BASE_PATH)/../thirdparty
LOCAL_C_INCLUDES += $(BASE_PATH)/.. $(THIRDPARTY_PATH)/include
LOCAL_C_INCLUDES += $(NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.9/include
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
    LOCAL_C_INCLUDES += $(NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi-v7a/include
else ifeq ($(TARGET_ARCH_ABI),armeabi)
    LOCAL_C_INCLUDES += $(NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi/include
else ifeq ($(TARGET_ARCH_ABI),x86)
    LOCAL_C_INCLUDES += $(NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.9/libs/x86/include
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    LOCAL_C_INCLUDES += $(NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.9/libs/arm64-v8a/include
else ifeq ($(TARGET_ARCH_ABI),x86)
    LOCAL_C_INCLUDES += $(NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.9/libs/x86/include
else ifeq ($(TARGET_ARCH_ABI),x86_64)
    LOCAL_C_INCLUDES += $(NDK_ROOT)/sources/cxx-stl/gnu-libstdc++/4.9/libs/x86_64/include
endif
LOCAL_C_INCLUDES += $(NDK_ROOT)/sources/android/support/include

LOCAL_SRC_FILES := android/android.cpp android/util.cpp common/bitreader.cpp common/data.cpp common/editbox.cpp common/reader.cpp error/error.cpp header/header.cpp internal/decode/annexb.cpp internal/decode/avcc.cpp internal/decode/h264_bytestream.cpp internal/demux/mp4.cpp mux/mp4.cpp settings/settings.cpp transform/stitch.cpp transform/trim.cpp util/caption.cpp

include $(BUILD_STATIC_LIBRARY)
