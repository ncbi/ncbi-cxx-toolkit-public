#ifndef CONNECT__NCBI_SFTP_IMPL__HPP
#define CONNECT__NCBI_SFTP_IMPL__HPP

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
#include <corelib/reader_writer.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbitime.hpp>
#include <connect/ncbi_sftp.hpp>

#ifdef HAVE_SFTP

#include <charconv>
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <deque>
#include <string_view>
#include <utility>

#include <fcntl.h>

#include <libssh/libssh.h>
#include <libssh/sftp.h>

BEGIN_NCBI_SCOPE


#define NCBI_SSH_TRACE(message)     _TRACE(message)


namespace NSftp
{


using TSftpSession = pair<ssh_session, sftp_session>;

struct SError
{
    SError(ssh_session s)
    {
        m_Value << ssh_get_error(s) << " (" << ssh_get_error_code(s) << ')';
    }

    SError(const TSftpSession& s)
    {
        m_Value << ssh_get_error(s.first) << " (" << ssh_get_error_code(s.first) << '/' << sftp_get_error(s.second) << ')';
    }

    friend ostream& operator<<(ostream& os, const SError& error) { return os << error.m_Value.str(); }

private:
    stringstream m_Value;
};

template <class T, typename = int> struct SHasStop                           : std::false_type {};
template <class T>                 struct SHasStop<T, decltype(&T::Stop, 0)> : std::true_type  {};

template <class TImpl, bool HasStop = SHasStop<TImpl>::value>
struct SGuard
{
    template <class... TArgs>
    SGuard(TArgs&&... args)
    {
        TImpl::Start(std::forward<TArgs>(args)...);
    }
};

template <typename T, typename... TArgs> struct SReturnType;
template <typename T, typename... TArgs> struct SReturnType<T(*)(TArgs...)> { using TType = T; };

template <class TImpl>
struct SGuard<TImpl, true>
{
    using THandle = SReturnType<decltype(&TImpl::Start)>::TType;

    template <class... TArgs>
    SGuard(TArgs&&... args) :
        m_Handle(TImpl::Start(std::forward<TArgs>(args)...))
    {
    }

    SGuard(SGuard&& other) :
        m_Handle(exchange(other.m_Handle, nullptr))
    {
    }

    SGuard& operator=(SGuard&& other)
    {
        TImpl::Stop(exchange(m_Handle, exchange(other.m_Handle, nullptr)));
    }

    ~SGuard()
    {
		TImpl::Stop(m_Handle);
    }

    THandle Get() { return m_Handle; }
    operator THandle() { return m_Handle; }
    explicit operator bool() const { return m_Handle; }

    SGuard(const SGuard&) = delete;
    SGuard& operator=(const SGuard&) = delete;

private:
    THandle m_Handle;
};

using TPath = filesystem::path;

inline TPath NormalizePath(TPath path)
{
    // Remove any trailing separators (incl. repeated)
    return path.append("").parent_path();
}

struct SSshLibrary
{
    static nullopt_t Start()
    {
        if (auto rv = ssh_init(); rv == SSH_OK) {
            NCBI_SSH_TRACE("initialized");
        } else {
            NCBI_SSH_TRACE("failed to initialize: " << rv);
            NCBI_THROW_FMT(CSFTP_Exception, eInternalError, "Failed to initialize libssh");
        }

        return nullopt;
    }

    static void Stop(nullopt_t)
    {
        if (auto rv = ssh_finalize(); rv == SSH_OK) {
            NCBI_SSH_TRACE("finalized");
        } else {
            NCBI_SSH_TRACE("failed to finalize: " << rv);
            ERR_POST(Warning << "Failed to finalize libssh");
        }
    }
};

struct SSsh
{
    static ssh_session Start()
    {
        auto rv = ssh_new();

        if (rv) {
            NCBI_SSH_TRACE(rv << " created");
        } else {
            NCBI_SSH_TRACE("failed to create");
            NCBI_THROW_FMT(CSFTP_Exception, eInternalError, "Failed to create SSH session");
        }

        return rv;
    }

    static void Stop(ssh_session s)
    {
        ssh_free(s);
        NCBI_SSH_TRACE(s << " freed");
    }
};

struct SSshConn
{
    static ssh_session Start(ssh_session s, const string& host, const string& u)
    {
        if (auto rv = ssh_options_set(s, SSH_OPTIONS_HOST, host.data()); rv == SSH_OK) {
            NCBI_SSH_TRACE(s << " host set: " << host);
        } else {
            NCBI_SSH_TRACE(rv << " failed to set host '" << host << "': " << SError(s));
            NCBI_THROW_FMT(CSFTP_Exception, eInvalidArg, "Failed to set host '" << host << "': " << SError(s));
        }

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

    static void Stop(ssh_session s)
    {
        ssh_disconnect(s);
        NCBI_SSH_TRACE(s << " disconnected");
    }
};

struct SSshVerify
{
    static void Start(ssh_session s, const string& u, const string& p)
    {
        if (auto rv = ssh_session_is_known_server(s); rv == SSH_KNOWN_HOSTS_OK) {
            NCBI_SSH_TRACE(s << " server verified");
        } else {
            NCBI_SSH_TRACE(s << " failed to verify server: " << SError(s));
            NCBI_THROW_FMT(CSFTP_Exception, eAuthenticationError, "Failed to verify server: " << SError(s));
        }

        if (auto rv = p.empty() ? ssh_userauth_gssapi(s) : ssh_userauth_password(s, nullptr, p.data()); rv == SSH_AUTH_SUCCESS) {
            NCBI_SSH_TRACE(s << " user authenticated");
        } else {
            if (u.empty()) {
                NCBI_SSH_TRACE(s << " failed to authenticate user: " << SError(s));
                NCBI_THROW_FMT(CSFTP_Exception, eAuthenticationError, "Failed to authenticate user: " << SError(s));
            } else {
                NCBI_SSH_TRACE(s << " failed to authenticate user '" << u << "': " << SError(s));
                NCBI_THROW_FMT(CSFTP_Exception, eAuthenticationError, "Failed to authenticate as user '" << u << "': " << SError(s));
            }
        }
    }
};

struct SSftp
{
    static sftp_session Start(ssh_session s)
    {
        auto rv = sftp_new(s);

        if (rv) {
            NCBI_SSH_TRACE(rv << " created");
        } else {
            NCBI_SSH_TRACE("failed to create: " << SError(s));
            NCBI_THROW_FMT(CSFTP_Exception, eInternalError, "Failed to create SFTP session: " << SError(s));
        }

        return rv;
    }

    static void Stop(sftp_session s)
    {
        sftp_free(s);
        NCBI_SSH_TRACE(s << " freed");
    }
};

struct SSftpInit
{
    static void Start(TSftpSession s)
    {
        if (auto rv = sftp_init(s.second); rv == SSH_OK) {
            NCBI_SSH_TRACE(s.second << " initialized");
        } else {
            NCBI_SSH_TRACE(s.second << " failed to initialize: " << SError(s));
            NCBI_THROW_FMT(CSFTP_Exception, eInternalError, "Failed to initialize SFTP session: " << SError(s));
        }
    }
};

struct SSshStr
{
    static void Stop(char* s)
    {
        if (s) {
            ssh_string_free_char(s);
            NCBI_SSH_TRACE((void*)s << " freed");
        }
    }
};

struct SSftpPathStr : SSshStr
{
    static char* Start(const TSftpSession& s, const TPath& p)
    {
        const auto& path = p.native();
        auto rv = sftp_canonicalize_path(s.second, path.data());

        if (rv) {
            NCBI_SSH_TRACE((void*)rv << " created canonical path: '" << rv << '\'');
        } else {
            NCBI_SSH_TRACE("failed to create canonical path '" << path << "': " << SError(s));
        }

        return rv;
    }
};

using TSftpDir = pair<TSftpSession, sftp_dir>;

struct SSftpDir
{
    static TSftpDir Start(const TSftpSession& s, const TPath& p)
    {
        const auto& path = p.native();
        auto rv = sftp_opendir(s.second, path.data());

        if (rv) {
            NCBI_SSH_TRACE(rv << " opened: " << path);
        } else {
            NCBI_SSH_TRACE("failed to open '" << path << "': " << SError(s));
        }

        return { s, rv };
    }

    static bool Eof(TSftpDir d)
    {
        auto rv = sftp_dir_eof(d.second);

        if (rv) {
            NCBI_SSH_TRACE(d.second << " eof");
        } else {
            NCBI_SSH_TRACE(d.second << " error: " << SError(d.first));
        }

        return rv;
    }

    static void Stop(TSftpDir d)
    {
        if (d.second) {
            if (auto rv = sftp_closedir(d.second); rv == SSH_NO_ERROR) {
                NCBI_SSH_TRACE(d.second << " closed");
            } else {
                NCBI_SSH_TRACE(d.second << " failed to close: " << SError(d.first));
            }
        }
    }
};

struct SSftpAttrs
{
    static void Stop(sftp_attributes a)
    {
        if (a) {
            sftp_attributes_free(a);
            NCBI_SSH_TRACE(a << " freed");
        }
    }
};

struct SSftpDirAttrs : SSftpAttrs
{
    static sftp_attributes Start(const TSftpDir& d)
    {
        auto rv = sftp_readdir(d.first.second, d.second);

        if (rv) {
            NCBI_SSH_TRACE(rv << " read dir");
        } else {
            NCBI_SSH_TRACE("failed to read dir '" << d.second << "': " << SError(d.first));
        }

        return rv;
    }
};

using TSftpFile = pair<TSftpSession, sftp_file>;

struct SSftpFile
{
    static TSftpFile Start(const TSftpSession& s, const TPath& f, int a = O_RDONLY, mode_t m = 0)
    {
        const auto& path = f.native();
        auto rv = sftp_open(s.second, path.data(), a, m);

        if (rv) {
            NCBI_SSH_TRACE(rv << " opened: " << path);
        } else {
            NCBI_SSH_TRACE("failed to open '" << path << "': " << SError(s));
        }

        return { s, rv };
    }

    static size_t Read(TSftpFile d, void* buf, size_t count)
    {
        auto rv = sftp_read(d.second, buf, count);

        if (rv > 0) {
            NCBI_SSH_TRACE(d.second << " read: " << rv);
            return static_cast<size_t>(rv);
        }

        if (rv == 0) {
            NCBI_SSH_TRACE(d.second << " eof");
        } else {
            NCBI_SSH_TRACE(d.second << " error: " << SError(d.first));
        }

        return 0;
    }

    static size_t Write(TSftpFile d, const void* buf, size_t count)
    {
        auto rv = sftp_write(d.second, buf, count);

        if (rv >= 0) {
            NCBI_SSH_TRACE(d.second << " wrote: " << rv);
            return static_cast<size_t>(rv);
        } else {
            NCBI_SSH_TRACE(d.second << " failed to write: " << SError(d.first));
            return 0;
        }
    }

    static bool Seek(TSftpFile d, uint64_t offset)
    {
        if (auto rv = sftp_seek64(d.second, offset); rv == 0) {
            NCBI_SSH_TRACE(d.second << " sought: " << offset);
            return true;
        } else {
            NCBI_SSH_TRACE(d.second << " failed to seek: " << SError(d.first));
            return false;
        }
    }

    static void Stop(TSftpFile d)
    {
        if (d.second) {
            if (auto rv = sftp_close(d.second); rv == SSH_NO_ERROR) {
                NCBI_SSH_TRACE(d.second << " closed");
            } else {
                NCBI_SSH_TRACE(d.second << " failed to close: " << SError(d.first));
            }
        }
    }
};

struct SSftpStatAttrs : SSftpAttrs
{
    static sftp_attributes Start(const TSftpSession& s, const TPath& p)
    {
        const auto& path = p.native();
        auto rv = sftp_lstat(s.second, path.data());

        if (rv) {
            NCBI_SSH_TRACE(rv << " got stats for: " << path);
        } else {
            NCBI_SSH_TRACE("failed to get stats for '" << path << "': " << SError(s));
        }

        return rv;
    }
};

struct SSftpMisc
{
    static bool Mkdir(const TSftpSession& s, TPath path)
    {
        if (auto rv = sftp_mkdir(s.second, path.native().data(), 0); !rv) {
            NCBI_SSH_TRACE("created: " << path);
            return true;
        } else {
            NCBI_SSH_TRACE("failed to create: '" << path << "': " << SError(s));
            return false;
        }
    }

    static bool Rmdir(const TSftpSession& s, TPath path)
    {
        if (auto rv = sftp_rmdir(s.second, path.native().data()); !rv) {
            NCBI_SSH_TRACE("removed: " << path);
            return true;
        } else {
            NCBI_SSH_TRACE("failed to remove '" << path << "': " << SError(s));
            return false;
        }
    }

    static bool Unlink(const TSftpSession& s, TPath path)
    {
        if (auto rv = sftp_unlink(s.second, path.native().data()); !rv) {
            NCBI_SSH_TRACE("unlinked: " << path);
            return true;
        } else {
            NCBI_SSH_TRACE("failed to unlink '" << path << "': " << SError(s));
            return false;
        }
    }

    static bool Rename(const TSftpSession& s, TPath original, TPath newname)
    {
        if (auto rv = sftp_rename(s.second, original.native().data(), newname.native().data()); !rv) {
            NCBI_SSH_TRACE("renamed '" << original << "' to '" << newname << '\'');
            return true;
        } else {
            NCBI_SSH_TRACE("failed to rename '" << original << "' to '" << newname << "': " << SError(s));
            return false;
        }
    }
};

struct SSession
{
    SSession(const string& host, const string& user, const string& password) :
        m_Library(),
        m_Ssh(),
        m_SshConn(m_Ssh, host, user),
        m_SshVerify(m_Ssh, user, password),
        m_Sftp(m_Ssh),
        m_SftpInit(*this)
    {
    }

    operator TSftpSession() { return { m_Ssh, m_Sftp }; }

private:
    SGuard<SSshLibrary> m_Library;
    SGuard<SSsh> m_Ssh;
    SGuard<SSshConn> m_SshConn;
    SGuard<SSshVerify> m_SshVerify;
    SGuard<SSftp> m_Sftp;
    SGuard<SSftpInit> m_SftpInit;
};

struct IState : IReaderWriter
{
    virtual void Reset(string_view = {}) {}

    ERW_Result PendingCount(size_t*) override
    {
        return eRW_NotImplemented;
    }

    ERW_Result Write(const void*, size_t count, size_t* bytes_written = 0) override
    {
        if (bytes_written) *bytes_written = count;
        return eRW_Success;
    }

    ERW_Result Flush() override { return eRW_Success; }
};

struct SFsmData
{
    SFsmData(TSftpSession session, TPath& current_path, uint64_t& offset) :
        m_Session(std::move(session)),
        m_CurrentPath(current_path),
        m_Offset(offset)
    {
    }

protected:
    TSftpSession m_Session;
    TPath& m_CurrentPath;
    uint64_t& m_Offset;
};

struct SStringReply : IState
{
    void Reset(string_view reply) override { m_Reply = std::move(reply); }

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0) override
    {
        _ASSERT(buf);
        count = m_Reply.copy(static_cast<char*>(buf), count);
        m_Reply.remove_prefix(count);

        if (bytes_read) *bytes_read = count;
        return m_Reply.size() ? eRW_Success : eRW_Timeout;
    }

protected:
    string_view m_Reply;
};

struct SStringReplyState : SStringReply, protected SFsmData
{
    SStringReplyState(SFsmData&& data) : SFsmData(std::move(data)) {}
};

struct SPwdState : SStringReplyState
{
    using SStringReplyState::SStringReplyState;

    void Reset(string_view) override
    {
        m_Reply = m_CurrentPath.native();
    }
};

struct SCdupState : SStringReplyState
{
    using SStringReplyState::SStringReplyState;

    void Reset(string_view) override
    {
        m_CurrentPath = m_CurrentPath.parent_path();
        m_Reply = m_Ok;
    }

private:
    const string_view m_Ok = "200"sv;
};

struct SCwdState : SStringReplyState
{
    SCwdState(SFsmData&& data, string_view start_path) :
        SStringReplyState(std::move(data))
    {
        m_CurrentPath = NormalizePath(start_path);
        Reset({});
    }

    void Reset(string_view new_path) override
    {
        if (auto canonical = SGuard<SSftpPathStr>(m_Session, NormalizePath(m_CurrentPath / new_path)); !canonical) {
            m_Reply = m_Error;
        } else if (auto dir = SGuard<SSftpDir>(m_Session, canonical.Get()); !dir.Get().second) {
            m_Reply = m_Error;
        } else {
            m_Reply = m_Ok;
            m_CurrentPath = canonical.Get();
        }
    }

private:
    const string_view m_Ok = "250"sv;
    const string_view m_Error = ""sv;
};

template <class TDerived>
struct SDirReplyState : IState, protected SFsmData
{
    SDirReplyState(SFsmData&& data) : SFsmData(std::move(data)) {}

    void Reset(string_view path) override;

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0) override;

private:
    deque<SGuard<SSftpDirAttrs>> m_DirContents;
    string m_Line;
    SStringReply m_LineReply;
};

struct SNlstState : SDirReplyState<SNlstState>
{
    using TBase = SDirReplyState<SNlstState>;

    using TBase::SDirReplyState;

    void Reset(string_view path) override
    {
        TBase::Reset(path);

        if (path.empty()) {
            m_Path.clear();
        } else if (auto n = NormalizePath(path); n.native() == "."s) {
            m_Path.clear();
        } else {
            m_Path = n.append("");
        }
    }

    string Prepare(sftp_attributes attrs)
    {
        return *attrs->name == '.' ? ""s : m_Path.native() + attrs->name + "\r\n"sv;
    }

private:
    TPath m_Path;
};

struct SListState : SDirReplyState<SListState>
{
    using SDirReplyState<SListState>::SDirReplyState;

    string Prepare(sftp_attributes attrs)
    {
        return *attrs->name == '.' ? ""s : attrs->longname + "\r\n"s;
    }
};

struct SMlsFormat
{
    string Prepare(sftp_attributes attrs, const TPath& path = {})
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
};

struct SMlsdState : SMlsFormat, SDirReplyState<SMlsdState>
{
    using SDirReplyState<SMlsdState>::SDirReplyState;
};

// Cannot use sftp_[l]stat as they do not return most of the attributes
struct SMlstState : SMlsFormat, SDirReplyState<SMlstState>
{
    using SDirReplyState<SMlstState>::SDirReplyState;

    string Prepare(sftp_attributes attrs)
    {
        return attrs->name == m_Name ? SMlsFormat::Prepare(attrs, m_Path) : ""s;
    }

    void Reset(string_view path) override
    {
        m_Path = path.empty() ? m_CurrentPath : NormalizePath(m_CurrentPath / path);
        m_Name = m_Path.filename();
        m_Path = m_Path.parent_path() / "";
        SDirReplyState<SMlstState>::Reset(m_Path.native());
    }

private:
    TPath m_Path;
    string m_Name;
};

struct SRetrState : IState, protected SFsmData
{
    SRetrState(SFsmData&& data) : SFsmData(std::move(data)) {}

    void Reset(string_view path) override;
    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0) override;

private:
    optional<SGuard<SSftpFile>> m_File;
};

struct SSizeState : SStringReplyState
{
    using SStringReplyState::SStringReplyState;

    void Reset(string_view path) override
    {
        auto attrs = SGuard<SSftpStatAttrs>(m_Session, m_CurrentPath / path);
        m_Size = attrs ? to_string(attrs.Get()->size) : string();
        m_Reply = m_Size;
    }

private:
    string m_Size;
};

struct SMdtmState : SStringReplyState
{
    using SStringReplyState::SStringReplyState;

    void Reset(string_view path) override
    {
        auto attrs = SGuard<SSftpStatAttrs>(m_Session, m_CurrentPath / path);
        m_Size = attrs ? to_string(attrs.Get()->mtime) : string();
        m_Reply = m_Size;
    }

private:
    string m_Size;
};

struct SRestState : SStringReplyState
{
    using SStringReplyState::SStringReplyState;

    void Reset(string_view offset) override
    {
        const auto end = offset.data() + offset.size();
        auto rv = from_chars(offset.data(), end, m_Offset);

        if ((rv.ptr == end) && (rv.ec == std::errc())) {
            m_Reply = m_Ok;
        } else {
            m_Reply = m_Error;
            m_Offset = 0;
        }
    }

private:
    const string_view m_Ok = "350"sv;
    const string_view m_Error = ""sv;
};

struct SWrtrState : SStringReplyState
{
    enum EMode { eStor, eAppe };
    SWrtrState(SFsmData&& data, EMode mode) : SStringReplyState(std::move(data)), m_Append(mode == eAppe) {}

    void Reset(string_view path) override;
    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0) override;
    ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0) override;

private:
    const bool m_Append;
    optional<SGuard<SSftpFile>> m_File;
    size_t m_Size = 0;
    string m_SizeReply;
};

struct SMkdState : SStringReplyState
{
    using SStringReplyState::SStringReplyState;

    void Reset(string_view path) override
    {
        auto new_dir = m_CurrentPath / path;

        if (SSftpMisc::Mkdir(m_Session, new_dir)) {
            m_Dir = new_dir;
        }

        m_Reply = m_Dir.native();
    }

private:
    TPath m_Dir;
};

struct SRmdState : SStringReplyState
{
    using SStringReplyState::SStringReplyState;

    void Reset(string_view path) override
    {
        m_Reply = SSftpMisc::Rmdir(m_Session, m_CurrentPath / path) ? m_Ok : m_Error;
    }

private:
    const string_view m_Ok = "250"sv;
    const string_view m_Error = ""sv;
};

struct SDeleState : SStringReplyState
{
    using SStringReplyState::SStringReplyState;

    void Reset(string_view path) override
    {
        m_Reply = SSftpMisc::Unlink(m_Session, m_CurrentPath / path) ? m_Ok : m_Error;
    }

private:
    const string_view m_Ok = "250"sv;
    const string_view m_Error = ""sv;
};

struct SRenState : SStringReplyState
{
    using SStringReplyState::SStringReplyState;

    void Reset(string_view args) override
    {
        string original, newname;
        auto split = NStr::SplitInTwo(args, " \t", original, newname, NStr::fSplit_CanDoubleQuote | NStr::fSplit_Tokenize);
        m_Reply = split && SSftpMisc::Rename(m_Session, m_CurrentPath / original, m_CurrentPath / newname) ? m_Ok : m_Error;
    }

private:
    const string_view m_Ok = "250"sv;
    const string_view m_Error = ""sv;
};

struct SRWFsm : IState
{
    SRWFsm(shared_ptr<SSession> session, string_view start_path, string_view file, uint64_t offset, bool upload);

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0) override;
    ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0) override;

private:
    SFsmData x_GetData() { return { *m_Session, m_CurrentPath, m_Offset }; }

    shared_ptr<SSession> m_Session;
    map<string, unique_ptr<IState>, PNocase> m_States;
    IState* m_CurrentState = nullptr;
    TPath m_CurrentPath;
    uint64_t m_Offset;
    string m_Buffer;
};


}


END_NCBI_SCOPE

#endif
#endif
