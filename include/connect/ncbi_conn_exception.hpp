#ifndef CONNECT___NCBI_CONN_EXCEPTION__HPP
#define CONNECT___NCBI_CONN_EXCEPTION__HPP

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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   CONN-library exception type
 *
 */

#include <corelib/ncbiexpt.hpp>
#include <connect/ncbi_core.h>


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
    virtual const char* GetErrCodeString(void) const;
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

    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CIO_Exception, CException);
};

/// Check EIO_Status, throw an exception if something is wrong
///
/// @sa EIO_Status
#define NCBI_IO_CHECK(errnum)                                           \
    do {                                                                \
        if ((errnum) != eIO_Success) {                                  \
            throw CIO_Exception(DIAG_COMPILE_INFO,                      \
                  0, (CIO_Exception::EErrCode)(errnum), "IO error.");   \
        }                                                               \
    } while (0)


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___NCBI_CONN_EXCEPTION__HPP */
