#ifndef VDBSRC_SRASEQUTIL__H
#define VDBSRC_SRASEQUTIL__H

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

/// @file srasequtil.h
/// File contains SRA sequence processing functions.
///
/// This header file contains low-level data structures and functions used
/// for processing sequence data coming from the SRA libraries and
/// transforming it to the formats required by the Blast engine.

#include "common_priv.h"
#include "vdb_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================================================//
// Data structures

/// Structure used for passing data in VDB APIs.

struct SVDBSRC_ByteArray
{
    /// Pointer to the first byte of the sequence data array.
    uint8_t* data;
    /// Size of the sequence data array in bytes.
    uint64_t size;
    /// Compression ratio, i.e. number of bases packed in a byte (1, 2, or 4).
    uint8_t basesPerByte;
    /// Number of bases stored in the first byte (1, 2, 3, or 4).
    uint8_t basesFirstByte;
    /// Number of bases stored in the last byte (1, 2, 3, or 4).
    uint8_t basesLastByte;
    /// Number of bases
    uint64_t basesTotal;

};
typedef struct SVDBSRC_ByteArray TByteArray;

/// Structure describing the properties of requested nucleotide data.
///
/// This structure is used to compactly describe the format and other
/// properties of the nucleotide data requested from one of the low-level
/// functions. 
struct SVDBSRC_NuclDataRequest
{
    /// Retrieve the data in NCBI-4na format (if FALSE, use NCBI-2na).
    Boolean read4na;
    /// Append sentinel bytes to both ends of the data.
    Boolean hasSentinelBytes;
    /// Convert the data to the Blastna format (used in Blast engine).
    Boolean convertDataToBlastna;

    Boolean copyData;

    uint64_t readId;
};
typedef struct SVDBSRC_NuclDataRequest TNuclDataRequest;


// ==========================================================================//
// Byte array functions

/// Initialize an empty ByteArray object.
/// @param byteArray Pointer to an existing ByteArray object. [in]
void
VDBSRC_InitByteArray_Empty(TByteArray* byteArray);

/// Initialize a ByteArray object from the given sequence data.
/// @param byteArray Pointer to an existing ByteArray object. [in]
/// @param data Pointer to the sequence data array. [in]
/// @param size Size of the sequence data array in bytes. [in]
/// @param isAllocated Take ownership of the sequence data array. [in]
/// @param basesPerByte Compression ratio, number of bases per byte. [in]
/// @param basesFirstByte Number of bases stored in the first byte. [in]
/// @param basesLastByte Number of bases stored in the last byte. [in]
void
VDBSRC_InitByteArray_Data(TByteArray* byteArray,
                     uint8_t* data,
                     uint64_t size,
                     uint8_t basesPerByte,
                     uint8_t basesFirstByte,
                     uint8_t basesLastByte,
                     uint64_t basesTotal);

// ==========================================================================//
// SRA Sequence-related functions - NCBI 2na

/// Get the specified subsequence in NCBI-2na format.
///
/// Call this function to initialize the destination object as a subsequence
/// of the source object described by the given base offset and base count.
/// This function only sets the pointers and other fields in the destination
/// object, it never copies the actual sequence data.
///
/// @param dataSeq ByteArray object. [out]
/// @param req2na Read 2na request [in]
/// @return TRUE if read was successful

Boolean VDBSRC_GetSeq2na(TVDBData * vdbData,
				      TByteArray* dataSeq,
				      TNuclDataRequest* req2na,
				      TVDBErrMsg * vdbErrMsg);

Boolean VDBSRC_Convert2naToString(TByteArray* dataSeq,
                                  char** strData);

// ==========================================================================//
// SRA Sequence-related functions - NCBI 4na

/// Get the specified subsequence in NCBI-4na format.
///
/// Call this function to initialize the destination object as a subsequence
/// of the source object described by the given base offset and base count.
/// This function looks at the nucleotide data request object to determine
/// the exact 4na format requirements and whether the data should be copied.
///
/// @param vdbData vdb data handle. [in]
/// @param dataSeq Source ByteArray object. [in/out]
/// @param req4na Nucleotide data request - format and properties. [in]
/// @return TRUE if the subsequence was successfully constructed.
Boolean VDBSRC_GetSeq4naCopy(TVDBData * vdbData,
						  	 TByteArray* dataSeq,
						  	 TNuclDataRequest* req4na,
						  	 TVDBErrMsg * vdbErrMsg);


Boolean VDBSRC_Convert4naToString(TByteArray* dataSeq,
								  TNuclDataRequest* req4na,
                                  char** strData);

/// Access and convert the selected sequence to a human-readable string.
/// @param vdbData Pointer to the SRA data. [in]
/// @param oid Ordinal number of the sequence in the BlastSeqSrc object. [in]
/// @param seqIupacna Sequence string in IUPAC nucleotide format. [out]
/// @return TRUE if the sequence was found and successfully converted.
Boolean
VDBSRC_Get4naSequenceAsString(TVDBData* vdbData,
                             uint64_t oid,
                             char** seqIupacna,
                             TVDBErrMsg * vdbErrMsg);

Boolean
VDBSRC_Get2naSequenceAsString(TVDBData* vdbData,
                               uint64_t oid,
                               char** seqIupacna,
                               TVDBErrMsg * vdbErrMsg);

// ==========================================================================//

#ifdef __cplusplus
}
#endif

#endif /* VDBSRC_SRASEQUTIL__H */
