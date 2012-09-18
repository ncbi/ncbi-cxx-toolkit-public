#ifndef _GI_CACHE_HPP__
#define _GI_CACHE_HPP__

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_GI_CACHE_PATH "//panfs/pan1.be-md.ncbi.nlm.nih.gov/id_dumps/gi_cache"
#define DEFAULT_GI_CACHE_PREFIX "gi2acc"
#define DEFAULT_64BIT_SUFFIX ".64"

/* Populates the cache. One of 3 options are available to determine which gis
 * to include in cache on this run:
 * 1. Use provided SQL condition;
 * 2. Use provided temporary table (in tempdb database) with a list of gis;
 * 3. Start from the next gi after the maximal gi currently present in cache;
 */
int         GICache_PopulateAccessions(char *server, const char *cache_prefix,
                                       const char *sql_gi_cond,
                                       const char *temptable);

/* Initializes the cache. If cache_prefix argument is not provided, default name
 * is used. If local cache is not available, use default path and prefix.
 * Return value: 0 on success, 1 on failure. 
 */
int         GICache_ReadData(const char *cache_prefix);
/* Remaps cache files */
void        GICache_ReMap(int delay_in_sec);
/* Retrieves accession.version by gi.
 * Accession buffer must be preallocated by caller. 
 * If buffer is too small, retrieved accession is truncated, and return code 
 * is 0, otherwise return code is 1.
 */
int         GICache_GetAccession(int gi, char* acc, int buf_len);
/* Retrieves gi length */
int         GICache_GetLength(int gi);
/* Returns maximal gi available in cache */
int         GICache_GetMaxGi(void);

/* Internal loading interface, non MT safe */
/* Initialize cache for loading */
int         GICache_LoadStart(const char *cache_prefix);
/* Add gi's data to cache */
int         GICache_LoadAdd(int gi, int len, const char* accession, int version);
/* Finish load, flush modifications to disk */
int         GICache_LoadEnd(void);

void        GICache_SetLog(void (*logfunc)(char*)); 

#ifdef __cplusplus
}
#endif

#endif
