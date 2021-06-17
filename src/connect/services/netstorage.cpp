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
 * Author:  Dmitry Kazimirov
 *
 * File Description:
 *   NetStorage implementation declarations.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/rwstream.hpp>
#include <connect/services/impl/netstorage_impl.hpp>
#include <connect/services/netstorage_ft.hpp>

BEGIN_NCBI_SCOPE


struct SEmbeddedStreamReaderWriter : IEmbeddedStreamReaderWriter
{
    SEmbeddedStreamReaderWriter(SNetStorageObjectImpl& impl) : m_Impl(impl) {}

    ERW_Result Read(void* b, size_t c, size_t* r)        override { return m_Impl->Read(b, c, r);   }
    ERW_Result PendingCount(size_t* c)                   override { return m_Impl->PendingCount(c); }
    ERW_Result Write(const void* b, size_t c, size_t* w) override { return m_Impl->Write(b, c, w);  }
    ERW_Result Flush()                                   override { return m_Impl->Flush();         }
    void       Close()                                   override {        m_Impl->Close();         }
    void       Abort()                                   override {        m_Impl->Abort();         }

private:
    SNetStorageObjectImpl& m_Impl;
};

// Ignore empty writes to iostream (CXX-8936)
struct SIoStreamEmbeddedStreamReaderWriter : SEmbeddedStreamReaderWriter
{
    SIoStreamEmbeddedStreamReaderWriter(SNetStorageObjectImpl& impl) : SEmbeddedStreamReaderWriter(impl) {}

    ERW_Result Write(const void* buf, size_t count, size_t* written) override
    {
        if (count) return SEmbeddedStreamReaderWriter::Write(buf, count, written);

        if (written) *written = 0;
        return eRW_Success;
    }
};

struct SNetStorageObjectRWStream : public CNcbiIostream
{
    SNetStorageObjectRWStream(SNetStorageObjectImpl* impl, IEmbeddedStreamReaderWriter *rw) :
        CNcbiIostream(0),
        m_Object(impl),
        m_Sb(rw, rw, 1, nullptr, CRWStreambuf::fLeakExceptions)
    {
        _ASSERT(impl);
        _ASSERT(rw);
        init(&m_Sb);
    }

    ~SNetStorageObjectRWStream() override { m_Object.Close(); }

private:
    CNetStorageObject m_Object;
    CRWStreambuf m_Sb;
};

ERW_Result SNetStorageObjectOState::Read(void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling Read() while writing " << GetLoc());
}

ERW_Result SNetStorageObjectOState::PendingCount(size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling PendingCount() while writing " << GetLoc());
}

bool SNetStorageObjectOState::Eof()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling Eof() while writing " << GetLoc());
}

ERW_Result SNetStorageObjectIState::Write(const void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling Write() while reading " << GetLoc());
}

ERW_Result SNetStorageObjectIState::Flush()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling Flush() while reading " << GetLoc());
}

Uint8 SNetStorageObjectIoState::GetSize()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling GetSize() while reading/writing " << GetLoc());
}

list<string> SNetStorageObjectIoState::GetAttributeList() const
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling GetAttributeList() while reading/writing " << GetLoc());
}

string SNetStorageObjectIoState::GetAttribute(const string&) const
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling GetAttribute() while reading/writing " << GetLoc());
}

void SNetStorageObjectIoState::SetAttribute(const string&, const string& )
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling SetAttribute() while reading/writing " << GetLoc());
}

CNetStorageObjectInfo SNetStorageObjectIoState::GetInfo()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling GetInfo() while reading/writing " << GetLoc());
}

void SNetStorageObjectIoState::SetExpiration(const CTimeout&)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling SetExpiration() while reading/writing " << GetLoc());
}

string SNetStorageObjectIoState::FileTrack_Path()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling FileTrack_Path() while reading/writing " << GetLoc());
}

string SNetStorageObjectIoState::Relocate(TNetStorageFlags, TNetStorageProgressCb)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling Relocate() while reading/writing " << GetLoc());
}

bool SNetStorageObjectIoState::Exists()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling Exists() while reading/writing " << GetLoc());
}

ENetStorageRemoveResult SNetStorageObjectIoState::Remove()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Calling Remove() while reading/writing " << GetLoc());
}

string SNetStorageObjectIoMode::ToString(EApi api, EMth mth)
{
    if (api == eBuffer) {
        if (mth == eRead)  return "Read(buffer)";
        if (mth == eWrite) return "Write(buffer)";
        if (mth == eEof)   return "Eof()";
    }
    
    if (api == eIoStream) {
        return "GetRWStream()";
    }
    
    if (api == eIReaderIWriter) {
        if (mth == eRead)  return "GetReader()";
        if (mth == eWrite) return "GetWriter()";
    }
    
    if (api == eString) {
        if (mth == eRead)  return "Read(string)";
        if (mth == eWrite) return "Write(string)";
    }
    
    _ASSERT(false);
    return ""; // Not reached
}

void SNetStorageObjectIoMode::Throw(EApi api, EMth mth, string object_loc)
{
    NCBI_THROW_FMT(CNetStorageException, eNotSupported,
            "Calling " << ToString(api, mth) << " after " <<
            ToString(m_Api, m_Mth) << " for " << object_loc);
}

IEmbeddedStreamReaderWriter& SNetStorageObjectImpl::GetReaderWriter()
{
    if (!m_ReaderWriter) m_ReaderWriter.reset(new SEmbeddedStreamReaderWriter(*this));
    return *m_ReaderWriter;
}

SNetStorageObjectImpl::~SNetStorageObjectImpl()
{
    try {
        if (m_Current) m_Current->Close();
    }
    NCBI_CATCH_ALL("Error while implicitly closing a NetStorage object.");
}

CNcbiIostream* SNetStorageObjectImpl::GetRWStream()
{
    if (!m_IoStreamReaderWriter) m_IoStreamReaderWriter.reset(new SIoStreamEmbeddedStreamReaderWriter(*this));
    return new SNetStorageObjectRWStream(this, m_IoStreamReaderWriter.get());
}

void SNetStorageObjectImpl::Close()
{
    _ASSERT(m_Current);
    m_IoMode.Reset();
    return m_Current->Close();
}

string CNetStorageObject::GetLoc() const
{
    return m_Impl--->GetLoc();
}

size_t CNetStorageObject::Read(void* buffer, size_t buf_size)
{
    size_t bytes_read;
    m_Impl->SetIoMode(SNetStorageObjectIoMode::eBuffer, SNetStorageObjectIoMode::eRead);
    m_Impl--->Read(buffer, buf_size, &bytes_read);
    return bytes_read;
}

void CNetStorageObject::Read(string* data)
{
    char buffer[64 * 1024];

    data->resize(0);
    size_t bytes_read;

    m_Impl->SetIoMode(SNetStorageObjectIoMode::eString, SNetStorageObjectIoMode::eRead);

    do {
        m_Impl--->Read(buffer, sizeof(buffer), &bytes_read);
        data->append(buffer, bytes_read);
    } while (!m_Impl--->Eof());

    Close();
}

IReader& CNetStorageObject::GetReader()
{
    m_Impl->SetIoMode(SNetStorageObjectIoMode::eIReaderIWriter, SNetStorageObjectIoMode::eRead);
    return m_Impl->GetReaderWriter();
}

bool CNetStorageObject::Eof()
{
    m_Impl->SetIoMode(SNetStorageObjectIoMode::eBuffer, SNetStorageObjectIoMode::eEof);
    return m_Impl--->Eof();
}

void CNetStorageObject::Write(const void* buffer, size_t buf_size)
{
    m_Impl->SetIoMode(SNetStorageObjectIoMode::eBuffer, SNetStorageObjectIoMode::eWrite);
    m_Impl--->Write(buffer, buf_size, NULL);
}

void CNetStorageObject::Write(const string& data)
{
    m_Impl->SetIoMode(SNetStorageObjectIoMode::eString, SNetStorageObjectIoMode::eWrite);
    m_Impl--->Write(data.data(), data.length(), NULL);
}

IEmbeddedStreamWriter& CNetStorageObject::GetWriter()
{
    m_Impl->SetIoMode(SNetStorageObjectIoMode::eIReaderIWriter, SNetStorageObjectIoMode::eWrite);
    return m_Impl->GetReaderWriter();
}

CNcbiIostream* CNetStorageObject::GetRWStream()
{
    m_Impl->SetIoMode(SNetStorageObjectIoMode::eIoStream, SNetStorageObjectIoMode::eAnyMth);
    return m_Impl->GetRWStream();
}

Uint8 CNetStorageObject::GetSize()
{
    return m_Impl--->GetSize();
}

CNetStorageObject::TAttributeList CNetStorageObject::GetAttributeList() const
{
    return m_Impl--->GetAttributeList();
}

string CNetStorageObject::GetAttribute(const string& attr_name) const
{
    return m_Impl--->GetAttribute(attr_name);
}

void CNetStorageObject::SetAttribute(const string& attr_name,
        const string& attr_value)
{
    m_Impl--->SetAttribute(attr_name, attr_value);
}

CNetStorageObjectInfo CNetStorageObject::GetInfo()
{
    return m_Impl--->GetInfo();
}

void CNetStorageObject::SetExpiration(const CTimeout& ttl)
{
    m_Impl--->SetExpiration(ttl);
}

void CNetStorageObject::Close()
{
    m_Impl->Close();
}

CNetStorage::CNetStorage(const string& init_string,
        TNetStorageFlags default_flags) :
    m_Impl(SNetStorage::CreateImpl(
                SNetStorage::SConfig::Build(init_string), default_flags))
{
}

CNetStorageObject CNetStorage::Create(TNetStorageFlags flags)
{
    return m_Impl->Create(flags);
}

CNetStorageObject CNetStorage::Open(const string& object_loc)
{
    return m_Impl->Open(object_loc);
}

string CNetStorage::Relocate(const string& object_loc, TNetStorageFlags flags, TNetStorageProgressCb cb)
{
    auto object = Open(object_loc);
    return object--->Relocate(flags, cb);
}

bool CNetStorage::Exists(const string& object_loc)
{
    auto object = Open(object_loc);
    return object--->Exists();
}

ENetStorageRemoveResult CNetStorage::Remove(const string& object_loc)
{
    auto object = Open(object_loc);
    return object--->Remove();
}

CNetStorageByKey::CNetStorageByKey(const string& init_string,
        TNetStorageFlags default_flags) :
    m_Impl(SNetStorage::CreateByKeyImpl(
                SNetStorage::SConfig::Build(init_string), default_flags))
{
}

CNetStorageObject CNetStorageByKey::Open(const string& unique_key,
        TNetStorageFlags flags)
{
    SNetStorage::SLimits::Check<SNetStorage::SLimits::SUserKey>(unique_key);
    return m_Impl->Open(unique_key, flags);
}

string CNetStorageByKey::Relocate(const string& unique_key,
        TNetStorageFlags flags, TNetStorageFlags old_flags,
        TNetStorageProgressCb cb)
{
    auto object = Open(unique_key, old_flags);
    return object--->Relocate(flags, cb);
}

bool CNetStorageByKey::Exists(const string& key, TNetStorageFlags flags)
{
    auto object = Open(key, flags);
    return object--->Exists();
}

ENetStorageRemoveResult CNetStorageByKey::Remove(const string& key,
        TNetStorageFlags flags)
{
    auto object = Open(key, flags);
    return object--->Remove();
}

inline 
CNetStorageException::EErrCode ConvertErrCode(CNetCacheException::TErrCode code)
{
    switch (code) {
    case CNetCacheException::eAuthenticationError:
    case CNetCacheException::eAccessDenied:
        return CNetStorageException::eAuthError;

    case CNetCacheException::eBlobNotFound:
        return CNetStorageException::eNotExists;

    case CNetCacheException::eKeyFormatError:
        return CNetStorageException::eInvalidArg;

    case CNetCacheException::eNotImplemented:
        return CNetStorageException::eNotSupported;

    case CNetCacheException::eServerError:
    case CNetCacheException::eUnknownCommand:
    case CNetCacheException::eInvalidServerResponse:
        return CNetStorageException::eServerError;

    case CNetCacheException::eBlobClipped:
        return CNetStorageException::eIOError;
    }

    return CNetStorageException::eUnknown;
}

void g_ThrowNetStorageException(const CDiagCompileInfo& compile_info,
        const CNetCacheException& prev_exception, const string& message)
{
    CNetStorageException::EErrCode err_code =
        ConvertErrCode(prev_exception.GetErrCode());
    throw CNetStorageException(compile_info, &prev_exception, err_code, message);
}

CNetStorageObject_FileTrack_Path::CNetStorageObject_FileTrack_Path(
        CNetStorageObject object)
    : m_Path(object--->FileTrack_Path())
{
}

void SNetStorage::SLimits::ThrowTooLong(const string& name, size_t max_length)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            name << " exceeds maximum allowed length of " <<
            max_length << " characters.");
}

void SNetStorage::SLimits::ThrowIllegalChars(const string& name,
        const string& value)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            name << " contains illegal characters: " <<
            NStr::PrintableString(value));
}

END_NCBI_SCOPE
