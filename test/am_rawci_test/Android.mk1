LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= dvr_write_test.c

LOCAL_MODULE:= dvr_write_test

LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES :=  $(LOCAL_PATH)/../../include/
#LOCAL_MULTILIB := 32

LOCAL_CFLAGS+=-DANDROID -DAMLINUX

LOCAL_SHARED_LIBRARIES := liblog libc

include $(BUILD_EXECUTABLE)
