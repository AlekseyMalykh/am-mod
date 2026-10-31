/* Execute the actual patch + stock focus traversal/expiry callback under QEMU.
 * Only hardware/UI-dependent repaint and Timer registration are substituted.
 * This cannot verify the real screen or hardware Timer lifecycle.
 */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

static unsigned char *base;
static void *started, *stopped;
static int milliseconds, starts, stops, repaints, selections;
static uint32_t colour_id;
static uint32_t native_colour(void *p, uint32_t id, int inherit) {
    assert(p && inherit == 0); colour_id = id; return 0xffeeeeee;
}
#define PTR(p, o) (*(void **)((unsigned char *)(p) + (o)))
#define FOCUS (*(void **)(base + 0x723cde0))

static void select_focus(void *container, void *p) { assert(container); FOCUS = p; selections++; }
static void timer_start(void *p, int ms) {
    started = p; milliseconds = ms; starts++;
    *(int *)((unsigned char *)p + 0x10) = ms; // Native startTimer stores interval here.
}
static void timer_stop(void *p) {
    stopped = p; stops++;
    *(int *)((unsigned char *)p + 0x10) = 0; // Native stopTimer clears interval.
}
static void paint(void *p) { assert(p); repaints++; }
static void stub(size_t offset, void *function) {
    uint32_t op[] = {0x58000050, 0xd61f0200}; /* ldr x16, .+8; br x16 */
    memcpy(base + offset, op, sizeof(op));
    memcpy(base + offset + 8, &function, sizeof(function));
}
static void clear_counts(void) {
    started = stopped = NULL;
    milliseconds = starts = stops = repaints = selections = 0;
}
static void load_range(FILE *f, size_t file_offset, size_t va, size_t n) {
    assert(fseek(f, file_offset, SEEK_SET) == 0);
    assert(fread(base + va, 1, n, f) == n);
}
static void run_at(uintptr_t address, const char *stock, const char *payload, const char *bad_payload) {
    base = mmap((void *)address, 0x7400000, PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    assert(base == (void *)address);
    FILE *f = fopen(stock, "rb"); assert(f);
    load_range(f, 0, 0, 0x6d3aff8); fclose(f);
    f = fopen(payload, "rb"); assert(f);
    size_t n = fread(base + 0x6d3b000, 1, 0x1000, f); assert(n > 292); fclose(f);
    uint32_t branch = 0x14000000 | (((0x6d3b000 - 0x16541a4) / 4) & 0x3ffffff);
    stub(0x15c13c0, select_focus);
    stub(0xff9370, timer_start);
    stub(0xff1bc0, timer_stop);
    stub(0x1149d70, paint);
    stub(0x1131fa0, native_colour);
    __builtin___clear_cache((char *)base, (char *)base + 0x7400000);
    unsigned char *screen = calloc(1, 0xa000), *controls = calloc(1, 0x2300);
    unsigned char *container = screen + 0x99c0;
    unsigned char sliders[4][0x350] = {{0}};
    unsigned char parent[0x30] = {0};
    void *vector[3];
    PTR(screen, 0) = base + 0x6e212a0;
    PTR(screen, 0x108) = controls;
    PTR(container, 0x70) = vector;
    PTR(container, 0x78) = vector + 3;
    for (int i = 0; i < 4; i++) {
        PTR(sliders[i], 0) = base + 0x706e5d0;
        PTR(sliders[i], 0x1d8) = base + 0x706e800;
        PTR(sliders[i], 0x18) = screen;
        *(double *)(sliders[i] + 0x250) = 123.0 + i;
        *(float *)(sliders[i] + 0x1b4) = 0.25f * i;
        if (i < 3) vector[i] = sliders[i] + 0x1d8;
    }
    void (*encoder)(void *, int) = (void *)(base + 0x16541a0); // Original BTI + patched B.
    void (*rotation)(void *, int) = (void *)(base + 0x6d3b124);
    void (*expiry)(void *) = (void *)(base + 0x3b7d2f0);
    uint32_t (*colour)(void *, uint32_t, int) = (void *)(base + 0x6d3b200);
    // Establish the ABI by executing the untouched original callback first.
    // Both versions must receive screen directly, never a synthetic listener.
    clear_counts(); FOCUS = vector[0];
    encoder(screen, 1);
    assert(FOCUS == vector[1] && selections == 1 && starts == 0 && stops == 0);
    controls[0x22a0] = 1;
    encoder(screen, 1); assert(FOCUS == vector[0]);
    encoder(screen, 1); assert(FOCUS == vector[2]);
    controls[0x22a0] = 0;
    encoder(screen, 1); assert(FOCUS == vector[0]);
    memcpy(base + 0x16541a4, &branch, 4);
    __builtin___clear_cache((char *)base + 0x16541a0, (char *)base + 0x16541c4);
    // Reproduce r1's actual bug in a child using that same, proven ABI.
    pid_t child = fork(); assert(child >= 0);
    if (child == 0) {
        struct rlimit no_core = {0, 0}; setrlimit(RLIMIT_CORE, &no_core);
        FILE *bad = fopen(bad_payload, "rb"); assert(bad);
        assert(fread(base + 0x6d3b000, 1, 608, bad) == 608); fclose(bad);
        __builtin___clear_cache((char *)base + 0x6d3b000, (char *)base + 0x6d3b260);
        encoder(screen, 1);
        _exit(99); // The saved broken release must fail with SIGSEGV.
    }
    int child_status; assert(waitpid(child, &child_status, 0) == child);
    assert(WIFSIGNALED(child_status) && WTERMSIG(child_status) == SIGSEGV);
    printf("PASS original callback ABI; saved r1 crash reproduced in isolated child\n");
    clear_counts();
    encoder(NULL, 0); assert(starts == 0 && selections == 0);
    FOCUS = vector[0];
    encoder(screen, 1);
    assert(FOCUS == vector[1] && selections == 1 && starts == 1 && milliseconds == 2000);
    assert(started == sliders[1] + 0x1f8 && stopped == sliders[0] + 0x1f8);
    assert(sliders[1][0x230] == 1 && sliders[0][0x230] == 0 && repaints == 2);
    assert(colour(sliders[1], 0x1001400, 0) == 0xffffc247);
    assert(colour(sliders[0], 0x1001400, 0) == 0xffeeeeee && colour_id == 0x1001400);
    // Changing the previewed value must turn it white without changing focus.
    rotation(sliders[1] + 0x1f8, 500);
    assert(milliseconds == 2001 && FOCUS == vector[1]);
    assert(colour(sliders[1], 0x1001400, 0) == 0xffeeeeee);
    // Even with multiple still-live value flags/timers, only focus can be yellow.
    for (int i = 0; i < 3; i++) {
        sliders[i][0x230] = 1;
        *(int *)(sliders[i] + 0x208) = 2000;
    }
    for (int i = 0; i < 3; i++)
        assert(colour(sliders[i], 0x1001400, 0) == (i == 1 ? 0xffffc247 : 0xffeeeeee));
    FOCUS = vector[2];
    assert(colour(sliders[1], 0x1001400, 0) == 0xffeeeeee);
    assert(colour(sliders[2], 0x1001400, 0) == 0xffffc247);
    // Background/property changes also become white, even if they set the stock flag.
    rotation(sliders[2] + 0x1f8, 500);
    assert(colour(sliders[2], 0x1001400, 0) == 0xffeeeeee);
    for (int i = 0; i < 3; i++) {
        sliders[i][0x230] = 0;
        *(int *)(sliders[i] + 0x208) = 0;
    }
    FOCUS = vector[1];
    controls[0x22a0] = 1;
    encoder(screen, 1);
    assert(FOCUS == vector[0] && sliders[0][0x230] == 1 && sliders[1][0x230] == 0);
    encoder(screen, 1); assert(FOCUS == vector[2]); // Reverse wrap, actual stock code.
    controls[0x22a0] = 0;
    encoder(screen, 1); assert(FOCUS == vector[0]); // Forward wrap.
    expiry(sliders[0] + 0x1f8); // Actual stock callback clears flag and stops timer.
    assert(sliders[0][0x230] == 0 && stopped == sliders[0] + 0x1f8);
    assert(colour(sliders[0], 0x1001400, 0) == 0xffeeeeee);
    FOCUS = NULL;
    encoder(screen, 1); assert(FOCUS == vector[0]);
    clear_counts();
    FOCUS = sliders[3] + 0x1d8; // A valid slider outside the live focus vector.
    encoder(screen, 1);
    assert(stops == 0 && FOCUS == vector[0] && starts == 1);
    clear_counts();
    PTR(screen, 0) = base + 0x6e170c0; // Other screen: preserve stock traversal only.
    encoder(screen, 1); assert(FOCUS == vector[1] && selections == 1 && starts == 0 && stops == 0);
    PTR(screen, 0) = base + 0x6e212a0;
    clear_counts();
    PTR(container, 0x78) = vector; // Empty live vector.
    encoder(screen, 1); assert(starts == 0 && stops == 0 && selections == 0);
    PTR(container, 0x78) = vector + 3;
    for (int i = 0; i < 3; i++) PTR(sliders[i], 0x1d8) = base + 0x1000;
    clear_counts(); encoder(screen, 1); assert(starts == 0 && stops == 0 && selections == 1);
    for (int i = 0; i < 3; i++) PTR(sliders[i], 0x1d8) = base + 0x706e800;
    clear_counts(); rotation(sliders[0] + 0x1f8, 500);
    assert(starts == 1 && milliseconds == 2001 && started == sliders[0] + 0x1f8);
    PTR(sliders[0], 0x18) = parent;
    PTR(parent, 0) = base + 0x1000; PTR(parent, 0x18) = screen;
    rotation(sliders[0] + 0x1f8, 500); assert(milliseconds == 2001);
    sliders[0][0x230] = 1;
    FOCUS = vector[0];
    assert(colour(sliders[0], 0x1001400, 0) == 0xffeeeeee);
    *(int *)(sliders[0] + 0x208) = 2000;
    assert(colour(sliders[0], 0x1001400, 0) == 0xffffc247);
    PTR(parent, 0x18) = NULL;
    rotation(sliders[0] + 0x1f8, 500); assert(milliseconds == 500);
    assert(colour(sliders[0], 0x1001400, 0) == 0xffeeeeee);
    PTR(parent, 0x18) = parent; // Bound traversal, even with a cyclic test graph.
    rotation(sliders[0] + 0x1f8, 500); assert(milliseconds == 500);
    assert(colour(sliders[0], 0x1001400, 0) == 0xffeeeeee);
    PTR(sliders[0], 0) = base + 0x1000;
    rotation(sliders[0] + 0x1f8, 500); assert(milliseconds == 500);
    for (int i = 0; i < 4; i++) {
        assert(*(double *)(sliders[i] + 0x250) == 123.0 + i);
        assert(*(float *)(sliders[i] + 0x1b4) == 0.25f * i);
    }
    printf("PASS actual ARM payload, stock focus traversal and expiry; PIE base 0x%lx\n", address);
    free(screen); free(controls);
    assert(munmap(base, 0x7400000) == 0);
}
int main(int argc, char **argv) {
    assert(argc == 4);
    run_at(0x10000000, argv[1], argv[2], argv[3]);
    run_at(0x30000000, argv[1], argv[2], argv[3]);
    return 0;
}
