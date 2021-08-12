LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
#LOCAL_VENDOR_MODULE := true

#for amstream.h
#AMADEC_C_INCLUDES:=hardware/amlogic/media/amcodec/include
#ANDROID_LOG_INCLUDE:=system/core/liblog/include

MEDIAHAL_INCLUDE:=$(wildcard media_hal/AmTsplayer/include)
ifeq ($(MEDIAHAL_INCLUDE),)
  MEDIAHAL_INCLUDE:=vendor/amlogic/common/mediahal_sdk/include
endif

LOCAL_SRC_FILES:= dvr_wrapper_test.c
LOCAL_MODULE:= dvr_wrapper_test
LOCAL_MODULE_TAGS := optional
#LOCAL_MULTILIB := 32
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
                    $(LOCAL_PATH)/../../include/ \
                    $(MEDIAHAL_INCLUDE) \
                    $(AMADEC_C_INCLUDES) \
                    $(ANDROID_LOG_INCLUDE) \

LOCAL_SHARED_LIBRARIES := libamdvr.product libmediahal_tsplayer.system
LOCAL_SHARED_LIBRARIES += libcutils liblog libdl libc

include $(BUILD_EXECUTABLE)
