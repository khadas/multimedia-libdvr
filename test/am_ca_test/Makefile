CROSS_COMPILE:=/opt/gcc-linaro-6.3.1-2017.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
#export CROSS_COMPILE=/opt/gcc-linaro-6.3.1-2017.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
CC=$(CROSS_COMPILE)gcc

OUTPUT = am_sc2_ca_test

OBJS = $(patsubst %.c,%.o,$(SRC_FILES))

SRC_FILES = am_sc2_ca_test.c am_inject.c am_dmx.c linux_dvb.c am_ca.c am_key.c

CFLAGS := -c -Wall

LDFLAGS := -lpthread

all : $(OBJS) $(OUTPUT)

$(OBJS) : %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT) : $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	@rm -f $(OBJS)
