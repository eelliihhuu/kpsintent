#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <alsa/asoundlib.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define SAMPLE_RATE 44100

typedef struct {
    float *ring_buffer;
    int buffer_size;
    int head;
} StringState;

static int l_play_string(lua_State *L) {
    int num_notes = 0;
    double *frequencies = NULL;

    // Handle either a single frequency or a table of frequencies
    if (lua_istable(L, 1)) {
        num_notes = (int)lua_rawlen(L, 1);
        frequencies = (double *)malloc(sizeof(double) * num_notes);
        for (int i = 1; i <= num_notes; i++) {
            lua_rawgeti(L, 1, i);
            frequencies[i - 1] = luaL_checknumber(L, -1);
            lua_pop(L, 1);
        }
    } else {
        num_notes = 1;
        frequencies = (double *)malloc(sizeof(double));
        frequencies[0] = luaL_checknumber(L, 1);
    }

    double duration_seconds = luaL_checknumber(L, 2);
    float decay = (float)luaL_optnumber(L, 3, 0.996);
    float brightness = (float)luaL_optnumber(L, 4, 0.5);
    float volume = (float)luaL_optnumber(L, 5, 0.5);

    int err;
    snd_pcm_t *handle;
    snd_pcm_sframes_t frames;

    if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        return luaL_error(L, "Playback open error: %s", snd_strerror(err));
    }

    if ((err = snd_pcm_set_params(handle,
                                  SND_PCM_FORMAT_S16_LE,
                                  SND_PCM_ACCESS_RW_INTERLEAVED,
                                  1,
                                  SAMPLE_RATE,
                                  1,
                                  500000)) < 0) {
        snd_pcm_close(handle);
        return luaL_error(L, "Playback parameters error: %s", snd_strerror(err));
    }

    StringState *strings = (StringState *)malloc(sizeof(StringState) * num_notes);
    for (int n = 0; n < num_notes; n++) {
        strings[n].buffer_size = (int)(SAMPLE_RATE / frequencies[n]);
        strings[n].ring_buffer = (float *)malloc(sizeof(float) * strings[n].buffer_size);
        strings[n].head = 0;

        if (!strings[n].ring_buffer) {
            // Simple cleanup for brevity; in production, loop back and free previously allocated buffers
            return luaL_error(L, "Memory allocation failed for ring buffer %d", n);
        }

        for (int i = 0; i < strings[n].buffer_size; i++) {
            strings[n].ring_buffer[i] = ((float)rand() / (float)(RAND_MAX)) * 2.0f - 1.0f;
        }
    }

    int total_frames = (int)(SAMPLE_RATE * duration_seconds);
    short *output_pcm = (short *)malloc(sizeof(short) * 1024);
    if (!output_pcm) {
        // Cleanup logic would go here
        snd_pcm_close(handle);
        return luaL_error(L, "Memory allocation failed for PCM buffer");
    }

    int processed_frames = 0;
    while (processed_frames < total_frames) {
        int chunk = (total_frames - processed_frames > 1024) ? 1024 : (total_frames - processed_frames);

        for (int i = 0; i < chunk; i++) {
            float mixed_sample = 0.0f;

            for (int n = 0; n < num_notes; n++) {
                float current_s = strings[n].ring_buffer[strings[n].head];
                int next_index = (strings[n].head + 1) % strings[n].buffer_size;
                float next_s = strings[n].ring_buffer[next_index];

                float new_s = (current_s * (1.0f - brightness) + next_s * brightness) * decay;
                strings[n].ring_buffer[strings[n].head] = new_s;
                strings[n].head = next_index;

                mixed_sample += current_s;
            }
            
            output_pcm[i] = (short)(mixed_sample * 32767.0f * volume);
        }

        frames = snd_pcm_writei(handle, output_pcm, chunk);
        if (frames < 0) {
            frames = snd_pcm_prepare(handle);
        }
        processed_frames += chunk;
    }

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    for (int n = 0; n < num_notes; n++) {
        free(strings[n].ring_buffer);
    }
    free(strings);
    free(output_pcm);
    free(frequencies);

    return 0;
}

static const struct luaL_Reg kps_funcs[] = {
    {"play", l_play_string},
    {NULL, NULL}
};

int luaopen_kpsintent(lua_State *L) {
    srand((unsigned int)time(NULL));
    luaL_newlib(L, kps_funcs);
    return 1;
}
