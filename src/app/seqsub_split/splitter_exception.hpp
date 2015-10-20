#ifndef _SPLITTER_EXCEPTION_HPP_
#define _SPLITTER_EXCEPTION_HPP_

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
 * Authors:  Justin Foley
 *
 * File Description:  Exception classes for seqsub_split
 *
 */


#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

class CSeqSubSplitException : public CException {
public:
    enum EErrCode {
        eInvalidAnnot,
        eInvalidSeqinst,
        eInvalidSeqid,
        eInvalidBioseq,
        eInvalidSortOrder,
        eEmptyOutputStub,
        eInputError,
        eOutputError
    };

    virtual const char* GetErrCodeString() const
    {
        switch (GetErrCode()) {
            case eInvalidAnnot:
                return "eInvalidAnnot";
            case eInvalidSeqinst:
                return "eInvalidSeqinst";
            case eInvalidSeqid:
                return "eInvalidSeqid";
            case eInvalidBioseq:
                return "eInvalidBioseq";
            case eInvalidSortOrder:
                return "eInvalidSortOrder";
            case eEmptyOutputStub:
                return "eEmptyOutputStub";
            case eInputError:
                return "eInputError";
            case eOutputError:
                return "eOutputError";
            default:
                return CException::GetErrCodeString();
        }
    };

    /// Include standard NCBI exception behaviour.
    NCBI_EXCEPTION_DEFAULT(CSeqSubSplitException, CException);
};


END_NCBI_SCOPE

#endif // _SPLITTER_EXCEPTION_HPP_
