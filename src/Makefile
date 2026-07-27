CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2

SRCS = server.c config.c ipc.c connection.c
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

TARGET = server

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.d: %.c
	$(CC) $(CFLAGS) -MM -MT $(@:.d=.o) $< > $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif
