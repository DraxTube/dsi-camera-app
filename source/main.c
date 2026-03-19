#include <nds.h>
#include <stdio.h>
#include <stdlib.h>

// Use Epicpkmn11/dsi-camera headers
#include "camera.h"

#define MAX_FRAMES 100
#define FRAME_SIZE (256 * 192 * 2)

u16* video_frames[MAX_FRAMES];
int total_recorded_frames = 0;
int current_playback_frame = 0;

enum State {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_PLAYING
};

enum State current_state = STATE_IDLE;

int main(void) {
    consoleDemoInit();

    if (!isDSiMode()) {
        iprintf("This app requires a Nintendo DSi!\n");
        iprintf("Please launch via TWiLight Menu++\n");
        iprintf("with DSi mode enabled.\n");
        while(1) swiWaitForVBlank();
    }

    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    int bg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 1, 0);
    u16* bg_ptr = bgGetGfxPtr(bg);

    iprintf("Nintendo DSi Video Recorder\n\n");
    
    iprintf("Allocating RAM...\n");
    for (int i = 0; i < MAX_FRAMES; i++) {
        video_frames[i] = (u16*)malloc(FRAME_SIZE);
        if (!video_frames[i]) {
            iprintf("Error: Out of memory at frame %d!\n", i);
            iprintf("Try reducing MAX_FRAMES.\n");
            while(1) swiWaitForVBlank();
        }
    }
    
    iprintf("Initializing camera...\n");
    pxiWaitRemote(PXI_CAMERA); // Wait for ARM7 to initialize PXI
    cameraInit();
    cameraActivate(CAM_OUTER);
    iprintf("Camera ready.\n\n");
    
    iprintf("Controls:\n");
    iprintf("Hold A: Record Video\n");
    iprintf("Press X: Playback Video\n");
    iprintf("Press B: Stop / Idle\n");

    // Previews camera into VRAM background buffer
    cameraTransferStart(bg_ptr, CAPTURE_MODE_PREVIEW);

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        int keys = keysHeld();
        int keys_down = keysDown();

        if (current_state == STATE_IDLE || current_state == STATE_RECORDING) {
            if (keys & KEY_A) {
                if (current_state != STATE_RECORDING) {
                    current_state = STATE_RECORDING;
                    total_recorded_frames = 0;
                    iprintf("\x1b[11;0HRecording...         \n");
                }
                
                if (total_recorded_frames < MAX_FRAMES) {
                    // Make sure a frame is ready
                    if (!cameraTransferActive()) {
                        cameraTransferStart(bg_ptr, CAPTURE_MODE_CAPTURE);
                    }
                    while(cameraTransferActive()) swiWaitForVBlank();
                    
                    // Copy to RAM
                    dmaCopy(bg_ptr, video_frames[total_recorded_frames], FRAME_SIZE);
                    total_recorded_frames++;
                } else {
                    iprintf("\x1b[11;0HMemory Full!         \n");
                }
            } else {
                if (current_state == STATE_RECORDING) {
                    current_state = STATE_IDLE;
                    cameraTransferStart(bg_ptr, CAPTURE_MODE_PREVIEW);
                    iprintf("\x1b[11;0HIdle. (%d frames)      \n", total_recorded_frames);
                }
            }
        }

        if (keys_down & KEY_X) {
            if (total_recorded_frames > 0) {
                current_state = STATE_PLAYING;
                current_playback_frame = 0;
                while(cameraTransferActive()) swiWaitForVBlank();
                cameraTransferStop();
                iprintf("\x1b[11;0HPlaying...           \n");
            } else {
                iprintf("\x1b[11;0HNo video to play.    \n");
            }
        }

        if (keys_down & KEY_B) {
            if (current_state == STATE_PLAYING) {
                current_state = STATE_IDLE;
                cameraTransferStart(bg_ptr, CAPTURE_MODE_PREVIEW);
                iprintf("\x1b[11;0HIdle. (%d frames)      \n", total_recorded_frames);
            }
        }

        if (current_state == STATE_PLAYING) {
            dmaCopy(video_frames[current_playback_frame], bg_ptr, FRAME_SIZE);
            current_playback_frame++;
            if (current_playback_frame >= total_recorded_frames) {
                current_playback_frame = 0; 
            }
        }
    }
    
    cameraDeactivate(CAM_OUTER);
    return 0;
}
