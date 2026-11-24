#ifndef PORT_FRAME_PROF_H
#define PORT_FRAME_PROF_H
/* Where each game frame's time goes ($TTYD_TIMING_LOG; port/src/common/frame_prof_pc.c).
 *
 * The main thread's time is cut at a few marks; each stretch since the last mark goes to the
 * phase that mark names. Every PROF_REPORT_FRAMES game frames one line goes to stdout (log.txt):
 * the mean milliseconds per game frame of each phase, the drawn frames' share, and the slowest
 * drawn frame's own phases -- enough to tell game logic, the draw list, Aurora's GX translation
 * and the render worker apart before optimising any of them. Without the variable each mark is
 * one predictable branch. */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PROF_DISP_DRAWN,   /* marioStDisp on a drawn frame: cameras, the draw list and its GX commands */
    PROF_DISP_UNDRAWN, /* marioStDisp on battery saver's undrawn frame: cameras, the draw list only */
    PROF_MAIN,         /* marioStMain: the game's logic, events, NPCs, battle */
    PROF_END_FRAME,    /* endFrame: the port's overlays, ImGui, aurora_end_frame (GX -> GL, the worker's hand-off) */
    PROF_SLEEP,        /* paceFrame's wait for the field */
    PROF_PRESENT,      /* putting the finished frame on screen (blit + swap) */
    PROF_OTHER,        /* the retrace's own work: threads, audio, disc, autosave... */
    PROF_PHASES
} PortProfPhase;

/* the stretch since the last mark was this phase */
void portProfMark(PortProfPhase phase);
/* a game frame is over (drawn: it was shown) */
void portProfFrameEnd(int drawn);

#ifdef __cplusplus
}
#endif
#endif
