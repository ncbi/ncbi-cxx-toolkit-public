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

#include <ncbi_pch.hpp>

#include <connect/ncbi_sftp.hpp>

#ifdef HAVE_SFTP

#include <corelib/ncbimisc.hpp>

#include <sys/stat.h>

#include "ncbi_sftp_impl.hpp"


BEGIN_NCBI_SCOPE


namespace NSftp
{


ssh_session SSshConn::Start(ssh_session s, const TParams& params)
{
    const auto& host = get<TValues::eHost>(params);

    if (auto rv = ssh_options_set(s, SSH_OPTIONS_HOST, host.data()); rv == SSH_OK) {
        NCBI_SSH_TRACE(s << " host set: " << host);
    } else {
        NCBI_SSH_TRACE(rv << " failed to set host '" << host << "': " << SError(s));
        NCBI_THROW_FMT(CSFTP_Exception, eInvalidArg, "Failed to set host '" << host << "': " << SError(s));
    }

    const auto& u = get<TValues::eUser>(params);

    if (!u.empty()) {
        if (auto rv = ssh_options_set(s, SSH_OPTIONS_USER, u.data()); rv == SSH_OK) {
            NCBI_SSH_TRACE(s << " user set: " << u);
        } else {
            NCBI_SSH_TRACE(rv << " failed to set user '" << u << "': " << SError(s));
            NCBI_THROW_FMT(CSFTP_Exception, eInvalidArg, "Failed to set user '" << u << "': " << SError(s));
        }
    }

    if (auto rv = ssh_connect(s); rv == SSH_OK) {
        NCBI_SSH_TRACE(s << " connected");
    } else {
        NCBI_SSH_TRACE(s << " failed to connect to host '" << host << "': " << SError(s));
        NCBI_THROW_FMT(CSFTP_Exception, eInvalidArg, "Failed to connect to host '" << host << "': " << SError(s));
    }

    return s;
}


void SSshVerify::Start(ssh_session s, const TParams& params)
{
    if (auto rv = ssh_session_is_known_server(s); rv == SSH_KNOWN_HOSTS_OK) {
        NCBI_SSH_TRACE(s << " server verified");
    } else {
        NCBI_SSH_TRACE(s << " failed to verify server: " << SError(s));
        NCBI_THROW_FMT(CSFTP_Exception, eAuthenticationError, "Failed to verify server: " << SError(s));
    }

    const auto& p = get<TValues::ePassword>(params);

    if (auto rv = p.empty() ? ssh_userauth_gssapi(s) : ssh_userauth_password(s, nullptr, p.data()); rv == SSH_AUTH_SUCCESS) {
        NCBI_SSH_TRACE(s << " user authenticated");
    } else {
        const auto& u = get<TValues::eUser>(params);

        if (u.empty()) {
            NCBI_SSH_TRACE(s << " failed to authenticate user: " << SError(s));
            NCBI_THROW_FMT(CSFTP_Exception, eAuthenticationError, "Failed to authenticate user: " << SError(s));
        } else {
            NCBI_SSH_TRACE(s << " failed to authenticate user '" << u << "': " << SError(s));
            NCBI_THROW_FMT(CSFTP_Exception, eAuthenticationError, "Failed to authenticate as user '" << u << "': " << SError(s));
        }
    }
}


template <class TDerived>
void SDirReplyState<TDerived>::Reset(string_view path)
{
    m_DirContents.clear();

    auto dir = SGuard<SSftpDir>(m_Session, m_CurrentPath / path);

    if (!dir.Get().second) {
        return;
    }

    while (auto attrs = SGuard<SSftpDirAttrs>(dir)) {
        m_DirContents.emplace_back(std::move(attrs));
    }

    if (!SSftpDir::Eof(dir)) {
        m_DirContents.clear();
    }
}

template <class TDerived>
ERW_Result SDirReplyState<TDerived>::Read(void* buf, size_t count, size_t* bytes_read)
{
    for (;;) {
        size_t read = 0;
        auto rv = m_LineReply.Read(buf, count, &read);

        if (bytes_read) *bytes_read = read;

        if (rv != eRW_Timeout) {
            return rv;

        } else if (m_DirContents.empty()) {
            return rv;

        } else if (read) {
            return eRW_Success;
        }

        m_Line = static_cast<TDerived*>(this)->Prepare(m_DirContents.front());
        m_LineReply.Reset(m_Line);
        m_DirContents.pop_front();
    }
}


string SMlsFormat::Prepare(sftp_attributes attrs, const TPath& path)
{
    ostringstream os;
    os << "modify=" << CTime(attrs->mtime).AsString("YMDhms") <<
        ";size=" << attrs->size <<
        ";type=";

    switch (attrs->type) {
        case SSH_FILEXFER_TYPE_DIRECTORY:
            if (path.empty()) {
                if (string_view(attrs->name) == "."sv)  os << 'c';
                if (string_view(attrs->name) == ".."sv) os << 'p';
            }

            os << "dir";
            break;
        case SSH_FILEXFER_TYPE_REGULAR:   os << "file"; break;
        case SSH_FILEXFER_TYPE_SYMLINK:   os << "OS.unix=symlink"; break;
        case SSH_FILEXFER_TYPE_SPECIAL:   os << "special"; break;
        default:                          os << "unknown"; break;
    }

    os << ";UNIX.group=" << attrs->gid <<
        ";UNIX.groupname=";

    if (attrs->group) os << attrs->group;

    os << ";UNIX.mode=0" << oct << (attrs->permissions & 0777) << dec <<
        ";UNIX.owner=" << attrs->uid <<
        ";UNIX.ownername=";

    if (attrs->owner) os << attrs->owner;

    os << "; " << NormalizePath((path / attrs->name).lexically_normal()).native();

    if (path.empty()) os << '\r';

    os << '\n';
    return os.str();
}


void SRetrState::Reset(string_view path)
{
    m_File.emplace(m_Session, m_CurrentPath / path);

    if (m_File->Get().second && m_Offset && !SSftpFile::Seek(*m_File, m_Offset)) {
        m_File.reset();
    }

    m_Offset = 0;
}

ERW_Result SRetrState::Read(void* buf, size_t count, size_t* bytes_read)
{
    _ASSERT(m_File);

    if (!m_File->Get().second) {
        return eRW_Timeout;
    }

    auto result = SSftpFile::Read(*m_File, buf, count);

    if (!result) {
        m_File.reset();
    }

    if (bytes_read) *bytes_read = result;
    return result > 0 ? eRW_Success : eRW_Timeout;
}


void SWrtrState::Reset(string_view path)
{
    const auto accesstype = O_WRONLY | O_CREAT | (m_Offset ? 0 : m_Append ? O_APPEND : O_TRUNC);
    const auto mode = S_IRUSR | S_IWUSR;
    m_File.emplace(m_Session, m_CurrentPath / path, accesstype, mode);

    if (m_File->Get().second && m_Offset && !SSftpFile::Seek(*m_File, m_Offset)) {
        m_File.reset();
    }

    m_Size = 0;
    m_Offset = 0;
}

ERW_Result SWrtrState::Read(void* buf, size_t count, size_t* bytes_read)
{
    if (m_File) {
        m_File.reset();
        m_SizeReply = to_string(m_Size);
        m_Reply = m_SizeReply;
    }

    return SStringReply::Read(buf, count, bytes_read);
}

ERW_Result SWrtrState::Write(const void* buf, size_t count, size_t* bytes_written)
{
    _ASSERT(m_File);

    if (m_File->Get().second) {
        count = SSftpFile::Write(*m_File, buf, count);

        if (!count) {
            m_File.reset();
        }
    }

    m_Size += count;
    if (bytes_written) *bytes_written = count;
    return eRW_Success;
}


SRWFsm::SRWFsm(shared_ptr<SSession> session, string_view start_path, string_view file, uint64_t offset, bool upload) :
    m_Session(std::move(session)),
    m_CurrentPath("/"),
    m_Offset(offset)
{
    m_States.emplace("CWD"s, make_unique<SCwdState>(x_GetData(), start_path));
    m_States.emplace("NLST"s, make_unique<SNlstState>(x_GetData()));
    m_States.emplace("LIST"s, make_unique<SListState>(x_GetData()));
    m_States.emplace("MLSD"s, make_unique<SMlsdState>(x_GetData()));
    m_States.emplace("MLST"s, make_unique<SMlstState>(x_GetData()));
    m_States.emplace("PWD"s, make_unique<SPwdState>(x_GetData()));
    m_States.emplace("CDUP"s, make_unique<SCdupState>(x_GetData()));
    m_States.emplace("SIZE"s, make_unique<SSizeState>(x_GetData()));
    m_States.emplace("REST"s, make_unique<SRestState>(x_GetData()));
    m_States.emplace("APPE"s, make_unique<SWrtrState>(x_GetData(), SWrtrState::eAppe));
    m_States.emplace("MKD"s, make_unique<SMkdState>(x_GetData()));
    m_States.emplace("RMD"s, make_unique<SRmdState>(x_GetData()));
    m_States.emplace("DELE"s, make_unique<SDeleState>(x_GetData()));
    m_States.emplace("REN"s, make_unique<SRenState>(x_GetData()));
    m_States.emplace("MDTM"s, make_unique<SMdtmState>(x_GetData()));

    auto retr = make_unique<SRetrState>(x_GetData());
    auto stor = make_unique<SWrtrState>(x_GetData(), SWrtrState::eStor);

    if (!file.empty()) {
        if (upload) {
            stor->Reset(file);
            m_CurrentState = stor.get();
        } else {
            retr->Reset(file);
            m_CurrentState = retr.get();
        }
    }

    m_States.emplace("RETR"s, std::move(retr));
    m_States.emplace("STOR"s, std::move(stor));
}

ERW_Result SRWFsm::Read(void* buf, size_t count, size_t* bytes_read)
{
    if (!m_CurrentState) {
        if (bytes_read) *bytes_read = 0;
        return eRW_Timeout;
    }

    auto rv = m_CurrentState->Read(buf, count, bytes_read);

    if (rv != eRW_Success) {
        m_CurrentState = nullptr;
    }

    return rv;
}

ERW_Result SRWFsm::Write(const void* buf, size_t count, size_t* bytes_written)
{
    if (m_CurrentState) {
        return m_CurrentState->Write(buf, count, bytes_written);
    }

    auto begin = static_cast<const char*>(buf);
    auto data = string_view(begin, count);

    if (auto eol = data.find('\n'); eol == data.npos) {
        m_Buffer += data;
        data.remove_prefix(data.size());
    } else {
        m_Buffer += data.substr(0, eol);
        data.remove_prefix(eol + 1);

        auto space = m_Buffer.find(' ');
        auto cmd = m_Buffer.substr(0, space);

        if (auto found = m_States.find(cmd); found != m_States.end()) {
            m_CurrentState = found->second.get();

            _ASSERT(m_CurrentState);
            m_CurrentState->Reset(string_view(m_Buffer).substr(space == m_Buffer.npos ? m_Buffer.size() : space + 1));
        }

        m_Buffer.clear();
    }

    if (bytes_written) *bytes_written = data.data() - begin;
    return eRW_Success;
}


}


const char* CSFTP_Exception::GetErrCodeString() const
{
    switch (GetErrCode())
    {
        case eInternalError:            return "eInternalError";
        case eInvalidArg:               return "eInvalidArg";
        case eAuthenticationError:      return "eAuthenticationError";
        default:                        return CException::GetErrCodeString();
    }
}


CSFTP_Session::CSFTP_Session(SParams params) :
    m_Impl(make_shared<NSftp::SSession>(NSftp::TParams(params)))
{
}

CSFTP_Stream::CSFTP_Stream(const CSFTP_Session& session, string_view path,
            string_view file, uint64_t offset, bool upload) :
    CRWStream(
            new NSftp::SRWFsm(static_pointer_cast<NSftp::SSession>(session.m_Impl), path, file, offset, upload),
            0, nullptr, CRWStreambuf::fOwnAll)
{
}


END_NCBI_SCOPE

#endif
