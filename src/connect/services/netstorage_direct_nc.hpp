#ifndef CONNECT_SERVICES__NETSTORAGE_DIRECT_NC__HPP
#define CONNECT_SERVICES__NETSTORAGE_DIRECT_NC__HPP

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
 *   Direct access to NetCache servers through NetStorage API (declarations).
 *
 */

#include "netstorage_rpc.hpp"
#include "netcache_rw.hpp"

BEGIN_NCBI_SCOPE

struct SNetStorage_NetCacheBlob : public INetStorageObjectState
{
private:
    struct SIState : public SNetStorageObjectChildState<SNetStorageObjectIState, string>
    {
        typedef SNetStorageObjectChildState<SNetStorageObjectIState, string> TBase;

        unique_ptr<CNetCacheReader> reader;

        using TBase::TBase;

        ERW_Result Read(void* buf, size_t count, size_t* read) override;
        ERW_Result PendingCount(size_t* count) override;
        bool Eof() override;

        void Close() override;
        void Abort() override;
    };

    struct SOState : public SNetStorageObjectChildState<SNetStorageObjectOState, string>
    {
        typedef SNetStorageObjectChildState<SNetStorageObjectOState, string> TBase;

        unique_ptr<IEmbeddedStreamWriter> writer;

        using TBase::TBase;

        ERW_Result Write(const void* buf, size_t count, size_t* written) override;
        ERW_Result Flush() override;

        void Close() override;
        void Abort() override;
    };

public:
    SNetStorage_NetCacheBlob(SNetStorageObjectImpl& fsm, CNetCacheAPI::TInstance netcache_api,
            const string& blob_key) :
        INetStorageObjectState(fsm),
        m_NetCacheAPI(netcache_api),
        m_BlobKey(blob_key),
        m_IState(fsm, m_BlobKey),
        m_OState(fsm, m_BlobKey)
    {
    }

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read) override;
    ERW_Result PendingCount(size_t* count) override;

    ERW_Result Write(const void* buf, size_t count, size_t* bytes_written) override;
    ERW_Result Flush() override;
    void Close() override;
    void Abort() override;

    string GetLoc() const override { return m_BlobKey; }
    bool Eof() override;
    Uint8 GetSize() override;
    list<string> GetAttributeList() const override;
    string GetAttribute(const string& attr_name) const override;
    void SetAttribute(const string& attr_name, const string& attr_value) override;
    CNetStorageObjectInfo GetInfo() override;
    void SetExpiration(const CTimeout&) override;

    string FileTrack_Path() override;

    void StartWriting();

private:
    CNetCacheAPI m_NetCacheAPI;
    string m_BlobKey;
    SIState m_IState;
    SOState m_OState;
};

class CDNCNetStorage
{
public:
    static CNetStorageObject Create(CNetCacheAPI::TInstance nc_api);
    static CNetStorageObject Open(CNetCacheAPI::TInstance nc_api,
        const string& blob_key);
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES__NETSTORAGE_DIRECT_NC__HPP */
