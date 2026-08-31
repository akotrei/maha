CC ?= gcc
# Added -I. to make 'src' the root directory for all #include "folder/file.h" paths
CFLAGS ?= -Wall -Wextra -O2 -I.

# Updated paths matching your new folder structure inside src/
SRCS = server/server.c \
       server/server_init.c \
       config/config.c \
       ipc/ipc.c \
       connection/connection.c \
       epoll_manager/epoll_manager.c

OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

TARGET = c_server

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

# Rule for compiling object files. Matches any nested folder structure.
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for generating auto-dependencies with correct include paths
%.d: %.c
	$(CC) $(CFLAGS) -MM -MT $(@:.d=.o) $< > $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif
