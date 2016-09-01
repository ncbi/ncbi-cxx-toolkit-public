#ifndef _GI_CACHE_HPP__
#define _GI_CACHE_HPP__

#include <ncbi_toolkit.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_GI_CACHE_PATH "//panfs/pan1.be-md.ncbi.nlm.nih.gov/id_dumps/gi_cache"
#define DEFAULT_GI_CACHE_PREFIX "gi2acc_lmdb"

#define META_INCREMENTAL_HOST "INC_HOST"
#define META_FULL_HOST "FULL_HOST"
#define META_INCREMENTAL_DB "INC_DB"
#define META_FULL_DB "FULL_DB"
#define META_INCREMENTAL_TIME "INC_TIME"
#define META_FULL_TIME "FULL_TIME"
#define META_GIBYTIME_TIME "GI_BYTIME_TIME"
#define RC_ALREADY_EXISTS 2

typedef struct {
	int64_t Count[256];
} FreqTab;

/* Initializes the cache. If cache_prefix argument is not provided, default name
 * is used. If local cache is not available, use default path and prefix.
 * Return value: 0 on success, 1 on failure. 
 * Resources should be released with GICache_ReadEnd() call
 */
int         GICache_ReadData(const char *cache_prefix);
/* Retrieves accession.version by gi.
 * Accession buffer must be preallocated by caller. 
 * If buffer is too small, retrieved accession is truncated, and return code 
 * is 0, otherwise return code is 1.
 */
int         GICache_GetAccession(int64_t gi, char* acc, int buf_len);
/* Retrieves gi length */
int64_t     GICache_GetLength(int64_t gi);
/* Returns maximal gi available in cache */
int64_t     GICache_GetMaxGi(void);
/* Finish read, release resources */
void         GICache_ReadEnd(void);
/* Internal loading interface, non MT safe */
/* Initialize cache for loading */
int         GICache_LoadStart(const char *cache_prefix, int enablesync);
/* Add gi's data to cache */
int         GICache_LoadAdd(int64_t gi, int64_t gi_len, const char* accession, int version, int is_incremental);
/* Finish load, flush modifications to disk */
int 		GiDataIndex_Commit(void);
/* Close DB handle */
int         GICache_LoadEnd(void);
/* Erase data in DB before full update */
int         GICache_DropDb(void);
/* Update meta info */
int			GICache_SetMeta(const char* Name, const char* Value);
/* Store meta info */
int         GICache_UpdateMeta(int is_incremental, const char* DB, time_t starttime);
/* Retrieve meta info */
int			GICache_GetMeta(const char* Name, char* Value, size_t ValueSz);
/* Install logger CB */
void        GICache_SetLog(void (*logfunc)(char*));
/* Dumps out content of the cache. Result is a text file with 3 columns: gi, accession, length  */
void		GICache_Dump(const char* cache_prefix, const char* filename);
int GICache_GetAccFreqTab(FreqTab* tab, const FreqTab* tablen);
#ifdef __cplusplus
}
#endif

#endif
