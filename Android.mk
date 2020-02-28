DVR_TOP := $(call my-dir)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

#for amstream.h
AMADEC_C_INCLUDES:=hardware/amlogic/media/amcodec/include\
ANDROID_LOG_INCLUDE:=system/core/liblog/include\

LOCAL_MODULE := libamdvr
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_TAGS := optional

LOCAL_FILE_LIST := $(wildcard $(LOCAL_PATH)/src/*.c)
LOCAL_SRC_FILES := $(LOCAL_FILE_LIST:$(LOCAL_PATH)/%=%)
LOCAL_SHARED_LIBRARIES += libcutils liblog libdl libc

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
                    $(AMADEC_C_INCLUDES)
                    $(ANDROID_LOG_INCLUDE)

LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)


include $(DVR_TOP)/test/dvr_chunk_test/Android.mk
include $(DVR_TOP)/test/dvr_segment_test/Android.mk
include $(DVR_TOP)/test/dvr_play_test/Android.mk
