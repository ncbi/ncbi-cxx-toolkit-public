#ifndef CONNECT__NCBI_SFTP__HPP
#define CONNECT__NCBI_SFTP__HPP

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
 * Authors: Rafael Sadyrov
 *
 */

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/rwstream.hpp>

#include <memory>

#if defined(NCBI_THREADS) && defined(HAVE_LIBSSH)
#   define HAVE_SFTP 1
#endif

#ifdef HAVE_SFTP

BEGIN_NCBI_SCOPE


class NCBI_XCONNSFTP_EXPORT CSFTP_Exception : public CException
{
public:
    enum EErrCode {
        eInternalError,
        eInvalidArg,
        eAuthenticationError,
    };

    virtual const char* GetErrCodeString() const override;

    NCBI_EXCEPTION_DEFAULT(CSFTP_Exception, CException);
};


/// An SFTP client session.
/// Required for SFTP client streams.
///
/// @sa CSFTP_Stream
///
class NCBI_XCONNSFTP_EXPORT CSFTP_Session
{
public:
    CSFTP_Session(const string& host,
                  const string& password = {});

    CSFTP_Session(const string& host,
                  const string& user,
                  const string& password = {});

private:
    shared_ptr<void> m_Impl;

    friend class CSFTP_Stream;
};


/// An SFTP client stream.
/// Supports most features of CConn_FtpStream,
/// see <connect/ncbi_ftp_connector.h> for detailed explanations
/// of supported features of the latter.
///
/// @sa CConn_FtpStream, FTP_CreateConnector
///
class NCBI_XCONNSFTP_EXPORT CSFTP_Stream : public CRWStream
{
public:
    CSFTP_Stream(CSFTP_Session session,
                 string_view path = {});
};


END_NCBI_SCOPE

#endif
#endif
