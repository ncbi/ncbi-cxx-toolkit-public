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
        eInvalidDenseg,
        eTranslateFailure,
        eMergeFailure,
        eUnknownMergeFailure
    };

    virtual const char *GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eInvalidRequest:       return "eInvalidRequest";
        case eConsensusNotPresent:  return "eConsensusNotPresent";
        case eInvalidSeqId:         return "eInvalidSeqId";
        case eInvalidRow:           return "eInvalidRow";
        case eInvalidSegment:       return "eInvalidSegment";
        case eInvalidDenseg:        return "eInvalidDenseg";
        case eTranslateFailure:     return "eTranslateFailure";
        case eMergeFailure:         return "eMergeFailure";
        case eUnknownMergeFailure:  return "eUnknownMergeFailure";
        default:                    return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CAlnException,CException);
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif  /* OBJECTS_ALNMGR___ALNEXCEPTION__HPP */

/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2004/08/19 13:06:18  dicuccio
 * Dropped export specifiers on inlined exceptions
 *
 * Revision 1.8  2003/08/20 14:35:14  todorov
 * Support for NA2AA Densegs
 *
 * Revision 1.7  2003/07/15 20:50:40  todorov
 * +eInvalidSeqId
 *
 * Revision 1.6  2003/03/05 16:20:13  todorov
 * +eInvalidRequest
 *
 * Revision 1.5  2002/12/26 12:38:07  dicuccio
 * Added Win32 export specifiers
 *
 * Revision 1.4  2002/12/18 15:00:26  ucko
 * +eMergeFailure
 *
 * Revision 1.3  2002/09/27 17:35:44  todorov
 * added a merge exception
 *
 * Revision 1.2  2002/09/26 18:13:52  todorov
 * introducing a few new exceptions
 *
 * Revision 1.1  2002/09/25 18:16:26  dicuccio
 * Reworked computation of consensus sequence - this is now stored directly
 * in the underlying CDense_seg
 * Added exception class; currently used only on access of non-existent
 * consensus.
 *
 * ===========================================================================
 */
