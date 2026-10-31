/* Purpose-limited patcher: accepts no path, can modify only the RAM copy.
 * Full before/after SHA-256 checks are performed by the surrounding loader.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

extern const unsigned char _binary_build_peek_bin_start[];
extern const unsigned char _binary_build_peek_bin_end[];
static const char target[] = "/run/mpc-clone-trial-v1/MPC-Sample";

static void die(const char *message) { fprintf(stderr, "K_PEEK_PATCH_FAILED: %s\n", message); exit(1); }
static void expect(int fd, off_t offset, const void *expected, size_t length) {
    unsigned char bytes[4096];
    if (length > sizeof(bytes) || pread(fd, bytes, length, offset) != (ssize_t)length
        || memcmp(bytes, expected, length)) die("original bytes differ");
}
static void write_at(int fd, off_t offset, const void *data, size_t length) {
    const unsigned char *bytes = data;
    while (length) {
        ssize_t n = pwrite(fd, bytes, length, offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) die("pwrite");
        bytes += n; offset += n; length -= (size_t)n;
    }
}
static uint32_t branch(uint64_t from, uint64_t to, int call) {
    int64_t delta = (int64_t)to - (int64_t)from;
    if ((delta & 3) || delta < -(1LL << 27) || delta >= (1LL << 27)) die("branch range");
    return (call ? 0x94000000U : 0x14000000U) | ((uint32_t)(delta / 4) & 0x3ffffffU);
}
int main(int argc, char **argv) {
    (void)argv;
    if (argc != 1) die("no arguments accepted");
    int fd = open(target, O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) die("open fixed RAM path");
    struct stat st; struct statfs fs;
    if (fstat(fd, &st) || fstatfs(fd, &fs)) die("stat");
    if (!S_ISREG(st.st_mode) || st.st_size != 119243440 || st.st_nlink != 1 || st.st_uid != 0)
        die("wrong file identity");
    if (fs.f_type != 0x01021994) die("target is not tmpfs");
    size_t length = _binary_build_peek_bin_end - _binary_build_peek_bin_start;
    if (length != 640) die("unexpected payload length");
    unsigned char zero[640] = {0};
    const uint64_t original_sizes[] = {0x6d3aff8, 0x6d3aff8};
    const uint32_t original_encoder = 0x37000041;
    uint32_t original_rotation = branch(0x3b7d360, 0xff9370, 1);
    uint32_t original_colour = branch(0x15cdeb4, 0x1131fa0, 1);
    expect(fd, 0xd0, original_sizes, sizeof(original_sizes));
    expect(fd, 0x16541a4, &original_encoder, 4);
    expect(fd, 0x3b7d360, &original_rotation, 4);
    expect(fd, 0x15cdeb4, &original_colour, 4);
    expect(fd, 0x6d3b000, zero, sizeof(zero));
    uint32_t encoder = branch(0x16541a4, 0x6d3b000, 0);
    uint32_t rotation = branch(0x3b7d360, 0x6d3b124, 1);
    uint32_t colour = branch(0x15cdeb4, 0x6d3b200, 1);
    uint64_t sizes[] = {0x6d3b000 + length, 0x6d3b000 + length};
    write_at(fd, 0x6d3b000, _binary_build_peek_bin_start, length);
    write_at(fd, 0x16541a4, &encoder, 4);
    write_at(fd, 0x3b7d360, &rotation, 4);
    write_at(fd, 0x15cdeb4, &colour, 4);
    write_at(fd, 0xd0, sizes, sizeof(sizes));
    if (fsync(fd) || close(fd)) die("finish RAM write");
    puts("K_PEEK_RAM_PATCH_OK");
    return 0;
}
