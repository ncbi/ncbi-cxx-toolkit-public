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

#include <objtools/readers/seqdb/seqdbcommon.hpp>


BEGIN_NCBI_SCOPE


// Temporary development tools/tricks

extern int seqdb_debug_class;
enum seqdb_debug_bits {
    debug_rh    = 1,
    debug_rhsum = 2,
    debug_mvol  = 4,
    debug_alias = 8,
    debug_oid   = 16
};

#define ifdebug_rh    if (seqdb_debug_class & debug_rh)    cerr
#define ifdebug_rhsum if (seqdb_debug_class & debug_rhsum) cerr
#define ifdebug_mvol  if (seqdb_debug_class & debug_mvol)  cerr
#define ifdebug_alias if (seqdb_debug_class & debug_alias) cerr
#define ifdebug_oid   if (seqdb_debug_class & debug_oid)   cerr


// Byte-order-nonspecific (long) versions

template<typename T>
inline T SeqDB_GetStdOrdUnaligned(const T * stdord_obj)
{
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

#define IS_POWER_OF_TWO(x)    ((x) & ((x)-1))
#define ALIGNED_TO_POW2(x,y)  (! ((x) & (-y)))

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

// Combine two paths, trying a little to avoid duplicated delimiters.
// If either string is empty, the other is returned.  Conceptually,
// the first path is "cwd" and the second path is the filename.  So,
// if the second path starts with "/", the first path is ignored.

string SeqDB_CombinePath(const string & path, const string & file);

// Returns a path minus filename (actually, last component).

string SeqDB_GetDirName (string s);

// Returns a filename minus greedy path.

string SeqDB_GetFileName(string s);

// Returns a filename minus non-greedy extension.

string SeqDB_GetBasePath(string s);

// Composition of the above two functions; returns a filename minus
// greedy path and non-greedy extension.

string SeqDB_GetBaseName(string s);

// Find the full name, minus extension, of a ".?al" or ".?in" file,
// and return it.  If not found, return null.

string SeqDB_FindBlastDBPath(const string & file_name, char dbtype);



END_NCBI_SCOPE

#endif // CORELIB__SEQDB__SEQDBGENERAL_HPP
