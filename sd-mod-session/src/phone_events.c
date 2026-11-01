#include "phone_events.h"
#include <pthread.h>
#include <string.h>
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static struct phone_event events[PHONE_EVENT_CAPACITY];
static unsigned read_pos, write_pos, count;
static int absolute_knob(const struct phone_event *e)
{
    return e->kind == PHONE_CONTROLLER && e->native[8] >= 0x10 && e->native[8] <= 0x12;
}
static int same_knob_context(const struct phone_event *a,const struct phone_event *b)
{
    return absolute_knob(a) && a->generation == b->generation && a->pad == b->pad &&
        a->step_ticket == b->step_ticket && a->workflow_ticket == b->workflow_ticket &&
        !memcmp(a->native,b->native,8) && !memcmp(a->native+10,b->native+10,6);
}
/* Absolute K1-K3 positions can replace an older position in the trailing
 * knob run. Move the replacement to its actual arrival position: K1,K2,K1
 * must leave focus on K1. Never cross a button/pad, relative encoder,
 * ownership/generation change or native source. A fast sweep then needs
 * at most three slots instead of overflowing and bypassing older UI work. */
static int replace_knob(const struct phone_event *event)
{
    if (!absolute_knob(event)) return 0;
    for (unsigned back=0;back<count;++back) {
        unsigned pos=(write_pos+PHONE_EVENT_CAPACITY-1-back)%PHONE_EVENT_CAPACITY;
        if (!same_knob_context(&events[pos],event)) break;
        if (events[pos].native[8] != event->native[8]) continue;
        while ((pos+1)%PHONE_EVENT_CAPACITY != write_pos) {
            unsigned next=(pos+1)%PHONE_EVENT_CAPACITY;
            events[pos]=events[next];pos=next;
        }
        events[pos]=*event;
        return 1;
    }
    return 0;
}
int phone_events_push(const struct phone_event *event)
{
    int ok = 0;
    pthread_mutex_lock(&mutex);
    if (replace_knob(event)) ok=1;
    else if (count < PHONE_EVENT_CAPACITY) {
        events[write_pos] = *event;
        write_pos = (write_pos + 1) % PHONE_EVENT_CAPACITY;
        ++count; ok = 1;
    }
    pthread_mutex_unlock(&mutex);
    return ok;
}
int phone_events_pop(struct phone_event *event)
{
    int ok = 0;
    pthread_mutex_lock(&mutex);
    if (count) {
        *event = events[read_pos];
        read_pos = (read_pos + 1) % PHONE_EVENT_CAPACITY;
        --count; ok = 1;
    }
    pthread_mutex_unlock(&mutex);
    return ok;
}
unsigned phone_events_pending(void)
{
    pthread_mutex_lock(&mutex);
    unsigned n = count;
    pthread_mutex_unlock(&mutex);
    return n;
}
