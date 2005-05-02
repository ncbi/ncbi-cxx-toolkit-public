#ifndef CONNECT___NCBI_CONN_EXCEPTION__HPP
#define CONNECT___NCBI_CONN_EXCEPTION__HPP

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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   CONN-library exception type
 *
 */

#include <connect/ncbi_core.h>
#include <corelib/ncbiexpt.hpp>


/** @addtogroup ConnExcep
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XCONNECT_EXPORT CConnException
    : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eConn
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eConn:    return "eConn";
        default:       return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CConnException, CException);
};

/// IO exception. 
/// Thrown if error is specific to the NCBI BDB C++ library.
///
/// @sa EIO_Status
class NCBI_XCONNECT_EXPORT CIO_Exception
    : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    /// @sa EIO_Status
    enum EErrCode {
        eTimeout      = eIO_Timeout,
        eClosed       = eIO_Closed,
        eInterrupt    = eIO_Interrupt,
        eInvalidArg   = eIO_InvalidArg,
        eNotSupported = eIO_NotSupported,
        eUnknown      = eIO_Unknown
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eTimeout:      return "eIO_Timeout";
        case eClosed:       return "eIO_Closed";
        case eInterrupt:    return "eIO_Interrupt";
        case eInvalidArg:   return "eIO_InvalidArg";
        case eNotSupported: return "eIO_NotSupported";
        case eUnknown:      return "eIO_Unknown";
        default:            return  CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CIO_Exception, CException);
};

/// Check EIO_Status, throw an exception if something is wrong
///
/// @sa EIO_Status
#define NCBI_IO_CHECK(errnum) \
    do { \
        if ( errnum != eIO_Success) { \
            throw CIO_Exception(DIAG_COMPILE_INFO, \
               0, (CIO_Exception::EErrCode)errnum, "IO error."); \
        } \
    } while (0)


/// Check ERW_Result, throw an exception if something is wrong
///
/// @sa ERW_Result
#define NCBI_IO_CHECK_RW(errnum) \
    do { \
        if ( errnum != eRW_Success) { \
            throw CIO_Exception(DIAG_COMPILE_INFO, \
               0, (CIO_Exception::EErrCode)errnum, "IO error."); \
        } \
    } while (0)


END_NCBI_SCOPE


/* @} */


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.10  2005/05/02 16:01:23  lavr
 * Proper order of included headers and exception class derivation
 *
 * Revision 6.9  2005/04/14 18:15:47  kuznets
 * +NCBI_IO_CHECK_RW
 *
 * Revision 6.8  2004/10/01 16:06:25  kuznets
 * CIO_Exception placed in ncbi_conn_exception.hpp
 *
 * Revision 6.7  2004/08/19 12:43:21  dicuccio
 * Dropped unnecessary export specifier
 *
 * Revision 6.6  2003/04/09 17:58:41  siyan
 * Added doxygen support
 *
 * Revision 6.5  2003/01/17 18:53:16  lavr
 * Explicitly include <connect/connect_export.h>
 *
 * Revision 6.4  2002/12/19 17:23:11  lavr
 * Replace CConn_Exception with CConnException derived from CException
 *
 * Revision 6.3  2002/12/19 14:51:48  dicuccio
 * Added export specifier for Win32 DLL builds.
 *
 * Revision 6.2  2002/06/12 19:19:12  lavr
 * Guard macro name standardized
 *
 * Revision 6.1  2002/06/06 18:59:45  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* CONNECT___NCBI_CONN_EXCEPTION__HPP */
