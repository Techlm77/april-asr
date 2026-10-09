/* Wrapped writes, full-buffer backpressure and concurrent sample ordering. */
#include <assert.h>
#include <stdlib.h>
#include "audio_provider.h"
#include "thread_compat.h"
#define SAMPLES 1000000
static int consume(void *arg) {
    AudioProvider p = arg;
    size_t read = 0;
    while (read < SAMPLES) {
        size_t count = 137;
        short *audio = ap_pull_audio(p, &count);
        if (!count) { thrd_yield(); continue; }
        for (size_t j = 0; j < count; ++j) assert(audio[j] == (short)((read + j) % 30000));
        read += count;
        ap_pull_audio_finish(p, count);
    }
    return 0;
}
int main(void) {
    AudioProvider p = ap_create();
    assert(p);
    short *full = calloc(48000, sizeof(short));
    assert(ap_push_audio(p, full, 48000));
    short value = 99;
    assert(!ap_push_audio(p, &value, 1));
    size_t count = 0;
    short *audio = ap_pull_audio(p, &count);
    assert(audio && count == 48000 && audio[count - 1] == 0);
    ap_pull_audio_finish(p, count);
    free(full);
    thrd_t consumer;
    assert(thrd_create(&consumer, consume, p) == thrd_success);
    size_t written = 0;
    short chunk[211];
    while (written < SAMPLES) {
        size_t n = SAMPLES - written < 211 ? SAMPLES - written : 211;
        for (size_t j = 0; j < n; ++j) chunk[j] = (short)((written + j) % 30000);
        if (ap_push_audio(p, chunk, n)) written += n;
        else thrd_yield();
    }
    thrd_join(consumer, NULL);
    count = 0;
    assert(!ap_pull_audio(p, &count) && count == 0);
    ap_free(p);
    return 0;
}
