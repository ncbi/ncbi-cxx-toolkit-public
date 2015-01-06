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
 * Authors:  Dmitry Kazimirov, Rafael Sadyrov
 *
 * File Description:
 *   Implementation of CNetStorageObjectInfo.
 *
 */

#include <ncbi_pch.hpp>
#include <connect/services/netstorage.hpp>


BEGIN_NCBI_SCOPE

// Context is shared between all lazy initializers
struct SContext
{
    struct SData
    {
        ENetStorageObjectLocation loc;
        CJsonNode loc_info;
        Uint8 file_size;
        CJsonNode st_info;

        SData() {}
        SData(ENetStorageObjectLocation l,
                CJsonNode::TInstance li,
                Uint8 fs,
                CJsonNode::TInstance si)
            : loc(l),
              loc_info(li),
              file_size(fs),
              st_info(si)
        {}
    };

    // Source of data
    enum { eNothing, eData, eJson } use;
    SData data;
    CJsonNode json;

    SContext() : use(eNothing) {}
    SContext(const SData& d) : use(eData), data(d) {}
    SContext(const CJsonNode& j) : use(eJson), json(j) {}

    template <ENetStorageObjectLocation>
    CTime GetTime();
};

// Lazy initialization
class CLazyInit : protected virtual SContext
{
protected:
    CLazyInit(int primary) : m_Initialized(use == primary) {}
    virtual ~CLazyInit() {}

    void Check()
    {
        if (!m_Initialized) {
            m_Initialized = true;
            Init();
        }
    }

private:
    virtual void Init() = 0;

    bool m_Initialized;
};

class CDataSource : protected CLazyInit
{
public:
    ENetStorageObjectLocation GetLocation() { Check(); return data.loc; }
    CJsonNode GetObjectLocInfo()            { Check(); return data.loc_info; }
    CTime GetCreationTime()                 { Check(); return m_CreationTime; }
    Uint8 GetSize()                         { Check(); return data.file_size; }
    CJsonNode GetStorageSpecificInfo()      { Check(); return data.st_info; }

protected:
    CDataSource();

private:
    void Init();
    void InitExtra();


    CTime m_CreationTime;
};

class CJsonSource : protected CLazyInit
{
public:
    CJsonNode GetJSON() { Check(); return json; }

protected:
    CJsonSource() : CLazyInit(eJson) { Clean(); }

private:
    void Clean();
    void Init();
};

struct SNetStorageObjectInfoImpl : CObject, CJsonSource, CDataSource
{
    SNetStorageObjectInfoImpl(const string& loc, const SContext::SData& data)
        : SContext(data), m_Loc(loc)
    {}

    SNetStorageObjectInfoImpl(const string& loc, const CJsonNode& json)
        : SContext(json), m_Loc(loc)
    {}

    string GetNFSPathname();

    // Convenience method to avoid using const_cast
    typedef SNetStorageObjectInfoImpl& TSelf;
    TSelf NonConst() const { return const_cast<TSelf>(*this); }

private:
    string m_Loc;
};


template <>
CTime SContext::GetTime<eNFL_FileTrack>()
{
    const char* const kISO8601TimeFormat = "Y-M-DTh:m:s.r:z";

    if (data.st_info) {
        if (CJsonNode ctime = data.st_info.GetByKeyOrNull("ctime")) {
            return CTime(ctime.AsString(), kISO8601TimeFormat).ToLocalTime();
        }
    }

    return CTime();
}

template <>
CTime SContext::GetTime<eNFL_NetCache>()
{
    const char* const kNCTimeFormat = "M/D/Y h:m:s.r";

    if (data.st_info) {
        if (CJsonNode ctime = data.st_info.GetByKeyOrNull("Write time")) {
            return CTime(ctime.AsString(), kNCTimeFormat);
        }
    }

    return CTime();
}

CDataSource::CDataSource()
    : CLazyInit(eData)
{
    if (use == eData) {
        InitExtra();
    }
}

void CDataSource::Init()
{
    // Init data from JSON

    _ASSERT(use == eJson);

    const string loc(json.GetString("Location"));
    CJsonNode size(json.GetByKeyOrNull("Size"));

    data.loc =
        loc == "NetCache"  ? eNFL_NetCache :
        loc == "FileTrack" ? eNFL_FileTrack :
        loc == "NotFound"  ? eNFL_NotFound : eNFL_Unknown;
    data.loc_info = json.GetByKey("ObjectLocInfo");
    data.file_size = size ? (Uint8) size.AsInteger() : 0;
    data.st_info = json.GetByKeyOrNull("StorageSpecificInfo");
    InitExtra();
}

void CDataSource::InitExtra()
{
    if (data.loc == eNFL_FileTrack) {
        m_CreationTime = GetTime<eNFL_FileTrack>();
    } else if (data.loc == eNFL_NetCache) {
        m_CreationTime = GetTime<eNFL_NetCache>();
    }
}

void CJsonSource::Clean()
{
    if (use == eJson) {
        // Remove server reply
        json.DeleteByKey("Type");
        json.DeleteByKey("Status");
        json.DeleteByKey("RE");
    }
}

void CJsonSource::Init()
{
    // Init JSON from data

    _ASSERT(use == eData);

    const char* const kOutputTimeFormat = "M/D/Y h:m:s";
    json = CJsonNode::NewObjectNode();

    switch (data.loc) {
    case eNFL_NetCache:
        json.SetByKey("CreationTime", CJsonNode::NewStringNode(
                GetTime<eNFL_NetCache>().AsString(kOutputTimeFormat)));
        json.SetString("Location", "NetCache");
        json.SetInteger("Size", data.file_size);
        break;
    case eNFL_FileTrack:
        json.SetByKey("CreationTime", CJsonNode::NewStringNode(
                GetTime<eNFL_FileTrack>().AsString(kOutputTimeFormat)));
        json.SetString("Location", "FileTrack");
        json.SetInteger("Size", data.file_size);
        break;
    default:
        json.SetString("Location", "NotFound");
    }

    if (data.loc_info)
        json.SetByKey("ObjectLocInfo", data.loc_info);

    if (data.st_info)
        json.SetByKey("StorageSpecificInfo", data.st_info);
}

string SNetStorageObjectInfoImpl::GetNFSPathname()
{
    try {
        if (CJsonNode st_info = GetStorageSpecificInfo())
            return st_info.GetString("path");
    }
    catch (CJsonException& e) {
        if (e.GetErrCode() != CJsonException::eKeyNotFound)
            throw;
    }

    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Cannot retrieve NFS path information for '" << m_Loc << '\'');
}

CNetStorageObjectInfo::CNetStorageObjectInfo(const string& object_loc,
        ENetStorageObjectLocation location,
        const CNetStorageObjectLoc* object_loc_struct,
        Uint8 file_size, CJsonNode::TInstance storage_specific_info)
    : m_Impl(new SNetStorageObjectInfoImpl(object_loc, SContext::SData(location,
            object_loc_struct ? object_loc_struct->ToJSON() : NULL,
            file_size, storage_specific_info)))
{
}

CNetStorageObjectInfo::CNetStorageObjectInfo(const string& object_loc,
        const CJsonNode& object_info_node)
    : m_Impl(new SNetStorageObjectInfoImpl(object_loc, object_info_node))
{
}

ENetStorageObjectLocation CNetStorageObjectInfo::GetLocation() const
{
    return m_Impl->NonConst().GetLocation();
}

CJsonNode CNetStorageObjectInfo::GetObjectLocInfo() const
{
    return m_Impl->NonConst().GetObjectLocInfo();
}

CTime CNetStorageObjectInfo::GetCreationTime() const
{
    return m_Impl->NonConst().GetCreationTime();
}

Uint8 CNetStorageObjectInfo::GetSize() const
{
    return m_Impl->NonConst().GetSize();
}

CJsonNode CNetStorageObjectInfo::GetStorageSpecificInfo() const
{
    return m_Impl->NonConst().GetStorageSpecificInfo();
}

string CNetStorageObjectInfo::GetNFSPathname() const
{
    return m_Impl->NonConst().GetNFSPathname();
}

CJsonNode CNetStorageObjectInfo::ToJSON()
{
    return m_Impl->GetJSON();
}

END_NCBI_SCOPE
