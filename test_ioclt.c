#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

// Визначення магії та команди з драйвера
#define SIMPLECHAR_IOC_MAGIC 'S'
#define SIMPLECHAR_IOC_GETDEBUG _IOR(SIMPLECHAR_IOC_MAGIC, 0, struct simplechar_debug_info)

// Структура, що відповідає драйверу
struct simplechar_debug_info {
    unsigned long size;
    unsigned long last_write_pos;
    int open_count;
};

int main() {
    int fd;
    struct simplechar_debug_info info;

    // Відкриваємо пристрій
    fd = open("/dev/simplechar", O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }
    write(fd, "hello", 5);
    // Викликаємо ioctl
    if (ioctl(fd, SIMPLECHAR_IOC_GETDEBUG, &info) < 0) {
        perror("ioctl failed");
        close(fd);
        return 1;
    }

    // Виводимо результати
    printf("Debug Info:\n");
    printf("  Buffer size: %lu bytes\n", info.size);
    printf("  Last write position: %lu\n", info.last_write_pos);
    printf("  Open count: %d\n", info.open_count);

    // Закриваємо пристрій
    close(fd);
    return 0;
}