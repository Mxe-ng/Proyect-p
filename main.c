// "Corazon Esquiva" - juego original para PSP (homebrew, PSPSDK)
// Mecanica: esquiva proyectiles dentro de una caja, luego elige HABLAR o PERDONAR.
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psprtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

PSP_MODULE_INFO("CorazonEsquiva", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define STRIDE 512
#define MAXB 96

typedef unsigned int u32;

static u32 *fb[2];
static int cur = 0;

static int exit_cb(int a, int b, void *c) { sceKernelExitGame(); return 0; }
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

#define RGB(r,g,b) (0xFF000000u | ((b)<<16) | ((g)<<8) | (r))

static void rect(int x, int y, int w, int h, u32 c) {
    u32 *buf = fb[cur];
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= SH) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= SW) continue;
            buf[j * STRIDE + i] = c;
        }
    }
}
static void frame(int x, int y, int w, int h, int t, u32 c) {
    rect(x, y, w, t, c);
    rect(x, y + h - t, w, t, c);
    rect(x, y, t, h, c);
    rect(x + w - t, y, t, h, c);
}
static void text(int col, int row, const char *s) {
    pspDebugScreenSetBase(fb[cur]);
    pspDebugScreenSetXY(col, row);
    pspDebugScreenPrintf("%s", s);
}

typedef struct { float x, y, vx, vy; int on; } Bullet;
static Bullet bl[MAXB];

static void spawn(float x, float y, float vx, float vy) {
    for (int i = 0; i < MAXB; i++) if (!bl[i].on) {
        bl[i] = (Bullet){x, y, vx, vy, 1};
        return;
    }
}

enum { ST_INTRO, ST_DODGE, ST_MENU, ST_WIN, ST_LOSE };

int main(void) {
    setup_callbacks();
    pspDebugScreenInit();
    sceDisplaySetMode(0, SW, SH);
    fb[0] = (u32 *)(0x44000000);
    fb[1] = (u32 *)(0x44000000 + STRIDE * SH * 4);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    srand(sceKernelLibcTime(NULL));

    const int bx = 140, by = 70, bw = 200, bh = 120; // caja de combate
    float px = bx + bw / 2, py = by + bh / 2;
    int hp = 20, state = ST_INTRO, timer = 0, turn = 0, talks = 0, sel = 0;
    u32 prev = 0;
    const char *lines[] = {
        "La Sombra Pixelada te mira con curiosidad.",
        "La Sombra se rie, pero no ataca fuerte.",
        "La Sombra parece cansada de pelear."
    };
    const char *msg = lines[0];

    while (1) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);
        u32 pressed = pad.Buttons & ~prev;
        prev = pad.Buttons;

        // ---------- logica ----------
        if (state == ST_INTRO) {
            if (pressed & PSP_CTRL_CROSS) { state = ST_DODGE; timer = 0; memset(bl, 0, sizeof bl); }
        } else if (state == ST_DODGE) {
            float sp = 1.8f;
            if (pad.Buttons & PSP_CTRL_LEFT)  px -= sp;
            if (pad.Buttons & PSP_CTRL_RIGHT) px += sp;
            if (pad.Buttons & PSP_CTRL_UP)    py -= sp;
            if (pad.Buttons & PSP_CTRL_DOWN)  py += sp;
            float ax = (pad.Lx - 128) / 128.0f, ay = (pad.Ly - 128) / 128.0f;
            if (fabsf(ax) > 0.25f) px += ax * sp;
            if (fabsf(ay) > 0.25f) py += ay * sp;
            if (px < bx + 4) px = bx + 4;
            if (px > bx + bw - 10) px = bx + bw - 10;
            if (py < by + 4) py = by + 4;
            if (py > by + bh - 10) py = by + bh - 10;

            timer++;
            int pat = turn % 3;
            if (pat == 0 && timer % 14 == 0)       // lluvia
                spawn(bx + rand() % bw, by, 0, 1.4f + turn * 0.2f);
            if (pat == 1 && timer % 20 == 0) {     // desde los lados
                int l = rand() & 1;
                spawn(l ? bx : bx + bw, by + rand() % bh, l ? 1.6f : -1.6f, 0);
            }
            if (pat == 2 && timer % 10 == 0) {     // espiral
                float a = timer * 0.18f;
                spawn(bx + bw / 2, by + bh / 2, cosf(a) * 1.3f, sinf(a) * 1.3f);
            }

            for (int i = 0; i < MAXB; i++) if (bl[i].on) {
                bl[i].x += bl[i].vx; bl[i].y += bl[i].vy;
                if (bl[i].x < bx - 10 || bl[i].x > bx + bw + 10 ||
                    bl[i].y < by - 10 || bl[i].y > by + bh + 200) bl[i].on = 0;
                if (bl[i].y > by + bh + 10) bl[i].on = 0;
                if (fabsf(bl[i].x - (px + 3)) < 6 && fabsf(bl[i].y - (py + 3)) < 6) {
                    bl[i].on = 0; hp -= 3;
                }
            }
            if (hp <= 0) { hp = 0; state = ST_LOSE; }
            else if (timer > 60 * 8) { state = ST_MENU; turn++; sel = 0; }
        } else if (state == ST_MENU) {
            if (pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) sel ^= 1;
            if (pressed & PSP_CTRL_CROSS) {
                if (sel == 0) {                       // HABLAR
                    talks++;
                    msg = lines[talks > 2 ? 2 : talks];
                    state = ST_DODGE; timer = 0; memset(bl, 0, sizeof bl);
                    px = bx + bw / 2; py = by + bh / 2;
                } else {                              // PERDONAR
                    if (talks >= 2) state = ST_WIN;
                    else {
                        msg = "Aun no confia en ti. Intenta hablar primero.";
                        state = ST_DODGE; timer = 0; memset(bl, 0, sizeof bl);
                    }
                }
            }
        } else if (pressed & PSP_CTRL_START) {        // reiniciar
            hp = 20; turn = 0; talks = 0; msg = lines[0]; state = ST_INTRO;
        }

        // ---------- dibujo ----------
        int back = cur ^ 1; cur = back;
        rect(0, 0, SW, SH, RGB(10, 10, 20));
        // enemigo original: una "sombra" hecha de rectangulos
        rect(205, 12, 70, 44, RGB(90, 60, 140));
        rect(220, 24, 10, 10, RGB(255, 255, 255));
        rect(250, 24, 10, 10, RGB(255, 255, 255));
        rect(225, 42, 30, 4, RGB(255, 255, 255));

        frame(bx, by, bw, bh, 3, RGB(255, 255, 255));
        for (int i = 0; i < MAXB; i++) if (bl[i].on)
            rect((int)bl[i].x - 3, (int)bl[i].y - 3, 7, 7, RGB(255, 255, 255));
        if (state == ST_DODGE)
            rect((int)px, (int)py, 7, 7, RGB(255, 40, 60));

        rect(140, 210, 100, 8, RGB(120, 0, 0));
        rect(140, 210, hp * 5, 8, RGB(255, 220, 0));

        text(2, 1, "CORAZON ESQUIVA");
        text(2, 27, msg);
        char buf[48];
        snprintf(buf, sizeof buf, "HP %d/20", hp);
        text(34, 26, buf);
        if (state == ST_INTRO) text(14, 4, "Pulsa X para empezar");
        if (state == ST_MENU) {
            text(20, 4, "Tu turno");
            text(20, 5, sel == 0 ? "> HABLAR     PERDONAR" : "  HABLAR   > PERDONAR");
        }
        if (state == ST_WIN)  text(14, 4, "Final pacifista! START = reiniciar");
        if (state == ST_LOSE) text(14, 4, "Game over. START = reiniciar");

        sceDisplayWaitVblankStart();
        sceDisplaySetFrameBuf(fb[back], STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, 0);
    }
    return 0;
}
