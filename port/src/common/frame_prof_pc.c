/* Where each game frame's time goes (port/include/port/frame_prof.h). */
#include "port/frame_prof.h"

#include <dolphin/os.h>
#include <stdio.h>
#include <stdlib.h>

#define PROF_REPORT_FRAMES 120 /* two seconds at 60 */

static const char* const kNames[PROF_PHASES] = {"disp", "disp_skip", "logic", "end_frame", "sleep", "present",
                                                "other"};

static int sOn = -1;
static OSTime sLast;
static OSTime sSum[PROF_PHASES];   /* this report's totals */
static OSTime sFrame[PROF_PHASES]; /* this game frame's */
static OSTime sWorst[PROF_PHASES]; /* the slowest drawn frame's phases */
static OSTime sWorstBusy;
static int sFrames, sDrawn;

static int on(void) {
    if (sOn < 0) {
        sOn = getenv("TTYD_TIMING_LOG") != NULL;
    }
    return sOn;
}

void portProfMark(PortProfPhase phase) {
    OSTime now;
    if (!on()) {
        return;
    }
    now = OSGetTime();
    if (sLast != 0 && phase >= 0 && phase < PROF_PHASES) {
        sFrame[phase] += now - sLast;
    }
    sLast = now;
}

static double ms(OSTime t) {
    return (double)OSTicksToMicroseconds(t) / 1000.0;
}

void portProfFrameEnd(int drawn) {
    int i;
    if (!on()) {
        return;
    }
    if (drawn) {
        /* the frame's own work, the waits left out */
        const OSTime busy = sFrame[PROF_DISP_DRAWN] + sFrame[PROF_MAIN] + sFrame[PROF_END_FRAME] +
                            sFrame[PROF_PRESENT] + sFrame[PROF_OTHER];
        if (busy > sWorstBusy) {
            sWorstBusy = busy;
            for (i = 0; i < PROF_PHASES; i++) {
                sWorst[i] = sFrame[i];
            }
        }
        sDrawn++;
    }
    for (i = 0; i < PROF_PHASES; i++) {
        sSum[i] += sFrame[i];
        sFrame[i] = 0;
    }
    if (++sFrames < PROF_REPORT_FRAMES) {
        return;
    }
    {
        char line[512];
        int n = snprintf(line, sizeof(line), "[PROF] %d frames, %d drawn (%.1f fps) | mean ms/frame:", sFrames,
                         sDrawn, sDrawn * 60.0 / sFrames);
        for (i = 0; i < PROF_PHASES && n < (int)sizeof(line); i++) {
            n += snprintf(line + n, sizeof(line) - n, " %s %.2f", kNames[i], ms(sSum[i]) / sFrames);
        }
        if (n < (int)sizeof(line)) {
            n += snprintf(line + n, sizeof(line) - n, " | worst drawn %.1f ms:", ms(sWorstBusy));
        }
        for (i = 0; i < PROF_PHASES && n < (int)sizeof(line); i++) {
            if (i != PROF_SLEEP && i != PROF_DISP_UNDRAWN) {
                n += snprintf(line + n, sizeof(line) - n, " %s %.1f", kNames[i], ms(sWorst[i]));
            }
        }
        printf("%s\n", line);
        fflush(stdout);
    }
    for (i = 0; i < PROF_PHASES; i++) {
        sSum[i] = 0;
        sWorst[i] = 0;
    }
    sWorstBusy = 0;
    sFrames = 0;
    sDrawn = 0;
}
