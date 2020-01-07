LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libdvr
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_TAGS := optional

LOCAL_FILE_LIST := $(wildcard $(LOCAL_PATH)/src/*.c)
LOCAL_SRC_FILES := $(LOCAL_FILE_LIST:$(LOCAL_PATH)/%=%)
LOCAL_SHARED_LIBRARIES += libcutils liblog libdl libc

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
	system/core/liblog/include

LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
