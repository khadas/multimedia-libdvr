LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= am_dmx_test.c am_dmx.c am_pes.c am_thread.c linux_dvb.c

LOCAL_MODULE:= aml_dmx_test

LOCAL_MODULE_TAGS := optional

#LOCAL_MULTILIB := 32

LOCAL_CFLAGS+=-DANDROID -DAMLINUX  -DLINUX_DVB_FEND

LOCAL_SHARED_LIBRARIES := liblog libc

include $(BUILD_EXECUTABLE)
