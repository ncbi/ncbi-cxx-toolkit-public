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
 *   Implementation of the unified network blob storage API.
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netstorage.hpp>
#include <connect/services/error_codes.hpp>

#define NCBI_USE_ERRCODE_X  NetStorage_RPC

#define READ_CHUNK_SIZE (4 * 1024)

BEGIN_NCBI_SCOPE

const char* CNetStorageException::GetErrCodeString() const
{
    switch (GetErrCode()) {
    case eInvalidArg:
        return "eInvalidArg";
    case eNotExist:
        return "eNotExist";
    case eIOError:
        return "eIOError";
    case eTimeout:
        return "eTimeout";
    default:
        return CException::GetErrCodeString();
    }
}

struct SNetStorageRPC : public SNetStorageImpl
{
    SNetStorageRPC(const string& /*init_string*/,
            TNetStorageFlags /*default_flags*/)
    {
    }

    virtual CNetFile Create(TNetStorageFlags flags = 0);
    virtual CNetFile Open(const string& file_id, TNetStorageFlags flags = 0);
    virtual string Relocate(const string& file_id, TNetStorageFlags flags);
    virtual bool Exists(const string& file_id);
    virtual void Remove(const string& file_id);
};

CNetFile SNetStorageRPC::Create(TNetStorageFlags /*flags*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

CNetFile SNetStorageRPC::Open(const string& /*file_id*/,
        TNetStorageFlags /*flags*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

string SNetStorageRPC::Relocate(const string& /*file_id*/,
        TNetStorageFlags /*flags*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

bool SNetStorageRPC::Exists(const string& /*file_id*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

void SNetStorageRPC::Remove(const string& /*file_id*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

CNetStorage::CNetStorage(const string& init_string,
        TNetStorageFlags default_flags) :
    m_Impl(new SNetStorageRPC(init_string, default_flags))
{
}

struct SNetFileRPC : public SNetFileImpl
{
    virtual string GetID();
    virtual size_t Read(void* buffer, size_t buf_size);
    virtual bool Eof();
    virtual void Write(const void* buffer, size_t buf_size);
    virtual Uint8 GetSize();
    virtual void Close();
};

string SNetFileRPC::GetID()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

size_t SNetFileRPC::Read(void* /*buffer*/, size_t /*buf_size*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

bool SNetFileRPC::Eof()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

void SNetFileRPC::Write(const void* /*buffer*/, size_t /*buf_size*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

Uint8 SNetFileRPC::GetSize()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

void SNetFileRPC::Close()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

void SNetFileImpl::Read(string* data)
{
    data->resize(data->capacity() == 0 ? READ_CHUNK_SIZE : data->capacity());

    size_t bytes_read = Read(const_cast<char*>(data->data()), data->capacity());

    data->resize(bytes_read);

    if (bytes_read < data->capacity())
        return;

    vector<string> chunks(1, *data);

    while (!Eof()) {
        string buffer(READ_CHUNK_SIZE, '\0');

        bytes_read = Read(const_cast<char*>(buffer.data()), buffer.length());

        if (bytes_read < buffer.length()) {
            buffer.resize(bytes_read);
            chunks.push_back(buffer);
            break;
        }

        chunks.push_back(buffer);
    }

    *data = NStr::Join(chunks, kEmptyStr);
}

struct SNetStorageByKeyRPC : public SNetStorageRPC
{
    SNetStorageByKeyRPC(const string& init_string,
            TNetStorageFlags default_flags) :
        SNetStorageRPC(init_string, default_flags)
    {
    }

    virtual CNetFile Open(const string& unique_key,
            TNetStorageFlags flags = 0);
    virtual string Relocate(const string& unique_key,
            TNetStorageFlags flags, TNetStorageFlags old_flags = 0);
    virtual bool Exists(const string& key, TNetStorageFlags flags = 0);
    virtual void Remove(const string& key, TNetStorageFlags flags = 0);
};


CNetFile SNetStorageByKeyRPC::Open(const string& /*unique_key*/,
        TNetStorageFlags /*flags*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

string SNetStorageByKeyRPC::Relocate(const string& /*unique_key*/,
        TNetStorageFlags /*flags*/, TNetStorageFlags /*old_flags*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

bool SNetStorageByKeyRPC::Exists(const string& /*key*/,
        TNetStorageFlags /*flags*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

void SNetStorageByKeyRPC::Remove(const string& /*key*/,
        TNetStorageFlags /*flags*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}


END_NCBI_SCOPE
