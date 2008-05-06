#ifndef OBJECTS_ALNMGR___ALNEXCEPTION__HPP
#define OBJECTS_ALNMGR___ALNEXCEPTION__HPP

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
 * Author:  Mike DiCuccio, NCBI
 *
 * File Description:
 *    Exception for various alignment manager related issues
 *
 */

#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


class CAlnException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eInvalidRequest,
        eConsensusNotPresent,
        eInvalidSeqId,
        eInvalidRow,
        eInvalidSegment,
        eInvalidAlignment,
        eInvalidDenseg,
        eTranslateFailure,
        eMergeFailure,
        eUnknownMergeFailure,
        eUnsupported,
        eInternalFailure
    };

    virtual const char *GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eInvalidRequest:       return "eInvalidRequest";
        case eConsensusNotPresent:  return "eConsensusNotPresent";
        case eInvalidSeqId:         return "eInvalidSeqId";
        case eInvalidRow:           return "eInvalidRow";
        case eInvalidSegment:       return "eInvalidSegment";
        case eInvalidAlignment:     return "eInvalidAlignment";
        case eInvalidDenseg:        return "eInvalidDenseg";
        case eTranslateFailure:     return "eTranslateFailure";
        case eMergeFailure:         return "eMergeFailure";
        case eUnknownMergeFailure:  return "eUnknownMergeFailure";
        case eInternalFailure:      return "eInternalFailure";
        case eUnsupported:          return "eUnsupported";
        default:                    return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CAlnException,CException);
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif  /* OBJECTS_ALNMGR___ALNEXCEPTION__HPP */
