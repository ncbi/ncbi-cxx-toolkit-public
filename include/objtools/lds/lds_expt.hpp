#ifndef LDS_EXPT_HPP__
#define LDS_EXPT_HPP__
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description: LDS exception.
 *
 */
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//////////////////////////////////////////////////////////////////
//
// LDS exception. 
// Thrown if error is specific to the local data storage.
//

class CLDS_Exception : EXCEPTION_VIRTUAL_BASE public CBDB_Exception
{
public:
    enum EErrCode {
        eRecordNotFound,
        eFileNotFound,
        eWrongEntry,
        eNotImplemented,
        eInvalidDataType,
        eCannotCreateDir,
        eNull,
        eDuplicateId
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch ( GetErrCode() ) {
        case eRecordNotFound:    return "eRecordNotFound";
        case eFileNotFound:      return "eFileNotFound";
        case eNull:              return "eNull";
        case eNotImplemented:    return "eNotImplemented";
        case eWrongEntry:        return "eWrongEntry";
        case eInvalidDataType:   return "eInvalidDataType";
        case eCannotCreateDir:   return "eCannotCreateDir";
        case eDuplicateId:       return "eDuplicateId";
        default:                 return  CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CLDS_Exception, CBDB_Exception);
};


#define LDS_THROW(errcode, message) \
    throw CLDS_Exception(DIAG_COMPILE_INFO, 0, CLDS_Exception::errcode, \
                          (message))


END_SCOPE(objects)
END_NCBI_SCOPE

#endif
