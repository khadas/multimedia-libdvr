DVR_TOP := $(call my-dir)

LOCAL_PATH := $(call my-dir)

#for amstream.h
AMADEC_C_INCLUDES:=hardware/amlogic/media/amcodec/include
ANDROID_LOG_INCLUDE:=system/core/liblog/include
MEDIAHAL_INCLUDE:=vendor/amlogic/common/mediahal_sdk/include
SECDMX_INCLUDE:=vendor/amlogic/common/libsecdmx_release/include
ifneq (,$(wildcard media_hal))
  MEDIAHAL_INCLUDE:=media_hal/AmTsplayer/include
endif

include $(CLEAR_VARS)
LOCAL_MODULE := libamdvr
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_FILE_LIST := $(wildcard $(LOCAL_PATH)/src/*.c)
LOCAL_SRC_FILES := $(LOCAL_FILE_LIST:$(LOCAL_PATH)/%=%)
LOCAL_SHARED_LIBRARIES += libcutils liblog libdl libc libmediahal_tsplayer libdmx_client
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
                    $(SECDMX_INCLUDE) \
                    $(MEDIAHAL_INCLUDE) \
                    $(AMADEC_C_INCLUDES) \
                    $(ANDROID_LOG_INCLUDE) 
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libamdvr.product
LOCAL_PRODUCT_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_FILE_LIST := $(wildcard $(LOCAL_PATH)/src/*.c)
LOCAL_SRC_FILES := $(LOCAL_FILE_LIST:$(LOCAL_PATH)/%=%)
LOCAL_SHARED_LIBRARIES += libcutils liblog libdl libc libmediahal_tsplayer.system libdmx_client_sys
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
                    $(SECDMX_INCLUDE) \
                    $(MEDIAHAL_INCLUDE) \
                    $(AMADEC_C_INCLUDES) \
                    $(ANDROID_LOG_INCLUDE) 
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)


#include $(DVR_TOP)/test/dvr_chunk_test/Android.mk
#include $(DVR_TOP)/test/dvr_segment_test/Android.mk
#include $(DVR_TOP)/test/dvr_play_test/Android.mk
#include $(DVR_TOP)/test/dvr_rec_test/Android.mk
