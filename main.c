#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdio.h>
#include <malloc.h>

PSP_MODULE_INFO("Sonic2D_Gu", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

// Buffer de comandos para el motor gráfico de la PSP
static unsigned int __attribute__((aligned(16))) list[256];

int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", (void*)exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int SetupCallbacks(void) {
    int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);
    return thid;
}

// Estructura para vértices de gráficos 2D con color
typedef struct {
    unsigned int color;
    short x, y, z;
} Vertex;

void drawRect(int x, int y, int w, int h, unsigned int color) {
    Vertex* vertices = (Vertex*) sceGuGetMemory(2 * sizeof(Vertex));
    vertices[0].color = color;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0;

    vertices[1].color = color;
    vertices[1].x = x + w;
    vertices[1].y = y + h;
    vertices[1].z = 0;

    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, vertices);
}

int main(void) {
    SetupCallbacks();

    // Inicializar el sistema gráfico GU
    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, 512);
    sceGuDispBuffer(480, 272, (void*)0x88000, 512);
    sceGuDepthBuffer((void*)0x110000, 512);
    sceGuOffset(260 / 2, 272 / 2);
    sceGuViewport(480 / 2, 272 / 2, 480 / 2, 272 / 2);
    sceGuDepthRange(0xcf7f, 0x0f7f);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuDisplay(1);

    int x = 50;
    int y = 160;
    int velocidad = 3;

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    while(1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_RIGHT) {
            x += velocidad;
            if (x > 420) x = 420;
        }
        if (pad.Buttons & PSP_CTRL_LEFT) {
            x -= velocidad;
            if (x < 10) x = 10;
        }

        if (pad.Buttons & PSP_CTRL_CROSS) {
            y = 120; // Salto
        } else {
            y = 160; // Piso
        }

        // Renderizado gráfico de la escena
        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF201010); // Color de fondo (Cielo oscuro / nocturno)
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        // Dibujar el suelo (Rectángulo verde)
        drawRect(0, 200, 480, 72, 0xFF00AA00);

        // Dibujar al personaje estilo Sonic (Rectángulo azul brillante)
        drawRect(x, y, 32, 40, 0xFFFF0000); // Color en formato AABBGGRR (Azul brillante)

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    return 0;
}
