#include "phone_events.h"
#include <pthread.h>
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static struct phone_event events[PHONE_EVENT_CAPACITY];
static unsigned read_pos, write_pos, count;
int phone_events_push(const struct phone_event *event)
{
    int ok = 0;
    pthread_mutex_lock(&mutex);
    if (count < PHONE_EVENT_CAPACITY) {
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
