#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "connection.h"


int conn_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
