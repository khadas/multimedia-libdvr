LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= am_sc2_smc_test.c am_smc.c aml.c am_time.c am_evt.c am_thread.c 

LOCAL_MODULE:= am_smc_test

LOCAL_MODULE_TAGS := optional

#LOCAL_MULTILIB := 32

LOCAL_CFLAGS+=-DANDROID -DAMLINUX -DCHIP_8226M -DLINUX_DVB_FEND
#LOCAL_C_INCLUDES :=  $(LOCAL_PATH)/../../android/ndk/include
				
LOCAL_SHARED_LIBRARIES :=  libc

include $(BUILD_EXECUTABLE)

