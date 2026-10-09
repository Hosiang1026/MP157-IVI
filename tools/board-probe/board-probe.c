#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>

static int write_rgb(unsigned char *p, int bpp, unsigned char r, unsigned char g, unsigned char b)
{
    if (bpp == 16) {
        unsigned short v = (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        memcpy(p, &v, 2);
        return 2;
    }
    if (bpp == 24) {
        p[0] = b;
        p[1] = g;
        p[2] = r;
        return 3;
    }
    if (bpp == 32) {
        p[0] = b;
        p[1] = g;
        p[2] = r;
        p[3] = 0xff;
        return 4;
    }
    return 0;
}

static int count_input_events(void)
{
    DIR *d = opendir("/dev/input");
    int n = 0;
    struct dirent *e;
    if (!d)
        return 0;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "event", 5) == 0)
            n++;
    }
    closedir(d);
    return n;
}

int main(int argc, char **argv)
{
    int no_draw = 0;
    int i;
    int fd;
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    size_t screen_size;
    unsigned char *fb;
    unsigned int line_bytes;
    unsigned int px;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--no-draw") == 0)
            no_draw = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("usage: %s [--no-draw]\n", argv[0]);
            return 0;
        }
    }

    fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        perror("open /dev/fb0");
        return 1;
    }
    if (ioctl(fd, FBIOGET_VSCREENINFO, &var) != 0) {
        perror("FBIOGET_VSCREENINFO");
        close(fd);
        return 1;
    }
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) != 0) {
        perror("FBIOGET_FSCREENINFO");
        close(fd);
        return 1;
    }

    line_bytes = fix.line_length ? fix.line_length : (var.xres * var.bits_per_pixel / 8);
    px = var.bits_per_pixel / 8;
    screen_size = (size_t)line_bytes * var.yres;
    printf("fb0 %ux%u %ubpp line=%u input_events=%d\n",
           var.xres, var.yres, var.bits_per_pixel, line_bytes, count_input_events());

    if (no_draw) {
        close(fd);
        return 0;
    }

    fb = mmap(NULL, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    for (i = 0; i < (int)var.yres; ++i) {
        unsigned int x;
        unsigned char *row = fb + (size_t)i * line_bytes;
        unsigned char r = (unsigned char)((i * 255) / (var.yres ? var.yres : 1));
        unsigned char g = 0x40;
        unsigned char b = (unsigned char)(255 - r);
        for (x = 0; x < var.xres; ++x) {
            if (!write_rgb(row + x * px, (int)var.bits_per_pixel, r, g, b))
                break;
        }
    }

    munmap(fb, screen_size);
    close(fd);
    printf("drew test pattern on /dev/fb0\n");
    return 0;
}
