/* Copyright (C) 2007-2010 Open Information Security Foundation
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * \file
 *
 * \author Victor Julien <victor@inliniac.net>
 *
 * Chained hash table implementation
 *
 * The 'Free' pointer can be used to have the API free your
 * hashed data. If it's NULL it's the callers responsebility
 */


#include "util-fwhashlist.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

FwHashListTable* FwHashListTableInit(uint32_t ipsize,uint32_t netsize, uint32_t (*Hash)(struct FwHashListTable_ *, void *, uint16_t,uint8_t), char (*Compare)(void *, uint16_t, void *, uint16_t), void (*Free)(void *)) {

    FwHashListTable *ht = NULL;

    if (ipsize == 0 || netsize == 0) {
        goto error;
    }

    if (Hash == NULL) {
        //printf("ERROR: FwHashListTableInit no Hash function\n");
        goto error;
    }

    /* setup the filter */
    ht = SCMalloc(sizeof(FwHashListTable));
    //if (unlikely(ht == NULL))
	if (ht == NULL)
    goto error;
    memset(ht,0,sizeof(FwHashListTable));
    ht->iparray_size = ipsize;
	ht->netarray_size = netsize;
    ht->Hash = Hash;
    ht->Free = Free;

    if (Compare != NULL)
        ht->Compare = Compare;
    else
        ht->Compare = FwHashListTableDefaultCompare;

    /* setup the bitarray */
    ht->iparray = SCMalloc(ht->iparray_size * sizeof(FwHashListTableBucketIp *));
    if (ht->iparray == NULL)
        goto error;
    memset(ht->iparray,0,ht->iparray_size * sizeof(FwHashListTableBucketIp *));

	ht->netarray = SCMalloc(ht->netarray_size * sizeof(FwHashListTableBucketNet *));
    if (ht->netarray == NULL)
        goto error;
    memset(ht->netarray,0,ht->netarray_size * sizeof(FwHashListTableBucketNet *));

    ht->iplisthead = NULL;
    ht->iplisttail = NULL;
	ht->netlisthead = NULL;
    ht->netlisttail = NULL;
	
    return ht;

error:
    if (ht != NULL) {
        if (ht->iparray != NULL)
            SCFree(ht->iparray);
		if (ht->netarray != NULL)
			SCFree(ht->netarray);

        SCFree(ht);
    }
    return NULL;
}

void FwHashListTableFree(FwHashListTable *ht)
{
    uint32_t i = 0;

    if (ht == NULL)
        return;

    /* free the ip buckets */
    for (i = 0; i < ht->iparray_size; i++) {
        FwHashListTableBucketIp *iphashbucket = ht->iparray[i];
        while (iphashbucket != NULL) {
            FwHashListTableBucketIp *next_iphashbucket = iphashbucket->bucknext;
            SCFree(iphashbucket);
            iphashbucket = next_iphashbucket;
        }
    }

	/* free the net buckets */
    for (i = 0; i < ht->netarray_size; i++) {
        FwHashListTableBucketNet *nethashbucket = ht->netarray[i];
        while (nethashbucket != NULL) {
            FwHashListTableBucketNet *next_nethashbucket = nethashbucket->bucknext;
            SCFree(nethashbucket);
            nethashbucket = next_nethashbucket;
        }
    }

    /* free the iparray */
    if (ht->iparray != NULL)
        SCFree(ht->iparray);

	/* free the netarray */
    if (ht->netarray != NULL)
        SCFree(ht->netarray);

    SCFree(ht);
}


uint32_t FwHashListTableGenericHash(FwHashListTable *ht, void *data, uint16_t datalen,uint8_t type)
{
     uint8_t *d = (uint8_t *)data;
     uint32_t i;
     uint32_t hash = 0;

     for (i = 0; i < datalen; i++) {
         if (i == 0)      hash += (((uint32_t)*d++));
         else if (i == 1) hash += (((uint32_t)*d++) * datalen);
         else             hash *= (((uint32_t)*d++) * i) + datalen + i;
     }

     hash *= datalen;
	 if(TYPE_IP == type)
     	hash %= ht->iparray_size;		/*iparray*/
	 if(TYPE_NET == type)
	 	hash %= ht->netarray_size;		/*netarray*/

     return hash;
}

char FwHashListTableDefaultCompare(void *data1, uint16_t len1, void *data2, uint16_t len2)
{
    if (len1 != len2)
        return 0;

	if(strncmp(data1,data2,len1) != 0)
		return 0;
	return 1;
    //if (SCMemcmp(data1,data2,len1) != 0)
	//if (SCMemcmp(data1,data2,len1) != 0)
    //    return 0;
}


/*add iparray node*/
int FwHashListTableAddIp(FwHashListTable *ht, void *data, uint16_t datalen)
{
    if (ht == NULL || data == NULL)
        return -1;

    uint32_t hash = ht->Hash(ht, data, datalen,TYPE_IP);

    //SCLogDebug("ht %p hash %"PRIu32"", ht, hash);

    FwHashListTableBucketIp *hb = SCMalloc(sizeof(FwHashListTableBucketIp));
    //if (unlikely(hb == NULL))
    if(hb == NULL)
        goto error;
    memset(hb, 0, sizeof(FwHashListTableBucketIp));
    strncpy(hb->data,data,datalen);
    hb->dsize = datalen;
    hb->bucknext = NULL;
    hb->listnext = NULL;
    hb->listprev = NULL;

    if (ht->iparray[hash] == NULL) {
        ht->iparray[hash] = hb;
    } else {
        hb->bucknext = ht->iparray[hash];
        ht->iparray[hash] = hb;
    }

    if (ht->iplisttail == NULL) {
        ht->iplisthead = hb;
        ht->iplisttail = hb;
    } else {
        hb->listprev = ht->iplisttail;
        ht->iplisttail->listnext = hb;
        ht->iplisttail = hb;
    }

    return 0;

error:
    return -1;
}


/*add netarray node*/
int FwHashListTableAddNet(FwHashListTable *ht, void *data, uint16_t datalen)
{
    if (ht == NULL || data == NULL)
        return -1;

    uint32_t hash = ht->Hash(ht, data, datalen,TYPE_NET);

    //SCLogDebug("ht %p hash %"PRIu32"", ht, hash);

    FwHashListTableBucketNet *hb = SCMalloc(sizeof(FwHashListTableBucketNet));
    //if (unlikely(hb == NULL))
    if(hb == NULL)
        goto error;
    memset(hb, 0, sizeof(FwHashListTableBucketNet));
    strncpy(hb->data,data,datalen);
    hb->dsize = datalen;
    hb->bucknext = NULL;
    hb->listnext = NULL;
    hb->listprev = NULL;

    if (ht->netarray[hash] == NULL) {
        ht->netarray[hash] = hb;
    } else {
        hb->bucknext = ht->netarray[hash];
        ht->netarray[hash] = hb;
    }

    if (ht->netlisttail == NULL) {
        ht->netlisthead = hb;
        ht->netlisttail = hb;
    } else {
        hb->listprev = ht->netlisttail;
        ht->netlisttail->listnext = hb;
        ht->netlisttail = hb;
    }

    return 0;

error:
    return -1;
}


/*look up ipnode from iparray*/
void *FwHashListTableLookupIp(FwHashListTable *ht, void *data, uint16_t datalen)
{

    if (ht == NULL) {
        //SCLogDebug("Hash List table is NULL");
        return NULL;
    }

    uint32_t hash = ht->Hash(ht, data, datalen,TYPE_IP);

    if (ht->iparray[hash] == NULL) {
        return NULL;
    }

    FwHashListTableBucketIp *hashbucket = ht->iparray[hash];
    do {
        if (ht->Compare(hashbucket->data,hashbucket->dsize,data,datalen) == 1)
            return hashbucket;

        hashbucket = hashbucket->bucknext;
    } while (hashbucket != NULL);

    return NULL;
}


/*look up netnode from netarray*/
void *FwHashListTableLookupNet(FwHashListTable *ht, void *data, uint16_t datalen)
{

    if (ht == NULL) {
        //SCLogDebug("Hash List table is NULL");
        return NULL;
    }

    uint32_t hash = ht->Hash(ht, data, datalen,TYPE_NET);

    if (ht->netarray[hash] == NULL) {
        return NULL;
    }

    FwHashListTableBucketNet *hashbucket = ht->netarray[hash];
    do {
        if (ht->Compare(hashbucket->data,hashbucket->dsize,data,datalen) == 1)
            return hashbucket;

        hashbucket = hashbucket->bucknext;
    } while (hashbucket != NULL);

    return NULL;
}


#if 0
void FwHashListTablePrint(FwHashListTable *ht)
{
    printf("\n----------- Hash Table Stats ------------\n");
    printf("Buckets:               %" PRIu32 "\n", ht->array_size);
    printf("Hash function pointer: %p\n", ht->Hash);
    printf("-----------------------------------------\n");
}


int FwHashListTableRemove(FwHashListTable *ht, void *data, uint16_t datalen)
{
    uint32_t hash = ht->Hash(ht, data, datalen);

    SCLogDebug("ht %p hash %"PRIu32"", ht, hash);

    if (ht->array[hash] == NULL) {
        SCLogDebug("ht->array[hash] NULL");
        return -1;
    }

    /* fast track for just one data part */
    if (ht->array[hash]->bucknext == NULL) {
        FwHashListTableBucket *hb = ht->array[hash];

        if (ht->Compare(hb->data,hb->size,data,datalen) == 1) {
            /* remove from the list */
            if (hb->listprev == NULL) {
                ht->listhead = hb->listnext;
            } else {
                hb->listprev->listnext = hb->listnext;
            }
            if (hb->listnext == NULL) {
                ht->listtail = hb->listprev;
            } else {
                hb->listnext->listprev = hb->listprev;
            }

            if (ht->Free != NULL)
                ht->Free(hb->data);

            SCFree(ht->array[hash]);
            ht->array[hash] = NULL;
            return 0;
        }

        SCLogDebug("fast track default case");
        return -1;
    }

    /* more data in this bucket */
    FwHashListTableBucket *hashbucket = ht->array[hash], *prev_hashbucket = NULL;
    do {
        if (ht->Compare(hashbucket->data,hashbucket->size,data,datalen) == 1) {

            /* remove from the list */
            if (hashbucket->listprev == NULL) {
                ht->listhead = hashbucket->listnext;
            } else {
                hashbucket->listprev->listnext = hashbucket->listnext;
            }
            if (hashbucket->listnext == NULL) {
                ht->listtail = hashbucket->listprev;
            } else {
                hashbucket->listnext->listprev = hashbucket->listprev;
            }

            if (prev_hashbucket == NULL) {
                /* root bucket */
                ht->array[hash] = hashbucket->bucknext;
            } else {
                /* child bucket */
                prev_hashbucket->bucknext = hashbucket->bucknext;
            }

            /* remove this */
            if (ht->Free != NULL)
                ht->Free(hashbucket->data);
            SCFree(hashbucket);
            return 0;
        }

        prev_hashbucket = hashbucket;
        hashbucket = hashbucket->bucknext;
    } while (hashbucket != NULL);

    SCLogDebug("slow track default case");
    return -1;
}



void *FwHashListTableLookup(FwHashListTable *ht, void *data, uint16_t datalen)
{

    if (ht == NULL) {
        SCLogDebug("Hash List table is NULL");
        return NULL;
    }

    uint32_t hash = ht->Hash(ht, data, datalen);

    if (ht->array[hash] == NULL) {
        return NULL;
    }

    FwHashListTableBucket *hashbucket = ht->array[hash];
    do {
        if (ht->Compare(hashbucket->data,hashbucket->size,data,datalen) == 1)
            return hashbucket->data;

        hashbucket = hashbucket->bucknext;
    } while (hashbucket != NULL);

    return NULL;
}

#endif
