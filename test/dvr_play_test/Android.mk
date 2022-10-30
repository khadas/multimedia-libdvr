LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := true



#for amstream.h
AMADEC_C_INCLUDES:=hardware/amlogic/media/amcodec/include \
ANDROID_LOG_INCLUDE:=system/core/liblog/include \

LOCAL_SRC_FILES:= dvr_play_test.c

LOCAL_MODULE:= dvr_play_test
LOCAL_LICENSE_KINDS := SPDX-license-identifier-GPL-2.0+ SPDX-license-identifier-LGPL-2.0+
LOCAL_LICENSE_CONDITIONS := notice

LOCAL_MODULE_TAGS := optional

#LOCAL_MULTILIB := 32

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
                    vendor/amlogic/common/mediahal_sdk/include \
                    $(LOCAL_PATH)/../../include/ \
                    $(AMADEC_C_INCLUDES)
                    $(ANDROID_LOG_INCLUDE)

LOCAL_SHARED_LIBRARIES := libamdvr libmediahal_tsplayer
LOCAL_SHARED_LIBRARIES += libcutils liblog libdl libc

include $(BUILD_EXECUTABLE)
