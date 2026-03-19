#include <nds.h>
#include <nds/camera.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ─────────────────────────────────────────
//  Configurazione
// ─────────────────────────────────────────
#define FRAME_W        256
#define FRAME_H        192
#define FRAME_PIXELS   (FRAME_W * FRAME_H)
#define MAX_FRAMES     60          // ~6 sec a 10 fps (limitato da RAM)
#define TARGET_FPS     10

// ─────────────────────────────────────────
//  Buffer video in RAM
// ─────────────────────────────────────────
static u16 frameBuffer[MAX_FRAMES][FRAME_PIXELS];
static int  frameCount = 0;

// ─────────────────────────────────────────
//  Stato dell'app
// ─────────────────────────────────────────
typedef enum {
    STATE_MENU,
    STATE_PREVIEW,
    STATE_RECORDING,
    STATE_PLAYBACK
} AppState;

static AppState state = STATE_MENU;

// ─────────────────────────────────────────
//  Utility: disegna testo su schermo
// ─────────────────────────────────────────
static void printStatus(const char *line1, const char *line2, const char *line3) {
    consoleClear();
    iprintf("\n\n");
    iprintf("  %-28s\n", line1 ? line1 : "");
    iprintf("\n");
    iprintf("  %-28s\n", line2 ? line2 : "");
    iprintf("\n");
    iprintf("  %-28s\n", line3 ? line3 : "");
}

// ─────────────────────────────────────────
//  Copia frame dalla cam al buffer
// ─────────────────────────────────────────
static void captureFrame(int index) {
    // La fotocamera DSi scrive direttamente in VRAM tramite DMA.
    // Copiamo il contenuto corrente dello schermo superiore (mappato
    // sulla VRAM-A dove la cam scrive in modalità display capture).
    u16 *vram = (u16 *)0x06000000;   // VRAM-A base
    dmaCopy(vram, frameBuffer[index], FRAME_PIXELS * 2);
}

// ─────────────────────────────────────────
//  Mostra un frame sul display principale
// ─────────────────────────────────────────
static void showFrame(int index) {
    u16 *vram = (u16 *)0x06000000;
    dmaCopy(frameBuffer[index], vram, FRAME_PIXELS * 2);
}

// ─────────────────────────────────────────
//  Setup display principale (schermo su)
// ─────────────────────────────────────────
static void setupMainDisplay(void) {
    videoSetMode(MODE_FB0);
    vramSetBankA(VRAM_A_LCD);
}

// ─────────────────────────────────────────
//  Setup console sul sub display (schermo giu)
// ─────────────────────────────────────────
static void setupSubConsole(void) {
    videoSetModeSub(MODE_0_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
}

// ─────────────────────────────────────────
//  Inizializza la fotocamera DSi
// ─────────────────────────────────────────
static bool initCamera(void) {
    if (!cameraIsAvailable()) return false;
    cameraInit();
    cameraStartTransfer(
        (u16 *)0x06000000,   // scrive diretto in VRAM-A
        false,               // non doppio buffer
        MCUREG_CAM_FORMAT_YUV
    );
    return true;
}

// ─────────────────────────────────────────
//  MENU
// ─────────────────────────────────────────
static void menuTick(void) {
    printStatus(
        "=== DSi Video Cam ===",
        "A  -> Anteprima cam",
        frameCount > 0 ? "B  -> Riproduci video" : "(nessun video)"
    );
    iprintf("\n  START -> Esci\n");

    scanKeys();
    u32 keys = keysDown();

    if (keys & KEY_A) {
        state = STATE_PREVIEW;
        setupMainDisplay();
        initCamera();
    }
    if (keys & KEY_B && frameCount > 0) {
        state = STATE_PLAYBACK;
        setupMainDisplay();
    }
}

// ─────────────────────────────────────────
//  PREVIEW (anteprima live + avvio rec)
// ─────────────────────────────────────────
static void previewTick(void) {
    printStatus(
        "=== Anteprima ===",
        "A  -> REGISTRA",
        "B  -> Torna al menu"
    );

    scanKeys();
    u32 keys = keysDown();

    if (keys & KEY_A) {
        frameCount = 0;
        state = STATE_RECORDING;
    }
    if (keys & KEY_B) {
        cameraStopTransfer();
        state = STATE_MENU;
        setupSubConsole();
    }
    // La cam continua a scrivere in VRAM in automatico -> live preview gratis
    swiWaitForVBlank();
}

// ─────────────────────────────────────────
//  RECORDING
// ─────────────────────────────────────────
static int recFrameTimer = 0;
static const int TICKS_PER_FRAME = 60 / TARGET_FPS;  // 6 tick = 1 frame a 10fps

static void recordingTick(void) {
    // Mostra info sul sub screen
    iprintf("\x1b[0;0H");   // home cursor senza clear (evita flickering)
    iprintf("  Registrazione...\n");
    iprintf("  Frame: %d / %d\n", frameCount, MAX_FRAMES);
    iprintf("  A -> STOP\n");

    recFrameTimer++;
    if (recFrameTimer >= TICKS_PER_FRAME && frameCount < MAX_FRAMES) {
        captureFrame(frameCount);
        frameCount++;
        recFrameTimer = 0;
    }

    scanKeys();
    u32 keys = keysDown();

    // Stop se premi A oppure buffer pieno
    if ((keys & KEY_A) || frameCount >= MAX_FRAMES) {
        cameraStopTransfer();
        // Vai subito in playback
        state = STATE_PLAYBACK;
        setupMainDisplay();
        printStatus("Riproduzione...", "", "B -> Menu");
    }

    swiWaitForVBlank();
}

// ─────────────────────────────────────────
//  PLAYBACK
// ─────────────────────────────────────────
static int playIndex  = 0;
static int playTimer  = 0;
static bool playLoop  = true;

static void playbackTick(void) {
    if (frameCount == 0) {
        state = STATE_MENU;
        return;
    }

    playTimer++;
    if (playTimer >= TICKS_PER_FRAME) {
        showFrame(playIndex);
        playIndex++;
        playTimer = 0;
        if (playIndex >= frameCount) {
            if (playLoop) {
                playIndex = 0;          // loop
            } else {
                playIndex = frameCount - 1;
            }
        }
    }

    // Aggiorna sub screen con info (cursor home per non flickerare)
    iprintf("\x1b[0;0H");
    iprintf("  Riproduzione  %d/%d \n", playIndex + 1, frameCount);
    iprintf("  L/R -> loop on/off\n");
    iprintf("  B   -> menu      \n");

    scanKeys();
    u32 keys = keysDown();

    if (keys & KEY_B) {
        playIndex = 0;
        playTimer = 0;
        state = STATE_MENU;
        setupSubConsole();
        setupMainDisplay();   // torna a FB0 vuoto (schermo nero)
    }
    if (keys & KEY_L) playLoop = true;
    if (keys & KEY_R) playLoop = false;

    swiWaitForVBlank();
}

// ─────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────
int main(void) {
    // Init di base
    defaultExceptionHandler();

    // Sub screen: console testo
    setupSubConsole();

    // Main screen: framebuffer (usato per cam + playback)
    setupMainDisplay();

    // Schermo principale in alto (cam output)
    lcdMainOnTop();

    // Splash iniziale
    consoleClear();
    iprintf("\n\n");
    iprintf("   ____  ____  _ ____\n");
    iprintf("  |  _ \\/ ___|| |_ _|\n");
    iprintf("  | | | \\___ \\| || |\n");
    iprintf("  | |_| |___) | || |\n");
    iprintf("  |____/|____/|_|___|\n");
    iprintf("\n");
    iprintf("  Video Cam per DSi\n");
    iprintf("  Premi START...\n");

    while (!(keysDown() & KEY_START)) {
        scanKeys();
        swiWaitForVBlank();
    }

    consoleClear();

    // ── Loop principale ──────────────────
    while (pmMainLoop()) {
        switch (state) {
            case STATE_MENU:      menuTick();      break;
            case STATE_PREVIEW:   previewTick();   break;
            case STATE_RECORDING: recordingTick(); break;
            case STATE_PLAYBACK:  playbackTick();  break;
        }
    }

    return 0;
}
