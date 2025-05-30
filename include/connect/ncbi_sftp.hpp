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
    enum EFlags {
        /// Update known hosts (~/.ssh/known_hosts) if host is trusted.
        /// @warning Updating known hosts affects other SSH/SFTP clients.
        fUpdateKnownHosts               = 1 << 0,
        /// Do not trust new (unknown) host.
        fDoNotTrustNewHost              = 1 << 1,
        /// Trust host with changed public key.
        fTrustChangedHost               = 1 << 2,
    };
    DECLARE_SAFE_FLAGS_TYPE(EFlags, TFlags);

    /// Session params.
    /// Make sure params do not outlive passed strings.
    struct SParams : private tuple<string_view, string_view, string_view, TFlags>
    {
        using TBase = tuple;
        enum EValues : size_t { eHost, eUser, ePassword, eFlags };

        // It must be strings as libssh requires null-terminated strings
        SParams(const string& host,
                const string& user = {},
                const string& password = {},
                TFlags        flags = 0)
            : tuple(host, user, password, flags)
        {
        }

        auto SetUser(const string& user)                    { return Set<eUser>(user); }
        auto SetPassword(const string& password)            { return Set<ePassword>(password); }
        auto SetFlag(EFlags flag) { get<eFlags>(*this) |= flag; return *this; }

    private:
        template <EValues what>
        SParams& Set(const string& value) { get<what>(*this) = value; return *this; }

        friend CSFTP_Session;
    };

    CSFTP_Session(SParams params);

    /// A shortcut
    template <class... TArgs>
    CSFTP_Session(TArgs&&... args) : CSFTP_Session(SParams(std::forward<TArgs>(args)...)) {}

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
    CSFTP_Stream(const CSFTP_Session& session,
                 string_view          path = {}) :
        CSFTP_Stream(session, path, {}, 0, false)
    {
    }

protected:
    CSFTP_Stream(const CSFTP_Session& session, string_view path,
            string_view file, uint64_t offset, bool upload);
};


/// CSFTP_Stream specialization (ctors) for download
///
/// @sa CSFTP_Stream, CConn_FTPDownloadStream
///
class NCBI_XCONNSFTP_EXPORT CSFTP_DownloadStream : public CSFTP_Stream
{
public:
    CSFTP_DownloadStream(const CSFTP_Session& session,
                         string_view          file,
                         string_view          path = {}) :
        CSFTP_Stream(session, path, file, 0, false)
    {
    }

    CSFTP_DownloadStream(const CSFTP_Session& session,
                         string_view          file,
                         uint64_t             offset,
                         string_view          path = {}) :
        CSFTP_Stream(session, path, file, offset, false)
    {
    }
};


/// CSFTP_Stream specialization (ctors) for upload
///
/// @sa CSFTP_Stream, CConn_FTPUploadStream
///
class NCBI_XCONNSFTP_EXPORT CSFTP_UploadStream : public CSFTP_Stream
{
public:
    CSFTP_UploadStream(const CSFTP_Session& session,
                       string_view          file,
                       string_view          path = {}) :
        CSFTP_Stream(session, path, file, 0, true)
    {
    }

    CSFTP_UploadStream(const CSFTP_Session& session,
                       string_view          file,
                       uint64_t             offset,
                       string_view          path = {}) :
        CSFTP_Stream(session, path, file, offset, true)
    {
    }
};


END_NCBI_SCOPE

#endif
#endif
