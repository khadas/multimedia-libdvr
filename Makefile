OUTPUT_FILES := libamdvr.so am_fend_test am_dmx_test am_smc_test dvr_wrapper_test

CFLAGS  := -Wall -O2 -fPIC -Iinclude
LDFLAGS := -L$(TARGET_DIR)/usr/lib -lmediahal_tsplayer -laudio_client -llog -lpthread -ldl
OUT_DIR ?= .

LIBAMDVR_SRCS := \
	src/dvb_dmx_wrapper.c\
	src/dvb_utils.c\
	src/dvr_record.c\
	src/dvr_utils.c\
	src/index_file.c\
	src/record_device.c\
	src/dvb_frontend_wrapper.c\
	src/dvr_playback.c\
	src/dvr_segment.c\
	src/dvr_wrapper.c\
	src/list_file.c\
	src/segment.c\
	src/segment_dataout.c\
	src/am_crypt.c\
	src/dvr_mutex.c

LIBAMDVR_OBJS := $(patsubst %.c,$(OUT_DIR)/%.o,$(LIBAMDVR_SRCS))

AM_FEND_TEST_SRCS := \
	test/am_fend_test/am_fend_test.c
AM_FEND_TEST_OBJS := $(patsubst %.c,$(OUT_DIR)/%.o,$(AM_FEND_TEST_SRCS))

AM_DMX_TEST_SRCS := \
	test/am_dmx_test/am_dmx_test.c
AM_DMX_TEST_OBJS := $(patsubst %.c,$(OUT_DIR)/%.o,$(AM_DMX_TEST_SRCS))

AM_SMC_TEST_SRCS := \
	test/am_smc_test/am_sc2_smc_test.c \
	test/am_smc_test/am_smc.c \
	test/am_smc_test/aml.c \
	test/am_smc_test/am_time.c \
	test/am_smc_test/am_evt.c \
	test/am_smc_test/am_thread.c
AM_SMC_TEST_OBJS := $(patsubst %.c,$(OUT_DIR)/%.o,$(AM_SMC_TEST_SRCS))

DVR_WRAPPER_TEST_SRCS := \
	test/dvr_wrapper_test/dvr_wrapper_test.c
DVR_WRAPPER_TEST_OBJS := $(patsubst %.c,$(OUT_DIR)/%.o,$(DVR_WRAPPER_TEST_SRCS))


all: $(OUTPUT_FILES)

$(OUT_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(CFLAGS)

libamdvr.so: $(LIBAMDVR_OBJS)
	$(CC) -shared -o $(OUT_DIR)/$@ $^ $(LDFLAGS)

am_fend_test: $(AM_FEND_TEST_OBJS) libamdvr.so
	$(CC) -o $(OUT_DIR)/$@ $(AM_FEND_TEST_OBJS) -L$(OUT_DIR) -lamdvr $(LDFLAGS)

am_dmx_test: $(AM_DMX_TEST_OBJS) libamdvr.so
	$(CC) -o $(OUT_DIR)/$@ $(AM_DMX_TEST_OBJS) -L$(OUT_DIR) -lamdvr $(LDFLAGS)

am_smc_test: $(AM_SMC_TEST_OBJS) libamdvr.so
	$(CC) -o $(OUT_DIR)/$@ $(AM_SMC_TEST_OBJS) -L$(OUT_DIR) -lamdvr $(LDFLAGS)

dvr_wrapper_test: $(DVR_WRAPPER_TEST_OBJS) libamdvr.so
	$(CC) -o $(OUT_DIR)/$@ $(DVR_WRAPPER_TEST_OBJS) -L$(OUT_DIR) -lamdvr $(LDFLAGS)

install: $(OUTPUT_FILES)
	# install folders
	install -d -m 0755 $(STAGING_DIR)/usr/include/libdvr
	install -d -m 0755 $(TARGET_DIR)/usr/include/libdvr
	install -d $(TARGET_DIR)/usr/lib
	install -d $(TARGET_DIR)/usr/bin
	# install so
	install -m 0755 $(OUT_DIR)/libamdvr.so $(STAGING_DIR)/usr/lib
	install -m 0755 $(OUT_DIR)/libamdvr.so $(TARGET_DIR)/usr/lib
	# install test
	install -m 0755 $(OUT_DIR)/am_fend_test $(STAGING_DIR)/usr/bin
	install -m 0755 $(OUT_DIR)/am_fend_test $(TARGET_DIR)/usr/bin
	install -m 0755 $(OUT_DIR)/am_dmx_test $(STAGING_DIR)/usr/bin
	install -m 0755 $(OUT_DIR)/am_dmx_test $(TARGET_DIR)/usr/bin
	install -m 0755 $(OUT_DIR)/am_smc_test $(STAGING_DIR)/usr/bin
	install -m 0755 $(OUT_DIR)/am_smc_test $(TARGET_DIR)/usr/bin
	install -m 0755 $(OUT_DIR)/dvr_wrapper_test $(STAGING_DIR)/usr/bin
	install -m 0755 $(OUT_DIR)/dvr_wrapper_test $(TARGET_DIR)/usr/bin
	# install headers
	install -m 0644 ./include/* $(STAGING_DIR)/usr/include/libdvr
	install -m 0644 ./include/* $(TARGET_DIR)/usr/include/libdvr
	install -m 0644 ./include/dvb_*.h $(TARGET_DIR)/usr/include
	install -m 0644 ./include/dvr_*.h $(TARGET_DIR)/usr/include
	install -m 0644 ./include/segment.h $(TARGET_DIR)/usr/include
	install -m 0644 ./include/segment_ops.h $(TARGET_DIR)/usr/include
	install -m 0644 ./include/list.h $(TARGET_DIR)/usr/include

clean:
	find $(OUT_DIR) -name \*.o -exec rm -f {} \;

.PHONY: all install clean
