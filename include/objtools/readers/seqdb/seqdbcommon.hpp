#ifndef CORELIB__SEQDB__SEQDBCOMMON_HPP
#define CORELIB__SEQDB__SEQDBCOMMON_HPP

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

#include <ncbiconf.h>
#include <corelib/ncbiobj.hpp>

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

// Protein / Nucleotide / Unknown are represented by 'p', 'n', and '-'.

const char kSeqTypeProt = 'p';
const char kSeqTypeNucl = 'n';
const char kSeqTypeUnkn = '-';

// Flag specifying whether to use memory mapping.

const bool kSeqDBMMap   = true;
const bool kSeqDBNoMMap = false;

// Portable byte swapping from marshalled version

#ifdef WORDS_BIGENDIAN

template<typename T>
inline T SeqDB_GetStdOrd(const T * stdord_obj)
{
    return *stdord_obj;
}

template<typename T>
inline T SeqDB_GetBroken(const T * stdord_obj)
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

#else

template<typename T>
inline T SeqDB_GetStdOrd(const T * stdord_obj)
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
inline T SeqDB_GetBroken(const T * stdord_obj)
{
    return *stdord_obj;
}

#endif

string SeqDB_CombinePath(const string & path, const string & file, char delim);
string SeqDB_GetDirName (string s);
string SeqDB_GetFileName(string s);
string SeqDB_GetBasePath(string s);
string SeqDB_GetBaseName(string s);

class CSeqDBException : public CException
{
public:
    enum EErrCode {
        eArgErr,
        eFileErr,
        eMemErr
    };
    
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eArgErr:         return "eArgErr";
        case eFileErr:        return "eFileErr";
        case eMemErr:         return "eMemErr";
        default:              return CException::GetErrCodeString();
        }
    }
    
    NCBI_EXCEPTION_DEFAULT(CSeqDBException,CException);
};

END_NCBI_SCOPE

#endif // CORELIB__SEQDB__SEQDBCOMMON_HPP


