#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stdlib.h>

PSP_MODULE_INFO("Sonic2D_PSP", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

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

    int x = 20;
    int y = 200; // Variable integrada correctamente en la vista
    int velocidad = 2;

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    while(1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_RIGHT) {
            x += velocidad;
            if (x > 450) x = 450;
        }
        if (pad.Buttons & PSP_CTRL_LEFT) {
            x -= velocidad;
            if (x < 10) x = 10;
        }

        int saltando = 0;
        if (pad.Buttons & PSP_CTRL_CROSS) {
            y = 150; 
            saltando = 1;
        } else {
            y = 200; 
        }

        pspDebugScreenSetXY(0, 0);
        pspDebugScreenPrintf("========================================\n");
        pspDebugScreenPrintf("      PROTOTIPO 2D - ESTILO SONIC       \n");
        pspDebugScreenPrintf("========================================\n\n");
        pspDebugScreenPrintf("  Controles:\n");
        pspDebugScreenPrintf("  [Cruzeta Izq / Der] -> Correr\n");
        pspDebugScreenPrintf("  [X]                 -> Saltar\n\n");
        pspDebugScreenPrintf("  Estado: %s (Altura Y: %d)\n", saltando ? "SALTANDO" : "CORRIENDO", y);

        int i;
        pspDebugScreenPrintf("\n\n\n\n");
        for(i = 0; i < x / 10; i++) pspDebugScreenPrintf(" ");
        pspDebugScreenPrintf("(O_O) -> Sonic\n");

        pspDebugScreenPrintf("\n----------------------------------------\n");
        pspDebugScreenPrintf("Piso: ==================================\n");

        sceDisplayWaitVblankStart();
    }

    return 0;
}
