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
 */

#ifndef __FwHashList_H__
#define __FwHashList_H__

#include<stdint.h>

#define SCMalloc malloc
#define SCFree	free
#define TYPE_IP 	0
#define TYPE_NET	1

/* hash bucket ip structure (sip,dip,dport) xxx.xxx.xxx.xxx#xxx.xxx.xxx.xxx#xxxxx*/
typedef struct FwHashListTableBucketIp_ {
    uint8_t data[64];
    uint16_t dsize;
	uint16_t falsecnt;
    struct FwHashListTableBucketIp_ *bucknext;
    struct FwHashListTableBucketIp_ *listnext;
    struct FwHashListTableBucketIp_ *listprev;
} FwHashListTableBucketIp;

/* hash bucket net structure (sip(net,255.255.255.0),dport) xxx.xxx.xxx.0/24#xxxxx*/
typedef struct FwHashListTableBucketNet_ {
    uint8_t data[64];
    uint16_t dsize;
	uint16_t falsecnt;
    struct FwHashListTableBucketNet_ *bucknext;
    struct FwHashListTableBucketNet_ *listnext;
    struct FwHashListTableBucketNet_ *listprev;
} FwHashListTableBucketNet;


/* hash table structure */
typedef struct FwHashListTable_ {
    FwHashListTableBucketIp **iparray;
	FwHashListTableBucketNet **netarray;
    FwHashListTableBucketIp *iplisthead;
	FwHashListTableBucketIp *iplisttail;
    FwHashListTableBucketNet *netlisthead;
	FwHashListTableBucketNet *netlisttail;
    uint32_t iparray_size;
	uint32_t netarray_size;
    uint32_t (*Hash)(struct FwHashListTable_ *, void *, uint16_t,uint8_t);
    char (*Compare)(void *, uint16_t, void *, uint16_t);
    void (*Free)(void *);
} FwHashListTable;

#define FwHashList_NO_SIZE 0

/* prototypes */
FwHashListTable* FwHashListTableInit(uint32_t,uint32_t, uint32_t (*Hash)(struct FwHashListTable_ *, void *, uint16_t,uint8_t), char (*Compare)(void *, uint16_t, void *, uint16_t), void (*Free)(void *));
void FwHashListTableFree(FwHashListTable *);
//void FwHashListTablePrint(FwHashListTable *);
int FwHashListTableAddIp(FwHashListTable *, void *, uint16_t);
int FwHashListTableAddNet(FwHashListTable *, void *, uint16_t);
//int FwHashListTableRemove(FwHashListTable *, void *, uint16_t);
void *FwHashListTableLookupIp(FwHashListTable *, void *, uint16_t);
void *FwHashListTableLookupNet(FwHashListTable *, void *, uint16_t);
uint32_t FwHashListTableGenericHash(FwHashListTable *, void *, uint16_t,uint8_t);
//FwHashListTableBucket *FwHashListTableGetListHead(FwHashListTable *);
//#define FwHashListTableGetListNext(hb) (hb)->listnext
//#define FwHashListTableGetListData(hb) (hb)->data
char FwHashListTableDefaultCompare(void *, uint16_t, void *, uint16_t);

//void FwHashListTableRegisterTests(void);

#endif /* __FwHashList_H__ */

