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

// Publically visible seqdb related definitions.

class CSeqDBException : public CException {
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

// Protein / Nucleotide / Unknown are represented by 'p', 'n', and '-'.

const char kSeqTypeProt = 'p';
const char kSeqTypeNucl = 'n';
const char kSeqTypeUnkn = '-';

// Two output formats, used by CSeqDB::GetAmbigSeq(...)

const Uint4 kSeqDBNuclNcbiNA8  = 0;
const Uint4 kSeqDBNuclBlastNA8 = 1;

// Flag specifying whether to use memory mapping.

const bool kSeqDBMMap   = true;
const bool kSeqDBNoMMap = false;

// Certain methods have an "Alloc" version; if this is used, the
// following can be used to indicate how to allocate returned data, so
// that the user can use corresponding methods to delete the data.

enum ESeqDBAllocType {
    eMalloc = 1,
    eNew
};


END_NCBI_SCOPE

#endif // CORELIB__SEQDB__SEQDBCOMMON_HPP


