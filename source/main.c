#include <nds.h>
#include <stdio.h>
#include <stdlib.h>

// Nintendo DSi app that records video to RAM and plays it back.
// Since DSi has 16MB RAM, we can store a limited number of frames.
// A 256x192 16-bit frame is 98,304 bytes. 
// 100 frames = ~9.8 MB.
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
    // Initialize standard DS console on bottom screen
    consoleDemoInit();

    // Ensure we are running on a DSi in DSi mode (16MB RAM + Camera support)
    if (!isDSiMode()) {
        iprintf("This app requires a Nintendo DSi!\n");
        iprintf("Please launch via TWiLight Menu++\n");
        iprintf("with DSi mode enabled.\n");
        while(1) swiWaitForVBlank();
    }

    // Set up top screen for camera video display
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    int bg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    u16* bg_ptr = bgGetGfxPtr(bg);

    iprintf("Nintendo DSi Video Recorder\n\n");
    
    // Allocate RAM for our video frames
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
    while(!cameraInit()) {
        swiWaitForVBlank();
    }
    cameraActivate();
    iprintf("Camera ready.\n\n");
    
    iprintf("Controls:\n");
    iprintf("Hold A: Record Video\n");
    iprintf("Press X: Playback Video\n");
    iprintf("Press B: Stop / Idle\n");

    // Start streaming camera to our VRAM background buffer
    // 1 = continuous capture mode, 3 = NDMA block size
    cameraStartTransfer(bg_ptr, 1, 3); 

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        int keys = keysHeld();
        int keys_down = keysDown();

        // Idle / Recording state logic
        if (current_state == STATE_IDLE || current_state == STATE_RECORDING) {
            if (keys & KEY_A) {
                if (current_state != STATE_RECORDING) {
                    current_state = STATE_RECORDING;
                    total_recorded_frames = 0;
                    iprintf("\x1b[11;0HRecording...         \n");
                }
                
                if (total_recorded_frames < MAX_FRAMES) {
                    // Save the current frame from VRAM to regular RAM
                    dmaCopy(bg_ptr, video_frames[total_recorded_frames], FRAME_SIZE);
                    total_recorded_frames++;
                } else {
                    iprintf("\x1b[11;0HMemory Full!         \n");
                }
            } else {
                if (current_state == STATE_RECORDING) {
                    current_state = STATE_IDLE;
                    iprintf("\x1b[11;0HIdle. (%d frames)      \n", total_recorded_frames);
                }
            }
        }

        // Start playing
        if (keys_down & KEY_X) {
            if (total_recorded_frames > 0) {
                current_state = STATE_PLAYING;
                current_playback_frame = 0;
                // Stop camera transfer so it doesn't overwrite our playback
                cameraStopCapture(); 
                iprintf("\x1b[11;0HPlaying...           \n");
            } else {
                iprintf("\x1b[11;0HNo video to play.    \n");
            }
        }

        // Stop playing
        if (keys_down & KEY_B) {
            if (current_state == STATE_PLAYING) {
                current_state = STATE_IDLE;
                // Resume streaming camera to background buffer
                cameraStartTransfer(bg_ptr, 1, 3); 
                iprintf("\x1b[11;0HIdle. (%d frames)      \n", total_recorded_frames);
            }
        }

        // Playback logic
        if (current_state == STATE_PLAYING) {
            // Copy recorded frame from RAM to VRAM
            dmaCopy(video_frames[current_playback_frame], bg_ptr, FRAME_SIZE);
            current_playback_frame++;
            if (current_playback_frame >= total_recorded_frames) {
                // Loop video
                current_playback_frame = 0; 
            }
        }
    }
    
    cameraDeactivate();
    return 0;
}
