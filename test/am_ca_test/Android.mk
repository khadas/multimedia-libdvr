LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= am_ca_test.c am_inject.c am_dmx.c linux_dvb.c am_ca.c am_key.c

LOCAL_MODULE:= am_ca_test

LOCAL_MODULE_TAGS := optional

#LOCAL_MULTILIB := 32

LOCAL_CFLAGS+=-DANDROID -DAMLINUX
#LOCAL_C_INCLUDES :=  $(LOCAL_PATH)/../../android/ndk/include

LOCAL_SHARED_LIBRARIES :=  libc

include $(BUILD_EXECUTABLE)

