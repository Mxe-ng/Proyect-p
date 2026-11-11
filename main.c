// "Corazon Esquiva" - juego original para PSP (homebrew, PSPSDK)
// Esquiva proyectiles dentro de una caja y luego elige HABLAR o PERDONAR.
// Musica y efectos: sintetizados en codigo (originales). Graficos: renderer por software.
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psprtc.h>
#include <pspaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

PSP_MODULE_INFO("CorazonEsquiva", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define STRIDE 512
#define MAXB 128
#define MAXP 160
#define NSTAR 48
#define PI_F 3.14159265f

#define RGB(r,g,b) (0xFF000000u | ((unsigned)(b) << 16) | ((unsigned)(g) << 8) | (unsigned)(r))

static volatile int g_song = 1;      // 0 batalla, 1 calma, 2 silencio
static volatile int g_sfx_req = 0;
static volatile int g_quit = 0;

enum { SFX_NONE, SFX_HIT, SFX_SELECT, SFX_CONFIRM, SFX_SPARE, SFX_LOSE, SFX_TALK };

/* ===================================================================== */
/*  CALLBACKS                                                            */
/* ===================================================================== */
static int exit_cb(int a, int b, void *c) { g_quit = 1; sceKernelExitGame(); return 0; }
static int cb_thread(SceSize args, void *argp) {
    int id = sceKernelCreateCallback("exit", exit_cb, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_callbacks(void) {
    int t = sceKernelCreateThread("cb", cb_thread, 0x11, 0xFA0, 0, NULL);
    if (t >= 0) sceKernelStartThread(t, 0, NULL);
}

/* ===================================================================== */
/*  AUDIO: sintetizador chiptune (musica original + efectos)             */
/* ===================================================================== */
#define SR 44100
#define NSAMP 512

static const unsigned char battle_lead[32] = {
    69,0,72,0,76,0,74,72,   71,0,74,0,79,0,76,74,
    72,0,76,0,81,0,79,76,   74,0,77,76,74,72,71,0 };
static const unsigned char battle_bass[32] = {
    45,0,45,0,52,0,45,0,    43,0,43,0,50,0,43,0,
    41,0,41,0,48,0,41,0,    50,0,50,0,57,0,50,0 };
static const unsigned char battle_arp[32] = {
    57,60,64,60,57,60,64,60, 55,59,62,59,55,59,62,59,
    53,57,60,57,53,57,60,57, 50,53,57,53,50,53,57,53 };

static const unsigned char calm_lead[32] = {
    76,0,79,0,81,79,76,0,   74,0,76,0,79,0,74,0,
    72,0,76,0,79,81,79,76,  74,0,0,0,72,0,0,0 };
static const unsigned char calm_bass[32] = {
    48,0,0,0,55,0,0,0,      43,0,0,0,50,0,0,0,
    45,0,0,0,52,0,0,0,      41,0,0,0,48,0,0,0 };
static const unsigned char calm_arp[32] = {
    60,64,67,64,60,64,67,64, 59,62,67,62,59,62,67,62,
    57,60,64,60,57,60,64,60, 57,60,65,60,57,60,65,60 };

static float mtof(int n) { return 440.0f * powf(2.0f, (n - 69) / 12.0f); }

static int audio_thread(SceSize args, void *argp) {
    int ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, NSAMP, PSP_AUDIO_FORMAT_STEREO);
    if (ch < 0) return 0;
    static short buf[NSAMP * 2];

    int song = -1, step = 0, samp = 0, spstep = 10000;
    float phl = 0, phb = 0, pha = 0, kph = 0;
    float fl = 0, fbs = 0, fa = 0;
    unsigned int rng = 2463534242u;
    int dtype = 0, dsamp = 0;
    int sfx = 0, sfxt = 0;
    float sfxph = 0;

    while (!g_quit) {
        for (int i = 0; i < NSAMP; i++) {
            int s = g_song;
            if (s != song) {
                song = s;
                spstep = (song == 0) ? (SR * 60 / (150 * 2)) : (SR * 60 / (96 * 2));
                step = 31;
                samp = spstep;
            }
            if (samp >= spstep) {
                samp = 0;
                step = (step + 1) & 31;
                if (song == 2) {
                    fl = fbs = fa = 0; dtype = 0;
                } else {
                    const unsigned char *L = (song == 0) ? battle_lead : calm_lead;
                    const unsigned char *B = (song == 0) ? battle_bass : calm_bass;
                    const unsigned char *A = (song == 0) ? battle_arp : calm_arp;
                    fl  = L[step] ? mtof(L[step]) : 0;
                    fbs = B[step] ? mtof(B[step]) : 0;
                    fa  = A[step] ? mtof(A[step]) : 0;
                    dtype = 0;
                    if (song == 0) {
                        if ((step & 7) == 0) dtype = 1;
                        else if ((step & 7) == 4) dtype = 2;
                        else if (step & 1) dtype = 3;
                    } else {
                        if ((step & 15) == 0) dtype = 1;
                        else if ((step & 3) == 2) dtype = 3;
                    }
                    dsamp = 0;
                }
            }
            float t = (float)samp / (float)spstep;
            float m = 0;

            if (fl > 0) {
                phl += fl / SR; if (phl >= 1.0f) phl -= 1.0f;
                m += (phl < 0.25f ? 1.0f : -1.0f) * 0.16f * (1.0f - 0.55f * t);
            }
            if (fbs > 0) {
                phb += fbs / SR; if (phb >= 1.0f) phb -= 1.0f;
                m += (4.0f * fabsf(phb - 0.5f) - 1.0f) * 0.30f * (1.0f - 0.3f * t);
            }
            if (fa > 0) {
                pha += fa / SR; if (pha >= 1.0f) pha -= 1.0f;
                m += (pha < 0.5f ? 1.0f : -1.0f) * 0.07f * (1.0f - t);
            }
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            float nz = (rng & 1) ? 1.0f : -1.0f;
            if (dtype) {
                int dur = (dtype == 1) ? 5500 : ((dtype == 2) ? 7000 : 1800);
                if (dsamp < dur) {
                    float dt = (float)dsamp / (float)dur;
                    if (dtype == 1) {
                        kph += (130.0f - 95.0f * dt) / SR; if (kph >= 1.0f) kph -= 1.0f;
                        m += (4.0f * fabsf(kph - 0.5f) - 1.0f) * 0.45f * (1.0f - dt);
                    } else if (dtype == 2) {
                        m += nz * 0.16f * (1.0f - dt);
                    } else {
                        m += nz * 0.05f * (1.0f - dt);
                    }
                    dsamp++;
                }
            }

            if (g_sfx_req) { sfx = g_sfx_req; g_sfx_req = 0; sfxt = 0; sfxph = 0; }
            if (sfx) {
                float f = 0, v = 0, x = 0;
                int dur = 1;
                switch (sfx) {
                case SFX_HIT:
                    dur = 9000; x = (float)sfxt / dur; f = 220.0f - 160.0f * x;
                    sfxph += f / SR; if (sfxph >= 1.0f) sfxph -= 1.0f;
                    v = ((sfxph < 0.5f ? 1.0f : -1.0f) * 0.5f + nz * 0.5f) * (1.0f - x) * 0.45f;
                    break;
                case SFX_SELECT:
                    dur = 2200; x = (float)sfxt / dur; f = 880.0f;
                    sfxph += f / SR; if (sfxph >= 1.0f) sfxph -= 1.0f;
                    v = (sfxph < 0.5f ? 1.0f : -1.0f) * 0.22f * (1.0f - x);
                    break;
                case SFX_CONFIRM:
                    dur = 6000; x = (float)sfxt / dur; f = (sfxt < 2500) ? 660.0f : 990.0f;
                    sfxph += f / SR; if (sfxph >= 1.0f) sfxph -= 1.0f;
                    v = (sfxph < 0.5f ? 1.0f : -1.0f) * 0.22f * (1.0f - x);
                    break;
                case SFX_SPARE: {
                    static const float arp[4] = { 523.0f, 659.0f, 784.0f, 1047.0f };
                    dur = 14000;
                    int idx = sfxt / 3500; if (idx > 3) idx = 3;
                    float local = (float)(sfxt % 3500) / 3500.0f;
                    f = arp[idx];
                    sfxph += f / SR; if (sfxph >= 1.0f) sfxph -= 1.0f;
                    v = (sfxph < 0.5f ? 1.0f : -1.0f) * 0.22f * (1.0f - 0.6f * local);
                    break;
                }
                case SFX_LOSE:
                    dur = 32000; x = (float)sfxt / dur;
                    f = 440.0f - 330.0f * x + 8.0f * sinf(sfxt * 0.002f);
                    sfxph += f / SR; if (sfxph >= 1.0f) sfxph -= 1.0f;
                    v = (sfxph < 0.5f ? 1.0f : -1.0f) * 0.25f * (1.0f - x);
                    break;
                case SFX_TALK: {
                    dur = 8000;
                    int slot = sfxt / 1300;
                    int loc = sfxt % 1300;
                    f = 300.0f + (float)((slot * 53) % 5) * 35.0f;
                    sfxph += f / SR; if (sfxph >= 1.0f) sfxph -= 1.0f;
                    v = (loc < 1000) ? (sfxph < 0.5f ? 1.0f : -1.0f) * 0.18f * (1.0f - (float)loc / 1000.0f) : 0.0f;
                    break;
                }
                default: dur = 1; break;
                }
                m += v;
                sfxt++;
                if (sfxt >= dur) sfx = 0;
            }

            samp++;
            m *= 0.8f;
            if (m > 1.0f) m = 1.0f;
            if (m < -1.0f) m = -1.0f;
            short o = (short)(m * 30000.0f);
            buf[i * 2] = o;
            buf[i * 2 + 1] = o;
        }
        sceAudioOutputBlocking(ch, PSP_AUDIO_VOLUME_MAX, buf);
    }
    sceAudioChRelease(ch);
    return 0;
}

static void sfx_play(int id) { g_sfx_req = id; }

/* ===================================================================== */
/*  GRAFICOS: canvas en RAM + alpha blending, copiado a VRAM             */
/* ===================================================================== */
static u32 canvas[STRIDE * SH] __attribute__((aligned(64)));
static u32 *vram[2];
static int ox = 0, oy = 0;

static inline u32 mixc(u32 d, u32 s, int a) {
    int dr = d & 255, dg = (d >> 8) & 255, db = (d >> 16) & 255;
    int sr = s & 255, sg = (s >> 8) & 255, sb = (s >> 16) & 255;
    int r = (dr * (255 - a) + sr * a) / 255;
    int g = (dg * (255 - a) + sg * a) / 255;
    int b = (db * (255 - a) + sb * a) / 255;
    return RGB(r, g, b);
}
static inline void put(int x, int y, u32 c) {
    x += ox; y += oy;
    if ((unsigned)x < SW && (unsigned)y < SH) canvas[y * STRIDE + x] = c;
}
static inline void putA(int x, int y, u32 c, int a) {
    x += ox; y += oy;
    if ((unsigned)x >= SW || (unsigned)y >= SH) return;
    u32 *p = &canvas[y * STRIDE + x];
    *p = mixc(*p, c, a);
}
static void rect(int x, int y, int w, int h, u32 c) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) put(x + i, y + j, c);
}
static void rectA(int x, int y, int w, int h, u32 c, int a) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) putA(x + i, y + j, c, a);
}
static void frame(int x, int y, int w, int h, int t, u32 c) {
    rect(x, y, w, t, c); rect(x, y + h - t, w, t, c);
    rect(x, y, t, h, c); rect(x + w - t, y, t, h, c);
}
static void circle(int cx, int cy, int r, u32 c) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        for (int x = -dx; x <= dx; x++) put(cx + x, cy + dy, c);
    }
}
static void circleA(int cx, int cy, int r, u32 c, int a) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        for (int x = -dx; x <= dx; x++) putA(cx + x, cy + dy, c, a);
    }
}

/* ---- fuente 5x7 (mayusculas, digitos y algo de puntuacion) ---- */
static const char font_order[] = " !',-./0123456789:>?ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const unsigned char font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x40,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03}, {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},
    {0x00,0x36,0x36,0x00,0x00}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}
};

static void draw_char(int x, int y, int s, u32 c, char ch) {
    const char *p = strchr(font_order, toupper((unsigned char)ch));
    int idx = (p && ch) ? (int)(p - font_order) : 0;
    for (int col = 0; col < 5; col++) {
        unsigned char bits = font5x7[idx][col];
        for (int row = 0; row < 7; row++)
            if (bits & (1 << row)) rect(x + col * s, y + row * s, s, s, c);
    }
}
static int text_w(int s, const char *str) { return (int)strlen(str) * 6 * s; }
static void text(int x, int y, int s, u32 c, const char *str) {
    for (int pass = 0; pass < 2; pass++) {
        int cx = x + (pass == 0 ? s : 0), cy = y + (pass == 0 ? s : 0);
        u32 col = (pass == 0) ? RGB(20, 10, 40) : c;
        for (const char *p = str; *p; p++) { draw_char(cx, cy, s, col, *p); cx += 6 * s; }
    }
}
static void textc(int cx, int y, int s, u32 c, const char *str) {
    while (s > 1 && text_w(s, str) > SW - 8) s--;
    text(cx - text_w(s, str) / 2, y, s, c, str);
}

/* ---- corazon ---- */
static const char *heart_spr[8] = {
    ".XXX.XXX.", "XXXXXXXXX", "XXXXXXXXX", "XXXXXXXXX",
    ".XXXXXXX.", "..XXXXX..", "...XXX...", "....X...." };
static void heart(int x, int y, u32 c, int a) {
    for (int r = 0; r < 8; r++) for (int q = 0; q < 9; q++)
        if (heart_spr[r][q] == 'X') { if (a >= 255) put(x + q, y + r, c); else putA(x + q, y + r, c, a); }
}

/* ---- fondo + estrellas ---- */
typedef struct { float x, y, v; } Star;
static Star stars[NSTAR];

static void draw_bg(int flash) {
    for (int y = 0; y < SH; y++) {
        float t = y / (float)SH;
        int r = 8 + (int)(26 * t) + flash;
        int g = 6 + (int)(10 * t);
        int b = 24 + (int)(44 * t);
        if (r > 255) r = 255;
        u32 c = RGB(r, g, b);
        u32 *p = &canvas[y * STRIDE];
        for (int x = 0; x < SW; x++) p[x] = c;
    }
}
static void draw_stars(void) {
    for (int i = 0; i < NSTAR; i++) {
        int br = 90 + (int)(stars[i].v * 50.0f);
        if (br > 255) br = 255;
        int sz = stars[i].v > 1.1f ? 2 : 1;
        rectA((int)stars[i].x, (int)stars[i].y, sz, sz, RGB(200, 210, 255), br);
    }
}

/* ---- particulas ---- */
typedef struct { float x, y, vx, vy; int life, max, kind; u32 col; } Part;
static Part pt[MAXP];
static void spawn_part(float x, float y, float vx, float vy, int life, int kind, u32 col) {
    for (int i = 0; i < MAXP; i++) if (pt[i].life <= 0) {
        pt[i] = (Part){x, y, vx, vy, life, life, kind, col};
        return;
    }
}
static void update_parts(void) {
    for (int i = 0; i < MAXP; i++) if (pt[i].life > 0) {
        pt[i].x += pt[i].vx; pt[i].y += pt[i].vy;
        if (pt[i].kind == 0) pt[i].vy += 0.06f;
        pt[i].life--;
    }
}
static void draw_parts(void) {
    for (int i = 0; i < MAXP; i++) if (pt[i].life > 0) {
        int a = 255 * pt[i].life / pt[i].max;
        if (pt[i].kind == 0) rectA((int)pt[i].x, (int)pt[i].y, 3, 3, pt[i].col, a);
        else heart((int)pt[i].x, (int)pt[i].y, pt[i].col, a);
    }
}
static void burst(float x, float y, int n, u32 col) {
    for (int i = 0; i < n; i++) {
        float a = (rand() % 628) / 100.0f, sp = 0.8f + (rand() % 200) / 100.0f;
        spawn_part(x, y, cosf(a) * sp, sinf(a) * sp - 0.8f, 30 + rand() % 25, 0, col);
    }
}

/* ---- enemigo original: "La Sombra Pixelada" (dibujado con formas) ---- */
static void draw_enemy(int cx, int cy, int t, int mood, float tx, float ty) {
    int bob = (int)(sinf(t * 0.05f) * 4.0f);
    int y0 = cy + bob;
    u32 outline = RGB(60, 30, 110), body = RGB(118, 78, 190), light = RGB(190, 150, 255);

    circleA(cx, y0 + 6, 46, RGB(120, 80, 220), 28);
    circleA(cx, y0 + 6, 36, RGB(140, 100, 240), 28);

    // cuerpo con borde ondulado
    for (int x = -25; x <= 24; x++) {
        int bottom = y0 + 28 + (int)(sinf(x * 0.45f + t * 0.15f) * 3.0f);
        rect(cx + x, y0, 1, bottom - y0 + 2, outline);
    }
    for (int x = -23; x <= 22; x++) {
        int bottom = y0 + 28 + (int)(sinf(x * 0.45f + t * 0.15f) * 3.0f);
        rect(cx + x, y0, 1, bottom - y0, body);
    }
    circle(cx, y0, 26, outline);
    circle(cx, y0, 24, body);
    circleA(cx - 9, y0 - 11, 8, light, 110);

    // antena con orbe pulsante
    rect(cx + 2, y0 - 38, 2, 14, outline);
    int pulse = 3 + (int)(sinf(t * 0.12f) * 1.5f + 1.5f);
    circleA(cx + 3, y0 - 40, pulse + 4, RGB(255, 230, 120), 70);
    circle(cx + 3, y0 - 40, 4, RGB(255, 240, 160));

    // ojos que siguen al jugador
    float ddx = (tx - cx) / 60.0f, ddy = (ty - y0) / 80.0f;
    if (ddx > 1) ddx = 1; if (ddx < -1) ddx = -1;
    if (ddy > 1) ddy = 1; if (ddy < -1) ddy = -1;
    int blink = (t % 200) < 6;
    for (int e = -1; e <= 1; e += 2) {
        int ex = cx + e * 11, ey = y0 - 2;
        if (blink || mood == 1) {
            rect(ex - 6, ey + (mood == 1 ? -1 : 1), 13, 3, RGB(255, 255, 255));
        } else {
            circle(ex, ey, 7, RGB(255, 255, 255));
            circle(ex + (int)(ddx * 3), ey + (int)(ddy * 3), 3, RGB(30, 20, 60));
        }
    }
    // boca
    if (mood == 1) {
        for (int x = -8; x <= 8; x++) rect(cx + x, y0 + 15 - (x * x) / 12, 2, 2, RGB(255, 255, 255));
    } else {
        rect(cx - 6, y0 + 14, 12, 2, RGB(255, 255, 255));
    }
}

/* ===================================================================== */
/*  JUEGO                                                                */
/* ===================================================================== */
typedef struct { float x, y, vx, vy; int on; } Bullet;
static Bullet bl[MAXB];

static void spawn(float x, float y, float vx, float vy) {
    for (int i = 0; i < MAXB; i++) if (!bl[i].on) { bl[i] = (Bullet){x, y, vx, vy, 1}; return; }
}

enum { ST_INTRO, ST_DODGE, ST_MENU, ST_WIN, ST_LOSE };

int main(void) {
    setup_callbacks();
    sceDisplaySetMode(0, SW, SH);
    vram[0] = (u32 *)(0x44000000);
    vram[1] = (u32 *)(0x44000000 + STRIDE * SH * 4);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    srand((unsigned)sceKernelLibcTime(NULL));

    int at = sceKernelCreateThread("audio", audio_thread, 0x12, 0x10000, THREAD_ATTR_USER, NULL);
    if (at >= 0) sceKernelStartThread(at, 0, NULL);

    for (int i = 0; i < NSTAR; i++) {
        stars[i].x = (float)(rand() % SW); stars[i].y = (float)(rand() % SH);
        stars[i].v = 0.3f + (rand() % 100) / 100.0f;
    }

    const int bx = 140, by = 92, bw = 200, bh = 104;
    float px = bx + bw / 2 - 4, py = by + bh / 2 - 4;
    int hp = 20, state = ST_INTRO, timer = 0, turn = 0, talks = 0, sel = 0, tick = 0;
    int invuln = 0, flash = 0;
    float shake = 0;
    u32 prev = 0;
    int cur = 0;

    const char *lines[] = {
        "LA SOMBRA TE MIRA CON CURIOSIDAD.",
        "LA SOMBRA SE RIE, PERO NO ATACA FUERTE.",
        "LA SOMBRA PARECE CANSADA DE PELEAR."
    };
    const char *msg = lines[0];

    while (!g_quit) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);
        u32 pressed = pad.Buttons & ~prev;
        prev = pad.Buttons;
        tick++;

        /* ---------------- logica ---------------- */
        if (state == ST_INTRO) {
            if (pressed & PSP_CTRL_CROSS) {
                sfx_play(SFX_CONFIRM);
                state = ST_DODGE; timer = 0; memset(bl, 0, sizeof bl);
            }
        } else if (state == ST_DODGE) {
            float sp = (pad.Buttons & PSP_CTRL_CIRCLE) ? 0.9f : 2.0f;
            float dx = 0, dy = 0;
            
