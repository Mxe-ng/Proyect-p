#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("PSP_YouTube_Player", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static unsigned int __attribute__((aligned(16))) list[262144];

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

typedef struct {
    char title[64];
    char channel[32];
    char duration[16];
} YTVideo;

YTVideo playlist[4] = {
    {"Naruto Shippuden - Opening 3 Silhouette", "Sony Music Anime", "3:25"},
    {"Cómo programar en C para PSP", "DevPSP Official", "15:40"},
    {"Lo-Fi Hip Hop Radio - Beats to Relax", "Lofi Girl", "LIVE"},
    {"Gameplay Test Homebrew PSP 2026", "Mxe-ng Dev", "8:12"}
};

int main(void) {
    SetupCallbacks();

    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, 512);
    sceGuDispBuffer(480, 272, (void*)0x88000, 512);
    sceGuDepthBuffer((void*)0x110000, 512);
    sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
    sceGuViewport(2048, 2048, 480, 272);
    sceGuDepthRange(0xcf7f, 0x0f7f);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuScissor(0, 0, 480, 272);
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuDisplay(1);

    int app_state = 0; // 0: Menú principal de videos, 1: Reproductor activo
    int selected_index = 0;
    
    int is_playing = 1;
    int progress = 80;

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    while(1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (app_state == 0) {
            // Navegación en el menú de videos
            if (pad.Buttons & PSP_CTRL_DOWN) {
                selected_index = (selected_index + 1) % 4;
                sceKernelDelayThread(150000);
            }
            if (pad.Buttons & PSP_CTRL_UP) {
                selected_index = (selected_index - 1 + 4) % 4;
                sceKernelDelayThread(150000);
            }
            // Entrar al reproductor con la X
            if (pad.Buttons & PSP_CTRL_CROSS) {
                app_state = 1;
                is_playing = 1;
                progress = 100;
                sceKernelDelayThread(250000);
            }
        }
        else if (app_state == 1) {
            // Controles dentro del reproductor
            if (pad.Buttons & PSP_CTRL_CROSS) {
                is_playing = 1 - is_playing; // Pausar / Reanudar
                sceKernelDelayThread(180000);
            }
            if (pad.Buttons & PSP_CTRL_RIGHT && progress < 420) {
                progress += 6; // Adelantar
            }
            if (pad.Buttons & PSP_CTRL_LEFT && progress > 40) {
                progress -= 6; // Retroceder
            }
            // Salir al menú con el Círculo (O)
            if (pad.Buttons & PSP_CTRL_CIRCLE) {
                app_state = 0;
                sceKernelDelayThread(250000);
            }
        }

        // Renderizado gráfico
        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF121212); // Fondo oscuro estilo YouTube
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        if (app_state == 0) {
            // Cabecera superior
            drawRect(0, 0, 480, 35, 0xFF212121);
            drawRect(15, 8, 30, 18, 0xFF0000FF); // Icono rojo YouTube
            drawRect(60, 8, 320, 18, 0xFF383838); // Barra de búsqueda

            // Lista de videos disponibles
            for (int i = 0; i < 4; i++) {
                int y_pos = 45 + (i * 54);
                unsigned int bg_color = (i == selected_index) ? 0xFF2B2B2B : 0xFF181818;
                
                drawRect(10, y_pos, 460, 48, bg_color);
                drawRect(15, y_pos + 4, 64, 40, 0xFF333333); // Miniatura simulada
                
                // Línea roja indicadora de selección
                if (i == selected_index) {
                    drawRect(10, y_pos, 4, 48, 0xFF0000FF);
                }
            }
        }
        else if (app_state == 1) {
            // --- PANTALLA DE REPRODUCCIÓN ACTIVA ---
            // Área de video simulada
            drawRect(0, 0, 480, 185, 0xFF000000);

            // Animación central de reproducción (Símbolo de play / pausa)
            drawRect(190, 65, 100, 50, 0xFF1F1F1F);
            if (is_playing) {
                drawRect(232, 80, 16, 20, 0xFF00FF00); // Play verde
            } else {
                drawRect(228, 80, 6, 20, 0xFF00FFFF); // Pausa cian
                drawRect(244, 80, 6, 20, 0xFF00FFFF);
            }

            // Barra de controles inferior
            drawRect(0, 185, 480, 87, 0xFF212121);

            // Barra de progreso interactiva
            drawRect(30, 205, 420, 6, 0xFF555555); // Fondo de barra
            drawRect(30, 205, progress, 6, 0xFF0000FF); // Barra de progreso roja
            drawRect(30 + progress - 3, 202, 6, 12, 0xFFFFFFFF); // Cabezal blanco

            // Información del video reproduciéndose
            drawRect(30, 230, 280, 12, 0xFFFFFFFF); // Simulación de título
            drawRect(30, 248, 140, 10, 0xFFAAAAAA); // Simulación de canal
        }

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    return 0;
}
