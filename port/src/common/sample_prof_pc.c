/* A sampling profiler for devices without perf ($TTYD_SAMPLE_PROFILE=1; Linux only).
 *
 * ITIMER_PROF raises SIGPROF once per millisecond of CPU the process uses, on the thread that
 * was running, so the samples follow CPU time across every thread (gperftools' method). The
 * handler stores the interrupted pc, the link register (its caller, for inlined leaves), the
 * thread and the room being played into a ring; a background thread appends the ring to
 * profile.bin every second and keeps profile.maps current: the executable's load address, the
 * threads' names and the rooms' names, everything tools need to symbolize the samples against
 * the unstripped build offline. Nothing runs without the variable. */
#include "port/sample_prof.h"

#if defined(__linux__) && !defined(__ANDROID__) && (defined(__aarch64__) || defined(__x86_64__))
#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <ucontext.h>
#include <unistd.h>

#define RING (1u << 16) /* samples between flushes: ~20 s of 3 busy cores at 1 kHz */
#define MAX_ROOMS 512

typedef struct {
    uint64_t pc;
    uint64_t lr;
    uint32_t tid;
    uint16_t room;
    uint16_t pad;
} Sample;

static Sample* sRing;
static atomic_uint sHead; /* samples written */
static unsigned sTail;    /* samples flushed */
static atomic_ushort sRoom;
static char sRooms[MAX_ROOMS][16];
static int sRoomCount;
static FILE* sOut;
static int sOn;

static void onSignal(int sig, siginfo_t* info, void* context) {
    const ucontext_t* uc = (const ucontext_t*)context;
    const unsigned i = atomic_fetch_add_explicit(&sHead, 1, memory_order_relaxed);
    Sample* s = &sRing[i & (RING - 1)];
    (void)sig;
    (void)info;
#if defined(__aarch64__)
    s->pc = uc->uc_mcontext.pc;
    s->lr = uc->uc_mcontext.regs[30];
#else
    s->pc = (uint64_t)uc->uc_mcontext.gregs[REG_RIP];
    s->lr = 0;
#endif
    s->tid = (uint32_t)syscall(SYS_gettid);
    s->room = atomic_load_explicit(&sRoom, memory_order_relaxed);
}

/* the executable's mappings, the threads' and the rooms' names (rewritten at each flush) */
static void writeMeta(void) {
    FILE* out = fopen("profile.maps", "w");
    FILE* in;
    char line[512];
    int i;
    if (out == NULL) {
        return;
    }
    if ((in = fopen("/proc/self/maps", "r")) != NULL) {
        while (fgets(line, sizeof(line), in) != NULL) {
            fputs(line, out);
        }
        fclose(in);
    }
    {
        /* (opendir, not popen: in a child process /proc/self is the child's) */
        DIR* dir = opendir("/proc/self/task");
        struct dirent* ent;
        while (dir != NULL && (ent = readdir(dir)) != NULL) {
            char path[96], name[64] = "";
            FILE* comm;
            if (ent->d_name[0] == '.') {
                continue;
            }
            snprintf(path, sizeof(path), "/proc/self/task/%s/comm", ent->d_name);
            if ((comm = fopen(path, "r")) != NULL) {
                if (fgets(name, sizeof(name), comm) != NULL) {
                    name[strcspn(name, "\n")] = '\0';
                }
                fclose(comm);
            }
            fprintf(out, "#thread %s %s\n", ent->d_name, name);
        }
        if (dir != NULL) {
            closedir(dir);
        }
    }
    for (i = 0; i < sRoomCount; i++) {
        fprintf(out, "#room %d %s\n", i, sRooms[i]);
    }
    fclose(out);
}

static void* flusher(void* arg) {
    (void)arg;
    for (;;) {
        unsigned head;
        sleep(1);
        head = atomic_load_explicit(&sHead, memory_order_acquire);
        if (head - sTail > RING) {
            sTail = head - RING; /* (fell behind: the oldest are gone) */
        }
        while (sTail != head) {
            const unsigned at = sTail & (RING - 1);
            const unsigned n = (head - sTail) < (RING - at) ? (head - sTail) : (RING - at);
            fwrite(&sRing[at], sizeof(Sample), n, sOut);
            sTail += n;
        }
        fflush(sOut);
        writeMeta();
    }
    return NULL;
}

void portSampleProfInit(void) {
    struct sigaction sa;
    struct itimerval timer;
    pthread_t thread;
    const char* want = getenv("TTYD_SAMPLE_PROFILE");
    if (want == NULL || want[0] == '0' || sOn) {
        return;
    }
    sRing = calloc(RING, sizeof(Sample));
    sOut = fopen("profile.bin", "wb");
    if (sRing == NULL || sOut == NULL) {
        printf("[sample-prof] could not start (memory or profile.bin)\n");
        return;
    }
    sOn = 1;
    snprintf(sRooms[0], sizeof(sRooms[0]), "(start)");
    sRoomCount = 1;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = onSignal;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPROF, &sa, NULL);
    pthread_create(&thread, NULL, flusher, NULL);
    pthread_detach(thread);
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 1000;
    timer.it_value = timer.it_interval;
    setitimer(ITIMER_PROF, &timer, NULL);
    printf("[sample-prof] sampling every 1 ms of CPU into profile.bin (profile.maps beside it)\n");
    fflush(stdout);
}

void portSampleProfRoom(const char* room) {
    int i;
    if (!sOn || room == NULL) {
        return;
    }
    if (strncmp(sRooms[atomic_load(&sRoom)], room, sizeof(sRooms[0]) - 1) == 0) {
        return;
    }
    for (i = 0; i < sRoomCount; i++) {
        if (strncmp(sRooms[i], room, sizeof(sRooms[0]) - 1) == 0) {
            break;
        }
    }
    if (i == sRoomCount && sRoomCount < MAX_ROOMS) {
        snprintf(sRooms[sRoomCount], sizeof(sRooms[0]), "%s", room);
        sRoomCount++;
    }
    if (i < sRoomCount) {
        atomic_store(&sRoom, (unsigned short)i);
        printf("[sample-prof] room %s\n", room);
        fflush(stdout);
    }
}
#else
void portSampleProfInit(void) {}
void portSampleProfRoom(const char* room) { (void)room; }
#endif
