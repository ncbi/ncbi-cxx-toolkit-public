#ifndef BDB_EXPT_HPP__
#define BDB_EXPT_HPP__

/* $Id$
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
 * Author:  Anatoliy Kuznetsov
 *   
 * File Description: Berkeley DB support library. 
 *                   Exception specifications and routines.
 *
 */

#include <corelib/ncbistd.hpp>

#include <db.h>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////
//
// Base BDB exception class
//

class CBDB_Exception : EXCEPTION_VIRTUAL_BASE public CException
{
    NCBI_EXCEPTION_DEFAULT(CBDB_Exception, CException);
};



/////////////////////////////////////////////////////////////////////////////
//
// Auxiliary class to wrap up Berkeley DB strerror function
// 

class CBDB_StrErrAdapt
{
public:
    static const char* strerror(int errnum)
    {
        return ::db_strerror(errnum);
    }
};



//////////////////////////////////////////////////////////////////
//
// BDB errno exception class. 
// Berkley DB can return two types of error codes:
//   0  - operation successfull
//  >0  - positive error code (errno) (file locked, no space on device, etc)
//  <0  - negative error code indicates Berkeley DB related problem.
//
//  db_strerror function provided by BerkeleyDB works as a superset of 
// ::strerror function returning valid error messages for both errno and 
// BDB error codes.
//

class CBDB_ErrnoException : 
    public CErrnoTemplExceptionEx<CBDB_Exception, CBDB_StrErrAdapt::strerror>
{
public:
    typedef CErrnoTemplExceptionEx<CBDB_Exception, CBDB_StrErrAdapt::strerror>
            CParent;

    enum EErrCode {
        eSystem,      // GetErrno() to return system lib specific error code
        eBerkeleyDB   // GetErrno() to return BerkeleyDB specific error code
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch ( GetErrCode() ) {
        case eSystem:       return "eSystem";
        case eBerkeleyDB:   return "eBerkeleyDB";
        default:            return  CException::GetErrCodeString();
        }
    }

    // Returns Berkley DB related error code.
    int BDB_GetErrno() const
    {
        return GetErrno();
    }

    NCBI_EXCEPTION_DEFAULT2(CBDB_ErrnoException, CParent, int);
};



//////////////////////////////////////////////////////////////////
//
// BDB library exception. 
// Thrown if error is specific to the BDB C++ library.
//

class CBDB_LibException : public CBDB_Exception
{
public:
    enum EErrCode {
        eOverflow,
        eType,
        eIdxSearch,
        eInvalidValue,
        eNull,
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch ( GetErrCode() ) {
        case eOverflow:          return "eOverflow";
        case eType:              return "eType";
        case eIdxSearch:         return "eIdxSearch";
        case eInvalidValue:      return "eInvalidValue";
        case eNull:              return "eNull";
        default:                 return  CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CBDB_LibException, CBDB_Exception);
};



#define BDB_THROW(errcode, message) \
    throw CBDB_LibException(__FILE__, __LINE__, 0, CBDB_LibException::errcode, \
                            (message))


#define BDB_ERRNO_THROW(errnum, message) \
    throw CBDB_ErrnoException(__FILE__, __LINE__, 0, \
         ((errnum > 0) ? CBDB_ErrnoException::eSystem : \
                         CBDB_ErrnoException::eBerkeleyDB), \
          (message), errnum)


#define BDB_CHECK(errnum, dbfile) \
    do { \
        if ( errnum ) { \
            std::string message = "BerkeleyDB error:"; \
            if (dbfile) { \
                message.append("File:'"); \
                message.append(dbfile); \
                message.append("'"); \
            } \
            BDB_ERRNO_THROW(errnum, message); \
        } \
    } while (0)


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/04/24 16:31:16  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif
