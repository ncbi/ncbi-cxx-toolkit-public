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

#include <inttypes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>
#include <time.h>

#include <lmdb.h>
#include "gicache.h"
#include "ncbi_toolkit.h"

#ifdef TEST_RUN
#  ifdef NDEBUG
#    undef NDEBUG
#  endif
#endif

#define MAX_ACCESSION_LENGTH	64
#define MAX_DBS					16
#define MAX_READERS				1024
#define MAP_SIZE_INIT		(256L * 1024 * 1024 * 1024)
#define MAP_SIZE_DELTA		(16L * 1024 * 1024 * 1024)
#define GI_DBI					"#GI_DBI"
#define META_DBI				"#META_DBI"
#define MAX_ROWS_PER_TRANSACTION	32

typedef struct {
    Uint1   m_ReadOnlyMode;
    char    m_FileName[PATH_MAX];
	MDB_env *m_env;
	MDB_dbi m_gi_dbi;
	MDB_dbi m_meta_dbi;
	MDB_txn *m_txn;
	int		m_txn_rowcount;
} SGiDataIndex;

static SGiDataIndex *gi_cache = NULL;

static void (*LogFunc)(char*) = NULL;

int x_Commit(SGiDataIndex* data_index) {
	int rv = 0;
	if (data_index && data_index->m_txn) {
		int rc;
		rc = mdb_txn_commit(data_index->m_txn);
		data_index->m_txn = NULL;
		data_index->m_txn_rowcount = 0;
		if (rc) {
			char logmsg[256];
			snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to commit transaction: %s\n", mdb_strerror(rc));
			if (LogFunc)
				LogFunc(logmsg);
			rv = -1;
		}
	}
	return rv;
}

int GiDataIndex_Commit(void) {
	return x_Commit(gi_cache);
}

/* Destructor */
static void GiDataIndex_Free(SGiDataIndex* data_index) {
	if (data_index) {
		if (data_index->m_txn)
			x_Commit(data_index);
		if (data_index->m_env) {
			if (data_index->m_gi_dbi) {
				mdb_dbi_close(data_index->m_env, data_index->m_gi_dbi);
				data_index->m_gi_dbi = 0;
			}
			if (data_index->m_meta_dbi) {
				mdb_dbi_close(data_index->m_env, data_index->m_meta_dbi);
				data_index->m_meta_dbi = 0;
			}
			mdb_env_close(data_index->m_env);
			data_index->m_env = NULL;
		}
		free(data_index);
		data_index = NULL;
	}
}

static int 
xFindFile(SGiDataIndex* data_index, struct stat* fstat) {
	int rv;
	rv = stat(data_index->m_FileName, fstat);
	return rv;
}

/* Constructor */
static SGiDataIndex*
GiDataIndex_New(const char* prefix, Uint1 readonly, Uint1 enablesync) { 
	int rc;
	char logmsg[256];
	SGiDataIndex*  data_index;
	struct stat fstat;
	int fstat_err;
	MDB_txn *txn = 0;
	int64_t mapsize;

    data_index = (SGiDataIndex*) malloc(sizeof(SGiDataIndex));
    memset(data_index, 0, sizeof(SGiDataIndex));
    data_index->m_ReadOnlyMode = readonly;
    assert(strlen(prefix) < PATH_MAX);
    snprintf(data_index->m_FileName, sizeof(data_index->m_FileName), "%s.db", prefix);
	fstat_err = xFindFile(data_index, &fstat);
	if (fstat_err != 0 || fstat.st_size == 0) {
		/* file does not exist */
		enablesync = 0;
		if (readonly) {
			rc = errno;
			snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to access file (%s): %s\n", data_index->m_FileName, strerror(rc));
			goto ERROR;
		}
	}

	rc = mdb_env_create(&data_index->m_env);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to init LMDB environment: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	rc = mdb_env_set_maxdbs(data_index->m_env, MAX_DBS);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to update max dbs to %d: %s\n", MAX_DBS, mdb_strerror(rc));
		goto ERROR;
	}

	rc = mdb_env_set_maxreaders(data_index->m_env, MAX_READERS);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to update max readers to %d: %s\n", MAX_READERS, mdb_strerror(rc));
		goto ERROR;
	}

	mapsize = MAP_SIZE_INIT;
	if (fstat_err == 0 && fstat.st_size + MAP_SIZE_DELTA >  mapsize)
		mapsize = fstat.st_size + MAP_SIZE_DELTA;

	rc = mdb_env_set_mapsize(data_index->m_env, mapsize);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to set map size of %" PRId64 ": %s\n", mapsize, mdb_strerror(rc));
		goto ERROR;
	}

	rc = mdb_env_open(data_index->m_env, data_index->m_FileName, MDB_NOSUBDIR | (readonly ? MDB_RDONLY : 0) | (enablesync ? 0 : (MDB_NOSYNC | MDB_NOMETASYNC)), 0644);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to open LMDB db (%s): %s\n", data_index->m_FileName, mdb_strerror(rc));
		goto ERROR;
	}

	rc = mdb_txn_begin(data_index->m_env, NULL, readonly ? MDB_RDONLY : 0, &txn);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to start transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	rc = mdb_dbi_open(txn, GI_DBI, MDB_INTEGERKEY | (readonly ? 0 : MDB_CREATE), &data_index->m_gi_dbi);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to open dbi (%s): %s\n", GI_DBI, mdb_strerror(rc));
		goto ERROR;
	}
	rc = mdb_dbi_open(txn, META_DBI, (readonly ? 0 : MDB_CREATE), &data_index->m_meta_dbi);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to open dbi (%s): %s\n", META_DBI, mdb_strerror(rc));
		goto ERROR;
	}

	rc = mdb_txn_commit(txn);
	txn = NULL;
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to commit transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}
	

    return data_index;

ERROR:
	if (txn) {
		mdb_txn_abort(txn);
		txn = NULL;
	}
	GiDataIndex_Free(data_index);
	data_index = NULL;
	if (LogFunc)
		LogFunc(logmsg);	
	return NULL;
}

static int x_DataToGiData(int64_t gi, MDB_val *data, char *acc_buf, int acc_buf_len, int64_t* gi_len) {
	char logmsg[256];
	uint8_t flags = *(uint8_t*)data->mv_data;
	int gi_len_bytes = flags & 0x7;
	int gi_len_neg = flags & 0x8;
	uint8_t acclen = *((uint8_t*)data->mv_data + gi_len_bytes + 1);
	char* pacc = (char*)data->mv_data + gi_len_bytes + 2;
	
	if (acclen >= MAX_ACCESSION_LENGTH) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: accession length (%d) exceeded limit (%d) for gi: %" PRId64 "\n", acclen, MAX_ACCESSION_LENGTH, gi);
		if (LogFunc)
			LogFunc(logmsg);
		return 1;
	}

	if (gi_len) {
		int64_t val = 0;
		memcpy(&val, ((uint8_t*)data->mv_data + 1), gi_len_bytes);
		if (gi_len_neg) {
			if (val == 0)
				val = -1;
			else
				val = -val;
		}
		*gi_len = val;
	}
	if (acc_buf) {
		if (acc_buf_len <= acclen) {
			snprintf(logmsg, sizeof(logmsg), "GI_CACHE: accession length buffer length (%d) is too small, actual accession length is %d for gi: %" PRId64 "\n", acc_buf_len, acclen, gi);
			if (LogFunc)
				LogFunc(logmsg);
			return 1;
		}
		memcpy(acc_buf, pacc, acclen);
		acc_buf[acclen] = 0;
	}
	return 0;
}

static int x_GetGiData(SGiDataIndex* data_index, int64_t gi, char *acc_buf, int acc_buf_len, int64_t* gi_len) {
	char logmsg[256];
	int rc;
	MDB_txn *txn = NULL;
	MDB_val key;
	MDB_val data = {0};
	int64_t gi64;

	logmsg[0] = 0;
	if (gi_len)
		*gi_len = 0;
	if (acc_buf && acc_buf_len > 0)
		acc_buf[0] = 0;

	if (!data_index || !data_index->m_env) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: DB is not open\n");
		goto ERROR;
	}

	rc = mdb_txn_begin(data_index->m_env, NULL, MDB_RDONLY, &txn);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to start transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	gi64 = gi;
	key.mv_size = sizeof(gi64);
	key.mv_data = &gi64;
	rc = mdb_get(txn, data_index->m_gi_dbi, &key, &data);
	if (rc == MDB_NOTFOUND) // silent error
		goto ERROR;
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to get data for gi=%" PRId64 ": %s\n", gi, mdb_strerror(rc));
		goto ERROR;
	}

	rc = x_DataToGiData(gi64, &data, acc_buf, acc_buf_len, gi_len);
	if (rc)
		goto ERROR;

	rc = mdb_txn_commit(txn);
	txn = NULL;
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to close transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	return 1;
ERROR:
	if (txn) {
		mdb_txn_abort(txn);
		txn = NULL;
	}
	if (LogFunc && logmsg[0]) {
		LogFunc(logmsg);
	}
	return 0;
}

static int64_t x_GetMaxGi(SGiDataIndex* data_index) {
	char logmsg[256];
	int rc;
	MDB_txn *txn = NULL;
	MDB_cursor *cur = NULL;
	MDB_val key;
	MDB_val data = {0};
	int64_t gi64 = 0;

	rc = mdb_txn_begin(data_index->m_env, NULL, MDB_RDONLY, &txn);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to start transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	rc = mdb_cursor_open(txn, data_index->m_gi_dbi, &cur);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to open cursor: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	key.mv_size = 0;
	key.mv_data = NULL;
	rc = mdb_cursor_get(cur, &key, &data, MDB_LAST);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to position cursor to last record: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	if (!key.mv_data || key.mv_size != sizeof(gi64)) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: last record contains no valid gi\n");
		goto ERROR;
	}
	gi64 = *(int64_t*)key.mv_data;

	mdb_cursor_close(cur);
	cur = NULL;

	rc = mdb_txn_commit(txn);
	txn = NULL;
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to close transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

    return gi64;

ERROR:
	if (cur) {
		mdb_cursor_close(cur);
		cur = NULL;
	}
	if (txn) {
		mdb_txn_abort(txn);
		txn = NULL;
	}
	if (LogFunc) {
		LogFunc(logmsg);
	}
	return -1;
}

static int x_PutData(SGiDataIndex* data_index, int64_t gi, int64_t gi_len, const char *acc, int overwrite, int is_incremental) {
	char logmsg[256];
	int rc;
	MDB_val key;
	MDB_val data = {0};
	int64_t gi64 = 0;
	int acclen, datalen;
	uint8_t *gidata = NULL;
	int gi_len_bytes = 0;
	int gi_len_neg;
	uint8_t flags;

	logmsg[0] = 0;
	acclen = strlen(acc);
	if (acclen > MAX_ACCESSION_LENGTH) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to put, provided accession is too long: %d (max supported length is %d)\n", acclen, MAX_ACCESSION_LENGTH);
		goto ERROR;
	}

	if (is_incremental) {
		char lacc[MAX_ACCESSION_LENGTH + 1];
		int64_t lgi_len = 0;
		rc = x_GetGiData(data_index, gi, lacc, sizeof(lacc), &lgi_len);
		if (rc == 1) {
			int changed_acc = 0;
			int changed_len = 0;
			changed_acc = strcmp(lacc, acc) != 0;
			changed_len = lgi_len != gi_len;
			if (!changed_acc && !changed_len)
				return RC_ALREADY_EXISTS;
			if (changed_acc)
				snprintf(logmsg, sizeof(logmsg), "GI_CACHE: gi %" PRId64 " changed accession from %s to %s\n", gi, lacc, acc);
			if (changed_len)
				snprintf(logmsg, sizeof(logmsg), "GI_CACHE: gi %" PRId64 " changed len from %" PRId64 " to %" PRId64 "\n", gi, lgi_len, gi_len);
		}
	}

	if (!data_index->m_txn) {
		rc = mdb_txn_begin(data_index->m_env, NULL, 0, &data_index->m_txn);
		if (rc) {
			data_index->m_txn = NULL;
			snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to start transaction: %s\n", mdb_strerror(rc));
			goto ERROR;
		}
	}

	gi64 = gi;	
	key.mv_size = sizeof(gi64);
	key.mv_data = &gi64;
	gi_len_neg = gi_len < 0;
	if (gi_len != 0 && gi_len != -1) {
		if (gi_len_neg)
			gi_len = -gi_len;
		int64_t v = gi_len;
		while (v != 0) {
			gi_len_bytes++;
			v >>= 8;
		}
	}
	
	flags = (gi_len_bytes & 0x7) | ((gi_len_neg ? 1 : 0) << 3);

	datalen =	1 + 			// flags
				gi_len_bytes +	// gi_len
				1 +				// acclen
				acclen;			// acc
	gidata = (uint8_t*)malloc(datalen);
	gidata[0] = flags;
	memcpy(gidata + 1, &gi_len, gi_len_bytes);
	gidata[1 + gi_len_bytes] = acclen;
	memcpy(gidata + 1 + gi_len_bytes + 1, acc, acclen);
	data.mv_size = datalen;
	data.mv_data = gidata;
	rc = mdb_put(data_index->m_txn, data_index->m_gi_dbi, &key, &data, overwrite ? 0 : MDB_NOOVERWRITE);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to start transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	if (data_index->m_txn_rowcount++ > MAX_ROWS_PER_TRANSACTION) {
		int rv = x_Commit(gi_cache);
		if (rv != 0)
			goto ERROR;
	}
	free(gidata);
	return 1;

ERROR:
	if (gidata) {
		free(gidata);
		gidata = NULL;
	}
	if (data_index->m_txn) {
		mdb_txn_abort(data_index->m_txn);
		data_index->m_txn = NULL;
	}
	if (LogFunc && logmsg[0]) {
		LogFunc(logmsg);
	}
	return 0;
}

static int x_GICacheInit(const char* prefix, Uint1 readonly, Uint1 enablesync) {
    char prefix_str[PATH_MAX];

    // First try local files
    snprintf(prefix_str, sizeof(prefix_str), "%s", (prefix ? prefix : DEFAULT_GI_CACHE_PREFIX));

    /* When reading data, use readonly mode. */
    if (gi_cache) {
		GiDataIndex_Free(gi_cache);
		gi_cache = NULL;
	}
    gi_cache = GiDataIndex_New(prefix_str, readonly, !readonly && enablesync);

    if (readonly) {
        /* Check whether gi cache is available at this location, by trying to
         * map it right away. If local cache isn't found, use default path and
         * try again.
         * NB: This is only done if nothing was provided in the input argument.
         */
        Uint1 cache_found = gi_cache != NULL;

        if (!cache_found && !prefix) {
            snprintf(prefix_str, sizeof(prefix_str), "%s/%s.", DEFAULT_GI_CACHE_PATH, DEFAULT_GI_CACHE_PREFIX);            
            gi_cache = GiDataIndex_New(prefix_str, readonly, 0);
        }
    }

    return (gi_cache ? 0 : 1);    
}

int GICache_ReadData(const char *prefix) {
    int rc;
    rc = x_GICacheInit(prefix, 1, 0);
    return rc;
}

int GICache_GetAccession(int64_t gi, char* acc, int buf_len) {
	int retval = 0;
	int rv;
	if (acc && buf_len > 0)
		acc[0] = NULLB;
	if (!gi_cache) {
		if (LogFunc)
			LogFunc("GICache_GetAccession: GI Cache is not initialized, call GICache_ReadData() first");
		return 0;
	}
	rv = x_GetGiData(gi_cache, gi, acc, buf_len, NULL);
	if (rv)
		retval = 1;
	return retval;
}

int64_t GICache_GetLength(int64_t gi) {
	int rv;
	int64_t gi_len = 0;
    if(!gi_cache) 
		return 0;
    rv = x_GetGiData(gi_cache, gi, NULL, 0, &gi_len);

    if(!rv) 
		return 0;

    return gi_len;
}

int64_t GICache_GetMaxGi(void) {
    if(!gi_cache) return 0;
    return x_GetMaxGi(gi_cache);
}

int GICache_LoadStart(const char* cache_prefix, int enablesync) {
    int rc;
    rc = x_GICacheInit(cache_prefix, 0, enablesync);
    return rc;
}

void GICache_ReadEnd() {
	if (gi_cache) {
		GiDataIndex_Free(gi_cache);
		gi_cache = NULL;
	}
}

int GICache_LoadAdd(int64_t gi, int64_t gi_len, const char* acc, int version, int is_incremental) {
    int acc_len;

    static char accbuf[MAX_ACCESSION_LENGTH];
    if(!gi_cache) return 0;

	if (version > 0)
		snprintf(accbuf, sizeof(accbuf), "%s.%d", acc, version);
	else 
		snprintf(accbuf, sizeof(accbuf), "%s", acc);
    
    return x_PutData(gi_cache, gi, gi_len, accbuf, 1, is_incremental);
}

int GICache_LoadEnd() {
	if (gi_cache) {
		x_Commit(gi_cache);
		GiDataIndex_Free(gi_cache);
		gi_cache = NULL;
	}
    return 0;
}

void GICache_SetLog(void (*logfunc)(char*)) {
    LogFunc = logfunc;
}

void GICache_Dump(const char* cache_prefix, const char* filename, volatile int * quitting) {
	FILE* f;
	char logmsg[256];
	int needopen;
	int rc;
	MDB_txn *txn = NULL;
	MDB_cursor *cur = NULL;
	MDB_val key;
	MDB_val data = {0};

	needopen = gi_cache == NULL;
	if (needopen) {
		rc = x_GICacheInit(cache_prefix, 1, 0);
		if(!gi_cache) return;
	}
	f = fopen(filename, "w");
	if (!f) {
		snprintf(logmsg, sizeof(logmsg), "Failed to open file %s, error: %d", filename, errno);
		goto ERROR;
	}
	setvbuf(f, NULL, _IOFBF, 128 * 1024);

	rc = mdb_txn_begin(gi_cache->m_env, NULL, MDB_RDONLY, &txn);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to start transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	rc = mdb_cursor_open(txn, gi_cache->m_gi_dbi, &cur);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to open cursor: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	key.mv_size = 0;
	key.mv_data = NULL;
	while ((rc = mdb_cursor_get(cur, &key, &data, MDB_NEXT)) == MDB_SUCCESS) {
		char acc_buf[MAX_ACCESSION_LENGTH];
		int64_t gi_len = 0;
		acc_buf[0] = 0;
		int64_t gi64 = 0;

		if (!key.mv_data || key.mv_size != sizeof(gi64)) {
			snprintf(logmsg, sizeof(logmsg), "GI_CACHE: last record contains no valid gi\n");
			if (LogFunc)
				LogFunc(logmsg);
			continue;
		}
		gi64 = *(int64_t*)key.mv_data;
		if (x_DataToGiData(gi64, &data, acc_buf, sizeof(acc_buf), &gi_len) == 0) {
			char line[512];
			snprintf(line, sizeof(line), "%" PRId64 " %s %" PRId64 "\n", gi64, acc_buf, gi_len);
			fputs(line, f);
		}
		if (quitting && *quitting)
			break;
	}

	mdb_cursor_close(cur);
	cur = NULL;

	rc = mdb_txn_commit(txn);
	txn = NULL;
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to close transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	fclose(f);
	if (needopen) {
		GICache_ReadEnd();
	}
	return;

ERROR:
	if (f) {
		fclose(f);
		f = NULL;
	}
	if (cur) {
		mdb_cursor_close(cur);
		cur = NULL;
	}
	if (txn) {
		mdb_txn_abort(txn);
		txn = NULL;
	}
	if (LogFunc) {
		LogFunc(logmsg);
	}
	if (needopen) {
		GICache_ReadEnd();
	}

}

int GICache_DropDb() {
	int rc;
	char logmsg[256];
	int transtarted = 0;

	if (!gi_cache || !gi_cache->m_env) {
		snprintf(logmsg, sizeof(logmsg), "GICache_DropDb: failed to drop DB, database is not open");
		goto ERROR;
	}
	if (gi_cache->m_ReadOnlyMode) {
		snprintf(logmsg, sizeof(logmsg), "GICache_DropDb: failed to drop DB, database is open in readonly mode");
		goto ERROR;
	}
	if (gi_cache->m_txn) {
		snprintf(logmsg, sizeof(logmsg), "GICache_DropDb: failed to drop DB, database has an active transaction");
		goto ERROR;
	}
	rc = mdb_txn_begin(gi_cache->m_env, NULL, 0, &gi_cache->m_txn);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to start transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}
	transtarted = 1;
	rc = mdb_drop(gi_cache->m_txn, gi_cache->m_gi_dbi, 0);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GICache_DropDb: failed to drop DB: %s\n", mdb_strerror(rc));
		goto ERROR;
	}
	rc = mdb_drop(gi_cache->m_txn, gi_cache->m_meta_dbi, 0);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GICache_DropDb: failed to drop meta DB: %s\n", mdb_strerror(rc));
		goto ERROR;
	}
	rc = mdb_txn_commit(gi_cache->m_txn);
	gi_cache->m_txn = NULL;
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to close transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}
	
    return 0;
ERROR:
	if (LogFunc) {
		LogFunc(logmsg);
	}
	if (gi_cache && gi_cache->m_txn && transtarted) {
		mdb_txn_abort(gi_cache->m_txn);
		gi_cache->m_txn = NULL;
	}
	return 1;
}

static int x_PutMeta(const char* Name, const char* Value) {
	MDB_val key;
	MDB_val data;
	int rc;

	key.mv_data = (char*)Name;
	key.mv_size = strlen(Name);
	if (Value) {
		data.mv_data = (char*)Value;
		data.mv_size = strlen(Value);
		rc = mdb_put(gi_cache->m_txn, gi_cache->m_meta_dbi, &key, &data, 0);
	}
	else {
		rc = mdb_del(gi_cache->m_txn, gi_cache->m_meta_dbi, &key, NULL);
		if (rc == MDB_NOTFOUND)
			rc = 0;
	}
	if (rc) {
		char logmsg[256];
		snprintf(logmsg, sizeof(logmsg), "GICache_UpdateMeta: failed to update META: %s\n", mdb_strerror(rc));
		if (LogFunc)
			LogFunc(logmsg);
	}
	return rc;
}

int GICache_SetMeta(const char* Name, const char* Value) {
	int rc;
	char logmsg[256];
	int transtarted = 0;	
	char meta[512], stime[128];

	logmsg[0] = 0;
	if (!gi_cache || !gi_cache->m_env) {
		snprintf(logmsg, sizeof(logmsg), "GICache_SetMeta: failed to update META, database is not open");
		goto ERROR;
	}
	if (gi_cache->m_ReadOnlyMode) {
		snprintf(logmsg, sizeof(logmsg), "GICache_SetMeta: failed to update META, database is open in readonly mode");
		goto ERROR;
	}
	if (gi_cache->m_txn) {
		snprintf(logmsg, sizeof(logmsg), "GICache_SetMeta: failed to update META, database has an active transaction");
		goto ERROR;
	}
	rc = mdb_txn_begin(gi_cache->m_env, NULL, 0, &gi_cache->m_txn);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to start transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}
	rc = x_PutMeta(Name, Value);
	if (rc)
		goto ERROR;
	rc = mdb_txn_commit(gi_cache->m_txn);
	gi_cache->m_txn = NULL;
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to commit transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}
	
    return 0;
ERROR:
	if (LogFunc && logmsg[0]) {
		LogFunc(logmsg);
	}
	if (gi_cache && gi_cache->m_txn && transtarted) {
		mdb_txn_abort(gi_cache->m_txn);
		gi_cache->m_txn = NULL;
	}
	return 1;
}

int GICache_UpdateMeta(int is_incremental, const char* DB, time_t starttime) {
	int rc;
	char logmsg[256];
	char meta[512], stime[128];

	logmsg[0] = 0;
	if (gethostname(meta, sizeof(meta)) != 0)
		meta[0] = 0;
	rc = GICache_SetMeta(is_incremental ? META_INCREMENTAL_HOST : META_FULL_HOST, meta);
	if (rc)
		goto ERROR;
	rc = GICache_SetMeta(is_incremental ? META_INCREMENTAL_DB : META_FULL_DB, DB);
	if (rc)
		goto ERROR;	
	snprintf(stime, sizeof(stime), "%" PRId64, (int64_t)starttime);
	rc = GICache_SetMeta(is_incremental ? META_INCREMENTAL_TIME : META_FULL_TIME, stime);
	if (rc)
		goto ERROR;
    return 0;
ERROR:
	if (LogFunc && logmsg[0]) {
		LogFunc(logmsg);
	}
	return 1;
}

int	GICache_GetMeta(const char* Name, char* Value, size_t ValueSz) {
	char logmsg[256];
	int rc;
	MDB_txn *txn = NULL;
	MDB_val key;
	MDB_val data;

	Value[0] = 0;
	logmsg[0] = 0;
	if (!gi_cache || !gi_cache->m_env) {
		snprintf(logmsg, sizeof(logmsg), "GICache_GetMeta: failed to read META, database is not open");
		goto ERROR;
	}
	rc = mdb_txn_begin(gi_cache->m_env, NULL, MDB_RDONLY, &txn);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to start transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}
	key.mv_size = strlen(Name);
	key.mv_data = (char*)Name;
	data.mv_size = 0;
	data.mv_data = 0;
	rc = mdb_get(txn, gi_cache->m_meta_dbi, &key, &data);
	if (rc == MDB_NOTFOUND) // silent error
		goto ERROR;
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GICache_GetMeta: failed to read meta: %s\n", mdb_strerror(rc));
		goto ERROR;
	}
	snprintf(Value, ValueSz, "%.*s", (int)data.mv_size, (const char*)data.mv_data);
	
	rc = mdb_txn_commit(txn);
	txn = NULL;
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to close transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}
	return 0;

ERROR:
	if (LogFunc && logmsg[0]) {
		LogFunc(logmsg);
	}
	if (gi_cache && txn) {
		mdb_txn_abort(txn);
		txn = NULL;
	}
	return 1;
}


int GICache_GetAccFreqTab(FreqTab* tab, const FreqTab* tablen) {
	char logmsg[256];
	int rc;
	MDB_txn *txn = NULL;
	MDB_cursor *cur = NULL;
	MDB_val key;
	MDB_val data = {0};
	int64_t totlen = 0, totcomprlen = 0;

	memset(tab, 0, sizeof(*tab));
	logmsg[0] = 0;
	if (!gi_cache || !gi_cache->m_env) {
		snprintf(logmsg, sizeof(logmsg), "GICache_GetAccFreqTab: failed to get frequency table, database is not open");
		goto ERROR;
	}
	
	rc = mdb_txn_begin(gi_cache->m_env, NULL, MDB_RDONLY, &txn);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to start transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	rc = mdb_cursor_open(txn, gi_cache->m_gi_dbi, &cur);
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to open cursor: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	key.mv_size = 0;
	key.mv_data = NULL;
	while ((rc = mdb_cursor_get(cur, &key, &data, MDB_NEXT)) == MDB_SUCCESS) {
		char acc_buf[MAX_ACCESSION_LENGTH];
		int64_t gi_len = 0;
		acc_buf[0] = 0;
		int64_t gi64 = 0;

		if (!key.mv_data || key.mv_size != sizeof(gi64)) {
			snprintf(logmsg, sizeof(logmsg), "GI_CACHE: record contains no valid gi\n");
			if (LogFunc)
				LogFunc(logmsg);
			continue;
		}
		gi64 = *(int64_t*)key.mv_data;
		if (x_DataToGiData(gi64, &data, acc_buf, sizeof(acc_buf), NULL) == 0) {
			char *p = acc_buf;
			int no_compr = 0;
			int compr = 0;
			int len = 0;
			while (*p) {
				tab->Count[(unsigned int)(*p)]++;
				if (tablen) {
					int bits = tablen->Count[(unsigned int)(*p)];
					if (bits == 0)
						no_compr = 1;
					else
						compr += bits;
				}
				else
					no_compr = 1;
				p++;
				len++;
			}
			totlen += len;
			if (no_compr)
				totcomprlen += len;
			else
				totcomprlen += (compr + 7) / 8;
		}
	}

	mdb_cursor_close(cur);
	cur = NULL;

	rc = mdb_txn_commit(txn);
	txn = NULL;
	if (rc) {
		snprintf(logmsg, sizeof(logmsg), "GI_CACHE: failed to close transaction: %s\n", mdb_strerror(rc));
		goto ERROR;
	}

	if (totlen == 0)
		totlen = 1;
	return (totcomprlen * 100L) / totlen;

ERROR:
	if (cur) {
		mdb_cursor_close(cur);
		cur = NULL;
	}
	if (txn) {
		mdb_txn_abort(txn);
		txn = NULL;
	}
	if (LogFunc && logmsg[0]) {
		LogFunc(logmsg);
	}
	return -1;
	
}