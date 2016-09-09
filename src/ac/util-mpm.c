
#include "ac.h"
/** \brief Initialized a Pmq
  * \param pmq Pattern matcher queue to be set up.
  */

int PmqSetup(PatternMatcherQueue *pmq, uint32_t patmaxid)
{
    if (pmq == NULL)
        return -1;

    memset(pmq, 0, sizeof(PatternMatcherQueue));
	
    if (patmaxid > 0) {
		if(patmaxid > 32)
        	pmq->pattern_id_array_size = patmaxid; /* Intial size, TODO Make this configure option */
		pmq->pattern_id_array_size = 32;
        pmq->pattern_id_array_cnt = 0;

        pmq->pattern_id_array = SCCalloc(pmq->pattern_id_array_size, sizeof(uint32_t));
        if (pmq->pattern_id_array == NULL) {
            return -1;
        }
    }

    return 0;
}

/** \brief Cleanup a Pmq
  * \param pmq Pattern matcher queue to be cleaned up.
  */
void PmqCleanup(PatternMatcherQueue *pmq)
{
    if (pmq == NULL)
        return;

    if (pmq->pattern_id_array != NULL) {
        SCFree(pmq->pattern_id_array);
        pmq->pattern_id_array = NULL;
    }

    pmq->pattern_id_array_cnt = 0;
    pmq->pattern_id_array_size = 0;
}

int MpmAddPidResize(PatternMatcherQueue *pmq, uint32_t new_size)
{
    /* Need to make the array bigger. Double the size needed to
     * also handle the case that sids_size might still be
     * larger than the old size.
     */
    new_size = new_size * 2;
    uint32_t *new_array = (uint32_t*)SCRealloc(pmq->pattern_id_array,
                                               new_size * sizeof(uint32_t));
    if (unlikely(new_array == NULL)) {
        // Failed to allocate 2x, so try 1x.
        new_size = new_size / 2;
        new_array = (uint32_t*)SCRealloc(pmq->pattern_id_array,
                                         new_size * sizeof(uint32_t));
        if (unlikely(new_array == NULL)) {
            //SCLogError(SC_ERR_MEM_ALLOC, "Failed to realloc PatternMatchQueue"
             //          " pattern ID array. Some new Pattern ID matches were lost.");
            return 0;
        }
    }
    pmq->pattern_id_array = new_array;
    pmq->pattern_id_array_size = new_size;

    return new_size;
}

void MpmAddPid(PatternMatcherQueue *pmq, uint32_t patid)
{
	if(pmq->pattern_id_array_size == 0)
		return;
    uint32_t new_size = pmq->pattern_id_array_cnt + 1;
    if (new_size > pmq->pattern_id_array_size)  {
        if (MpmAddPidResize(pmq, new_size) == 0)
            return;
    }
    pmq->pattern_id_array[pmq->pattern_id_array_cnt] = patid;
    pmq->pattern_id_array_cnt = new_size;
    //SCLogDebug("pattern_id_array_cnt %u", pmq->pattern_id_array_cnt);
}


/**
 * \internal
 * \brief Creates a hash of the pattern.  We use it for the hashing process
 *        during the initial pattern insertion time, to cull duplicate sigs.
 *
 * \param pat    Pointer to the pattern.
 * \param patlen Pattern length.
 *
 * \retval hash A 32 bit unsigned hash.
 */
static inline uint32_t MpmInitHashRaw(uint8_t *pat, uint16_t patlen)
{
    uint32_t hash = patlen * pat[0];
    if (patlen > 1)
        hash += pat[1];

    return (hash % MPM_INIT_HASH_SIZE);
}

/**
 * \internal
 * \brief Looks up a pattern.  We use it for the hashing process during the
 *        the initial pattern insertion time, to cull duplicate sigs.
 *
 * \param ctx    Pointer to the AC ctx.
 * \param pat    Pointer to the pattern.
 * \param patlen Pattern length.
 * \param flags  Flags.  We don't need this.
 *
 * \retval hash A 32 bit unsigned hash.
 */
static inline MpmPattern *MpmInitHashLookup(MpmCtx *ctx, uint8_t *pat,
                                                  uint16_t patlen, char flags,
                                                  uint32_t pid)
{
    uint32_t hash = MpmInitHashRaw(pat, patlen);

    if (ctx->init_hash == NULL) {
        return NULL;
    }

    MpmPattern *t = ctx->init_hash[hash];
    for ( ; t != NULL; t = t->next) {
        if (!(flags & MPM_PATTERN_CTX_OWNS_ID)) {
            if (t->id == pid)
                return t;
        } else {
            if (t->len == patlen &&
                    memcmp(pat, t->original_pat, patlen) == 0 &&
                    t->flags == flags)
            {
                return t;
            }
        }
    }

    return NULL;
}

/**
 * \internal
 * \brief Allocs a new pattern instance.
 *
 * \param mpm_ctx Pointer to the mpm context.
 *
 * \retval p Pointer to the newly created pattern.
 */
static inline MpmPattern *MpmAllocPattern(MpmCtx *mpm_ctx)
{
    MpmPattern *p = SCMalloc(sizeof(MpmPattern));
    if (unlikely(p == NULL)) {
        exit(EXIT_FAILURE);
    }
    memset(p, 0, sizeof(MpmPattern));

    mpm_ctx->memory_cnt++;
    mpm_ctx->memory_size += sizeof(MpmPattern);

    return p;
}

/**
 * \internal
 * \brief Used to free MpmPattern instances.
 *
 * \param mpm_ctx Pointer to the mpm context.
 * \param p       Pointer to the MpmPattern instance to be freed.
 */
void MpmFreePattern(MpmCtx *mpm_ctx, MpmPattern *p)
{
    if (p != NULL && p->cs != NULL && p->cs != p->ci) {
        SCFree(p->cs);
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= p->len;
    }

    if (p != NULL && p->ci != NULL) {
        SCFree(p->ci);
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= p->len;
    }

    if (p != NULL && p->original_pat != NULL) {
        SCFree(p->original_pat);
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= p->len;
    }

    if (p != NULL) {
        SCFree(p);
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= sizeof(MpmPattern);
    }
    return;
}

static inline uint32_t MpmInitHash(MpmPattern *p)
{
    uint32_t hash = p->len * p->original_pat[0];
    if (p->len > 1)
        hash += p->original_pat[1];

    return (hash % MPM_INIT_HASH_SIZE);
}

static inline int MpmInitHashAdd(MpmCtx *ctx, MpmPattern *p)
{
    uint32_t hash = MpmInitHash(p);

    if (ctx->init_hash == NULL) {
        return 0;
    }

    if (ctx->init_hash[hash] == NULL) {
        ctx->init_hash[hash] = p;
        return 0;
    }

    MpmPattern *tt = NULL;
    MpmPattern *t = ctx->init_hash[hash];

    /* get the list tail */
    do {
        tt = t;
        t = t->next;
    } while (t != NULL);

    tt->next = p;

    return 0;
}


/**
 * \internal
 * \brief Add a pattern to the mpm-ac context.
 *
 * \param mpm_ctx Mpm context.
 * \param pat     Pointer to the pattern.
 * \param patlen  Length of the pattern.
 * \param pid     Pattern id
 * \param sid     Signature id (internal id).
 * \param flags   Pattern's MPM_PATTERN_* flags.
 *
 * \retval  0 On success.
 * \retval -1 On failure.
 */
int MpmAddPattern(MpmCtx *mpm_ctx, uint8_t *pat, uint16_t patlen,
                            uint16_t offset, uint16_t depth, uint32_t pid,
                            SigIntId sid, uint8_t flags)
{
    //SCLogDebug("Adding pattern for ctx %p, patlen %"PRIu16" and pid %" PRIu32,
    //           mpm_ctx, patlen, pid);

    if (patlen == 0) {
        //SCLogWarning(SC_ERR_INVALID_ARGUMENTS, "pattern length 0");
        return 0;
    }

   // if (flags & MPM_PATTERN_CTX_OWNS_ID)
        //pid = UINT_MAX;

    /* check if we have already inserted this pattern */
    MpmPattern *p = MpmInitHashLookup(mpm_ctx, pat, patlen, flags, pid);
    if (p == NULL) {
       // SCLogDebug("Allocing new pattern");

        /* p will never be NULL */
        p = MpmAllocPattern(mpm_ctx);

        p->len = patlen;
        p->flags = flags;
        if (flags & MPM_PATTERN_CTX_OWNS_ID)
            p->id = mpm_ctx->max_pat_id++;
        else
            p->id = pid;

        p->original_pat = SCMalloc(patlen);
        if (p->original_pat == NULL)
            goto error;
        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size += patlen;
        memcpy(p->original_pat, pat, patlen);

        p->ci = SCMalloc(patlen);
        if (p->ci == NULL)
            goto error;
        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size += patlen;
        memcpy_tolower(p->ci, pat, patlen);

        /* setup the case sensitive part of the pattern */
        if (p->flags & MPM_PATTERN_FLAG_NOCASE) {
            /* nocase means no difference between cs and ci */
            p->cs = p->ci;
        } else {
            if (memcmp(p->ci, pat, p->len) == 0) {
                /* no diff between cs and ci: pat is lowercase */
                p->cs = p->ci;
            } else {
                p->cs = SCMalloc(patlen);
                if (p->cs == NULL)
                    goto error;
                mpm_ctx->memory_cnt++;
                mpm_ctx->memory_size += patlen;
                memcpy(p->cs, pat, patlen);
            }
        }

        /* put in the pattern hash */
        MpmInitHashAdd(mpm_ctx, p);

        mpm_ctx->pattern_cnt++;

        if (mpm_ctx->maxlen < patlen)
            mpm_ctx->maxlen = patlen;

        if (mpm_ctx->minlen == 0) {
            mpm_ctx->minlen = patlen;
        } else {
            if (mpm_ctx->minlen > patlen)
                mpm_ctx->minlen = patlen;
        }

        /* we need the max pat id */
        if (p->id > mpm_ctx->max_pat_id)
            mpm_ctx->max_pat_id = p->id;

        p->sids_size = 1;
        p->sids = SCMalloc(p->sids_size * sizeof(SigIntId));
        BUG_ON(p->sids == NULL);
        p->sids[0] = sid;
    } else {
        /* we can be called multiple times for the same sid in the case
         * of the 'single' modus. Here multiple rule groups share the
         * same mpm ctx and might be adding the same pattern to the
         * mpm_ctx */
        int found = 0;
        uint32_t x = 0;
        for (x = 0; x < p->sids_size; x++) {
            if (p->sids[x] == sid) {
                found = 1;
                break;
            }
        }

        if (!found) {
            SigIntId *sids = SCRealloc(p->sids, (sizeof(SigIntId) * (p->sids_size + 1)));
            BUG_ON(sids == NULL);
            p->sids = sids;
            p->sids[p->sids_size] = sid;
            p->sids_size++;
        }
    }

    return 0;

error:
    MpmFreePattern(mpm_ctx, p);
    return -1;
}


