
#ifndef __UTIL_MPM_AC__H__
#define __UTIL_MPM_AC__H__

#include"ac.h"


#define SC_AC_STATE_TYPE_U16 uint16_t
#define SC_AC_STATE_TYPE_U32 uint32_t


typedef struct SCACPatternList_ {
    uint8_t *cs;
    uint16_t patlen;

    /* sid(s) for this pattern */
    uint32_t sids_size;
    SigIntId *sids;
} SCACPatternList;

typedef struct SCACOutputTable_ {
    /* list of pattern sids */
    uint32_t *pids;
    /* no of entries we have in pids */
    uint32_t no_of_entries;
} SCACOutputTable;

typedef struct SCACCtx_ {
    /* pattern arrays.  We need this only during the goto table creation phase */
    MpmPattern **parray;

    /* no of states used by ac */
    uint32_t state_count;

    uint32_t pattern_id_bitarray_size;

    /* the all important memory hungry state_table */
    SC_AC_STATE_TYPE_U16 (*state_table_u16)[256];
    /* the all important memory hungry state_table */
    SC_AC_STATE_TYPE_U32 (*state_table_u32)[256];

    /* goto_table, failure table and output table.  Needed to create state_table.
     * Will be freed, once we have created the state_table */
    int32_t (*goto_table)[256];
    int32_t *failure_table;
    SCACOutputTable *output_table;
    SCACPatternList *pid_pat_list;

    /* the size of each state */
    uint32_t single_state_size;

    uint32_t allocated_state_count;

} SCACCtx;

void SCACInitCtx(MpmCtx *);
void SCACDestroyCtx(MpmCtx *);
int SCACAddPatternCS(MpmCtx *, uint8_t *, uint16_t, uint16_t, uint16_t,uint32_t, SigIntId, uint8_t);
int SCACPreparePatterns(MpmCtx *mpm_ctx);
uint32_t SCACSearch(const MpmCtx *mpm_ctx, PatternMatcherQueue *pmq, const uint8_t *buf, uint16_t buflen);


#endif /* __UTIL_MPM_AC__H__ */
