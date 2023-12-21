#ifndef PSG_MYNCBI__MYNCBI_EXCEPTION__HPP
#define PSG_MYNCBI__MYNCBI_EXCEPTION__HPP

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
 * Authors: Dmitrii Saprykin
 *
 * File Description:
 *
 *  Exceptions used in the psg_myncbi library
 *
 */

#include <corelib/ncbistr.hpp>
#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

class CPSG_MyNCBIException: public CException
{
public:
    enum EErrCode
    {
        eMyNCBIGeneralError = 2000,
        eMyNCBIResolveError,
        eMyNCBINotImplemented,
    };

    const char* GetErrCodeString() const override
    {
        switch ( GetErrCode() ) {
            case eMyNCBIGeneralError: return "eMyNCBIGeneralError";
            case eMyNCBIResolveError: return "eMyNCBIResolveError";
            case eMyNCBINotImplemented: return "eMyNCBINotImplemented";
            default: return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CPSG_MyNCBIException, CException);
};

END_NCBI_SCOPE

#endif  // PSG_MYNCBI__MYNCBI_EXCEPTION__HPP
