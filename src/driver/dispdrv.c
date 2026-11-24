// Display driver: queues draw callbacks per camera with a render mode and depth each frame, sorts
// them and calls them in order.

#include "driver/dispdrv.h"
#include "memory.h"
#include "system.h"
#include <dolphin/mtx.h>
#include <string.h>
#include "driver/fadedrv.h"
#include "version.h"
#ifdef TARGET_PC
#include "driver/mapdrv.h"
#include "port/hud.h"
#include "port/platform.h"
#include "port/toggles.h"
#include "port/gx_arrays.h"

extern DispCallback portMapDispObjCb(void);
extern int portMapIsSafeOpaqueObj(const void* param);
extern void portMapDispRunBegin(void);
extern void portMapDispRunEnd(void);

#define DISP_MAX_ENTRIES_PC 1024
#define DISP_MAX_CAMS_PC 16

typedef struct DispBatchItem {
    DispEntry* entry;
    PortMapBatchKey key;
    s32 origIdx;
} DispBatchItem;

static DispBatchItem sBatchWork[DISP_MAX_ENTRIES_PC];
static DispBatchItem sBatchTmp[DISP_MAX_ENTRIES_PC];
static DispEntry* sOtherWork[DISP_MAX_ENTRIES_PC];
static DispEntry* sSortTmp[DISP_MAX_ENTRIES_PC];
static s32 sCamStart[DISP_MAX_CAMS_PC];
static s32 sCamEnd[DISP_MAX_CAMS_PC];
static int sCamBucketsValid = 0;
static s32 sSortedEntryN = 0;
static int sSafeMapRunActive = 0;
static u8 sSafeMapCam = 0;
static u8 sSafeMapMode = 0;

static inline int _batchItemLessOrEq(const DispBatchItem* a, const DispBatchItem* b) {
    const PortMapBatchKey* ka = &a->key;
    const PortMapBatchKey* kb = &b->key;
    if (ka->k0 != kb->k0) return ka->k0 < kb->k0;
    if (ka->lightMask != kb->lightMask) return ka->lightMask < kb->lightMask;
    if (ka->flags1 != kb->flags1) return ka->flags1 < kb->flags1;
    if (ka->material != kb->material) return ka->material < kb->material;
    if (ka->kc0Packed != kb->kc0Packed) return ka->kc0Packed < kb->kc0Packed;
    if (ka->cullMode != kb->cullMode) return ka->cullMode < kb->cullMode;
    if (ka->elementMask != kb->elementMask) return ka->elementMask < kb->elementMask;
    if (ka->vcdTable != kb->vcdTable) return ka->vcdTable < kb->vcdTable;
    if (ka->partCount != kb->partCount) return ka->partCount < kb->partCount;
    return a->origIdx <= b->origIdx;
}

static void _sortBatchItems(DispBatchItem* arr, s32 n) {
    s32 width, i;
    if (n <= 1) {
        return;
    }
    if (n <= 16) {
        for (i = 1; i < n; i++) {
            DispBatchItem cur = arr[i];
            s32 j = i - 1;
            while (j >= 0 && !_batchItemLessOrEq(&arr[j], &cur)) {
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = cur;
        }
        return;
    }
    for (width = 1; width < n; width <<= 1) {
        for (i = 0; i < n; i += (width << 1)) {
            s32 mid = i + width;
            s32 right = i + (width << 1);
            s32 a, b, dst;
            if (mid >= n) {
                continue;
            }
            if (right > n) {
                right = n;
            }
            if (_batchItemLessOrEq(&arr[mid - 1], &arr[mid])) {
                continue;
            }
            a = i;
            b = mid;
            dst = i;
            while (a < mid && b < right) {
                if (_batchItemLessOrEq(&arr[a], &arr[b])) {
                    sBatchTmp[dst++] = arr[a++];
                } else {
                    sBatchTmp[dst++] = arr[b++];
                }
            }
            while (a < mid) {
                sBatchTmp[dst++] = arr[a++];
            }
            while (b < right) {
                sBatchTmp[dst++] = arr[b++];
            }
            memcpy(&arr[i], &sBatchTmp[i], (size_t)(right - i) * sizeof(DispBatchItem));
        }
    }
}

static void _mergeSortEntries(DispEntry** arr, s32 n) {
    s32 width, i;
    if (n <= 1) {
        return;
    }
    if (n <= 16) {
        for (i = 1; i < n; i++) {
            DispEntry* cur = arr[i];
            f32 co = cur->order;
            s32 j = i - 1;
            while (j >= 0 && arr[j]->order > co) {
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = cur;
        }
        return;
    }
    for (width = 1; width < n; width <<= 1) {
        for (i = 0; i < n; i += (width << 1)) {
            s32 mid = i + width;
            s32 right = i + (width << 1);
            s32 a, b, dst;
            if (mid >= n) {
                continue;
            }
            if (right > n) {
                right = n;
            }
            if (arr[mid - 1]->order <= arr[mid]->order) {
                continue;
            }
            a = i;
            b = mid;
            dst = i;
            while (a < mid && b < right) {
                if (arr[a]->order <= arr[b]->order) {
                    sSortTmp[dst++] = arr[a++];
                } else {
                    sSortTmp[dst++] = arr[b++];
                }
            }
            while (a < mid) {
                sSortTmp[dst++] = arr[a++];
            }
            while (b < right) {
                sSortTmp[dst++] = arr[b++];
            }
            memcpy(&arr[i], &sSortTmp[i], (size_t)(right - i) * sizeof(DispEntry*));
        }
    }
}
#endif

#define DISP_MAX_ENTRIES 1024

//.sdata
f32 offset_tbl[] = {
    400000.0f, 400000.0f, 600000.0f, 100000.0f, 100000.0f,
    300000.0f, 900000.0f, 1100000.0f, 1000000.0f, 500000.0f,
#if VERSION >= VERSION_JP
    700000.0f //700K new in retail
#endif
};

//.sbss
DispEntry* currentWorkPtr;
s32 entry_n;
DispEntry** pSortWork;
DispEntry* pDispWork;

// local prototypes
static s32 _sort(const void* sortA, const void* sortB);
#ifdef TARGET_PC
static int sForceDrawThisFrame = 0;
#endif

/** Allocates the display list of 1024 draw requests and its sort table at boot. */
void dispInit(void) {
    pDispWork = __memAlloc(HEAP_DEFAULT, sizeof(DispEntry) * DISP_MAX_ENTRIES);
    pSortWork = __memAlloc(HEAP_DEFAULT, sizeof(DispEntry*) * DISP_MAX_ENTRIES);
    memset(pDispWork, 0, sizeof(DispEntry) * DISP_MAX_ENTRIES);
    memset(pSortWork, 0, sizeof(DispEntry*) * DISP_MAX_ENTRIES);
    entry_n = 0;
#ifdef TARGET_PC
    sForceDrawThisFrame = 0;
#endif
}

/** Empties the display list at the start of each frame. */
void dispReInit(void) {
    entry_n = 0;
#ifdef TARGET_PC
    sCamBucketsValid = 0;
    sSortedEntryN = 0;
    sForceDrawThisFrame = 0;
#endif
}

/**
 * Queues a draw callback for camera camId this frame.
 * renderMode picks the blend, alpha test and depth setup (0 opaque, 1 alpha-tested, 2/5 translucent,
 * 3/4 no depth) and a base depth from offset_tbl;
 * order shifts it within that band (translucent modes draw far to near, the rest near to far).
 * NaN orders are dropped. The JP and later releases add translucent modes 9 and 10.
 */
void dispEntry(CameraId camId, f32 order, s32 renderMode, DispCallback callback, void* param) {
    DispEntry* entry = &pDispWork[entry_n];

    if (order != order)
        return;
    entry->cameraId = camId;
    entry->renderMode = (u8)renderMode;
    entry->callback = callback;
    entry->param = param;

    switch (renderMode) {
        case 2:
        case 5:
#if VERSION >= VERSION_JP
        case 9:
        case 10:
#endif
            entry->order = offset_tbl[renderMode] + order;
            break;
        default:
            entry->order = offset_tbl[renderMode] - order;
            break;
    }

    pSortWork[entry_n] = entry;
    entry_n++;
}

/** qsort comparator for draw requests: ascending by their combined order. */
static s32 _sort(const void* sortA, const void* sortB) {
    f32 orderB;
    f32 orderA;

    orderA = (*(DispEntry* const*)sortA)->order;
    orderB = (*(DispEntry* const*)sortB)->order;
    if (orderA > orderB) {
        return 1;
    }
    if (orderA < orderB) {
        return -1;
    }
    return 0;
}

/** Sorts this frame's draw requests by order before the cameras are drawn. */
#ifdef TARGET_PC
static void portDispGroupMapObjects(void) {
    DispCallback mapCb = portMapDispObjCb();
    s32 i = 0;

    while (i < entry_n) {
        u8 camId = pSortWork[i]->cameraId;
        u8 mode = pSortWork[i]->renderMode;
        s32 j = i;
        s32 batchN = 0;
        s32 otherN = 0;

        /* Modes 0 (opaque) and 1 (alpha-test cutout) share the same 400k depth band,
         * write Z unconditionally for visible texels, and use GX_BM_NONE; grouping both
         * across a contiguous (camId, mode 0|1) slice avoids interleaved 0/1 state splits. */
        if (mode == 0 || mode == 1) {
            while (j < entry_n && pSortWork[j]->cameraId == camId &&
                   (pSortWork[j]->renderMode == 0 || pSortWork[j]->renderMode == 1)) {
                j++;
            }
            if ((j - i) > 1) {
                s32 k;
                for (k = i; k < j; k++) {
                    DispEntry* e = pSortWork[k];
                    if (e->callback == mapCb && portMapIsSafeOpaqueObj(e->param)) {
                        sBatchWork[batchN].entry = e;
                        portMapFillBatchKey(e->param, e->renderMode, &sBatchWork[batchN].key);
                        sBatchWork[batchN].origIdx = k;
                        batchN++;
                    } else {
                        sOtherWork[otherN++] = e;
                    }
                }
                if (batchN > 1 || (batchN > 0 && otherN > 0)) {
                    s32 outIdx = i;
                    if (batchN > 1) {
                        _sortBatchItems(sBatchWork, batchN);
                    }
                    for (k = 0; k < otherN; k++) {
                        if (sOtherWork[k]->order < 400000.0f) {
                            pSortWork[outIdx++] = sOtherWork[k];
                        }
                    }
                    for (k = 0; k < batchN; k++) {
                        pSortWork[outIdx++] = sBatchWork[k].entry;
                    }
                    for (k = 0; k < otherN; k++) {
                        if (sOtherWork[k]->order >= 400000.0f) {
                            pSortWork[outIdx++] = sOtherWork[k];
                        }
                    }
                }
            }
        } else {
            while (j < entry_n && pSortWork[j]->cameraId == camId && pSortWork[j]->renderMode == mode) {
                j++;
            }
        }
        i = j;
    }
}
#endif

void dispSort(void) {
#ifdef TARGET_PC
	if (entry_n <= DISP_MAX_ENTRIES_PC) {
		s32 counts[DISP_MAX_CAMS_PC] = {0};
		s32 pos[DISP_MAX_CAMS_PC];
		s32 i;
		int allSmall = 1;
		for (i = 0; i < entry_n; i++) {
			u8 c = pDispWork[i].cameraId;
			if (c >= DISP_MAX_CAMS_PC) {
				allSmall = 0;
				break;
			}
			counts[c]++;
		}
		if (allSmall) {
			s32 offset = 0;
			for (i = 0; i < DISP_MAX_CAMS_PC; i++) {
				sCamStart[i] = offset;
				pos[i] = offset;
				offset += counts[i];
				sCamEnd[i] = offset;
			}
			for (i = 0; i < entry_n; i++) {
				DispEntry* e = &pDispWork[i];
				pSortWork[pos[e->cameraId]++] = e;
			}
			for (i = 0; i < DISP_MAX_CAMS_PC; i++) {
				s32 cnt = sCamEnd[i] - sCamStart[i];
				if (cnt > 1) {
					_mergeSortEntries(&pSortWork[sCamStart[i]], cnt);
				}
			}
			sCamBucketsValid = 1;
			sSortedEntryN = entry_n;
			portDispGroupMapObjects();
			return;
		}
	}
	sCamBucketsValid = 0;
	qqsort(pSortWork, entry_n, sizeof(DispEntry*), _sort);
	portDispGroupMapObjects();
#else
	qqsort(pSortWork, entry_n, sizeof(DispEntry*), _sort);
#endif
}

/**
 * Calls every queued draw callback for camera camId in sorted order, first setting the GX blend,
 * alpha compare and depth state its render mode asks for.
 * Colour updates are always on; alpha updates are off for cameras 4, 5 and 7.
 */
void dispDraw(CameraId camId) {
    s32 i;
#ifdef TARGET_PC
    if (portBatteryUndrawn()) {
        return; /* battery saver: a frame that runs without being drawn (port/toggles.h) */
    }
#endif

#ifdef TARGET_PC
    {
        s32 iStart = (sCamBucketsValid && (u32)camId < DISP_MAX_CAMS_PC && entry_n == sSortedEntryN) ? sCamStart[camId] : 0;
        s32 iEnd = (sCamBucketsValid && (u32)camId < DISP_MAX_CAMS_PC && entry_n == sSortedEntryN) ? sCamEnd[camId] : entry_n;
        for (i = iStart; i < iEnd; i++) {
#else
    for (i = 0; i < entry_n; i++) {
#endif
        DispEntry* entry = pSortWork[i];
#ifdef TARGET_PC
        portGXKick(32 * 1024); /* (Aurora's GX thread: translating while the list is drawn) */
        if (i == 0 && sSafeMapRunActive) {
            portMapDispRunEnd();
            sSafeMapRunActive = 0;
        }
#endif
        if (entry->cameraId == camId) {
#ifdef TARGET_PC
            int isSafeMap = ((entry->renderMode == 0 || entry->renderMode == 1) &&
                             entry->callback == portMapDispObjCb() &&
                             portMapIsSafeOpaqueObj(entry->param));
            if (sSafeMapRunActive && isSafeMap && sSafeMapCam == camId &&
                sSafeMapMode == entry->renderMode) {
                currentWorkPtr = entry;
                entry->callback(camId, entry->param);
                if (i + 1 >= entry_n || pSortWork[i + 1]->cameraId != camId) {
                    portMapDispRunEnd();
                    sSafeMapRunActive = 0;
                }
                continue;
            }
            if (sSafeMapRunActive) {
                portMapDispRunEnd();
                sSafeMapRunActive = 0;
            }
            if (isSafeMap) {
                portMapDispRunBegin();
                sSafeMapRunActive = 1;
                sSafeMapCam = camId;
                sSafeMapMode = entry->renderMode;
            }
#endif
            GXSetColorUpdate(GX_TRUE);
            switch (camId) {
                case 4:
                case 5:
                case 7:
                    GXSetAlphaUpdate(GX_FALSE);
                    break;
                default:
                case 6:
                    GXSetAlphaUpdate(GX_TRUE);
                    break;
            }
#if VERSION >= VERSION_US
            GXSetZScaleOffset(fadeConstOne, 0.0f); // US reads the 1.0f constant defined in fadedrv.c
#else
            GXSetZScaleOffset(1.0f, 0.0f);
#endif
            switch (entry->renderMode) {
                case 0:
                    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_OR);
                    GXSetZCompLoc(GX_TRUE);
                    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
                    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
                    break;
                case 3:
                    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
                    GXSetZCompLoc(GX_TRUE);
                    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
                    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
                    break;
                case 1:
                    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
                    GXSetZCompLoc(GX_FALSE);
                    GXSetAlphaCompare(GX_GEQUAL, 0x80, GX_AOP_OR, GX_NEVER, 0);
                    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
                    break;
                case 4:
                    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
                    GXSetZCompLoc(GX_FALSE);
                    GXSetAlphaCompare(GX_GEQUAL, 0x80, GX_AOP_OR, GX_NEVER, 0);
                    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
                    break;
                case 2:
                case 5:
#if VERSION >= VERSION_JP
                case 9:
                case 10:
#endif
                    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
                    GXSetZCompLoc(GX_TRUE);
                    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
                    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);
                    break;
            }
            currentWorkPtr = entry;
#if defined(TARGET_PC) && defined(TTYD_BACKEND_AURORA)
            portHudDraw(camId, entry->callback, entry->param); /* PC port: HUD scale (port/hud.h) */
#endif
            entry->callback(camId, entry->param);
        }
#ifdef TARGET_PC
        }
#endif
    }
#if defined(TARGET_PC) && defined(TTYD_BACKEND_AURORA)
    portHudDraw(camId, NULL, NULL);
#endif
}

/**
 * Turns a world position into a dispEntry order from its projected depth under the 3D camera,
 * scaled to 0..10000 (clamped) and negated, so callers can sort objects by distance.
 */
f32 dispCalcZ(Vec* input) {
    CameraEntry* camera;
    Vec output;
    f32 calc;

    camera = camGetPtr(CAMERA_3D);
    MTXMultVec(camera->view, input, &output);
    MTX44MultVec(camera->projection, &output, &output);
    calc = (5000.0f * output.z) + 5000.0f;
    if (calc < 0.0f) {
        calc = 0.0f;
    } else if (calc > 10000.0f) {
        calc = 10000.0f;
    }
    calc *= -1.0f;
    return calc;
}

/** Returns the draw request whose callback is running now, so a callback can read its own render mode. */
DispEntry* dispGetCurWork(void) {
    return currentWorkPtr;
}

#ifdef TARGET_PC
extern int portImgHasPendingCapture(void);
extern DispCallback portExtMakeTexture;

void portDispForceDrawThisFrame(void) {
    sForceDrawThisFrame = 1;
}

/**
 * Returns 1 if the queued display list contains work that must not be skipped on an undrawn frame
 * (one-shot offscreen texture builds such as extMakeTexture / puniMakeTexture, or pending imgdrv captures).
 * Note: animPaperPoseDisp queues on CAMERA_OFFSCREEN every frame with param == NULL and is skipped when
 * the rest of the frame is undrawn.
 */
int portDispMustDraw(void) {
    s32 i;
    if (sForceDrawThisFrame) {
        return 1;
    }
    for (i = 0; i < entry_n; i++) {
        const DispEntry* e = &pDispWork[i];
        if (e->callback == portExtMakeTexture || (e->cameraId == CAMERA_OFFSCREEN && e->param != NULL)) {
            return 1;
        }
    }
    return portImgHasPendingCapture();
}
#endif
