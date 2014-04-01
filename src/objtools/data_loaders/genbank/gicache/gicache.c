/*****************************************************************************
* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*  Authors: Ilya Dondoshansky, Michael Kimelman
*
* ===========================================================================
*
*  gicache.c
*
*****************************************************************************/

#include "gicache.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <poll.h>
#include "ncbi_toolkit.h"

/****************************************************************************
 *
 * gi_data_index.h 
 *
 ****************************************************************************/

#ifdef TEST_RUN
#  ifdef NDEBUG
#    undef NDEBUG
#  endif
#endif

#define MAX_ACCESSION_LENGTH 64

#define kPageSize            8
#define kPageMask            0xff

#define kLocalOffsetMask     0x0000ffff
#define kFullOffsetMask      0x7fffffff
#define kTopBit              0x80000000
#define kUint4Mask           0xffffffff
#define kTopPageMask         0x0000007f

#define INDEX_CACHE_SIZE     32768
#define DATA_CACHE_SIZE      8192

// Each top-level page correponds to a range of 2^25 gis (because 2 gis are
// packed in each slot on leaf pages). Since gi is a signed integer, there are
// a total of 2^31/2^25 = 2^6 = 64 top level pages.
// The offset header contains 8-byte starting offsets for each top-level page,
// but size is measured in 4-byte units of the m_GiIndex array, so it's
// sufficient to have header size = 64 * 8 / 4 = 128.
// However page size for the index file is 256, and it's inconvenient to use
// fraction of a page for the header. Hence use one whole page.
#define kOffsetHeaderSize    256

// Given a Uint4* p
#define GET_INT8(p) (((Int8)(*(p) & kUint4Mask))<<32) | ((Int8)(*((p)+1)))

// If offset for top-level page corresponding to a given gi is not set ( = 0),
// go back to previous top-level pages until a non-zero offset is found.
#define GET_TOP_PAGE_OFFSET(gi) GET_INT8(&data_index->m_GiIndex[2*((((Uint4)gi)>>25)&kTopPageMask)])


#define SET_TOP_PAGE_OFFSET(gi,off) \
    data_index->m_GiIndex[2*((((Uint4)gi)>>25)&kTopPageMask)] = (Uint4)((off)>>32);\
    data_index->m_GiIndex[2*((((Uint4)gi)>>25)&kTopPageMask)+1] = (Uint4)((off)&kUint4Mask);

typedef struct {
    Uint4*  m_GiIndex;
    char*   m_Data;
    int     m_GiIndexFile;
    int     m_DataFile;
    Uint4   m_GiIndexLen;
    Int8    m_DataLen;
    Int8    m_MappedDataLen;
    Uint4   m_MappedIndexLen;
    Uint1   m_ReadOnlyMode;
    Uint4   m_IndexCacheLen;
    Uint4   m_DataCacheLen;
    char    m_FileNamePrefix[256];
    Uint4   m_DataUnitSize;
    Uint4   m_IndexCacheSize;
    Uint4*  m_IndexCache;
    Uint4   m_DataCacheSize;
    char*   m_DataCache;
    Uint1   m_SequentialData;
    Uint1   m_FreeOnDrop;
    volatile int m_Remapping; /* Count of threads currently trying to remap */
    volatile Uint1 m_NeedRemap; /* Is remap needed? */
    Uint1   m_RemapOnRead; /* Is remap allowed when reading data? */
    Uint4   m_OffsetHeaderSize; /* 0 for 32-bit version, 256 for 64-bit */
    char*   m_OldDataPtr;
    Int8    m_OldMappedDataLen;
    ino_t   m_IndexInode;
    ino_t   m_DataInode;
} SGiDataIndex;

static void (*LogFunc)(char*) = NULL;

/****************************************************************************
 *
 * gi_data_index.c
 *
 ****************************************************************************/

static void x_DumpIndexCache(SGiDataIndex* data_index)
{
    int bytes_written = 0;
    if (data_index->m_GiIndexFile >= 0 && data_index->m_IndexCacheLen > 0) {
        assert(data_index->m_GiIndexLen*sizeof(Uint4) ==
               lseek(data_index->m_GiIndexFile, 0, SEEK_CUR));
        assert(data_index->m_GiIndexLen*sizeof(Uint4) ==
               lseek(data_index->m_GiIndexFile, 0, SEEK_END));
        /* Write to the index file whatever is still left in cache. */
        bytes_written =
            write(data_index->m_GiIndexFile, data_index->m_IndexCache,
                  data_index->m_IndexCacheLen*sizeof(int));
        assert(bytes_written == data_index->m_IndexCacheLen*sizeof(int));
        data_index->m_GiIndexLen += data_index->m_IndexCacheLen;
        assert(data_index->m_GiIndexLen*sizeof(Uint4) ==
               lseek(data_index->m_GiIndexFile, 0, SEEK_CUR));
        assert(data_index->m_GiIndexLen*sizeof(Uint4) ==
               lseek(data_index->m_GiIndexFile, 0, SEEK_END));
        data_index->m_IndexCacheLen = 0;
    }
}

static void x_DumpDataCache(SGiDataIndex* data_index)
{
    int bytes_written = 0;
    if (data_index->m_DataFile >= 0 && data_index->m_DataCacheLen > 0) {
        assert(data_index->m_DataLen ==
               lseek(data_index->m_DataFile, 0, SEEK_CUR));
        assert(data_index->m_DataLen ==
               lseek(data_index->m_DataFile, 0, SEEK_END));
        /* Write to the data file whatever is still left in cache. */
        bytes_written = 
            write(data_index->m_DataFile, data_index->m_DataCache,
                  data_index->m_DataCacheLen);
        assert(bytes_written == data_index->m_DataCacheLen);
        data_index->m_DataLen += data_index->m_DataCacheLen;
        assert(data_index->m_DataLen == 
               lseek(data_index->m_DataFile, 0, SEEK_CUR));
        assert(data_index->m_DataLen == 
               lseek(data_index->m_DataFile, 0, SEEK_END));
        data_index->m_DataCacheLen = 0;
    }
}

static void x_CloseIndexFile(SGiDataIndex* data_index)
{
    if (data_index->m_GiIndexFile >= 0) {
        close(data_index->m_GiIndexFile);
        data_index->m_GiIndexFile = -1;
        data_index->m_GiIndexLen = 0;
        data_index->m_MappedIndexLen = 0;
    }
}

static void x_CloseDataFile(SGiDataIndex* data_index)
{
    if (data_index->m_DataFile >= 0) {
        close(data_index->m_DataFile);
        data_index->m_DataFile = -1;
        data_index->m_DataLen = 0;
        data_index->m_MappedDataLen = 0;
    }
}

/* Closes all files */
static void x_CloseFiles(SGiDataIndex* data_index)
{
    x_CloseIndexFile(data_index);
    x_CloseDataFile(data_index);
}

static void x_UnMapIndex(SGiDataIndex* data_index)
{
    if (data_index->m_GiIndex != MAP_FAILED) {
        if (LogFunc) {
            char logmsg[256];
            sprintf(logmsg,
                    "GI_CACHE: Unmapping index file, filedes %d, map length %ld, path %s\n",
                    data_index->m_GiIndexFile, data_index->m_MappedIndexLen,
                    data_index->m_FileNamePrefix);
            LogFunc(logmsg);
        }
        munmap((char*)data_index->m_GiIndex,
               data_index->m_MappedIndexLen*sizeof(Uint4));
        data_index->m_GiIndex = (Uint4*)MAP_FAILED;
        data_index->m_MappedIndexLen = 0;
    }
}

static void x_UnMapData(SGiDataIndex* data_index)
{
    if (data_index->m_Data != MAP_FAILED) {
        if (LogFunc) {
            char logmsg[256];
            sprintf(logmsg,
                    "GI_CACHE: Unmapping data file, filedes %d, map length %ld, path %s\n",
                    data_index->m_DataFile, data_index->m_MappedDataLen,
                    data_index->m_FileNamePrefix);
            LogFunc(logmsg);
        }
        data_index->m_OldDataPtr = data_index->m_Data;
        data_index->m_OldMappedDataLen = data_index->m_MappedDataLen;
        munmap((char*)data_index->m_Data, data_index->m_MappedDataLen);
        data_index->m_Data = (char*)MAP_FAILED;
        data_index->m_MappedDataLen = 0;
    }
}

/* Unmaps data and index files */
static void x_UnMap(SGiDataIndex* data_index)
{
    x_UnMapIndex(data_index);
    x_UnMapData(data_index);
}

static Uint1 x_OpenIndexFile(SGiDataIndex* data_index)
{
    char buf[256];
    int flags;

    if (data_index->m_GiIndexFile >= 0)
        return 1;
    
    flags = (data_index->m_ReadOnlyMode?O_RDONLY:O_RDWR|O_APPEND|O_CREAT);
    
    x_UnMapIndex(data_index);
    
    strcpy(buf, data_index->m_FileNamePrefix);

    strcat(buf, "idx");
    data_index->m_GiIndexFile = open(buf,flags,0644);
    data_index->m_GiIndexLen = 
        (data_index->m_GiIndexFile >= 0 ? 
         lseek(data_index->m_GiIndexFile, 0, SEEK_END)/sizeof(Uint4) : 0);

    /* Save inode number */
    struct stat stat_buf;
    fstat(data_index->m_GiIndexFile, &stat_buf);
    data_index->m_IndexInode = stat_buf.st_ino;

    if (LogFunc) {
        char logmsg[256];
        sprintf(logmsg,
                "GI_CACHE: Opened index file %s; filedes %d, inode %d, length %ld, path %s\n",
                buf, data_index->m_GiIndexFile, data_index->m_IndexInode,
                data_index->m_GiIndexLen, data_index->m_FileNamePrefix);
        LogFunc(logmsg);
    }

    if (data_index->m_GiIndexLen == 0 && !data_index->m_ReadOnlyMode &&
        data_index->m_GiIndexFile) {
        Uint4* b;
        int bytes_written = 0;
        /* For 64-bit version only, the first page of the index is reserved to
           store 8-byte starting offsets for data corresponding to the 64 top
           level pages.
           The next (first for 32-bit version, second for 64-bit) page of the
           index is reserved for the pointers to other pages.
        */
        data_index->m_GiIndexLen = data_index->m_OffsetHeaderSize + (1<<kPageSize);
        
        b = (Uint4*) calloc(data_index->m_GiIndexLen, sizeof(Uint4));
        assert(0 == lseek(data_index->m_GiIndexFile, 0, SEEK_END));
        bytes_written = write(data_index->m_GiIndexFile, b,
                              data_index->m_GiIndexLen*sizeof(Uint4));
        assert(bytes_written == data_index->m_GiIndexLen*sizeof(Uint4));
        free(b);
        assert(data_index->m_GiIndexLen*sizeof(Uint4) ==
               lseek(data_index->m_GiIndexFile, 0, SEEK_CUR));
        assert(data_index->m_GiIndexLen*sizeof(Uint4) ==
               lseek(data_index->m_GiIndexFile, 0, SEEK_END));
    }
    
    return (data_index->m_GiIndexFile >= 0);
}

/* Opens data file, including check if they are already open. */
static Uint1 x_OpenDataFile(SGiDataIndex* data_index)
{
    char buf[256];
    int flags;

    if (data_index->m_DataFile >= 0)
        return 1;
    
    flags = (data_index->m_ReadOnlyMode?O_RDONLY:O_RDWR|O_APPEND|O_CREAT);
    
    strcpy(buf, data_index->m_FileNamePrefix);
    strcat(buf, "dat");
    data_index->m_DataFile = open(buf,flags,0644);
    data_index->m_DataLen = (data_index->m_DataFile >= 0 ? 
                             lseek(data_index->m_DataFile, 0, SEEK_END) : 0);
    /* Save inode number */
    struct stat stat_buf;
    fstat(data_index->m_DataFile, &stat_buf);
    data_index->m_DataInode = stat_buf.st_ino;

    if (LogFunc) {
        char logmsg[256];
        sprintf(logmsg,
                "GI_CACHE: Opened data file %s; filedes %d, inode %d, length %ld, path %s\n",
                buf, data_index->m_DataFile, data_index->m_DataInode,
                data_index->m_DataLen, data_index->m_FileNamePrefix);
        LogFunc(logmsg);
    }

    if (data_index->m_DataLen == 0 && !data_index->m_ReadOnlyMode &&
        data_index->m_DataFile) {
        /* Fill first 2*sizeof(int) bytes with 0: this guarantees that all 
         * offsets into data file will be > 0, allowing 0 to mean absense of
         * data. There reason to write 2 integers can be helpful in case of
         * binary data, making sure that data is aligned.
         */
        int  b[2];
        int bytes_written = 0;
        memset(b, 0, sizeof(b));
        assert(0 == lseek(data_index->m_DataFile, 0, SEEK_END));
        bytes_written = write(data_index->m_DataFile, b, sizeof(b));
        assert(bytes_written == sizeof(b));
        data_index->m_DataLen = sizeof(b);
        assert(data_index->m_DataLen ==
               lseek(data_index->m_DataFile, 0, SEEK_CUR));
        assert(data_index->m_DataLen ==
               lseek(data_index->m_DataFile, 0, SEEK_END));
    }
    
    return (data_index->m_DataFile >= 0);
}

/* hack needed for SGI IRIX where MAP_NORESERVE isn't defined. Dima */
#ifndef MAP_NORESERVE
#define MAP_NORESERVE (0)
#endif

static Uint1 x_MapIndex(SGiDataIndex* data_index)
{
    int prot;
    Uint4 map_size;

    if (!x_OpenIndexFile(data_index)) return 0;

    if (data_index->m_GiIndex != MAP_FAILED) return 1;
    
    if (data_index->m_ReadOnlyMode) {
        map_size = lseek(data_index->m_GiIndexFile, 0, SEEK_END);
        data_index->m_GiIndexLen = map_size / sizeof(Uint4);
        prot = PROT_READ;
    } else {
        /* If there's anything in local memory cache, write it to disk. */
        x_DumpIndexCache(data_index);
        map_size = data_index->m_GiIndexLen*sizeof(Uint4);
        assert(map_size == lseek(data_index->m_GiIndexFile, 0, SEEK_CUR));
        assert(map_size == lseek(data_index->m_GiIndexFile, 0, SEEK_END));
        prot = PROT_READ | PROT_WRITE;
    }
    data_index->m_GiIndex =
        (Uint4*)mmap(0, map_size, prot, MAP_SHARED|MAP_NORESERVE, 
                     data_index->m_GiIndexFile, 0);
    data_index->m_MappedIndexLen = data_index->m_GiIndexLen;

    if (LogFunc) {
        char logmsg[256];
        sprintf(logmsg,
                "GI_CACHE: Memory mapped index file, filedes %d, map length %ld, path %s\n",
                data_index->m_GiIndexFile, data_index->m_MappedIndexLen,
                data_index->m_FileNamePrefix);
        LogFunc(logmsg);
    }

    return (data_index->m_GiIndex != MAP_FAILED);
}

#define MAP_EXTRA_DATA_LEN 1000000

static Uint1 x_MapData(SGiDataIndex* data_index)
{
    if (!x_OpenDataFile(data_index)) return 0;

    if (data_index->m_Data == MAP_FAILED) {
        int prot;
        Int8 map_size;

        if (data_index->m_ReadOnlyMode) {
            data_index->m_DataLen = lseek(data_index->m_DataFile, 0, SEEK_END);
            map_size = 
                (data_index->m_DataLen < data_index->m_OldMappedDataLen ?
                 data_index->m_OldMappedDataLen :
                 data_index->m_DataLen + MAP_EXTRA_DATA_LEN);
            prot = PROT_READ;
        } else {
            /* If there's anything in local memory cache, write it to disk. */
            x_DumpDataCache(data_index);
            map_size = data_index->m_DataLen;
            assert(map_size == lseek(data_index->m_DataFile, 0, SEEK_CUR));
            assert(map_size == lseek(data_index->m_DataFile, 0, SEEK_END));
            prot = PROT_READ | PROT_WRITE;
        }
        
        data_index->m_Data =
            (char*) mmap(data_index->m_OldDataPtr, map_size, prot,
                         MAP_SHARED|MAP_NORESERVE, data_index->m_DataFile, 0);
        data_index->m_MappedDataLen = map_size;

        if (LogFunc) {
            char logmsg[256];
            sprintf(logmsg,
                    "GI_CACHE: Memory mapped data file, filedes %d, map length %ld, path %s\n",
                    data_index->m_DataFile, data_index->m_MappedDataLen,
                    data_index->m_FileNamePrefix);
            LogFunc(logmsg);
        }
    }

    return (data_index->m_Data != MAP_FAILED);
}

static void x_FlushData(SGiDataIndex* data_index)
{
    if (data_index->m_DataFile >= 0 && !data_index->m_ReadOnlyMode) {
        if (data_index->m_DataCacheLen > 0)
            x_DumpDataCache(data_index);
        /* Synchronize memory mapped data with the file on disk. */
        msync(data_index->m_Data, data_index->m_DataLen, MS_SYNC);
    }
}

static void x_FlushIndex(SGiDataIndex* data_index)
{
    /* Flush index file */
    if (data_index->m_GiIndexFile >= 0 && !data_index->m_ReadOnlyMode) {
        if (data_index->m_IndexCacheLen > 0)
            x_DumpIndexCache(data_index);
        /* Synchronize memory mapped data with the file on disk. */
        msync((char*)data_index->m_GiIndex, data_index->m_GiIndexLen, MS_SYNC);
    }
}

static void x_Flush(SGiDataIndex* data_index)
{
    x_FlushIndex(data_index);
    x_FlushData(data_index);
}

static Uint1 x_ReMapIndex(SGiDataIndex* data_index)
{
    x_FlushIndex(data_index);
    x_UnMapIndex(data_index);
    if (!x_MapIndex(data_index)) return 0;
    
    return 1;
}

static Uint1 x_ReMapData(SGiDataIndex* data_index)
{
    x_FlushData(data_index);
    x_UnMapData(data_index);
    if (!x_MapData(data_index)) return 0;
    
    return 1;
}

static Uint1 GiDataIndex_ReMap(SGiDataIndex* data_index, int delay)
{
    /* If some other thread has already done the remapping or is in the process
       of doing it, there is nothing to do here. */
    if (!data_index->m_NeedRemap || data_index->m_Remapping)
        return 1;

    ++data_index->m_Remapping;

    /* Wait a little bit and check if some other thread has started doing the
       remapping. In that case let the other thread do it. */
    poll(NULL, 0, delay);

    if (data_index->m_Remapping > 1) {
        data_index->m_Remapping--;
        return 0;
    }

    assert(data_index->m_Remapping == 1);

    /* In read-only mode, check if inode numbers for underlying files have
     * changed. If so, close and reopen the file descriptors.
     * NB: both index and data file inodes must change before file descriptors
     * are closed, otherwise it is possible to end up with mismatched files. 
     */
    if (data_index->m_ReadOnlyMode) {
        int index_inode_changed = 0;
        int data_inode_changed = 0;
        struct stat stat_buf;
        char buf[256];

        memset(&stat_buf, 0, sizeof(stat_buf));
        sprintf(buf, "%sidx", data_index->m_FileNamePrefix);
        stat(buf, &stat_buf);
        if (stat_buf.st_ino != data_index->m_IndexInode)
            index_inode_changed = 1;
        sprintf(buf, "%sdat", data_index->m_FileNamePrefix);
        stat(buf, &stat_buf);
        if (stat_buf.st_ino != data_index->m_DataInode)
            data_inode_changed = 1;
        
        if (index_inode_changed && data_inode_changed)
            x_CloseFiles(data_index);
    }

    if (!x_ReMapIndex(data_index))
        return 0;
    if (!x_ReMapData(data_index))
        return 0;

    /* Inform any other threads that may want remapped data that remapping has
       already finished. */
    data_index->m_Remapping = 0;
    data_index->m_NeedRemap = 0;

    return 1;
}

/* For each page of 256 2-byte index array slots, the first 2 slots (0 and 1)
 * contain the full 4-byte offset for page element 0.
 * To catch up with the array index, the relative offsets for page elements
 * 1 and 2 are encoded in the 1-byte parts of slot 2. 
 * All other relative 2-byte offsets are encoded in slots corresponding to 
 * the page element with the same index.
 * To distinguish presence of element 0 in the index, the 4-byte offsets for 
 * leaf pages are encoded in the first 31 bits, and top bit is only set when
 * 0th element exists.
 * On error condition this function returns -1
 */
static int
x_GetIndexOffset(SGiDataIndex* data_index, int gi, Uint4 page, int level)
{
    int base = 0;
    Uint4 base_page;
    Uint4 remainder = page & kPageMask;
    Uint4* gi_index = NULL;
    Uint1 remap_done = 0;

    if (page >= data_index->m_GiIndexLen + data_index->m_IndexCacheLen) {
        /* Try remapping and check again */
        data_index->m_NeedRemap = 1;
        if (!data_index->m_RemapOnRead || !x_ReMapIndex(data_index))
            return -1;
        
        /* If page is still outside of the index range after remapping, it's an
           error condition */
        if (page >= data_index->m_GiIndexLen + data_index->m_IndexCacheLen)
            return -1;
        remap_done = 1;
    }

    if (page < data_index->m_GiIndexLen) {
        /* If page is outside of the mapped part of the index, try remapping,
           unless remapping has already been done. */
        if (page >= data_index->m_MappedIndexLen) {
            if (remap_done)
                return -1;
            data_index->m_NeedRemap = 1;
            if (!data_index->m_RemapOnRead || !x_ReMapIndex(data_index))
                return -1;
        }
        gi_index = data_index->m_GiIndex;
    } else {
        page -= data_index->m_GiIndexLen;
        gi_index = data_index->m_IndexCache;
    }

    /* Non-leaf pages contain direct offsets to other index pages.
     * When data is added to index sequentially, the leaf offsets can be
     * encoded in 2 bytes as increment to the offset on the beginning of the
     * page, i.e. 2 gis can be encoded in the same 4-byte index array
     * element.
     */
    if (level > 0 || !data_index->m_SequentialData) {
        base = (int) gi_index[page];
    } else if (remainder == 0 && (gi&1) == 0) {
        if ((gi_index[page] & kTopBit) == kTopBit)
            base = gi_index[page] & kFullOffsetMask;
        else
            base = -1;
    } else {
        Uint4 mask;
        Uint4 base_offset;
        
        base_page = page - remainder;
        if (remainder == 0 && (gi&1) == 1) {
            base = (gi_index[base_page+1]>>24);
            mask = kPageMask;
        } else if (remainder == 1 && (gi&1) == 0) {
            base = ((gi_index[page]>>16) & kPageMask);
            mask = kPageMask;
        } else if ((gi&1) == 0) {
            base = (gi_index[page]>>16);
            mask = kLocalOffsetMask;
        } else { /* (gi&1) == 1 */
            base = (gi_index[page]&kLocalOffsetMask);
            mask = kLocalOffsetMask;
        }
        
        if (base == 0) {
            base = -1;
        } else {
            base_offset = (gi_index[base_page] == kFullOffsetMask ? 0 :
                           gi_index[base_page]);
            if ((Uint4)base == mask)
                base = (Uint4) base_offset & kFullOffsetMask;
            else
                base += (Uint4) base_offset & kFullOffsetMask;
        }
    }

    return base;
}

static int
x_SetIndexOffset(SGiDataIndex* data_index, int gi, Uint4 page, int level,
                 Uint4 offset)
{
    Uint4 base_page;
    Uint4 remainder = page & kPageMask;
    Uint4* gi_index = NULL;
    Uint1 remap_done = 0;

    if (page >= data_index->m_GiIndexLen + data_index->m_IndexCacheLen) {
        /* Try remapping and check again */
        data_index->m_NeedRemap = 1;
        if (!x_ReMapIndex(data_index))
            return -1;
        
        /* If page is still outside of the index range after remapping, it's an
           error condition */
        if (page >= data_index->m_GiIndexLen + data_index->m_IndexCacheLen)
            return -1;
        remap_done = 1;
    } else {
        if (page < data_index->m_GiIndexLen) {
            if (remap_done)
                return -1;
            data_index->m_NeedRemap = 1;
            if (page >= data_index->m_MappedIndexLen && !x_ReMapIndex(data_index))
                return -1;
            gi_index = data_index->m_GiIndex;
        } else {
            page -= data_index->m_GiIndexLen;
            gi_index = data_index->m_IndexCache;
        }

        if (level > 0 || !data_index->m_SequentialData) {
            gi_index[page] = offset;
        } else if (remainder == 0 && (gi&1) == 0) {
            /* For gi which starts the leaf page, set the top bit, in order to 
               distinguish absent vs present starting gi. */
            gi_index[page] = offset | kTopBit;
        } else {
            Uint4 base_offset;
            Uint4 local_offset;

            base_page = page - remainder;
            base_offset = gi_index[base_page];

            /* If base offset is not yet available, it must be set now.
               NB: For 64-bit version, the base page offset is relative to the
               base top-level page offset, hence it may be = 0. To distinguish 0
               value from "not-set", use a special mask value. In 32-bit version
               this is never necessary.
            */
            if (base_offset == 0) {
                base_offset = offset;
                gi_index[base_page] = (offset > 0 ? offset : kFullOffsetMask);
            } else {
                // Get rid of the top bit if it's set (indicating availability
                // of the first gi on the page).
                base_offset &= kFullOffsetMask;
                // Check for the special value indicating relative offset = 0.
                if (base_offset == kFullOffsetMask)
                    base_offset = 0;
            }

            local_offset = offset - base_offset;
            /* If base offset was not previously set, use a special value for
               the relative offset to distinguish a 0 offset from absence of 
               data. */
            if (local_offset == 0)
               local_offset = kLocalOffsetMask;
            if (remainder == 0 && (gi&1) == 1) {
                gi_index[page+1] |= (local_offset<<24);
            } else if (remainder == 1 && ((gi&1) == 0)) {
                gi_index[page] |= ((local_offset&kPageMask)<<16);
            } else if ((gi&1) == 0) {
                gi_index[page] |= (local_offset<<16);
            } else { /* (gi&1) == 1 */
                gi_index[page] |= local_offset;
            }
        }
    }
    return 0;
}

static char* x_GetGiData(SGiDataIndex* data_index, int gi)
{
    Uint4 page = 0;
    int base = data_index->m_OffsetHeaderSize;
    int shift = (data_index->m_SequentialData ? 1 : 0);
    int level;
    Uint1 is_64bit = (data_index->m_OffsetHeaderSize > 0);

    /* If some thread is currently remapping, the data is in an inconsistent
       state, therefore return NULL. */
    if (data_index->m_Remapping)
        return NULL;

    if ((data_index->m_GiIndex == MAP_FAILED ||
         data_index->m_Data == MAP_FAILED)) {
        data_index->m_NeedRemap = 1;

        if (!data_index->m_RemapOnRead || !GiDataIndex_ReMap(data_index, 0))
            return NULL;
    }

    assert((data_index->m_GiIndex != MAP_FAILED) && 
           (data_index->m_Data != MAP_FAILED));

    for (level = 3; level >= 0; --level) {

        /* Get this gi's page number and find the starting offset for that page's 
           information in the index file. */
        page = (Uint4)base + ((gi>>(level*kPageSize+shift)) & kPageMask);

        /* The page can never point beyond the length of the index. If that 
         * happens, bail out.
         * If we got to a page that has been written, but not yet mapped, 
         * remapping must be done here.
         */
        base = x_GetIndexOffset(data_index, gi, page, level);
        
        /* 
         * If base wasn't found, bail out, but set flag for remapping - maybe
         * the data on disk has gone further...
        */
        if (base == -1) {
            data_index->m_NeedRemap = 1;
            return NULL;
        }
    }
    
    Int8 gi_offset = 0;

    if (is_64bit) {
        gi_offset = GET_TOP_PAGE_OFFSET(gi);
        /* If top page offset is not set, it means no gis from this whole top page
           have been saved in cache so far. */
        if (gi_offset == 0)
            return NULL;
    }
    
    gi_offset += base;

    /* If offset points beyond the combined length of the mapped data and cache,
       try to remap data. If that still doesn't help, bail out. */
    if (gi_offset >= data_index->m_DataLen + data_index->m_DataCacheLen) {
        data_index->m_NeedRemap = 1;
        if (!data_index->m_RemapOnRead || !x_ReMapData(data_index) ||
            gi_offset >= data_index->m_DataLen + data_index->m_DataCacheLen) 
            return NULL;
    }

    /* If offset is beyond the mapped data, get the data from cache, otherwise
       from the memory mapped location. */
    if (gi_offset >= data_index->m_DataLen) {
        return data_index->m_DataCache + (gi_offset - data_index->m_DataLen);
    } else {
        /* If offset points to data that has been written to disk but not yet
           mapped, remap now. */
        if (gi_offset >= data_index->m_MappedDataLen) {
            data_index->m_NeedRemap = 1;
            if (!data_index->m_RemapOnRead || !x_ReMapData(data_index))
                return NULL;
        }

        return data_index->m_Data + gi_offset;
    }
}

/* Constructor */
static SGiDataIndex*
GiDataIndex_New(SGiDataIndex* data_index, int unit_size, const char* name,
                Uint1 readonly, Uint1 sequential, Uint1 is_64bit)
{
    if (!data_index) {
        data_index = (SGiDataIndex*) malloc(sizeof(SGiDataIndex));
        memset( data_index, 0, sizeof(SGiDataIndex));
        data_index->m_FreeOnDrop = 1;
    } else {
        data_index->m_FreeOnDrop = 0;
    }

    data_index->m_ReadOnlyMode = readonly;
    assert(strlen(name) < 256);
    strncpy(data_index->m_FileNamePrefix, name, 256);
    data_index->m_DataUnitSize = unit_size;
    data_index->m_SequentialData = sequential;
    data_index->m_GiIndex = ((Uint4*)MAP_FAILED);
    data_index->m_Data = ((char*)MAP_FAILED);
    data_index->m_GiIndexFile = -1;
    data_index->m_DataFile = -1;
    data_index->m_GiIndexLen = 0;
    data_index->m_DataLen = 0;
    data_index->m_MappedDataLen = 0;
    data_index->m_MappedIndexLen = 0;
    data_index->m_IndexCacheLen = 0;
    data_index->m_IndexCacheSize = INDEX_CACHE_SIZE;
    data_index->m_IndexCache =
        (Uint4*) malloc(data_index->m_IndexCacheSize*sizeof(Uint4));
    data_index->m_DataCacheLen = 0;
    data_index->m_DataCacheSize = unit_size*DATA_CACHE_SIZE;
    data_index->m_DataCache = (char*) malloc(data_index->m_DataCacheSize);
    data_index->m_Remapping = 0;
    data_index->m_NeedRemap = 1;
    data_index->m_RemapOnRead = 1;
    data_index->m_OffsetHeaderSize = (is_64bit ? kOffsetHeaderSize : 0);
    data_index->m_OldDataPtr = ((char*)MAP_FAILED);
    data_index->m_OldMappedDataLen = 0;

    return data_index;
}

/* Destructor */
static SGiDataIndex* GiDataIndex_Free(SGiDataIndex* data_index)
{
    if (!data_index)
        return NULL;

    x_Flush(data_index);
    x_UnMap(data_index);
    x_CloseFiles(data_index);
    free(data_index->m_IndexCache);
    free(data_index->m_DataCache);
    if (data_index->m_FreeOnDrop) {
      free(data_index);
      data_index=NULL;
    }
    return data_index;
}

/* Returns data corresponding to a given gi for reading only. */
static const char* GiDataIndex_GetData(SGiDataIndex* data_index, int gi)
{
    return x_GetGiData(data_index, gi);
}

/* Writes data for a gi. */
static Uint1
GiDataIndex_PutData(SGiDataIndex* data_index, int gi, const char* data,
                    Uint1 overwrite, Uint4 data_size)
{
    Uint4 page = 0;  
    Int8 base = (Int8) data_index->m_OffsetHeaderSize;
    Int8 top_page_offset = 0;
    int shift = (data_index->m_SequentialData ? 1 : 0);
    int level;
    Uint1 is_64bit = (data_index->m_OffsetHeaderSize > 0);

    /* No writing can occur in read-only mode. */
    if (data_index->m_ReadOnlyMode)
        return 0;

    /* Check if index and data memory maps are open. */
    if (data_index->m_GiIndex == MAP_FAILED && !x_MapIndex(data_index))
        return 0;

    if (data_index->m_Data == MAP_FAILED && !x_MapData(data_index))
        return 0;

    if ((data_index->m_GiIndexLen + (1<<kPageSize))*sizeof(Uint4) >= kFullOffsetMask)
        return 0; /* can not map this amount of data anyway */
    
    /* For 32-bit version check the data file length too, and return error if
       that file has reached maximal size. */
    if (!is_64bit &&  
        data_index->m_DataLen + sizeof(Uint4)*(1<<kPageSize) >= kFullOffsetMask)
        return 0;

    for (level = 3; level >= 0; --level) {
        if (base < 0)
            return 0;

        page = (Uint4)base + ((gi>>(level*kPageSize+shift)) & kPageMask);

        /* Find next level page offset for this gi. On the leaf level,
         * if offset is not found, base will be set to -1, to distinguish from a
         * 0 relative offset.
         * NB: in particular, page can never point beyond the length of the
         * index. If that happens, set next base to -1, so new page could be
         * allocated for this data.
         * NB2: If we got to a page that has been written, but not yet mapped, 
         * remapping must be done here. If remapping fails, the error is
         * unrecoverable within the current process.
         */
        base = x_GetIndexOffset(data_index, gi, page, level);

        /* If there are no gis from the same page in the index yet, assign a new
           page in the index for this gi's page. */
        if (level > 0 && base == 0) {
            const Uint4 kPageBitSize = 1<<kPageSize;
            Uint4* b = (Uint4*) calloc(kPageBitSize, sizeof(Uint4));

            /* Assign pointer to the new page. */
            base = (int) (data_index->m_GiIndexLen + data_index->m_IndexCacheLen);

            x_SetIndexOffset(data_index, gi, page, level, base);

            /* Add the new page. */
            if ((data_index->m_IndexCacheLen + kPageBitSize) >
                data_index->m_IndexCacheSize) {
                x_DumpIndexCache(data_index);
            }
            assert(data_index->m_GiIndexLen*sizeof(Uint4) == 
                    lseek(data_index->m_GiIndexFile, 0, SEEK_END));
            memcpy((void*)(&data_index->m_IndexCache[data_index->m_IndexCacheLen]),
                   b, kPageBitSize*sizeof(Uint4));
            data_index->m_IndexCacheLen += kPageBitSize;
            free(b);
        }
    }

    if (data_size== 0)
        data_size = data_index->m_DataUnitSize;

    if (is_64bit)
        top_page_offset = GET_TOP_PAGE_OFFSET(gi);

    /* Check if data is already present. If it is, and overwrite is not 
     * requested, just return, otherwise write new data in place of the old one.
     * If previous data for this gi is not available, write the new data at the
     * end of the data file.
     */
    if (base >= 0 && (!is_64bit || top_page_offset > 0)) {
        if (!overwrite)
            return 0;
        
        base += top_page_offset;

        if (base >= (Int8)data_index->m_DataLen) {
            /* The previous data for this gi is currently in cache. */
            if (base + data_size <=
                data_index->m_DataLen + data_index->m_DataCacheLen) {
                memcpy(data_index->m_DataCache + base - data_index->m_DataLen,
                       data, data_size);
            } else {
                /* The index got corrupted, and previous data cannot be found. */
                base = 0;
            }
        } else {
            /* If this base is in the part that has already been written to 
               disk, but not yet mapped, remap now. */
            if (base >= (int)data_index->m_MappedDataLen)
                x_ReMapData(data_index);
            memcpy(data_index->m_Data + base, data, data_size);
        }
    } else {
        if (is_64bit && top_page_offset == 0) {
            top_page_offset = data_index->m_DataLen + data_index->m_DataCacheLen;
            SET_TOP_PAGE_OFFSET(gi, top_page_offset);
        }
        
        x_SetIndexOffset(data_index, gi, page, 0,
                         data_index->m_DataLen + data_index->m_DataCacheLen -
                         top_page_offset);

        /* This should already be valid, but in case of corruption, make sure
         * that value data_index->m_DataLen reflects what is actually available
         * on disk.
         */
        data_index->m_DataLen = (Int8) lseek(data_index->m_DataFile, 0, SEEK_END);
        /* Check if there is space for current data in cache. If not, flush the 
           cache. */
        if (data_index->m_DataCacheLen + data_size >=
            data_index->m_DataCacheSize)
            x_DumpDataCache(data_index);
    
        assert(data_index->m_DataCacheLen + data_size <=
               data_index->m_DataCacheSize);
        /* Write the current data into cache. */
        memcpy(data_index->m_DataCache + data_index->m_DataCacheLen, data,
               data_size);
        data_index->m_DataCacheLen += data_size;
    }

    return 1;
}

#ifdef ALLOW_IN_PLACE_MODIFICATION
/* Returns data corresponding to a given gi, for possible modification. */
static char* GiDataIndex_SetData(SGiDataIndex* data_index, int gi)
{
    if (data_index->m_ReadOnlyMode)
        return NULL;

    return x_GetGiData(data_index, gi);
}

/* Deletes data for a gi. */
static Uint1 GiDataIndex_DeleteData(SGiDataIndex* data_index, int gi)
{
    int page = 0; 
    int base = 0;
    int index;
    
    /* No writing can occur in read-only mode. */
    if (data_index->m_ReadOnlyMode)
        return 0;

    /* Check if index and data memory maps are open. */
    if (data_index->m_GiIndex == MAP_FAILED && !x_MapIndex(data_index))
        return 0;

    if (data_index->m_Data == MAP_FAILED && !x_MapData(data_index))
        return 0;

    for (index = 3; index >= 0; --index) {

        page = base + ((gi>>(index*kPageSize)) & kPageMask);

        /* The page can never point beyond the length of the index. If that 
         * happens, bail out.
         * If we got to a page that has been written, but not yet mapped, 
         * remapping must be done here.
         */
        if (page >= (int)data_index->m_GiIndexLen) {
            return 0;
        } else if (page < (int)data_index->m_GiIndexLen) {
            if (page >= (int)data_index->m_MappedIndexLen) {
                if (!x_ReMapIndex(data_index))
                    return 0;
            }
            base = data_index->m_GiIndex[page + data_index->m_OffsetHeaderSize];
        } else {
            base = (int) data_index->m_IndexCache[page-data_index->m_GiIndexLen];
        }

        /* If there are no gis from this page in the index, there is nothing to
           delete. Return success. */
        if (base == 0)
            return 1;
    }
     
    /* Check if data is already present. If it is not, there is nothing to 
       delete. */
    if (base) {
        if (page < (int)data_index->m_GiIndexLen)
            data_index->m_GiIndex[page] = 0;
        else
            data_index->m_IndexCache[page-data_index->m_GiIndexLen] = 0;

        if (base >= (int)data_index->m_DataLen) {
            /* The previous data for this gi is currently in cache. */
            assert(base + data_index->m_DataUnitSize <=
                   data_index->m_DataLen + data_index->m_DataCacheLen);
            memset(data_index->m_DataCache + base - data_index->m_DataLen, 0,
                   data_index->m_DataUnitSize);
        } else {
            /* If this base is in the part that has already been written to 
               disk, but not yet remapped, remap now. */
            if (base >= (int)data_index->m_MappedDataLen) {
                data_index->m_NeedRemap = 1;
                if (!GiDataIndex_ReMap(data_index, 0))
                    return 0;
            }
            memset(data_index->m_Data + base, 0, data_index->m_DataUnitSize);
        }
    }
    return 1;
}

/* Returns pointer to the start of data in the data file. Needed when
 * the whole data file needs to be read sequentially.
 * NB: This may involve remapping, hence no 'const' qualifier
 * for the object!
 */
static void
GiDataIndex_GetAllData(SGiDataIndex* data_index, const char* *data_ptr,
                       Uint4* data_size)
{
    if (!data_ptr)
        return;
    
    *data_ptr = NULL;
    *data_size = 0;
        
    if (data_index->m_Data == MAP_FAILED) {
        if (!x_MapData(data_index))
            return;
    } else if (data_index->m_DataCacheLen > 0) {
        if (!x_ReMapData(data_index))
            return;
    }

    /* The first 2*sizeof(int) bytes are filled with 0's for convenience, the
       actual data starts immediately after. */
    *data_ptr = data_index->m_Data + 2*sizeof(int);
    *data_size = data_index->m_DataLen + data_index->m_DataCacheLen - 2*sizeof(int);
}

static int GiDataIndex_GetMappedSize(SGiDataIndex* data_index)
{
    return data_index->m_MappedDataLen + data_index->m_MappedIndexLen;
}
#endif

static int GiDataIndex_GetMaxGi(SGiDataIndex* data_index)
{
    int base = data_index->m_OffsetHeaderSize;
    int page = 0;
    int gi = 0;
    int index;
    int shift = (data_index->m_SequentialData ? 1 : 0);
    int remainder = 0;
    Uint4* gi_index;
    Int8 base_offset;

    x_Flush(data_index);

    if (data_index->m_GiIndex == MAP_FAILED && !x_MapIndex(data_index))
        return -1;

    gi_index = data_index->m_GiIndex;

    for (index = 3; index >=0; --index) {
        /* Find largest page present in the gi index.
         * Check if referenced page points beyond index size. If invalid page is
         * found, fix the index by resetting it to 0 (unless it's a read-only
         * mode).
         */
        for (page = base + kPageMask; page >= 0; --page) {
            if (gi_index[page] == 0)
                continue;
            if (index > 0 && gi_index[page] >= data_index->m_GiIndexLen) {
                if (!data_index->m_ReadOnlyMode)
                    gi_index[page] = 0;
                continue;
            } else {
                break;
            }
        }
        if(page<0)
          return -1;
        if (gi_index[page] != 0) {
            remainder = page - base;
            gi |= (remainder<<(index*kPageSize+shift));
            base = (int) gi_index[page];
        }
    }

    if (data_index->m_SequentialData) {
        /* Because of the 2-gi per page slot encoding of data offsets, check
           which exact gi is the maximal. */
        int max_gi;
        int min_gi;
        if (remainder == 0) {
            return gi;
        } else if (remainder > 1) {
            max_gi = gi + 1;
            min_gi = gi;
        } else {
            max_gi = gi + 1;
            min_gi = gi - 1;
        }

        for (gi = max_gi; gi >= min_gi; --gi) {
            if (x_GetIndexOffset(data_index, gi, page, 0) > 0)
                break;
        }
    }

    return gi;
}

/* When encoding in 4 bytes, top bit serves as control */
static INLINE int s_EncodeInt4(char* buf, Uint4 val)
{
    int bytes = (val > 0x7fff ? 4 : 2);
    char* ptr = buf;
    int i;
    for (i = bytes - 1; i >= 0; --i, ++ptr) {
        *ptr = ((val>>(8*i)) & 0xff);
    }

    if (bytes == 4)
        buf[0] |= 0x80;

    return bytes;
}

static INLINE int s_EncodeInt2(char* buf, Uint2 val)
{
    if (val <= 0x7f) {
        *buf = (char) (val & 0x7f);
        return 1;
    } else {
        *buf = 0x80 | (((val)>>8) & 0x7f);
        *(buf+1) = (val) & 0xff;
        return 2;
    }
}

static INLINE int s_DecodeInt4(const char* buf, int* val)
{
    if ((buf[0] & 0x80) != 0) {
        *val = ((buf[0]&0x7f)<<24) | ((buf[1]&0xff)<<16) |
               ((buf[2]&0xff)<<8)  | (buf[3]&0xff);
        return 4;
    } else {
        *val = ((buf[0]&0x7f)<<8) | (buf[1]&0xff);
        return 2;
    }
}

static INLINE int s_DecodeInt2(const char* buf, int* val)
{
    if ((buf[0] & 0x80) != 0) {
        *val = ((buf[0]&0x7f)<<8) | (buf[1]&0xff);
        return 2;
    } else {
        *val = buf[0] & 0x7f;
        return 1;
    }
}

static INLINE
int s_Encode3Plus5Accession(char* buf, const char* accession, int suffix,
                            char* *buf_ptr)
{
    if (!(accession[0] >= 'A' && accession[0] <= 'Z' &&
           accession[1] >= 'A' && accession[1] <= 'Z' &&
           accession[2] >= 'A' && accession[2] <= 'Z' &&
          ((suffix>>17) == 0))) {
        return 0;
    }
    /* 1st prefix character + top 3 bits of 2nd prefix character */
    buf[0] = ((accession[0] - 'A' + 1)<<3) | ((accession[1] - 'A' + 1)>>2);
    /* bottom 2 bits of 2nd prefix character + 3rd prefix character + top 1 bit
       of integer suffix */
    buf[1] = ((accession[1] - 'A' + 1)<<6) | ((accession[2] - 'A' + 1)<<1) |
        ((suffix>>16) & 0xff);
    /* Bits 8-15 of integer suffix */
    buf[2] = (suffix>>8) & 0xff;
    /* Bits 0-7 of integer suffix */
    buf[3] = (suffix & 0xff); 

    *buf_ptr += 4;

    return 1;
}

static INLINE
int s_Decode3Plus5Accession(const char* buf, char* prefix, 
                             int* prefix_length, int* suffix)
{
    if ((prefix[0] = ((buf[0]&0xff)>>3) + 'A' - 1) > 'Z')
        return -1;
    if ((prefix[1] = (((buf[0]&0x07)<<2) | ((buf[1]&0xff)>>6)) + 'A' - 1) > 'Z')
        return -1;
    if ((prefix[2] = (((buf[1]&0xff)>>1) & 0x1f) + 'A' - 1) > 'Z')
        return -1;
    *prefix_length = 3;
    *suffix = ((int)(buf[1] & 0x01)<<16) | (((int)(buf[2]&0xff)<<8) & 0xff00) |
               ((int)(buf[3] & 0xff));
    if (*suffix >= 100000)
        return -1;

    return 1;
}

static INLINE
int s_Encode2LetterAccession(char* buf, const char* accession, int suffix,
                             int prefix_length, char* *buf_ptr)
{
    if (accession[0] < 'A' || accession[0] > 'Z')
        return 0;

    /* 1st prefix character */
    buf[0] = (accession[0] - 'A' + 1)<<3;
    if (prefix_length == 2) {
        /* top 3 bits of 2nd prefix character */
        buf[0] |= (accession[1] - 'A' + 1)>>2;
        /* bottom 2 bits of 2nd prefix character */
        buf[1] = ((accession[1] - 'A' + 1)<<6);
    } else {
        buf[1] = 0;
    }
    /* If integer suffix fits into 22 bits, we need only 2 extra bytes, otherwise
       use 3 extra bytes. */
    if (suffix>>22 != 0) {
        /* Bits 24-28 of the integer suffix (currently always 0) */
        buf[1] |= ((suffix>>24) & 0x1f);
        /* Bits 16-23 of the integer suffix */
        buf[2] = (suffix>>16) & 0xff;
        /* Bits 8-15 of the integer suffix */
        buf[3] = (suffix>>8) & 0xff;
        /* Bits 0-7 of the integer suffix */
        buf[4] = (suffix & 0xff);
        *buf_ptr += 5;
    } else {
        /* Bits 16-21 of the integer suffix (and control bit = 0) */
        buf[1] |= ((suffix>>16) & 0x3f);
        /* Bits 8-15 of the integer suffix */
        buf[2] = (suffix>>8) & 0xff;
        /* Bits 0-7 of the integer suffix */
        buf[3] = (suffix & 0xff);
        *buf_ptr += 4;
    }

    return 1;
}

static INLINE
int s_Decode2LetterAccession(const char* buf, Uint1 control_byte, char* prefix,
                             int* prefix_length, int* suffix)
{
    Uint1 is_refseq = ((control_byte & (1<<5)) != 0);
    Uint1 large_suffix = ((control_byte & (1<<6)) != 0);
    Uint1 byte;

    if ((prefix[0] = ((buf[0]&0xff)>>3) + 'A' - 1) > 'Z')
        return -1;

    byte = ((buf[0]&0x07)<<2) | ((buf[1]&0xff)>>6);
    if (byte == 0) {
        *prefix_length = 1;
    } else {
        if ((prefix[1] = byte + 'A' - 1) > 'Z')
            return -1;
        if (is_refseq) {
            prefix[2] = '_';
            *prefix_length = 3;
        } else {
            *prefix_length = 2;
        } 
    }

    if (large_suffix) {
        *suffix = (((int)(buf[1] & 0x1f)<<24) | (((int)(buf[2]) & 0xff)<<16) |
                  ((int)(buf[3] & 0xff)<<8) | ((int)(buf[4] & 0xff)));
    } else {
        *suffix = (((int)(buf[1] & 0x3f)<<16) | (((int)(buf[2]) & 0xff)<<8) |
                  ((int)(buf[3] & 0xff)));
    }
    return 1;
}

static INLINE
int s_Encode4Plus9Accession(char* buf, const char* accession, int suffix,
                            char* *buf_ptr)
{
    if (!(accession[0] >= 'A' && accession[0] <= 'Z' &&
           accession[1] >= 'A' && accession[1] <= 'Z' &&
           accession[2] >= 'A' && accession[2] <= 'Z' &&
           accession[3] >= 'A' && accession[3] <= '_' &&
          ((suffix>>27) == 0)))
        return 0;
    
    /* 1st prefix character + top 3 bits of 2nd prefix character */
    buf[0] = ((accession[0] - 'A' + 1)<<3) | ((accession[1] - 'A' + 1)>>2);
    /* bottom 2 bits of 2nd prefix character + 3rd prefix character + top 1 bit
       of 4th prefix character */
    buf[1] = ((accession[1] - 'A' + 1)<<6) | ((accession[2] - 'A' + 1)<<1) | 
        ((accession[3] - 'A' + 1)>>4);
    /* Bottom 4 bits of 4th prefix character + bits 24-27 of integer suffix */
    buf[2] = ((accession[3] - 'A' + 1)<<4) | ((suffix>>24) & 0x0f); 
    /* Bits 16-23 of the integer suffix */
    buf[3] = (suffix>>16) & 0xff;
    /* Bits 8-15 of the integer suffix */
    buf[4] = (suffix>>8) & 0xff;
    /* Bits 0-7 of the integer suffix */
    buf[5] = (suffix & 0xff);

    *buf_ptr += 6;

    return 1;
}

static INLINE
int s_Decode4Plus9Accession(const char* buf, Uint1 control_byte, char* prefix,
                            int* prefix_length, int* suffix)
{
    int pos = 0;
    if ((control_byte & (1<<5)) != 0) {
        /* NZ_-type Refseq */
        sprintf(prefix, "NZ_");
        pos = 3;
    }

    if ((prefix[pos] = ((buf[0]&0xff)>>3) + 'A' - 1) > 'Z')
        return -1; 
    if ((prefix[pos+1] = (((buf[0]&0x07)<<2) | ((buf[1]&0xff)>>6)) + 'A' - 1) > 'Z')
        return -1;
    if ((prefix[pos+2] = (((buf[1]&0xff)>>1) & 0x1f) + 'A' - 1) > 'Z')
        return -1;
    if ((prefix[pos+3] = (((buf[1]&0x01)<<4) | ((buf[2]&0xff)>>4)) + 'A' - 1) > 'Z')
        return -1;
    *prefix_length = pos + 4;
    *suffix = ((int)(buf[2]&0x0f)<<24) | ((int)(buf[3] & 0xff)<<16) | 
        ((int)(buf[4]&0xff)<<8) | ((int)(buf[5]&0xff));

    return 1;
}

static int
s_EncodeGiData(const char* accession, int version, int seq_length,
               char* outbuf, int* encoded_length)
{
    int acc_length = strlen(accession);
    Uint1 suffix_length = 0;
    /* Control byte structure:
     * Bits 0-2: Length of the integer suffix: 
     *           0 means no suffix, like for PDB, otherwise (length-2), because
     *           suffixes with length <= 2 are not worth encoding!
     * Bit    3: Is version byte present in the encoding?
     * NB: No version byte for version = 1; version byte = 0 if no version 
     * Bits 4-7: Various special cases
     * 1...: 2+N type accession 
     * 11..: Refseq (i.e. AB_ prefix)
     * 1.1.: integer suffix fits into 22 bits
     * NB: only combinations 1000, 1100 and 1110 are possible, but not 1010!
     * Compressed in 4-5 bytes:
     *     Byte 1, Byte 2, bits 0-1: prefix compressed 5 bits per letter
     *     If only 1 letter, second is 00000.
     *     If suffix fits in 22 bits, then 
     *        Byte 2, bits 2-7, Bytes 3,4: 22-bit integer suffix
     *     else
     *        Byte 2, bits 2-7, Bytes 3-5: 30-bit integer suffix
     * 0100: 3+5 type accession (ABC12345)
     * Compressed in 4 bytes:
     *     15 top bits for the prefix;
     *     17 bottom bits for the integer suffix.
     * 0010: 4+[8|9] accession (ABCD0[0-2]1234567)
     * Compressed in 6 bytes:
     *     Byte 1, bit 0: is there an NZ_ pre-prefix?
     *     Byte 1, bit 1 - Byte 3, bit 4: 4 letters encoded 5 bits per letter
     *     Byte 3, bit 5-7, Bytes 4-6: integer suffix.
     *     This gives a total of 27 bits for the integer suffix, more than enough
     *     for any combination of 8 digits (i.e. 1st of 9 digits must be 0!).
     * The above covers all cases with integer suffix length > 2. In all other
     * cases there is no compression - just plain accession with no suffix and an
     * extra null byte.
     *
     * Is accession prefix encoded in compressed form in 2 bytes? 
     * NB: This means that prefix consists of <=3 capital letters or a 2 capital
     * letters + '_'. Then top bit of Byte 1 is set when '_' is present, and
     * remaining 15 bits encode prefix 5 bits per letter (char_val - 'A' + 1).
     * If < 3 letters, the bits corresponding to missing letters are 00000;
     */
    int suffix = 0;
    char* buf_ptr;
    int acc_pos;
    Uint1 no_encoding = 0;

    outbuf[0] = 0;

    buf_ptr = &outbuf[1];

    buf_ptr += s_EncodeInt4(buf_ptr, seq_length);

    if (version != 1) {
        outbuf[0] |= (1<<3);
        buf_ptr += s_EncodeInt2(buf_ptr, version);
    }

    acc_pos = acc_length - 1;

    for ( ; acc_pos >= 0; --acc_pos) {
        if (isdigit(accession[acc_pos]))
            ++suffix_length;
        else
            break;
    }

    ++acc_pos;

    /* Only encode suffix as integer if it is > 2 bytes long.
     * NB: if suffix length is <= 4, it is certain that integer suffix will fit
     * in 2 bytes, so it's still worth encoding!
     */
    if (suffix_length > 3) {
        Uint1 is_refseq = (acc_pos >= 3 && accession[2] == '_');
        suffix = atol(&accession[acc_pos]);
        if (acc_pos == 2 || (is_refseq && acc_pos == 3)) {
            outbuf[0] |= (1<<4);
            if (is_refseq) {
                outbuf[0] |= (1<<5);
                --acc_pos;
            }
            if (suffix>>22 != 0)
                outbuf[0] |= (1<<6);
            if (!s_Encode2LetterAccession(buf_ptr, accession, suffix, acc_pos, &buf_ptr))
                return 0;
        } else if (suffix_length == 5 && acc_pos == 3) {
            outbuf[0] |= (1<<5);
            if (!s_Encode3Plus5Accession(buf_ptr, accession, suffix, &buf_ptr))
                return 0;
        } else if (suffix_length >= 8 && acc_pos >= 4) {
            const char* acc_ptr = accession;
            outbuf[0] |= (1<<6);
            if (is_refseq) {
                outbuf[0] |= (1<<5);
                acc_ptr += 3;
            }
            if (!s_Encode4Plus9Accession(buf_ptr, acc_ptr, suffix, &buf_ptr))
                return 0;
        } else {
            /* Non-standard case - no prefix compression, end prefix with null
               byte. For suffix use 2 or 4 bytes depending on its value. */
            if (acc_pos > 0) {
                memcpy(buf_ptr, accession, acc_pos);
                buf_ptr += acc_pos;
            }
            *buf_ptr = 0;
            ++buf_ptr;
            buf_ptr += s_EncodeInt4(buf_ptr, suffix);
        }

        suffix_length -= 2;
    } else {
        no_encoding = 1; 
        suffix_length = 0;
    }

    outbuf[0] |= (suffix_length == 0 ? 0 : suffix_length);

    if (no_encoding) {
        /* Sanity check - the input buffer size is = MAX_ACCESSION_LENGTH, so we 
           cannot save more than this size. */
        int available_size = MAX_ACCESSION_LENGTH - 1 - (buf_ptr - outbuf);
        if (acc_length > available_size) {
            strncpy(buf_ptr, accession, available_size);
            buf_ptr[available_size] = NULLB;
            buf_ptr += available_size + 1;
        } else {
            strncpy(buf_ptr, accession, acc_length+1);
            buf_ptr += acc_length + 1;
        }
    }

    *encoded_length = buf_ptr - outbuf;

    return 1;
}

static int
s_DecodeGiAccession(const char* inbuf, char* acc, int acc_len)
{
    Uint1 control_byte = *inbuf;
    Uint1 suffix_length = control_byte & 0x07;
    int version = 1;
    /* Use internal buffer to retrieve accession */
    char acc_buf[MAX_ACCESSION_LENGTH];
    int retval = 1;

    const char* buf = inbuf + 1;

    /* Skip the bytes containing sequence length */
    if ((buf[0]) & 0x80) {
        buf += 4;
    } else {
        buf += 2;
    }

    /* Retrieve version */
    if (control_byte & (1<<3))
        buf += s_DecodeInt2(buf, &version);

    /* Retrieve integer accession suffix */
    if (suffix_length > 0) {
        int suffix = 0;
        int prefix_length = 0;
        if ((control_byte & (1<<4)) != 0) {
            retval = s_Decode2LetterAccession(buf, control_byte, acc_buf,
                                              &prefix_length, &suffix);
        } else if ((control_byte & 0xf0) == (1<<5)) {
            retval = s_Decode3Plus5Accession(buf, acc_buf, &prefix_length,
                                             &suffix);
        } else if ((control_byte & (1<<6)) != 0) {
            retval = s_Decode4Plus9Accession(buf, control_byte, acc_buf,
                                             &prefix_length, &suffix);
        } else {
            int version_len = 0;
            if (version > 0) {
                char version_buf[6];
                sprintf(version_buf, "%d", version);
                version_len = strlen(version_buf) + 1; /* including '.' */
            }
            if ((prefix_length = strlen(buf)) <=
                MAX_ACCESSION_LENGTH - suffix_length - version_len - 1) {
                strncpy(acc_buf, buf, prefix_length);
                buf += prefix_length + 1;
                buf += s_DecodeInt4(buf, &suffix);
            } else
                retval = -1;
        }

        if (retval < 0)
            return retval;
        sprintf(acc_buf+prefix_length, "%.*d", suffix_length+2, suffix);
    } else
    {
        /* sprintf(acc_buf, "%s", buf); */
        strncpy( acc_buf, buf, MAX_ACCESSION_LENGTH  - 8);
    }

    if (version > 0)
        sprintf(&acc_buf[strlen(acc_buf)], ".%d", version);

    /* If retrieved accession fits into the client-supplied buffer, then just
     * copy, otherwise copy the part that fits into the supplied buffer.
     */
    if (strlen(acc_buf) < acc_len) {
        strcpy(acc, acc_buf);
    } else {
        strncpy(acc, acc_buf, acc_len - 1);
        acc[acc_len-1] = NULLB;
        retval = 0;
    }
    return retval;
}

/****************************************************************************
 *
 * gicache_lib.c 
 *
 ****************************************************************************/

static SGiDataIndex *gi_cache=NULL;

static int x_GICacheInit(const char* prefix, Uint1 readonly, Uint1 is_64bit)
{
    char prefix_str[256];

    // First try local files
    sprintf(prefix_str, "%s", (prefix ? prefix : DEFAULT_GI_CACHE_PREFIX));

    if (!prefix && is_64bit)
       strcat(prefix_str, DEFAULT_64BIT_SUFFIX);

    strcat(prefix_str, ".");

    /* When reading data, use readonly mode. */
    if (gi_cache) return 0;

    gi_cache = GiDataIndex_New(NULL, MAX_ACCESSION_LENGTH, prefix_str, readonly,
                               1, is_64bit);

    if (readonly) {
        /* Check whether gi cache is available at this location, by trying to
           map it right away. If local cache isn't found, use default path and
           try again. */
        Uint1 cache_found = GiDataIndex_ReMap(gi_cache, 0);

        if (!cache_found) {
            const char* suffix = (is_64bit ? DEFAULT_64BIT_SUFFIX : "");
            sprintf(prefix_str, "%s/%s%s.", DEFAULT_GI_CACHE_PATH,
                    DEFAULT_GI_CACHE_PREFIX, suffix);
            gi_cache = GiDataIndex_Free(gi_cache);
            gi_cache = GiDataIndex_New(NULL, MAX_ACCESSION_LENGTH, prefix_str,
                                       readonly, 1, is_64bit);
        }
    }

    return (gi_cache ? 0 : 1);    
}

int GICache_ReadData(const char *prefix)
{
    Uint1 is_64bit = 0;
    int rc;

    if (sizeof(void*) >= 8 &&
        (!prefix || (strstr(prefix, DEFAULT_64BIT_SUFFIX) != NULL)))
        is_64bit = 1;

    rc = x_GICacheInit(prefix, 1, is_64bit);

    return rc;
}

void GICache_ReMap(int delay_in_sec) {
    /* If this library function is being called, delayed remapping is
       established, i.e. any future remapping is only done from here, but not on
       read attempts. */
    if(gi_cache) {
      gi_cache->m_RemapOnRead = 0;
      GiDataIndex_ReMap(gi_cache, delay_in_sec*1000);
    }
}

int GICache_GetAccession(int gi, char* acc, int acc_len)
{
    int retval = 0;
    if(!gi_cache) return 0;
    const char* gi_data = GiDataIndex_GetData(gi_cache, gi);
    if (gi_data) {
        if ((retval = s_DecodeGiAccession(gi_data, acc, acc_len)) < 0) {
            /* If returned "accession" is invalid, force a remap and return empty
               string */
            acc[0] = NULLB;
            gi_cache->m_NeedRemap = 1;
        }
    } else {
        acc[0] = NULLB;
    }
    return retval;
}

int GICache_GetLength(int gi)
{
    int length = 0;
    if(!gi_cache) return 0;
    const char *x = GiDataIndex_GetData(gi_cache, gi);

    if(!x) return 0;

    x++; /* Skip control byte */
    x += s_DecodeInt4(x, &length);
    return length;
}

int GICache_GetMaxGi()
{
    if(!gi_cache) return 0;
    return GiDataIndex_GetMaxGi(gi_cache);
}

int GICache_LoadStart(const char* cache_prefix)
{
    Uint1 is_64bit = 0;
    int rc;

    if (sizeof(void*) >= 8 && cache_prefix &&
        (strstr(cache_prefix, DEFAULT_64BIT_SUFFIX) != NULL))
        is_64bit = 1;

    rc = x_GICacheInit(cache_prefix, 0, is_64bit);

    return rc;
}

int GICache_LoadAdd(int gi, int len, const char* acc, int version)
{
    int acc_len;

    static char buf[MAX_ACCESSION_LENGTH];
    if(!gi_cache) return 0;
    
    if (!s_EncodeGiData(acc, version, len, buf, &acc_len))
        return 0;

    /* Primary accession and length for a given gi never change, hence there
     * is never a need to overwrite gi data if it is already present in
     * cache.
     * NB: The "overwrite" parameter is 1, because the only possible change
     * in gi data is that it gets a version when previously there was no
     * version. This does not change the encoded data size, so data can be
     * modified in place.
     */
    return GiDataIndex_PutData(gi_cache, gi, buf, 1, acc_len);
}

int GICache_LoadEnd()
{
    /* NB: This will not free the structure, and in particular the prefix name 
       would still be available. */
    gi_cache = GiDataIndex_Free(gi_cache);
    return 0;
}

void GICache_SetLog(void (*logfunc)(char*))
{
    LogFunc = logfunc;
}
