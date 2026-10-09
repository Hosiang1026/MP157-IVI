#include <dirent.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static int probe_fb(const char *path, int draw)
{
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        perror(path);
        return -1;
    }

    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &var) || ioctl(fd, FBIOGET_FSCREENINFO, &fix)) {
        perror("FBIOGET_*SCREENINFO");
        close(fd);
        return -1;
    }

    printf("fb: %s %ux%u bpp=%u line=%u id=%s\n",
           path, var.xres, var.yres, var.bits_per_pixel, fix.line_length, fix.id);

    size_t screen_size = (size_t)fix.line_length * var.yres;
    unsigned char *base = mmap(NULL, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }

    if (draw && var.bits_per_pixel >= 16) {
        unsigned bytes = var.bits_per_pixel / 8;
        for (unsigned y = 0; y < var.yres; ++y) {
            unsigned char *row = base + (size_t)y * fix.line_length;
            unsigned band = (y * 3) / (var.yres ? var.yres : 1);
            unsigned short c16 = (band == 0) ? 0xF800 : (band == 1) ? 0x07E0 : 0x001F;
            unsigned c32 = (band == 0) ? 0xFFFF0000u : (band == 1) ? 0xFF00FF00u : 0xFF0000FFu;
            for (unsigned x = 0; x < var.xres; ++x) {
                if (bytes == 2)
                    memcpy(row + (size_t)x * 2, &c16, 2);
                else if (bytes == 4)
                    memcpy(row + (size_t)x * 4, &c32, 4);
            }
        }
        printf("fb: drew RGB bars\n");
    }

    munmap(base, screen_size);
    close(fd);
    return 0;
}

static int is_touch_device(int fd)
{
    unsigned long absbits[(ABS_MAX + 1 + 8 * sizeof(long) - 1) / (8 * sizeof(long))];
    memset(absbits, 0, sizeof(absbits));
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) < 0)
        return 0;
    int has_x = absbits[ABS_X / (8 * sizeof(long))] & (1UL << (ABS_X % (8 * sizeof(long))));
    int has_y = absbits[ABS_Y / (8 * sizeof(long))] & (1UL << (ABS_Y % (8 * sizeof(long))));
    int has_mt = absbits[ABS_MT_POSITION_X / (8 * sizeof(long))] &
                 (1UL << (ABS_MT_POSITION_X % (8 * sizeof(long))));
    return (has_x && has_y) || has_mt;
}

static int probe_input(int wait_sec)
{
    DIR *dir = opendir("/dev/input");
    if (!dir) {
        perror("/dev/input");
        return -1;
    }

    int touch_fd = -1;
    char touch_path[256] = {0};
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) != 0)
            continue;
        char path[256];
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            continue;
        char name[256] = {0};
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        int touch = is_touch_device(fd);
        printf("input: %s name=\"%s\" touch=%d\n", path, name[0] ? name : "?", touch);
        if (touch && touch_fd < 0) {
            touch_fd = fd;
            snprintf(touch_path, sizeof(touch_path), "%s", path);
        } else {
            close(fd);
        }
    }
    closedir(dir);

    if (touch_fd < 0) {
        printf("input: no touch ABS device found\n");
        return -1;
    }

    if (wait_sec <= 0) {
        close(touch_fd);
        return 0;
    }

    printf("input: waiting %ds for touch on %s (Ctrl+C to abort)\n", wait_sec, touch_path);
    fd_set rfds;
    struct timeval tv;
    int left = wait_sec;
    while (left > 0) {
        FD_ZERO(&rfds);
        FD_SET(touch_fd, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int r = select(touch_fd + 1, &rfds, NULL, NULL, &tv);
        if (r > 0 && FD_ISSET(touch_fd, &rfds)) {
            struct input_event ev;
            ssize_t n = read(touch_fd, &ev, sizeof(ev));
            if (n == (ssize_t)sizeof(ev) &&
                (ev.type == EV_ABS || ev.type == EV_KEY || ev.type == EV_SYN)) {
                printf("input: got event type=%u code=%u value=%d\n",
                       ev.type, ev.code, ev.value);
                close(touch_fd);
                return 0;
            }
        }
        --left;
    }
    printf("input: timeout, no event\n");
    close(touch_fd);
    return -1;
}

int main(int argc, char **argv)
{
    int draw = 1;
    int wait_touch = 0;
    const char *fb = "/dev/fb0";

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--no-draw"))
            draw = 0;
        else if (!strcmp(argv[i], "--touch") && i + 1 < argc)
            wait_touch = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fb") && i + 1 < argc)
            fb = argv[++i];
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("usage: %s [--fb /dev/fb0] [--no-draw] [--touch SEC]\n", argv[0]);
            return 0;
        }
    }

    int rc = 0;
    if (probe_fb(fb, draw) != 0)
        rc = 1;
    if (probe_input(wait_touch) != 0 && wait_touch > 0)
        rc = 1;
    else if (wait_touch == 0)
        probe_input(0);
    return rc;
}
