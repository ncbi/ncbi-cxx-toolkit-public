#ifndef _GI_CACHE_HPP__
#define _GI_CACHE_HPP__

#ifdef __cplusplus
extern "C" {
#endif

//int         GICache_PopulateAccessions(char *server,const char *cache_prefix,
//                                       const char *sql_gi_cond);
void        GICache_ReadData(const char *cache_prefix);
//void        GICache_ReMap(int delay_in_sec);
/* Accession buffer must be preallocated by caller. 
 * If buffer is too small, retrieved accession is truncated, and return code 
 * is 0, otherwise return code is 1.
 */
int        GICache_GetAccession(int gi, char* acc, int buf_len);
int         GICache_GetLength(int gi);

#ifdef __cplusplus
}
#endif

#endif
