#ifndef SRASRC_COMMON_PRIV__H
#define SRASRC_COMMON_PRIV__H

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

/// @file common_priv.h
/// File contains common includes and definitions used internally by the library.
///
/// This header includes the most common files used internally by the library,
/// including the SRA headers and Blast core headers. It defines constants
/// used for retrieving and storing data from the SRAs, including relevant
/// column names and data types, thresholds for dropping tiny reads, maximum
/// allowed storage space for names, etc.
///
/// General notes
///
/// The following variable naming conventions were used:
/// 
/// - hdlX
///     SRA library handle to object X (e.g. hdlTable, hdlMgr, hdlCol)
/// - sraX
///     SRA-related object X defined in this library, that either wraps
///     around an SRA handle or describes a lower-level object having no
///     direct analogies in the SRA library (e.g. sraSpot, sraColumn)
/// - dataX, sizeX, offsetX
///     the data, size and offset values read from an SRA column for object X
///     (e.g. dataRaw2na, offsetRaw2na, sizeRaw2na)
/// - nX, numX, lenX, sizeX, iX, indexX
///     integer values storing general-purpose numbers, lengths, sizes,
///     loop counters, indices, etc.
/// - isX, hasX
///     boolean values
/// 
/// Naming conventions for functions and globals/constants:
///
/// - Always preceded by SRASRC_, constants are all capitalized
/// 
/// - NewX, InitX, OpenX, CopyX, ReleaseX, FreeX
///     correspond to functions for allocating, intializing, opening, copying,
///     releasing/finalizing, and deallocating the objects of type X
///
/// Naming conventions for structures and other types:
/// 
/// - Structs are preceded by SSRASRC_, enums by ESRASRC_
/// - Typedefs of the form TShortName are provided for shorter notation
///     (e.g. TByteArray = SSRASRC_ByteArray)
/// 
/// Basic types:
/// 
/// - For internal functions, the SRA library conventions were used,
///     i.e. integer types named uint8_t, uint32_t, etc
/// - For top-level functions called from Blast, the corresponding Blast
///     conventions were used, i.e. integer types named Uint1, Uint4, etc
/// 

// ==========================================================================//

// Blast includes
#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_seqsrc_impl.h>
#include <algo/blast/core/blast_util.h>
#include <corelib/ncbi_limits.h>

// VDB includes
#include <ncbi/vdb-blast.h>
#include <ncbi/vdb-blast-priv.h>

// ==========================================================================//
// Definitions / Constants

/// Maximum allowed length of a spot name.
#define VDBSRC_MAX_SPOT_NAME                  256
/// Any read shorter than this threshold should be dropped.
#define VDBSRC_MIN_READ_LENGTH                8
/// Maximum length of the name string for loaded SRA data.
#define VDBSRC_MAX_SRA_NAME                   256

//Return value if overflow occurs
#define VDBSRC_OVERFLOW_RV					  -1

//Packed2naRead Default Buffer Size
#define VDBSRC_CACHE_ITER_BUF_SIZE			 1000


// ==========================================================================//
// Types / Typedefs

/// OID type (must be a SIGNED integer type).
typedef Int4 oid_t;

// ==========================================================================//

#endif /* SRASRC_COMMON_PRIV__H */
