#ifndef __AC_H__
#define __AC_H__

#include<stdint.h>
#include<stddef.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>

#define SigIntId uint32_t
#define SCFree free
#define SCReturnInt(x)  return x
#define SCReturn     return
#define SCEnter(...)
#define SCMalloc malloc
#define SCRealloc realloc
#define SCCalloc calloc

#define unlikely(expr) __builtin_expect(!!(expr), 0)
#define BUG_ON(x)
#define u8_tolower(c) tolower((uint8_t)(c))


#include"util-mpm.h"
#include"util-mpm-ac.h"


/**
 * \internal
 * \brief Does a memcpy of the input string to lowercase.
 *
 * \param d   Pointer to the target area for memcpy.
 * \param s   Pointer to the src string for memcpy.
 * \param len len of the string sent in s.
 */
static inline void memcpy_tolower(uint8_t *d, const uint8_t *s, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++)
        d[i] = u8_tolower(s[i]);

    return;
}

#define SCMemcmp(a,b,c) ({ \
    memcmp((a), (b), (c)) ? 1 : 0; \
})


#endif

