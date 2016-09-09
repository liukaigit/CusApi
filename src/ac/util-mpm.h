#ifndef __UTIL_MPM_H__
#define __UTIL_MPM_H__

#include"ac.h"

#define MPM_INIT_HASH_SIZE 65536

enum {
    MPM_NOTSET = 0,

    MPM_AC,
    MPM_HS,
    /* table size */
    MPM_TABLE_SIZE,
};


/* Internal Pattern Index: 0 to pattern_cnt-1 */
typedef uint32_t MpmPatternIndex;

/** \brief helper structure for the pattern matcher engine. The Pattern Matcher
 *         thread has this and passes a pointer to it to the pattern matcher.
 *         The actual pattern matcher will fill the structure. */
typedef struct PatternMatcherQueue_ {
	
	uint32_t *pattern_id_array; 	/** array with pattern id's that had a
										   pattern match. These will be inspected
										   futher by the detection engine. */
	uint32_t pattern_id_array_cnt;  /**< Number currently stored */
	uint32_t pattern_id_array_size; /**< Allocated size in bytes */

} PatternMatcherQueue;

typedef struct MpmPattern_ {
    /* length of the pattern */
    uint16_t len;
    /* flags decribing the pattern */
    uint8_t flags;
    /* holds the original pattern that was added */
    uint8_t *original_pat;
    /* case sensitive */
    uint8_t *cs;
    /* case INsensitive */
    uint8_t *ci;
    /* pattern id */
    uint32_t id;

    /* sid(s) for this pattern */
    uint32_t sids_size;
    SigIntId *sids;

    struct MpmPattern_ *next;
} MpmPattern;

typedef struct MpmCtx_ {
    void *ctx;
    uint16_t mpm_type;

    /* Indicates if this a global mpm_ctx.  Global mpm_ctx is the one that
     * is instantiated when we use "single".  Non-global is "full", i.e.
     * one per sgh.  We are using a uint16_t here to avoiding using a pad.
     * You can use a uint8_t here as well. */
    uint16_t global;

    /* unique patterns */
    uint32_t pattern_cnt;

    uint16_t minlen;
    uint16_t maxlen;

    uint32_t memory_cnt;
    uint32_t memory_size;

    uint32_t max_pat_id;

    /* hash used during ctx initialization */
    MpmPattern **init_hash;
} MpmCtx;

/* if we want to retrieve an unique mpm context from the mpm context factory
 * we should supply this as the key */
#define MPM_CTX_FACTORY_UNIQUE_CONTEXT -1

/** pattern is case insensitive */
#define MPM_PATTERN_FLAG_NOCASE     0x01
/** pattern is negated */
#define MPM_PATTERN_FLAG_NEGATED    0x02
/** pattern has a depth setting */
#define MPM_PATTERN_FLAG_DEPTH      0x04
/** pattern has an offset setting */
#define MPM_PATTERN_FLAG_OFFSET     0x08
/** one byte pattern (used in b2g) */
#define MPM_PATTERN_ONE_BYTE        0x10
/** the ctx uses it's own internal id instead of
 *  what is passed through the API */
#define MPM_PATTERN_CTX_OWNS_ID     0x20


void PmqCleanup(PatternMatcherQueue *);

void PmqFree(PatternMatcherQueue *);

int MpmAddPatternCS(struct MpmCtx_ *mpm_ctx, uint8_t *pat, uint16_t patlen,
                    uint16_t offset, uint16_t depth,
                    uint32_t pid, SigIntId sid, uint8_t flags);
int MpmAddPatternCI(struct MpmCtx_ *mpm_ctx, uint8_t *pat, uint16_t patlen,
                    uint16_t offset, uint16_t depth,
                    uint32_t pid, SigIntId sid, uint8_t flags);

void MpmFreePattern(MpmCtx *mpm_ctx, MpmPattern *p);

int MpmAddPattern(MpmCtx *mpm_ctx, uint8_t *pat, uint16_t patlen,
                            uint16_t offset, uint16_t depth, uint32_t pid,
                            SigIntId sid, uint8_t flags);

/* Resize Pattern ID array. Only called from MpmAddPid(). */
int MpmAddPidResize(PatternMatcherQueue *pmq, uint32_t new_size);

void MpmAddPid(PatternMatcherQueue *pmq, uint32_t patid);

#endif /* __UTIL_MPM_H__ */
