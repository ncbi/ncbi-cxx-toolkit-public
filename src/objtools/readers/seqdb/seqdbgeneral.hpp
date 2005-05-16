#ifndef CORELIB__SEQDB__SEQDBGENERAL_HPP
#define CORELIB__SEQDB__SEQDBGENERAL_HPP

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
 * Author:  Kevin Bealer
 *
 */


/// @file seqdbgeneral.hpp
/// This file defines several SeqDB utility functions related to byte
/// order and file system portability.
/// Implemented for: UNIX, MS-Windows


#include <objtools/readers/seqdb/seqdbcommon.hpp>
#include <corelib/ncbi_bswap.hpp>
#include "seqdbatlas.hpp"


BEGIN_NCBI_SCOPE

// Byte-order-nonspecific (long) versions

template<typename T>
inline T SeqDB_GetStdOrdUnaligned(const T * stdord_obj)
{
#ifdef WORDS_BIGENDIAN
    unsigned char * stdord =
	(unsigned char*)(stdord_obj);
    
    unsigned char * pend = stdord + sizeof(T) - 1;
    unsigned char * pcur = stdord;
    
    T retval = *pcur;
    
    while(pcur < pend) {
	retval <<= 8;
	retval += *++pcur;
    }
    
    return retval;
#else
    if (sizeof(T) == 8) {
        return (T) CByteSwap::GetInt8((unsigned char *) stdord_obj);
    } else if (sizeof(T) == 4) {
        return (T) CByteSwap::GetInt4((unsigned char *) stdord_obj);
    } else if (sizeof(T) == 2) {
        return (T) CByteSwap::GetInt2((unsigned char *) stdord_obj);
    }
    
    _ASSERT(sizeof(T) == 1);
    
    return *stdord_obj;
#endif
}

template<typename T>
inline T SeqDB_GetBrokenUnaligned(const T * stdord_obj)
{
    unsigned char * stdord =
	(unsigned char*)(stdord_obj);
    
    unsigned char * pend = stdord;
    unsigned char * pcur = stdord + sizeof(T) - 1;
    
    T retval = *pcur;
    
    while(pcur > pend) {
	retval <<= 8;
	retval += *--pcur;
    }
    
    return retval;
}

// Macro Predicates for binary qualities

#define IS_POWER_OF_TWO(x)    (((x) & ((x)-1)) == 0)
#define ALIGNED_TO_POW2(x,y)  (! ((x) & (0-y)))

#define PTR_ALIGNED_TO_SELF_SIZE(x) \
    (IS_POWER_OF_TWO(sizeof(*x)) && ALIGNED_TO_POW2((size_t)(x), sizeof(*x)))


// Portable byte swapping from marshalled version

#ifdef WORDS_BIGENDIAN

template<typename T>
inline T SeqDB_GetStdOrd(const T * stdord_obj)
{
    if (PTR_ALIGNED_TO_SELF_SIZE(stdord_obj)) {
        return *stdord_obj;
    } else {
        return SeqDB_GetStdOrdUnaligned(stdord_obj);
    }
}

template<typename T>
inline T SeqDB_GetBroken(const T * stdord_obj)
{
    return SeqDB_GetBrokenUnaligned(stdord_obj);
}

#else

template<typename T>
inline T SeqDB_GetStdOrd(const T * stdord_obj)
{
    return SeqDB_GetStdOrdUnaligned(stdord_obj);
}

template<typename T>
inline T SeqDB_GetBroken(const T * stdord_obj)
{ 
    if (PTR_ALIGNED_TO_SELF_SIZE(stdord_obj)) {
        return *stdord_obj;
    } else {
        return SeqDB_GetBrokenUnaligned(stdord_obj);
    }
}

#endif

/// Combine a filesystem path and file name
///
/// Combine a provided filesystem path and a file name.  This function
/// tries to avoid duplicated delimiters.  If either string is empty,
/// the other is returned.  Conceptually, the first path might be the
/// current working directory and the second path is a filename.  So,
/// if the second path starts with "/", the first path is ignored.
/// Also, care is taken to avoid duplicated delimiters.  If the first
/// path ends with the delimiter character, another delimiter will not
/// be added between the strings.  The delimiter used will vary from
/// operating system to operating system, and is adjusted accordingly.
///
/// @param path
///   The filesystem path to use
/// @param file
///   The name of the file (may include path components)
/// @return
///   A combination of the path and the file name
string SeqDB_CombinePath(const string & path, const string & file);

/// Returns a path minus filename.
///
/// This returns the part of a file path before the last path
/// delimiter, or the whole path if no delimiter was found.
///
/// @param s
///   Input path
/// @return
///   Path minus file extension
string SeqDB_GetDirName(string s);

/// Returns a filename minus greedy path.
///
/// This returns the part of a file name after the last path
/// delimiter, or the whole path if no delimiter was found.
///
/// @param s
///   Input path
/// @return
///   Filename portion of path
string SeqDB_GetFileName(string s);

/// Returns a filename minus greedy path.
///
/// This returns the part of a file name after the last path
/// delimiter, or the whole path if no delimiter was found.
///
/// @param s
///   Input path
/// @return
///   Path minus file extension
string SeqDB_GetBasePath(string s);

/// Returns a filename minus greedy path.
///
/// This is just the composition of the SeqDB_GetDirName and
/// SeqDB_GetFileName functions; it returns a filename minus greedy
/// path and non-greedy extension.
///
/// @param s
///   Input path
/// @return
///   Filename portion of path, minus extension
string SeqDB_GetBaseName(string s);

// Find the full name, minus extension, of a ".?al" or ".?in" file,
// and return it.  If not found, return null.

/// Finds a file in the search path.
///
/// This function resolves the full name of a file.  It searches for a
/// file of the provided base name and returns the provided name with
/// the full path attached.  If the exact_name flag is set, the file
/// is assumed to have any extension it may need, and none is added
/// for searching or stripped from the return value.  If exact_name is
/// not set, the file is assumed to end in ".pin", ".nin", ".pal", or
/// ".nal", and if such a file is found, that extension is stripped
/// from the returned string.  Furthermore, in the exact_name == false
/// case, only file extensions relevant to the dbtype are considered.
/// Thus, if dbtype is set to 'p' for protein, only ".pin" and ".pal"
/// are checked for; if it is set to nucleotide, only ".nin" and
/// ".nal" are considered.  The places where the file may be found are
/// dependant on the search path.  The search path consists of the
/// current working directory, the contents of the BLASTDB environment
/// variable, the BLASTDB member of the BLAST group of settings in the
/// NCBI meta-registry.  This registry is an interface to settings
/// found in (for example) a ".ncbirc" file found in the user's home
/// directory (but several paths are usually checked).  Finally, if
/// the provided file_name starts with the default path delimiter
/// (which is OS dependant, but for example, "/" on Linux), the path
/// will be taken to be absolute, and the search path will not affect
/// the results.
/// 
/// @param file_name
///   File base name for which to search
/// @param dbtype
///   Input file base name
/// @param sp
///   If non-null, the ":" delimited search path is returned here
/// @param exact
///   If true, the file_name already includes any needed extension
/// @param atlas
///   The memory management layer.
/// @param locked
///   The lock holder object for this thread.
/// @return
///   Fully qualified filename and path, minus extension
string SeqDB_FindBlastDBPath(const string   & file_name,
                             char             dbtype,
                             string         * sp,
                             bool             exact,
                             CSeqDBAtlas    & atlas,
                             CSeqDBLockHold & locked);

/// Join two strings with a delimiter
///
/// This function returns whichever of two provided strings is
/// non-empty.  If both are non-empty, they are joined with a
/// delimiter placed between them.  It is intended for use when
/// combining strings, such as a space delimited list of database
/// volumes.  It is probably not suitable for joining file system
/// paths with filenames (use something like SeqDB_CombinePaths).
///
/// @param a
///   First component and returned path
/// @param b
///   Second component
/// @param delim
///   The delimiter to use when joining elements
void SeqDB_JoinDelim(string & a, const string & b, const string & delim);

/// Thow a SeqDB exception; this is seperated into a function
/// primarily to allow a breakpoint to be set.
void SeqDB_ThrowException(CSeqDBException::EErrCode code, const string & msg);

END_NCBI_SCOPE

#endif // CORELIB__SEQDB__SEQDBGENERAL_HPP

