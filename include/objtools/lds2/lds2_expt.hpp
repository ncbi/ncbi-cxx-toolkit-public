#ifndef LDS2_EXPT_HPP__
#define LDS2_EXPT_HPP__
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
 * Author: Anatoliy Kuznetsov, Aleksey Grichenko
 *
 * File Description: LDS v.2 exception.
 *
 */

#include <corelib/ncbiexpt.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//////////////////////////////////////////////////////////////////
//
// LDS2 exception. 
// Thrown if error is specific to the local data storage.
//

class CLDS2_Exception : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eInvalidDbFile,
        eFileNotFound,
        eNotImplemented,
        eIndexerError,
        eDuplicateId
    };

    virtual const char* GetErrCodeString(void) const override
    {
        switch ( GetErrCode() ) {
        case eInvalidDbFile:     return "eInvalidDbFile";
        case eFileNotFound:      return "eFileNotFound";
        case eNotImplemented:    return "eNotImplemented";
        case eIndexerError:      return "eIndexerError";
        case eDuplicateId:       return "eDuplicateId";
        default:                 return  CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CLDS2_Exception, CException);
};


#define LDS2_THROW(errcode, message) \
    throw CLDS2_Exception(DIAG_COMPILE_INFO, 0, CLDS2_Exception::errcode, \
                         (message))


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // LDS2_EXPT_HPP__
