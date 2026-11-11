#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stdlib.h>

PSP_MODULE_INFO("Sonic2D_PSP", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

// Función para salir limpiamente del juego en la PSP
int exit_callback(int arg1, int arg2, int common) {
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
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

int main(void) {
    pspDebugScreenInit();
    SetupCallbacks();

    // Posición inicial de nuestro personaje tipo Sonic
    int x = 20;
    int y = 200;
    int velocidad = 2;

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    while(1) {
        sceCtrlReadBufferPositive(&pad, 1);

        // Movimiento horizontal (simulando velocidad y físicas rápidas)
        if (pad.Buttons & PSP_CTRL_RIGHT) {
            x += velocidad;
            if (x > 450) x = 450; // Límite de pantalla
        }
        if (pad.Buttons & PSP_CTRL_LEFT) {
            x -= velocidad;
            if (x < 10) x = 10;
        }

        // Si se presiona la X, salta (simulación visual en pantalla)
        int saltando = 0;
        if (pad.Buttons & PSP_CTRL_CROSS) {
            y = 150; // Altura del salto
            saltando = 1;
        } else {
            y = 200; // Piso normal
        }

        // Limpiar pantalla y dibujar la escena en tiempo real
        pspDebugScreenSetXY(0, 0);
        pspDebugScreenPrintf("========================================\n");
        pspDebugScreenPrintf("      PROTOTIPO 2D - ESTILO SONIC       \n");
        pspDebugScreenPrintf("========================================\n\n");
        pspDebugScreenPrintf("  Controles:\n");
        pspDebugScreenPrintf("  [Cruzeta Izq / Der] -> Correr\n");
        pspDebugScreenPrintf("  [X]                 -> Saltar\n\n");
        pspDebugScreenPrintf("  Estado: %s\n", saltando ? "SALTANDO (¡Spin Dash!)" : "CORRIENDO EN ZONA VERDE");

        // Dibujar el escenario y al personaje gráficamente en el buffer de texto/pantalla
        // (Espacios en blanco para simular el movimiento del personaje en el nivel)
        int i;
        pspDebugScreenPrintf("\n\n\n\n");
        for(i = 0; i < x / 10; i++) pspDebugScreenPrintf(" ");
        pspDebugScreenPrintf("(O_O) -> Sonic\n"); // Representación visual del personaje

        pspDebugScreenPrintf("\n----------------------------------------\n");
        pspDebugScreenPrintf("Piso: ==================================\n");

        sceDisplayWaitVblankStart();
    }

    return 0;
}
