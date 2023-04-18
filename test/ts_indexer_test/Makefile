OUTPUT := ts_indexer_test
SRCS := ts_indexer.c ts_indexer_test.c
OBJS=$(SRCS:.c=.o)

CFLAGS += -Wall -O2 -g -fPIC -I./../../include

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	gcc $(LDFLAGS) -o $@ $^

.c.o:
	gcc $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(OUTPUT)

