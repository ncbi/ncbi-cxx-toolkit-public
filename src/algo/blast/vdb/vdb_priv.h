#ifndef VDBSRC_SRADB_PRIV__H
#define VDBSRC_SRADB_PRIV__H

/*  $Id$
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
 *
 * ===========================================================================
 *
 * Author:  Vahram Avagyan
 *
 */

/// @file vdb_priv.h
/// File contains internal structures and functions for reading VDB databases.
///
/// This header file contains the top-level internal data structures and
/// functions used for reading data from the SRA databases. It includes
/// functions for initializing the SRA data access and reading individual
/// sequences identified by OIDs. It also includes various mapping functions.

#include "common_priv.h"
#include "error_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SVDBSRC_SeqSrc_2na_IterReader
{
	VdbBlast2naReader * reader;
	Packed2naRead * buffer;
	// total # of reads in current buf
	uint32_t max_index;
	uint32_t current_index;
	uint8_t * chunked_buf; //for storing ptr to memory allocated for chunked seq
	uint32_t chunked_buf_size;
};
typedef struct SVDBSRC_SeqSrc_2na_IterReader TVDB2naICReader;

struct  SVDBSRC_Partial_Fetching_Ranges {
    /** Oid in BLAST database, index in an array of sequences, etc [in] */
    Int4 oid;
    /** Number of actual ranges contained */
    Int4 num_ranges;
    Int4 * ranges;
};
typedef struct  SVDBSRC_Partial_Fetching_Ranges TVDBPartialFetchingRanges;


// ==========================================================================//

/// Structure providing top-level VDB data access.
///
/// This structure stores a handle to the VDB manager used for opening
/// runs in the database. It is used as a top-level data access
/// object by this library.
struct SVBDSRC_SeqSrc_Data
{
    /// vdb manager used for opening the runs.
    VdbBlastMgr* hdlMgr;

    /// Number of open VDB runs.
    uint32_t numRuns;

    uint64_t numSeqs;

    VdbBlastRunSet * runSet;
    VdbBlastReferenceSet * refSet;

    /// Names of the VDB data represented by this object
    /// (usually this will include all the SVDB run accessions).
    char* names;

   	TVDB2naICReader * reader_2na;
   	VdbBlast4naReader * reader_4na;

   	TVDBPartialFetchingRanges * range_list;

    /// Is the object initialized.
    Boolean isInitialized;
};
typedef struct SVBDSRC_SeqSrc_Data TVDBData;

/// Structure used for initializing the SRA data access.
///
/// This structure stores the string values necessary to initialize
/// the SRA manager and the run accessions to open in the SRA database.
struct SVDBSRC_NewArgs
{
    /// Array of SRA accession strings identifying the runs to open.
    const char** vdbRunAccessions;
    /// Number of runs to open.
    uint32_t numRuns;

    Boolean isProtein;

    // BlastSeqSrcNew Return value
    // Indicate if run in corresponding
    // vdbRunAccessions array has been
    // excluded in he run set or not
    Boolean * isRunExcluded;

    //return status
    // 0 - successful
    // -1 < failed
    // > 0 # of runs excluded
    uint32_t status;

    // Make a seqsrc for CSRA if set to true
    Boolean isCSRA;
};
typedef struct SVDBSRC_NewArgs TVDBNewArgs;

// ==========================================================================//
// SRA Data functions

/// Get the maximum sequence length in the open SRA data.
/// @param vdbData Pointer to the SRA data. [in]
/// @return Maximum sequence length (in bases).
/// @note this function returns an approximation (upper bound)
uint64_t
VDBSRC_GetMaxSeqLen(TVDBData* vdbData);

/// Get the average sequence length in the open SRA data.
/// @param vdbData Pointer to the SRA data. [in]
/// @return Average sequence length (in bases).
/// @note this function returns an approximation
uint64_t
VDBSRC_GetAvgSeqLen(TVDBData* vdbData);

/// Get the total sequence length in the open SRA data.
/// @param vdbData Pointer to the SRA data. [in]
/// @return Total sequence length (in bases).
/// @note this function returns an approximation (upper bound)
uint64_t
VDBSRC_GetTotSeqLen(TVDBData* vdbData);

/// Get sequence length by oid
/// @param vdbData Pointer to the SRA data. [in]
/// @param oid [in]
/// @return Total sequence length (in bases).
/// @note this function returns an approximation (upper bound)
uint64_t
VDBSRC_GetSeqLen(TVDBData* vdbData, uint64_t oid);

/// Get if run set is protein or nucl
/// @param vdbData Pointer to the VDB data. [in]
/// @return  true if runset is protein, otherwise false [out]
bool
VDBSRC_GetIsProtein(TVDBData* vdbData);

/// Allocate a new SRA data object, flag it as not initialized.
/// @param sraErrMsg Pointer to an existing Error message object. [out]
/// @return Pointer to the newly allocated SRA data.
TVDBData*
VDBSRC_NewData(TVDBErrMsg* vdbErrMsg);


/// Initialize the VDB data.
///
/// This function takes a TVDBNewArgs structure and initializes the
/// VDB data by setting up the VDB manager and adding VDB runs
/// to the VDB RunSet.
///
/// @param vdbData Pointer to the destination VDB data. [in]
/// @param sraArgs Pointer to a TSRANewArgs structure. [in]
/// @param sraErrMsg Pointer to an existing Error message object. [out]
/// @return 0  if No error, all runs have been added successfully
///         -1 if failed to initialize Mgr or RunSet
///				  Or failed to add any run to RunSet
///          >0  # of runs not added to RunSet
void
VDBSRC_InitData(TVDBData* vdbData,
                TVDBNewArgs* vdbArgs,
                TVDBErrMsg* vdbErrMsg,
                Boolean getStats);

/// Release the SRA data and free the memory allocated for the object.
/// @param vdbData Pointer to the SRA data to free. [in]
/// @param sraErrMsg Pointer to an existing Error message object. [out]
void
VDBSRC_FreeData(TVDBData* vdbData);

void
VDBSRC_ResetReader(TVDBData * vdbData);

void
VDBSRC_ResetReader_2na(TVDBData * vdbData);

void
VDBSRC_ResetReader_4na(TVDBData * vdbData);

void
VDBSRC_Init2naReader(TVDBData* vdbData, TVDBErrMsg* vdbErrMsg);

void
VDBSRC_Init4naReader(TVDBData* vdbData, TVDBErrMsg* vdbErrMsg);


/// Need to call free data if error is returned
TVDBData*
VDBSRC_CopyData(TVDBData* vdbData,
                TVDBErrMsg* vdbErrMsg);
/// Get the SRA-specific sequence information for the given OID.
/// @param vdbData Pointer to the SRA data. [in]
/// @param oid Ordinal number of the sequence in the BlastSeqSrc object. [in]
/// @param nameRun Pointer to the SRA Run accession. [out]
/// @param buf_size buffer size for nameRun [in]
/// @return TRUE if the OID was found and successfully converted to SRA info.
Boolean
VDBSRC_GetReadNameForOID(TVDBData* vdbData,
                       Int4 oid,
                       char* nameRun,
                       size_t buf_size);

/// Get the sequence OID given its SRA-specific sequence information.
/// @param vdbData Pointer to the SRA data. [in]
/// @param nameRun SRA Run accession. [in]
/// @param indexSpot SRA Spot ID in the Run. [in]
/// @param indexRead SRA Read index (e.g. 1,2) in the Spot. [in]
/// @param oid Ordinal number of the sequence in the BlastSeqSrc object. [out]
/// @return TRUE if the SRA info was successfully converted to an OID.
Boolean
VDBSRC_GetOIDFromReadName(TVDBData* vdbData,
                          const char* nameRun,
                          Int4* oid);

Int2
VDBSRC_Load2naSeqToBuffer(TVDBData * vdbData, TVDBErrMsg * vdbErrMsg);

/// This will call VdbBlastInit and intiailize a singleton for VDBBlastMgr
/// This needs to be called in the main thread first before mutlithreading
VdbBlastMgr* VDBSRC_GetVDBManager(uint32_t * status);

/// This needs to be called if VDBSRC_GetVDBManager has been called in the main thread
void VDBSRC_ReleaseVDBManager();

/// Return 1 if run is csra, 0 if not and -1 for error
int
VDBSRC_IsCSRA(const char * run);

void
VDBSRC_InitCSRAData(TVDBData* vdbData, TVDBNewArgs* vdbArgs,
                    TVDBErrMsg* vdbErrMsg, Boolean getStats);

void
VDBSRC_MakeCSRASeqSrcFromSRASeqSrc(TVDBData* vdbData, TVDBErrMsg* vdbErrMsg, Boolean getStats);

void VDBSRC_ResetPartialFetchingList(TVDBData * vdbData);
void VDBSRC_FillPartialFetchingList(TVDBData* vdbData, Int4 num_ranges);

#define VDB_REF_SEQ_MAX_SIZE (1000000000)
#define VDB_2NA_CHUNK_BUF_SIZE (VDB_REF_SEQ_MAX_SIZE/COMPRESSION_RATIO + 1)
#define VDB_4NA_CHUNK_BUF_SIZE (VDB_REF_SEQ_MAX_SIZE)
#define REF_SEQ_ID_MASK (0x8000000000000000)
// ==========================================================================//

#ifdef __cplusplus
}
#endif

#endif /* VDBSRC_SRADB_PRIV__H */
