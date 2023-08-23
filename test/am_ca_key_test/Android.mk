LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= am_ca_key_test.c am_ca_t5d.c
		inject_record_t5d/am_dmx/am_dmx.c
		inject_record_t5d/am_dmx/linux_dvb/linux_dvb.c
		inject_record_t5d/am_dvr/am_dvr.c
		inject_record_t5d/am_dvr/linux_dvb/linux_dvb.c
		inject_record_t5d/am_av/am_av.c
		inject_record_t5d/am_av/aml/aml.c
		inject_record_t5d/am_misc/am_misc.c
		inject_record_t5d/am_misc/am_adplock.c
		inject_record_t5d/am_tfile/am_tfile.c
		inject_record_t5d/am_time/am_time.c
		inject_record_t5d/am_inject_record.c

LOCAL_MODULE:= am_ca_key_test
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice

LOCAL_MODULE_TAGS := optional

#LOCAL_MULTILIB := 32

LOCAL_CFLAGS+=-DANDROID -DAMLINUX
#LOCAL_C_INCLUDES :=  $(LOCAL_PATH)/../../android/ndk/include

LOCAL_SHARED_LIBRARIES :=  libc

include $(BUILD_EXECUTABLE)

