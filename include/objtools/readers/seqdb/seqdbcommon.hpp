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

/// @file seqdbcommon.hpp
/// Defines exception class and several constants for SeqDB.
/// 
/// Defines classes:
///     CSeqDBException
/// 
/// Implemented for: UNIX, MS-Windows

#include <ncbiconf.h>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

// Publically visible seqdb related definitions.

class CSeqDBException : public CException {
public:
    /// Errors are classified into one of three types.
    enum EErrCode {
        eArgErr,
        eFileErr,
        eMemErr
    };
    
    /// Get a message describing the situation leading to the throw.
    virtual const char* GetErrCodeString() const
    {
        switch ( GetErrCode() ) {
        case eArgErr:  return "eArgErr";
        case eFileErr: return "eFileErr";
        case eMemErr:  return "eMemErr";
        default:       return CException::GetErrCodeString();
        }
    }
    
    NCBI_EXCEPTION_DEFAULT(CSeqDBException,CException);
};


/// Used to indicate protein sequences.
const char kSeqTypeProt = 'p';

/// Used to indicate nucleotide sequences.
const char kSeqTypeNucl = 'n';

/// Used to indicate that sequence type should be selected by SeqDB;
/// this is no longer supported and will probably be removed soon.
const char kSeqTypeUnkn = '-';


/// Used to request ambiguities in Ncbi/NA8 format.
const Uint4 kSeqDBNuclNcbiNA8  = 0;

/// Used to request ambiguities in BLAST/NA8 format.
const Uint4 kSeqDBNuclBlastNA8 = 1;


/// Used to request that memory mapping be used.
const bool kSeqDBMMap   = true;

/// Used to request that memory mapping not be used.
const bool kSeqDBNoMMap = false;


/// Certain methods have an "Alloc" version.  When these methods are
/// used, the following constants can be specified to indicate which
/// libraries to use to allocate returned data, so the corresponding
/// calls (delete[] vs. free()) can be used to delete the data.

enum ESeqDBAllocType {
    eMalloc = 1,
    eNew
};


END_NCBI_SCOPE

#endif // CORELIB__SEQDB__SEQDBCOMMON_HPP


