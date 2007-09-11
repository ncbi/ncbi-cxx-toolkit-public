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

/// @file bdb_expt.hpp
/// Exception specifications for BDB library.

#include <corelib/ncbiexpt.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup BDB
 *
 * @{
 */


/// Base BDB exception class

class CBDB_Exception : EXCEPTION_VIRTUAL_BASE public CException
{
    NCBI_EXCEPTION_DEFAULT(CBDB_Exception, CException);
};


/// Auxiliary exception class to wrap up Berkeley DB strerror function
/// 

class NCBI_BDB_EXPORT CBDB_StrErrAdapt
{
public:
    // Dummy method.
    // The real error code pass over from CBDB_ErrnoException constructor.
    static int GetErrCode(void);
    // Get error message
    static const char* GetErrCodeString(int errnum);
};


/// BDB errno exception class. 
///
/// Berkley DB can return two types of error codes:
///   0  - operation successfull
///  >0  - positive error code (errno) (file locked, no space on device, etc)
///  <0  - negative error code indicates Berkeley DB related problem.
///  db_strerror function provided by BerkeleyDB works as a superset of 
/// ::strerror function returning valid error messages for both errno and 
/// BDB error codes.

class NCBI_BDB_EXPORT CBDB_ErrnoException : 
    public CErrnoTemplExceptionEx<CBDB_Exception, CBDB_StrErrAdapt::GetErrCode, CBDB_StrErrAdapt::GetErrCodeString>
{
public:
    typedef CErrnoTemplExceptionEx<CBDB_Exception, CBDB_StrErrAdapt::GetErrCode, CBDB_StrErrAdapt::GetErrCodeString>
            CParent;

    /// Exception types
    enum EErrCode {
        eSystem,      //!< GetErrno() to return system lib specific error code
        eBerkeleyDB   //!< GetErrno() to return BerkeleyDB specific error code
    };

    virtual const char* GetErrCodeString(void) const;

    /// Return Berkley DB related error code.
    int BDB_GetErrno() const { return GetErrno(); }

    /// Returns TRUE if error is BerekleyDB ENOMEM (insufficient buffer size)
    bool IsNoMem() const;

    /// Returns TRUE if error is BerekleyDB DB_BUFFER_SMALL (insufficient buffer size)
    bool IsBufferSmall() const;

    /// Returns TRUE if error is BerkeleyDB DB_LOCK_DEADLOCK
    bool IsDeadLock() const;

    /// If it is DB_RUNRECOVERY error
    bool IsRecovery() const;

    NCBI_EXCEPTION_DEFAULT2(CBDB_ErrnoException, CParent, int);
};



/// BDB library exception. 
/// Thrown if error is specific to the NCBI BDB C++ library.

class CBDB_LibException : public CBDB_Exception
{
public:
    enum EErrCode {
        eOverflow,
        eType,
        eIdxSearch,
        eInvalidValue,
        eInvalidOperation,
		eInvalidType,
        eTransInProgress,
        eNull,
        eQueryError,
        eQuerySyntaxError,
        eCannotOpenOverflowFile,
        eOverflowFileIO,
        eFileIO,
        eQuotaLimit,
        eForeignTransaction,
        eCompressorError,
        eIdConflict,
        eTooManyChunks,
        eRaceCondition
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eOverflow:                return "eOverflow";
        case eType:                    return "eType";
        case eIdxSearch:               return "eIdxSearch";
        case eInvalidValue:            return "eInvalidValue";
        case eInvalidOperation:        return "eInvalidOperation";
        case eInvalidType:  	       return "eInvalidType";
        case eNull:                    return "eNull";
        case eTransInProgress:         return "eTransInProgress";
        case eQueryError:              return "eQueryError";
        case eQuerySyntaxError:        return "eQuerySyntaxError";
        case eCannotOpenOverflowFile:  return "eCannotOpenOverflowFile";
        case eOverflowFileIO:          return "eOverflowFileIO";
        case eFileIO:                  return "eFileIO";
        case eQuotaLimit:              return "eQuotaLimit";
        case eForeignTransaction:      return "eForeignTransaction";
        case eCompressorError:         return "eCompressorError";
        case eIdConflict:              return "eIdConflict";
        case eTooManyChunks:           return "eTooManyChunks";
        case eRaceCondition:           return "eRaceCondition";

        default: return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CBDB_LibException, CBDB_Exception);
};



#define BDB_THROW(errcode, message) \
    throw CBDB_LibException(DIAG_COMPILE_INFO, 0, CBDB_LibException::errcode, \
                            (message))


#define BDB_ERRNO_THROW(errnum, message) \
    throw CBDB_ErrnoException(DIAG_COMPILE_INFO, 0, \
         ((errnum > 0) ? CBDB_ErrnoException::eSystem : \
                         CBDB_ErrnoException::eBerkeleyDB), \
          (message), errnum)


#define BDB_CHECK(errnum, x_db_object__) \
    do { \
        if ( errnum ) { \
            std::string message = "BerkeleyDB error: "; \
            message.append(CBDB_StrErrAdapt::GetErrCodeString(errnum)); \
            if (x_db_object__) { \
                message.append(" Object:'"); \
                message.append(x_db_object__); \
                message.append("'"); \
            } \
            BDB_ERRNO_THROW(errnum, message); \
        } \
    } while (0)


/* @} */


END_NCBI_SCOPE

#endif
