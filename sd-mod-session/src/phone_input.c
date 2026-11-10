#include "phone_input.h"
#include <string.h>
#include "pad_palette.h"

void phone_init(struct phone_input *s, size_t limit)
{
    memset(s, 0, sizeof(*s));
    s->limit = limit < PHONE_MAX_CHARS ? limit : PHONE_MAX_CHARS;
}

int phone_sync(struct phone_input *s, const char *utf8, size_t cursor)
{
    uint32_t decoded[PHONE_MAX_CHARS];
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)utf8;
    if (!p) return 0;
    while (*p) {
        uint32_t c = *p++;
        unsigned extra = 0;
        uint32_t min = 0;
        if (c >= 0x80) {
            if ((c & 0xe0) == 0xc0) { c &= 0x1f; extra = 1; min = 0x80; }
            else if ((c & 0xf0) == 0xe0) { c &= 0xf; extra = 2; min = 0x800; }
            else if ((c & 0xf8) == 0xf0) { c &= 7; extra = 3; min = 0x10000; }
            else return 0;
            while (extra--) {
                if ((*p & 0xc0) != 0x80) return 0;
                c = (c << 6) | (*p++ & 0x3f);
            }
            if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return 0;
        }
        if (n >= s->limit) return 0; /* Never truncate an existing name. */
        decoded[n++] = c;
    }
    memcpy(s->text, decoded, n * sizeof(decoded[0]));
    s->length = n;
    s->cursor = cursor > n ? n : cursor;
    s->pending = 0;
    s->pending_pad = 0;
    return 1;
}

size_t phone_utf8(const struct phone_input *s, char *out, size_t cap)
{
    size_t n = 0;
    if (!cap) return 0;
    for (size_t i = 0; i < s->length; ++i) {
        uint32_t c = s->text[i];
        unsigned bytes = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
        if (n + bytes >= cap) { out[0] = 0; return 0; }
        if (bytes == 1) out[n++] = (char)c;
        else {
            unsigned shift = 6 * (bytes - 1);
            out[n++] = (char)((bytes == 2 ? 0xc0 : bytes == 3 ? 0xe0 : 0xf0) | (c >> shift));
            while (shift) { shift -= 6; out[n++] = (char)(0x80 | ((c >> shift) & 0x3f)); }
        }
    }
    out[n] = 0;
    return n;
}

void phone_commit(struct phone_input *s)
{
    if (s->pending) s->cursor = s->pending_position + 1;
    s->pending = 0;
    s->pending_pad = 0;
    s->cycle = 0;
}
void phone_view(const struct phone_input*s,size_t columns,char*out,size_t capacity){
    struct phone_input view=*s;
    size_t caret=s->pending?s->pending_position+1:s->cursor;
    if(caret>s->length)caret=s->length;
    if(columns>PHONE_MAX_CHARS)columns=PHONE_MAX_CHARS;
    if(!columns)columns=1;
    size_t visible=columns-1;
    size_t start=s->length>visible&&caret>visible/2?caret-visible/2:0;
    if(s->length>visible&&start>s->length-visible)start=s->length-visible;
    view.length=s->length-start;
    if(view.length>visible)view.length=visible;
    memmove(view.text,s->text+start,view.length*sizeof(view.text[0]));
    size_t at=caret-start;if(at>view.length)at=view.length;
    memmove(view.text+at+1,view.text+at,(view.length-at)*sizeof(view.text[0]));view.text[at]='|';++view.length;
    phone_utf8(&view,out,capacity);
}

void phone_native_boundary(struct phone_input *s)
{
    /* Native controls own their cursor movement. Advancing here as well would
     * make + skip a position and make the encoder edit the next character. */
    s->pending = 0;
    s->pending_pad = 0;
    s->cycle = 0;
}
void phone_move(struct phone_input*s,int delta){
    phone_commit(s);
    if(delta<0){size_t n=(size_t)(-(int64_t)delta);s->cursor=n>s->cursor?0:s->cursor-n;}
    else{size_t n=(size_t)delta;s->cursor=n>s->length-s->cursor?s->length:s->cursor+n;}
}

void phone_enable(struct phone_input *s, int enabled)
{
    phone_commit(s);
    s->enabled = !!enabled;
    s->uppercase = 0;
    s->held = 0;
}

int phone_tick(struct phone_input *s, uint64_t now_ms)
{
    /* Exactly 1000 ms still belongs to the same multi-tap group. */
    if (s->pending && now_ms >= s->last_ms && now_ms - s->last_ms > PHONE_MULTITAP_MS) {
        phone_commit(s);
        return 1;
    }
    return 0;
}

static int put(struct phone_input *s, uint32_t c,int replace)
{
    if (s->cursor >= s->limit) return 0;
    if(!replace){
        if(s->length>=s->limit)return 0;
        memmove(s->text+s->cursor+1,s->text+s->cursor,(s->length-s->cursor)*sizeof(s->text[0]));++s->length;
    }
    s->text[s->cursor] = c;
    return 1;
}

/* Delete the character before the cursor, or the one at the cursor when
 * forward is set (Shift+pad 16, and Erase in encoder mode). */
int phone_delete(struct phone_input *s, int forward)
{
    phone_commit(s);
    if (forward ? s->cursor >= s->length : !s->cursor) return 0;
    if (!forward) --s->cursor;
    memmove(s->text + s->cursor, s->text + s->cursor + 1,
            (s->length - s->cursor - 1) * sizeof(s->text[0]));
    --s->length;
    return 1;
}
int phone_pad(struct phone_input *s, unsigned pad, int down, uint64_t now_ms)
{
    static const char *const letters[17] = {
        0, 0, " ", 0, 0, "pqrs", "tuv", "wxyz", ".-_#",
        "ghi", "jkl", "mno", 0, "_", "abc", "def", 0
    };
    static const char digits[17] = {0,0,'0',0,0,'7','8','9',0,'4','5','6',0,'1','2','3',0};
    if (!s->enabled || pad < 1 || pad > 16) return 0;
    unsigned bit = 1u << (pad - 1);
    if (!down) {
        int was_held = !!(s->held & bit);
        s->held &= ~bit;
        if (pad == 12 && was_held) { phone_commit(s); s->uppercase = 0; }
        return 1;
    }
    /* A duplicate down right after the first is not another tap. A down long
     * after it means the release was lost (another window took it, a mode
     * change): treat it as a new tap so the pad never stays locked. */
    if ((s->held & bit) && now_ms >= s->down_ms[pad - 1] && now_ms - s->down_ms[pad - 1] < PHONE_DUPLICATE_DOWN_MS) return 1;
    s->held |= bit;
    s->down_ms[pad - 1] = now_ms;
    phone_tick(s, now_ms);
    if (pad == 12) { phone_commit(s); s->uppercase = 1; return 1; }
    if (pad == 4) { phone_commit(s); s->numeric = !s->numeric; return 1; }
    if (pad == 1 || pad == 3) {
        phone_commit(s);
        if (pad == 1 && s->cursor) --s->cursor;
        if (pad == 3 && s->cursor < s->length) ++s->cursor;
        return 1;
    }
    if (pad == 16) {
        phone_delete(s, s->shift);
        return 1;
    }
    const char *group = letters[pad];
    if (s->numeric && digits[pad]) {
        phone_commit(s);
        if (put(s, (uint32_t)digits[pad],0)) ++s->cursor;
        return 1;
    }
    if (!group) return 1;
    size_t count = strlen(group);
    int replace=s->pending && s->pending_pad == pad;
    if (replace) {
        s->cycle = (s->cycle + 1) % (unsigned)count;
        s->cursor = s->pending_position;
    } else {
        phone_commit(s);
        s->cycle = 0;
    }
    uint32_t c = (unsigned char)group[s->cycle];
    if (s->uppercase && c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (put(s, c,replace)) {
        s->pending_position = s->cursor;
        s->pending_pad = pad;
        s->last_ms = now_ms;
        s->pending = 1;
        if (count == 1) phone_commit(s);
    }
    return 1;
}

uint64_t phone_light(const struct phone_input *s, unsigned pad)
{
    uint32_t rgb;
    float brightness = 0.25f;
    if (pad == 16) { rgb = 0xff0000; brightness = 0.65f; }
    else if (pad == 8 || (pad == 13 && !s->numeric)) rgb = 0xffc247;
    else if (pad == 12) { rgb = 0xa84aff; brightness = s->uppercase ? 0.55f : 0.15f; }
    else if (pad == 1 || pad == 3) { rgb = 0x2030ff; brightness = 0.40f; }
    /* The naming keypad has its own functional legend, independent of the
     * musical user/service palette edited in Mod Settings. */
    else rgb = s->numeric ? 0x00dc64 : 0xffffffu;
    if (pad >= 1 && pad <= 16 && (s->held & (1u << (pad - 1))))
        brightness = pad == 16 ? 1.0f : 0.85f;
    uint32_t bits;
    memcpy(&bits, &brightness, sizeof(bits));
    return ((uint64_t)bits << 32) | rgb;
}

void phone_turn_character(struct phone_input*name,int delta)
{
    static const char alphabet[]=" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.";
    phone_commit(name);
    if(!delta||name->cursor>=name->limit)return;
    int position=0;
    if(name->cursor<name->length)
        for(unsigned i=0;alphabet[i];++i)if((unsigned char)alphabet[i]==name->text[name->cursor]){position=(int)i;break;}
    int length=sizeof(alphabet)-1;
    position=(position+delta)%length;
    if(position<0)position+=length;
    if(name->cursor==name->length)++name->length;
    name->text[name->cursor]=(unsigned char)alphabet[position];
}
