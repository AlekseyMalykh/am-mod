#ifndef MPC_PHONE_INPUT_H
#define MPC_PHONE_INPUT_H
#include <stddef.h>
#include <stdint.h>

#define PHONE_MAX_CHARS 128
#define PHONE_MULTITAP_MS 1000

/* UI-thread state. Text is Unicode so native encoder edits are preserved. */
struct phone_input {
    uint32_t text[PHONE_MAX_CHARS];
    size_t length, cursor, limit;
    uint64_t last_ms;
    size_t pending_position;
    unsigned held, pending_pad, cycle;
    int enabled, numeric, uppercase, pending, shift;
};
void phone_init(struct phone_input *s, size_t limit);
int phone_sync(struct phone_input *s, const char *utf8, size_t cursor);
size_t phone_utf8(const struct phone_input *s, char *out, size_t capacity);
/* Display-only Unicode window with a visible insertion caret; text unchanged. */
void phone_view(const struct phone_input*,size_t columns,char*out,size_t capacity);
void phone_commit(struct phone_input *s);
void phone_native_boundary(struct phone_input *s);
void phone_move(struct phone_input *s,int delta);
void phone_enable(struct phone_input *s, int enabled);
int phone_tick(struct phone_input *s, uint64_t now_ms);
int phone_pad(struct phone_input *s, unsigned pad, int down, uint64_t now_ms);
uint64_t phone_light(const struct phone_input *s, unsigned pad);
#endif
