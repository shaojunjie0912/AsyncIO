#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int set_fd_nonblocking(int fd) {
    // 获取当前的文件描述符状态标志
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }

    // 设置 O_NONBLOCK 标志
    flags |= O_NONBLOCK;

    // 更新文件描述符的状态标志
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }

    return 0;  // 成功
}

int main() {
    int fd = STDIN_FILENO;  // 示例：标准输入的文件描述符
    if (set_fd_nonblocking(fd) == 0) {
        printf("File descriptor set to non-blocking mode successfully.\n");
    } else {
        printf("Failed to set file descriptor to non-blocking mode.\n");
    }

    return 0;
}
