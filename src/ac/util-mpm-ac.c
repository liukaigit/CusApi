
#include "ac.h"

/* a placeholder to denote a failure transition in the goto table */
#define SC_AC_FAIL (-1)

#define STATE_QUEUE_CONTAINER_SIZE 65536

#define AC_CASE_MASK    0x80000000
#define AC_PID_MASK     0x7FFFFFFF
#define AC_CASE_BIT     31

static int construct_both_16_and_32_state_tables = 0;

/**
 * \brief Helper structure used by AC during state table creation
 */
typedef struct StateQueue_ {
    int32_t store[STATE_QUEUE_CONTAINER_SIZE];
    int top;
    int bot;
} StateQueue;


/**
 * \internal
 * \brief Initialize the AC context with user specified conf parameters.  We
 *        aren't retrieving anything for AC conf now, but we will certainly
 *        need it, when we customize AC.
 */
static void SCACGetConfig()
{
    //ConfNode *ac_conf;
    //const char *hash_val = NULL;

    //ConfNode *pm = ConfGetNode("pattern-matcher");

    return;
}

/**
 * \internal
 * \brief Initialize a new state in the goto and output tables.
 *
 * \param mpm_ctx Pointer to the mpm context.
 *
 * \retval The state id, of the newly created state.
 */
static inline int SCACReallocState(SCACCtx *ctx, uint32_t cnt)
{
    void *ptmp;
    int size = 0;

    /* reallocate space in the goto table to include a new state */
    size = cnt * ctx->single_state_size;
    ptmp = SCRealloc(ctx->goto_table, size);
    if (ptmp == NULL) {
        SCFree(ctx->goto_table);
        ctx->goto_table = NULL;
        //SCLogError(SC_ERR_MEM_ALLOC, "Error allocating memory");
        exit(EXIT_FAILURE);
    }
    ctx->goto_table = ptmp;

    /* reallocate space in the output table for the new state */
    int oldsize = ctx->state_count * sizeof(SCACOutputTable);
    size = cnt * sizeof(SCACOutputTable);
    //SCLogDebug("oldsize %d size %d cnt %u ctx->state_count %u",
    //        oldsize, size, cnt, ctx->state_count);

    ptmp = SCRealloc(ctx->output_table, size);
    if (ptmp == NULL) {
        SCFree(ctx->output_table);
        ctx->output_table = NULL;
        //SCLogError(SC_ERR_MEM_ALLOC, "Error allocating memory");
        exit(EXIT_FAILURE);
    }
    ctx->output_table = ptmp;

    memset(((uint8_t *)ctx->output_table + oldsize), 0, (size - oldsize));

    /* \todo using it temporarily now during dev, since I have restricted
     *       state var in SCACCtx->state_table to uint16_t. */
    //if (ctx->state_count > 65536) {
    //    printf("state count exceeded\n");
    //    exit(EXIT_FAILURE);
    //}

    return 0;//ctx->state_count++;
}

/** \internal
 *  \brief Shrink state after setup is done
 *
 *  Shrinks only the output table, goto table is freed after calling this
 */
static void SCACShrinkState(SCACCtx *ctx)
{
    /* reallocate space in the output table for the new state */
#ifdef DEBUG
    int oldsize = ctx->allocated_state_count * sizeof(SCACOutputTable);
#endif
    int newsize = ctx->state_count * sizeof(SCACOutputTable);

    //SCLogDebug("oldsize %d newsize %d ctx->allocated_state_count %u "
    //           "ctx->state_count %u: shrink by %d bytes", oldsize,
    //           newsize, ctx->allocated_state_count, ctx->state_count,
    //           oldsize - newsize);

    void *ptmp = SCRealloc(ctx->output_table, newsize);
    if (ptmp == NULL) {
        SCFree(ctx->output_table);
        ctx->output_table = NULL;
        //SCLogError(SC_ERR_MEM_ALLOC, "Error allocating memory");
        exit(EXIT_FAILURE);
    }
    ctx->output_table = ptmp;
}

static inline int SCACInitNewState(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;;

    /* Exponentially increase the allocated space when needed. */
    if (ctx->allocated_state_count < ctx->state_count + 1) {
        if (ctx->allocated_state_count == 0)
            ctx->allocated_state_count = 256;
        else
            ctx->allocated_state_count *= 2;

        SCACReallocState(ctx, ctx->allocated_state_count);

    }
#if 0
    if (ctx->allocated_state_count > 260) {
        SCACOutputTable *output_state = &ctx->output_table[260];
        SCLogInfo("output_state %p %p %u", output_state, output_state->pids, output_state->no_of_entries);
    }
#endif
    int ascii_code = 0;
    /* set all transitions for the newly assigned state as FAIL transitions */
    for (ascii_code = 0; ascii_code < 256; ascii_code++) {
        ctx->goto_table[ctx->state_count][ascii_code] = SC_AC_FAIL;
    }

    return ctx->state_count++;
}

/**
 * \internal
 * \brief Adds a pid to the output table for a state.
 *
 * \param state   The state to whose output table we should add the pid.
 * \param pid     The pattern id to add.
 * \param mpm_ctx Pointer to the mpm context.
 */
static void SCACSetOutputState(int32_t state, uint32_t pid, MpmCtx *mpm_ctx)
{
    void *ptmp;
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    SCACOutputTable *output_state = &ctx->output_table[state];
    uint32_t i = 0;

    for (i = 0; i < output_state->no_of_entries; i++) {
        if (output_state->pids[i] == pid)
            return;
    }

    output_state->no_of_entries++;
    ptmp = SCRealloc(output_state->pids,
                     output_state->no_of_entries * sizeof(uint32_t));
    if (ptmp == NULL) {
        SCFree(output_state->pids);
        output_state->pids = NULL;
        //SCLogError(SC_ERR_MEM_ALLOC, "Error allocating memory");
        exit(EXIT_FAILURE);
    }
    output_state->pids = ptmp;

    output_state->pids[output_state->no_of_entries - 1] = pid;

    return;
}

/**
 * \brief Helper function used by SCACCreateGotoTable.  Adds a pattern to the
 *        goto table.
 *
 * \param pattern     Pointer to the pattern.
 * \param pattern_len Pattern length.
 * \param pid         The pattern id, that corresponds to this pattern.  We
 *                    need it to updated the output table for this pattern.
 * \param mpm_ctx     Pointer to the mpm context.
 */
static inline void SCACEnter(uint8_t *pattern, uint16_t pattern_len, uint32_t pid,
                             MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    int32_t state = 0;
    int32_t newstate = 0;
    int i = 0;
    int p = 0;

    /* walk down the trie till we have a match for the pattern prefix */
    state = 0;
    for (i = 0; i < pattern_len; i++) {
        if (ctx->goto_table[state][pattern[i]] != SC_AC_FAIL) {
            state = ctx->goto_table[state][pattern[i]];
        } else {
            break;
        }
    }

    /* add the non-matching pattern suffix to the trie, from the last state
     * we left off */
    for (p = i; p < pattern_len; p++) {
        newstate = SCACInitNewState(mpm_ctx);
        ctx->goto_table[state][pattern[p]] = newstate;
        state = newstate;
    }

    /* add this pattern id, to the output table of the last state, where the
     * pattern ends in the trie */
    SCACSetOutputState(state, pid, mpm_ctx);

    return;
}

/**
 * \internal
 * \brief Create the goto table.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
static inline void SCACCreateGotoTable(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    uint32_t i = 0;

    /* add each pattern to create the goto table */
    for (i = 0; i < mpm_ctx->pattern_cnt; i++) {
        SCACEnter(ctx->parray[i]->ci, ctx->parray[i]->len,
                  ctx->parray[i]->id, mpm_ctx);
    }

    int ascii_code = 0;
    for (ascii_code = 0; ascii_code < 256; ascii_code++) {
        if (ctx->goto_table[0][ascii_code] == SC_AC_FAIL) {
            ctx->goto_table[0][ascii_code] = 0;
        }
    }

    return;
}

static inline void SCACDetermineLevel1Gap(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    uint32_t u = 0;

    int map[256];
    memset(map, 0, sizeof(map));

    for (u = 0; u < mpm_ctx->pattern_cnt; u++)
        map[ctx->parray[u]->ci[0]] = 1;

    for (u = 0; u < 256; u++) {
        if (map[u] == 0)
            continue;
        int32_t newstate = SCACInitNewState(mpm_ctx);
        ctx->goto_table[0][u] = newstate;
    }

    return;
}

static inline int SCACStateQueueIsEmpty(StateQueue *q)
{
    if (q->top == q->bot)
        return 1;
    else
        return 0;
}

static inline void SCACEnqueue(StateQueue *q, int32_t state)
{
    int i = 0;

    /*if we already have this */
    for (i = q->bot; i < q->top; i++) {
        if (q->store[i] == state)
            return;
    }

    q->store[q->top++] = state;

    if (q->top == STATE_QUEUE_CONTAINER_SIZE)
        q->top = 0;

    if (q->top == q->bot) {
        //SCLogCritical(SC_ERR_AHO_CORASICK, "Just ran out of space in the queue.  "
        //              "Fatal Error.  Exiting.  Please file a bug report on this");
        exit(EXIT_FAILURE);
    }

    return;
}

static inline int32_t SCACDequeue(StateQueue *q)
{
    if (q->bot == STATE_QUEUE_CONTAINER_SIZE)
        q->bot = 0;

    if (q->bot == q->top) {
        //SCLogCritical(SC_ERR_AHO_CORASICK, "StateQueue behaving weirdly.  "
        //              "Fatal Error.  Exiting.  Please file a bug report on this");
        exit(EXIT_FAILURE);
    }

    return q->store[q->bot++];
}

/**
 * \internal
 * \brief Club the output data from 2 states and store it in the 1st state.
 *        dst_state_data = {dst_state_data} UNION {src_state_data}
 *
 * \param dst_state First state(also the destination) for the union operation.
 * \param src_state Second state for the union operation.
 * \param mpm_ctx Pointer to the mpm context.
 */
static inline void SCACClubOutputStates(int32_t dst_state, int32_t src_state,
                                        MpmCtx *mpm_ctx)
{
    void *ptmp;
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    uint32_t i = 0;
    uint32_t j = 0;

    SCACOutputTable *output_dst_state = &ctx->output_table[dst_state];
    SCACOutputTable *output_src_state = &ctx->output_table[src_state];

    for (i = 0; i < output_src_state->no_of_entries; i++) {
        for (j = 0; j < output_dst_state->no_of_entries; j++) {
            if (output_src_state->pids[i] == output_dst_state->pids[j]) {
                break;
            }
        }
        if (j == output_dst_state->no_of_entries) {
            output_dst_state->no_of_entries++;

            ptmp = SCRealloc(output_dst_state->pids,
                             (output_dst_state->no_of_entries * sizeof(uint32_t)));
            if (ptmp == NULL) {
                SCFree(output_dst_state->pids);
                output_dst_state->pids = NULL;
                //SCLogError(SC_ERR_MEM_ALLOC, "Error allocating memory");
                exit(EXIT_FAILURE);
            }
            output_dst_state->pids = ptmp;

            output_dst_state->pids[output_dst_state->no_of_entries - 1] =
                output_src_state->pids[i];
        }
    }

    return;
}

/**
 * \internal
 * \brief Create the failure table.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
static inline void SCACCreateFailureTable(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    int ascii_code = 0;
    int32_t state = 0;
    int32_t r_state = 0;

    StateQueue q;
    memset(&q, 0, sizeof(StateQueue));

    /* allot space for the failure table.  A failure entry in the table for
     * every state(SCACCtx->state_count) */
    ctx->failure_table = SCMalloc(ctx->state_count * sizeof(int32_t));
    if (ctx->failure_table == NULL) {
        //SCLogError(SC_ERR_MEM_ALLOC, "Error allocating memory");
        exit(EXIT_FAILURE);
    }
    memset(ctx->failure_table, 0, ctx->state_count * sizeof(int32_t));

    /* add the failure transitions for the 0th state, and add every non-fail
     * transition from the 0th state to the queue for further processing
     * of failure states */
    for (ascii_code = 0; ascii_code < 256; ascii_code++) {
        int32_t temp_state = ctx->goto_table[0][ascii_code];
        if (temp_state != 0) {
            SCACEnqueue(&q, temp_state);
            ctx->failure_table[temp_state] = 0;
        }
    }

    while (!SCACStateQueueIsEmpty(&q)) {
        /* pick up every state from the queue and add failure transitions */
        r_state = SCACDequeue(&q);
        for (ascii_code = 0; ascii_code < 256; ascii_code++) {
            int32_t temp_state = ctx->goto_table[r_state][ascii_code];
            if (temp_state == SC_AC_FAIL)
                continue;
            SCACEnqueue(&q, temp_state);
            state = ctx->failure_table[r_state];

            while(ctx->goto_table[state][ascii_code] == SC_AC_FAIL)
                state = ctx->failure_table[state];
            ctx->failure_table[temp_state] = ctx->goto_table[state][ascii_code];
            SCACClubOutputStates(temp_state, ctx->failure_table[temp_state],
                                 mpm_ctx);
        }
    }

    return;
}

/**
 * \internal
 * \brief Create the delta table.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
static inline void SCACCreateDeltaTable(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    int ascii_code = 0;
    int32_t r_state = 0;

    if ((ctx->state_count < 32767) || construct_both_16_and_32_state_tables) {
        ctx->state_table_u16 = SCMalloc(ctx->state_count *
                                        sizeof(SC_AC_STATE_TYPE_U16) * 256);
        if (ctx->state_table_u16 == NULL) {
            //SCLogError(SC_ERR_MEM_ALLOC, "Error allocating memory");
            exit(EXIT_FAILURE);
        }
        memset(ctx->state_table_u16, 0,
               ctx->state_count * sizeof(SC_AC_STATE_TYPE_U16) * 256);

        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size += (ctx->state_count *
                                 sizeof(SC_AC_STATE_TYPE_U16) * 256);

        StateQueue q;
        memset(&q, 0, sizeof(StateQueue));

        for (ascii_code = 0; ascii_code < 256; ascii_code++) {
            SC_AC_STATE_TYPE_U16 temp_state = ctx->goto_table[0][ascii_code];
            ctx->state_table_u16[0][ascii_code] = temp_state;
            if (temp_state != 0)
                SCACEnqueue(&q, temp_state);
        }

        while (!SCACStateQueueIsEmpty(&q)) {
            r_state = SCACDequeue(&q);

            for (ascii_code = 0; ascii_code < 256; ascii_code++) {
                int32_t temp_state = ctx->goto_table[r_state][ascii_code];
                if (temp_state != SC_AC_FAIL) {
                    SCACEnqueue(&q, temp_state);
                    ctx->state_table_u16[r_state][ascii_code] = temp_state;
                } else {
                    ctx->state_table_u16[r_state][ascii_code] =
                        ctx->state_table_u16[ctx->failure_table[r_state]][ascii_code];
                }
            }
        }
    }

    if (!(ctx->state_count < 32767) || construct_both_16_and_32_state_tables) {
        /* create space for the state table.  We could have used the existing goto
         * table, but since we have it set to hold 32 bit state values, we will create
         * a new state table here of type SC_AC_STATE_TYPE(current set to uint16_t) */
        ctx->state_table_u32 = SCMalloc(ctx->state_count *
                                        sizeof(SC_AC_STATE_TYPE_U32) * 256);
        if (ctx->state_table_u32 == NULL) {
            //SCLogError(SC_ERR_MEM_ALLOC, "Error allocating memory");
            exit(EXIT_FAILURE);
        }
        memset(ctx->state_table_u32, 0,
               ctx->state_count * sizeof(SC_AC_STATE_TYPE_U32) * 256);

        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size += (ctx->state_count *
                                 sizeof(SC_AC_STATE_TYPE_U32) * 256);

        StateQueue q;
        memset(&q, 0, sizeof(StateQueue));

        for (ascii_code = 0; ascii_code < 256; ascii_code++) {
            SC_AC_STATE_TYPE_U32 temp_state = ctx->goto_table[0][ascii_code];
            ctx->state_table_u32[0][ascii_code] = temp_state;
            if (temp_state != 0)
                SCACEnqueue(&q, temp_state);
        }

        while (!SCACStateQueueIsEmpty(&q)) {
            r_state = SCACDequeue(&q);

            for (ascii_code = 0; ascii_code < 256; ascii_code++) {
                int32_t temp_state = ctx->goto_table[r_state][ascii_code];
                if (temp_state != SC_AC_FAIL) {
                    SCACEnqueue(&q, temp_state);
                    ctx->state_table_u32[r_state][ascii_code] = temp_state;
                } else {
                    ctx->state_table_u32[r_state][ascii_code] =
                        ctx->state_table_u32[ctx->failure_table[r_state]][ascii_code];
                }
            }
        }
    }

    return;
}

static inline void SCACClubOutputStatePresenceWithDeltaTable(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    int ascii_code = 0;
    uint32_t state = 0;
    uint32_t temp_state = 0;

    if ((ctx->state_count < 32767) || construct_both_16_and_32_state_tables) {
        for (state = 0; state < ctx->state_count; state++) {
            for (ascii_code = 0; ascii_code < 256; ascii_code++) {
                temp_state = ctx->state_table_u16[state & 0x7FFF][ascii_code];
                if (ctx->output_table[temp_state & 0x7FFF].no_of_entries != 0)
                    ctx->state_table_u16[state & 0x7FFF][ascii_code] |= (1 << 15);
            }
        }
    }

    if (!(ctx->state_count < 32767) || construct_both_16_and_32_state_tables) {
        for (state = 0; state < ctx->state_count; state++) {
            for (ascii_code = 0; ascii_code < 256; ascii_code++) {
                temp_state = ctx->state_table_u32[state & 0x00FFFFFF][ascii_code];
                if (ctx->output_table[temp_state & 0x00FFFFFF].no_of_entries != 0)
                    ctx->state_table_u32[state & 0x00FFFFFF][ascii_code] |= (1 << 24);
            }
        }
    }

    return;
}

static inline void SCACInsertCaseSensitiveEntriesForPatterns(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    uint32_t state = 0;
    uint32_t k = 0;

    for (state = 0; state < ctx->state_count; state++) {
        if (ctx->output_table[state].no_of_entries == 0)
            continue;

        for (k = 0; k < ctx->output_table[state].no_of_entries; k++) {
            if (ctx->pid_pat_list[ctx->output_table[state].pids[k]].cs != NULL) {
                ctx->output_table[state].pids[k] &= AC_PID_MASK;
                ctx->output_table[state].pids[k] |= ((uint32_t)1 << AC_CASE_BIT);
            }
        }
    }

    return;
}

/**
 * \brief Process the patterns and prepare the state table.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
static void SCACPrepareStateTable(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;

    /* create the 0th state in the goto table and output_table */
    SCACInitNewState(mpm_ctx);

    SCACDetermineLevel1Gap(mpm_ctx);

    /* create the goto table */
    SCACCreateGotoTable(mpm_ctx);
    /* create the failure table */
    SCACCreateFailureTable(mpm_ctx);
    /* create the final state(delta) table */
    SCACCreateDeltaTable(mpm_ctx);
    /* club the output state presence with delta transition entries */
    SCACClubOutputStatePresenceWithDeltaTable(mpm_ctx);

    /* club nocase entries */
    SCACInsertCaseSensitiveEntriesForPatterns(mpm_ctx);

    /* shrink the memory */
    SCACShrinkState(ctx);

#if 0
    SCACPrintDeltaTable(mpm_ctx);
#endif

    /* we don't need these anymore */
    SCFree(ctx->goto_table);
    ctx->goto_table = NULL;
    SCFree(ctx->failure_table);
    ctx->failure_table = NULL;

    return;
}

/**
 * \brief Process the patterns added to the mpm, and create the internal tables.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
int SCACPreparePatterns(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;

    if (mpm_ctx->pattern_cnt == 0 || mpm_ctx->init_hash == NULL) {
    //    SCLogDebug("no patterns supplied to this mpm_ctx");
        return 0;
    }

    /* alloc the pattern array */
    ctx->parray = (MpmPattern **)SCMalloc(mpm_ctx->pattern_cnt *
                                           sizeof(MpmPattern *));
    if (ctx->parray == NULL)
        goto error;
    memset(ctx->parray, 0, mpm_ctx->pattern_cnt * sizeof(MpmPattern *));
    mpm_ctx->memory_cnt++;
    mpm_ctx->memory_size += (mpm_ctx->pattern_cnt * sizeof(MpmPattern *));

    /* populate it with the patterns in the hash */
    uint32_t i = 0, p = 0;
    for (i = 0; i < MPM_INIT_HASH_SIZE; i++) {
        MpmPattern *node = mpm_ctx->init_hash[i], *nnode = NULL;
        while(node != NULL) {
            nnode = node->next;
            node->next = NULL;
            ctx->parray[p++] = node;
            node = nnode;
        }
    }

    /* we no longer need the hash, so free it's memory */
    SCFree(mpm_ctx->init_hash);
    mpm_ctx->init_hash = NULL;

    /* the memory consumed by a single state in our goto table */
    ctx->single_state_size = sizeof(int32_t) * 256;

    /* handle no case patterns */
    ctx->pid_pat_list = SCMalloc((mpm_ctx->max_pat_id + 1)* sizeof(SCACPatternList));
    if (ctx->pid_pat_list == NULL) {
        //SCLogError(SC_ERR_MEM_ALLOC, "Error allocating memory");
        exit(EXIT_FAILURE);
    }
    memset(ctx->pid_pat_list, 0, (mpm_ctx->max_pat_id + 1) * sizeof(SCACPatternList));

    for (i = 0; i < mpm_ctx->pattern_cnt; i++) {
        if (!(ctx->parray[i]->flags & MPM_PATTERN_FLAG_NOCASE)) {
            ctx->pid_pat_list[ctx->parray[i]->id].cs = SCMalloc(ctx->parray[i]->len);
            if (ctx->pid_pat_list[ctx->parray[i]->id].cs == NULL) {
                //SCLogError(SC_ERR_MEM_ALLOC, "Error allocating memory");
                exit(EXIT_FAILURE);
            }
            memcpy(ctx->pid_pat_list[ctx->parray[i]->id].cs,
                   ctx->parray[i]->original_pat, ctx->parray[i]->len);
            ctx->pid_pat_list[ctx->parray[i]->id].patlen = ctx->parray[i]->len;
        }

        /* ACPatternList now owns this memory */
        //SCLogInfo("ctx->parray[i]->sids_size %u", ctx->parray[i]->sids_size);
        ctx->pid_pat_list[ctx->parray[i]->id].sids_size = ctx->parray[i]->sids_size;
        ctx->pid_pat_list[ctx->parray[i]->id].sids = ctx->parray[i]->sids;

        ctx->parray[i]->sids_size = 0;
        ctx->parray[i]->sids = NULL;
    }

    /* prepare the state table required by AC */
    SCACPrepareStateTable(mpm_ctx);

#ifdef __SC_CUDA_SUPPORT__
    if (mpm_ctx->mpm_type == MPM_AC_CUDA) {
        int r = SCCudaMemAlloc(&ctx->state_table_u32_cuda,
                               ctx->state_count * sizeof(unsigned int) * 256);
        if (r < 0) {
            SCLogError(SC_ERR_AC_CUDA_ERROR, "SCCudaMemAlloc failure.");
            exit(EXIT_FAILURE);
        }

        r = SCCudaMemcpyHtoD(ctx->state_table_u32_cuda,
                             ctx->state_table_u32,
                             ctx->state_count * sizeof(unsigned int) * 256);
        if (r < 0) {
            SCLogError(SC_ERR_AC_CUDA_ERROR, "SCCudaMemcpyHtoD failure.");
            exit(EXIT_FAILURE);
        }
    }
#endif

    /* free all the stored patterns.  Should save us a good 100-200 mbs */
    for (i = 0; i < mpm_ctx->pattern_cnt; i++) {
        if (ctx->parray[i] != NULL) {
            MpmFreePattern(mpm_ctx, ctx->parray[i]);
        }
    }
    SCFree(ctx->parray);
    ctx->parray = NULL;
    mpm_ctx->memory_cnt--;
    mpm_ctx->memory_size -= (mpm_ctx->pattern_cnt * sizeof(MpmPattern *));

    ctx->pattern_id_bitarray_size = (mpm_ctx->max_pat_id / 8) + 1;
   // SCLogDebug("ctx->pattern_id_bitarray_size %u", ctx->pattern_id_bitarray_size);

    return 0;

error:
    return -1;
}


/**
 * \brief Initialize the AC context.
 *
 * \param mpm_ctx       Mpm context.
 */
void SCACInitCtx(MpmCtx *mpm_ctx)
{
    if (mpm_ctx->ctx != NULL)
        return;

    mpm_ctx->ctx = SCMalloc(sizeof(SCACCtx));
    if (mpm_ctx->ctx == NULL) {
        exit(EXIT_FAILURE);
    }
    memset(mpm_ctx->ctx, 0, sizeof(SCACCtx));

    mpm_ctx->memory_cnt++;
    mpm_ctx->memory_size += sizeof(SCACCtx);

    /* initialize the hash we use to speed up pattern insertions */
    mpm_ctx->init_hash = SCMalloc(sizeof(MpmPattern *) * MPM_INIT_HASH_SIZE);
    if (mpm_ctx->init_hash == NULL) {
        exit(EXIT_FAILURE);
    }
    memset(mpm_ctx->init_hash, 0, sizeof(MpmPattern *) * MPM_INIT_HASH_SIZE);

    /* get conf values for AC from our yaml file.  We have no conf values for
     * now.  We will certainly need this, as we develop the algo */
    SCACGetConfig();

    SCReturn;
}

/**
 * \brief Destroy the mpm context.
 *
 * \param mpm_ctx Pointer to the mpm context.
 */
void SCACDestroyCtx(MpmCtx *mpm_ctx)
{
    SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    if (ctx == NULL)
        return;

    if (mpm_ctx->init_hash != NULL) {
        SCFree(mpm_ctx->init_hash);
        mpm_ctx->init_hash = NULL;
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= (MPM_INIT_HASH_SIZE * sizeof(MpmPattern *));
    }

    if (ctx->parray != NULL) {
        uint32_t i;
        for (i = 0; i < mpm_ctx->pattern_cnt; i++) {
            if (ctx->parray[i] != NULL) {
                MpmFreePattern(mpm_ctx, ctx->parray[i]);
            }
        }

        SCFree(ctx->parray);
        ctx->parray = NULL;
        mpm_ctx->memory_cnt--;
        mpm_ctx->memory_size -= (mpm_ctx->pattern_cnt * sizeof(MpmPattern *));
    }

    if (ctx->state_table_u16 != NULL) {
        SCFree(ctx->state_table_u16);
        ctx->state_table_u16 = NULL;

        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size -= (ctx->state_count *
                                 sizeof(SC_AC_STATE_TYPE_U16) * 256);
    }
    if (ctx->state_table_u32 != NULL) {
        SCFree(ctx->state_table_u32);
        ctx->state_table_u32 = NULL;

        mpm_ctx->memory_cnt++;
        mpm_ctx->memory_size -= (ctx->state_count *
                                 sizeof(SC_AC_STATE_TYPE_U32) * 256);
    }

    if (ctx->output_table != NULL) {
        uint32_t state_count;
        for (state_count = 0; state_count < ctx->state_count; state_count++) {
            if (ctx->output_table[state_count].pids != NULL) {
                SCFree(ctx->output_table[state_count].pids);
            }
        }
        SCFree(ctx->output_table);
    }

    if (ctx->pid_pat_list != NULL) {
        uint32_t i;
        for (i = 0; i < (mpm_ctx->max_pat_id + 1); i++) {
            if (ctx->pid_pat_list[i].cs != NULL)
                SCFree(ctx->pid_pat_list[i].cs);
            if (ctx->pid_pat_list[i].sids != NULL)
                SCFree(ctx->pid_pat_list[i].sids);
        }
        SCFree(ctx->pid_pat_list);
    }

    SCFree(mpm_ctx->ctx);
    mpm_ctx->memory_cnt--;
    mpm_ctx->memory_size -= sizeof(SCACCtx);

    return;
}

/**
 * \brief The aho corasick search function.
 *
 * \param mpm_ctx        Pointer to the mpm context.
 * \param mpm_thread_ctx Pointer to the mpm thread context.
 * \param pmq            Pointer to the Pattern Matcher Queue to hold
 *                       search matches.
 * \param buf            Buffer to be searched.
 * \param buflen         Buffer length.
 *
 * \retval matches Match count.
 */
uint32_t SCACSearch(const MpmCtx *mpm_ctx, PatternMatcherQueue *pmq, const uint8_t *buf, uint16_t buflen)
{
    const SCACCtx *ctx = (SCACCtx *)mpm_ctx->ctx;
    int i = 0;
    int matches = 0;

	//printf("SCACSEARCH---\n");
    /* \todo tried loop unrolling with register var, with no perf increase.  Need
     * to dig deeper */
    /* \todo Change it for stateful MPM.  Supply the state using mpm_thread_ctx */
    SCACPatternList *pid_pat_list = ctx->pid_pat_list;

    uint8_t bitarray[ctx->pattern_id_bitarray_size];
    memset(bitarray, 0, ctx->pattern_id_bitarray_size);

    if (ctx->state_count < 32767) {
        register SC_AC_STATE_TYPE_U16 state = 0;
        SC_AC_STATE_TYPE_U16 (*state_table_u16)[256] = ctx->state_table_u16;
        for (i = 0; i < buflen; i++) {
            state = state_table_u16[state & 0x7FFF][u8_tolower(buf[i])];
            if (state & 0x8000) {
                uint32_t no_of_entries = ctx->output_table[state & 0x7FFF].no_of_entries;
                uint32_t *pids = ctx->output_table[state & 0x7FFF].pids;
                uint32_t k;
                for (k = 0; k < no_of_entries; k++) {
                    if (pids[k] & AC_CASE_MASK) {
                        uint32_t lower_pid = pids[k] & AC_PID_MASK;
                        if (SCMemcmp(pid_pat_list[lower_pid].cs,
                                     buf + i - pid_pat_list[lower_pid].patlen + 1,
                                     pid_pat_list[lower_pid].patlen) != 0) {
                            /* inside loop */
                            continue;
                        }
                        if (bitarray[(lower_pid) / 8] & (1 << ((lower_pid) % 8))) {
                            ;
                        } else {
                            bitarray[(lower_pid) / 8] |= (1 << ((lower_pid) % 8));
                            MpmAddPid(pmq, lower_pid);
                        }
                        matches++;
                    } else {
                        if (bitarray[pids[k] / 8] & (1 << (pids[k] % 8))) {
                            ;
                        } else {
                            bitarray[pids[k] / 8] |= (1 << (pids[k] % 8));
                            MpmAddPid(pmq, pids[k]);
                        }
                        matches++;
                    }
                    //loop1:
                    //;
                }
            }
        } /* for (i = 0; i < buflen; i++) */

    } else {
        register SC_AC_STATE_TYPE_U32 state = 0;
        SC_AC_STATE_TYPE_U32 (*state_table_u32)[256] = ctx->state_table_u32;
        for (i = 0; i < buflen; i++) {
            state = state_table_u32[state & 0x00FFFFFF][u8_tolower(buf[i])];
            if (state & 0xFF000000) {
                uint32_t no_of_entries = ctx->output_table[state & 0x00FFFFFF].no_of_entries;
                uint32_t *pids = ctx->output_table[state & 0x00FFFFFF].pids;
                uint32_t k;
                for (k = 0; k < no_of_entries; k++) {
                    if (pids[k] & AC_CASE_MASK) {
                        uint32_t lower_pid = pids[k] & 0x0000FFFF;
                        if (SCMemcmp(pid_pat_list[lower_pid].cs,
                                     buf + i - pid_pat_list[lower_pid].patlen + 1,
                                     pid_pat_list[lower_pid].patlen) != 0) {
                            /* inside loop */
                            continue;
                        }
                        if (bitarray[(lower_pid) / 8] & (1 << ((lower_pid) % 8))) {
                            ;
                        } else {
                            bitarray[(lower_pid) / 8] |= (1 << ((lower_pid) % 8));
                            MpmAddPid(pmq, lower_pid);
                        }
                        matches++;
                    } else {
                        if (bitarray[pids[k] / 8] & (1 << (pids[k] % 8))) {
                            ;
                        } else {
                            bitarray[pids[k] / 8] |= (1 << (pids[k] % 8));
                            MpmAddPid(pmq, pids[k]);
                        }
                        matches++;
                    }
                    //loop1:
                    //;
                }
            }
        } /* for (i = 0; i < buflen; i++) */
    }

    return matches;
}

/**
 * \brief Add a case sensitive pattern.  Although we have different calls for
 *        adding case sensitive and insensitive patterns, we make a single call
 *        for either case.  No special treatment for either case.
 *
 * \param mpm_ctx Pointer to the mpm context.
 * \param pat     The pattern to add.
 * \param patnen  The pattern length.
 * \param offset  Ignored.
 * \param depth   Ignored.
 * \param pid     The pattern id.
 * \param sid     Ignored.
 * \param flags   Flags associated with this pattern.
 *
 * \retval  0 On success.
 * \retval -1 On failure.
 */
int SCACAddPatternCS(MpmCtx *mpm_ctx, uint8_t *pat, uint16_t patlen,
                     uint16_t offset, uint16_t depth, uint32_t pid,
                     SigIntId sid, uint8_t flags)
{
    return MpmAddPattern(mpm_ctx, pat, patlen, offset, depth, pid, sid, flags);
}

