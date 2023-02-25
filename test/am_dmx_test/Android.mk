LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= am_dmx_test.c

LOCAL_MODULE:= aml_dmx_test
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice

LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES :=  $(LOCAL_PATH)/../../include/
#LOCAL_MULTILIB := 32

LOCAL_CFLAGS+=-DANDROID -DAMLINUX  -DLINUX_DVB_FEND

LOCAL_SHARED_LIBRARIES := libamdvr.product liblog libc

include $(BUILD_EXECUTABLE)
