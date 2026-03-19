#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Use Epicpkmn11/dsi-camera headers
#include "camera.h"

// --- Configurazione ---
// MAX_FRAMES ridotto: 100 frame * 256*192*2 byte = ~9.4 MB, troppo per la RAM del DSi.
// Con 30 frame si usano ~2.8 MB, entro i limiti sicuri dell'heap ARM9.
#define MAX_FRAMES  30
#define FRAME_SIZE  (256 * 192 * 2)

// Buffer allocati staticamente in BSS per evitare frammentazione heap
// e crash da malloc fallito silenziosamente.
// 30 * 98304 = 2.949.120 byte (~2.8 MB)
static u16 frame_storage[MAX_FRAMES][256 * 192];

int total_recorded_frames = 0;
int current_playback_frame = 0;

typedef enum {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_PLAYING
} AppState;

AppState current_state = STATE_IDLE;

// Stampa la riga di stato senza sporcare lo schermo
static void print_status(const char* msg) {
    // Posiziona il cursore alla riga 11, colonna 0, poi stampa 30 spazi per pulire
    iprintf("\x1b[11;0H%-30s\n", msg);
}

int main(void) {
    consoleDemoInit();

    if (!isDSiMode()) {
        iprintf("This app requires a Nintendo DSi!\n");
        iprintf("Please launch via TWiLight Menu++\n");
        iprintf("with DSi mode enabled.\n");
        while (1) swiWaitForVBlank();
    }

    // Imposta il modo video 2D con bitmap a 16 bit
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    int bg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 1, 0);
    u16* bg_ptr = bgGetGfxPtr(bg);

    iprintf("Nintendo DSi Video Recorder\n\n");
    iprintf("Initializing camera...\n");

    // FIX 1: pxiWaitRemote non esiste nella libnds pubblica.
    // La sincronizzazione ARM7/ARM9 avviene tramite swiWaitForVBlank()
    // nelle prime iterazioni; usiamo un breve spin-wait invece.
    // (Se la libreria usata espone pxiWaitRemote, questa riga è sufficiente
    //  a sostituire quella errata; in caso contrario rimuovila.)
    // pxiWaitRemote(PXI_CAMERA); // <-- rimosso: simbolo non standard

    // Piccolo ritardo per lasciare tempo all'ARM7 di inizializzare il PXI
    for (int i = 0; i < 10; i++) swiWaitForVBlank();

    cameraInit();
    cameraActivate(CAM_OUTER);
    iprintf("Camera ready.\n\n");

    iprintf("Controls:\n");
    iprintf("Hold A : Record Video\n");
    iprintf("Press X: Playback\n");
    iprintf("Press B: Stop / Idle\n\n");
    iprintf("Max frames: %d\n", MAX_FRAMES);

    print_status("Idle.");

    // Avvia preview live dalla camera
    cameraTransferStart(bg_ptr, CAPTURE_MODE_PREVIEW);

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        u32 keys      = keysHeld();
        u32 keys_down = keysDown();

        // --- REGISTRAZIONE ---
        if (keys & KEY_A) {
            if (current_state != STATE_RECORDING) {
                // FIX 2: quando si passa a STATE_RECORDING bisogna
                // fermare eventualmente la preview e resettare il contatore.
                current_state = STATE_RECORDING;
                total_recorded_frames = 0;

                // Ferma trasferimenti in corso prima di cambiare modalità
                while (cameraTransferActive()) swiWaitForVBlank();
                cameraTransferStop();

                print_status("Recording...");
            }

            if (total_recorded_frames < MAX_FRAMES) {
                // FIX 3: CAPTURE_MODE_CAPTURE avvia un singolo scatto;
                // bisogna avviarlo ogni volta e poi attenderne il completamento.
                cameraTransferStart(bg_ptr, CAPTURE_MODE_CAPTURE);
                while (cameraTransferActive()) swiWaitForVBlank();

                // Copia il frame catturato nel buffer statico
                memcpy(frame_storage[total_recorded_frames], bg_ptr, FRAME_SIZE);
                total_recorded_frames++;

                // FIX 4: aggiornamento contatore a video durante registrazione
                iprintf("\x1b[11;0HRecording... %3d/%d  \n",
                        total_recorded_frames, MAX_FRAMES);
            } else {
                print_status("Memory Full!");
            }
        }
        else if (current_state == STATE_RECORDING) {
            // A rilasciato → torna in idle e riprende la preview
            current_state = STATE_IDLE;

            // FIX 5: assicurarsi che non ci siano trasferimenti pendenti
            while (cameraTransferActive()) swiWaitForVBlank();

            cameraTransferStart(bg_ptr, CAPTURE_MODE_PREVIEW);

            char buf[32];
            snprintf(buf, sizeof(buf), "Idle. (%d frames)", total_recorded_frames);
            print_status(buf);
        }

        // --- PLAYBACK ---
        if (keys_down & KEY_X) {
            if (total_recorded_frames > 0) {
                current_state = STATE_PLAYING;
                current_playback_frame = 0;

                // FIX 6: fermare la preview/capture prima del playback
                while (cameraTransferActive()) swiWaitForVBlank();
                cameraTransferStop();

                print_status("Playing...");
            } else {
                print_status("No video to play.");
            }
        }

        // --- STOP ---
        if (keys_down & KEY_B) {
            if (current_state == STATE_PLAYING || current_state == STATE_RECORDING) {
                // FIX 7: gestire lo stop anche durante la registrazione
                current_state = STATE_IDLE;

                while (cameraTransferActive()) swiWaitForVBlank();

                cameraTransferStart(bg_ptr, CAPTURE_MODE_PREVIEW);

                char buf[32];
                snprintf(buf, sizeof(buf), "Idle. (%d frames)", total_recorded_frames);
                print_status(buf);
            }
        }

        // --- RENDERING PLAYBACK ---
        if (current_state == STATE_PLAYING) {
            // FIX 8: bounds-check esplicito prima di accedere all'array
            if (current_playback_frame < 0 ||
                current_playback_frame >= total_recorded_frames) {
                current_playback_frame = 0;
            }

            memcpy(bg_ptr, frame_storage[current_playback_frame], FRAME_SIZE);

            current_playback_frame++;
            if (current_playback_frame >= total_recorded_frames) {
                current_playback_frame = 0; // loop
            }
        }
    }

    // Pulizia (mai raggiunta nel loop infinito, ma buona pratica)
    cameraDeactivate(CAM_OUTER);
    return 0;
}
