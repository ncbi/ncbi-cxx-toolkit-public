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
        eNull
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
        default:                 return  CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CLDS_Exception, CBDB_Exception);
};


#define LDS_THROW(errcode, message) \
    throw CLDS_Exception(__FILE__, __LINE__, 0, CLDS_Exception::errcode, \
                          (message))


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/08/19 13:07:14  dicuccio
 * Dropped export specifiers on inlined exceptions
 *
 * Revision 1.4  2003/08/12 14:07:56  kuznets
 * Added eCannotCreateDir exception type
 *
 * Revision 1.3  2003/06/09 16:35:28  kuznets
 * Added new error codes.
 *
 * Revision 1.2  2003/06/03 19:14:02  kuznets
 * Added lds dll export/import specifications
 *
 * Revision 1.1  2003/05/22 13:24:45  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif
