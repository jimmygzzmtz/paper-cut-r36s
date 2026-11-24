#ifndef PORT_SAMPLE_PROF_H
#define PORT_SAMPLE_PROF_H
/* A sampling profiler for devices without perf: $TTYD_SAMPLE_PROFILE=1 writes profile.bin and
 * profile.maps to the working folder (port/src/common/sample_prof_pc.c; Linux only). */
#ifdef __cplusplus
extern "C" {
#endif

/* main_aurora.c: at start, before the game's threads */
void portSampleProfInit(void);
/* each frame: the room being played (gp->currentMapName), samples tagged with it */
void portSampleProfRoom(const char* room);

#ifdef __cplusplus
}
#endif
#endif
