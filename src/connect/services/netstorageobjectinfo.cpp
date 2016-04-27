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
#include <connect/services/impl/netstorage_impl.hpp>


BEGIN_NCBI_SCOPE

typedef ENetStorageObjectLocation TLocation;

struct SData
{
    TLocation location;
    string object_loc;
    string object_key;
    CJsonNode object_loc_info;
    Uint8 file_size;
    CJsonNode st_info;

    SData() {}
    SData(TLocation l,
            const string& ol,
            const string& ok,
            CJsonNode::TInstance li,
            Uint8 fs,
            CJsonNode::TInstance si)
        : location(l),
          object_loc(ol),
          object_key(ok),
          object_loc_info(li),
          file_size(fs),
          st_info(si)
    {}
};

struct SLazyInitData : SData
{
    CJsonNode json;
    CTime time;

    SLazyInitData(const SData& d)
        : SData(d),
          data_source(true),
          initialized(false)
    {
        InitExtra();
    }

    SLazyInitData(const CJsonNode& j)
        : json(j),
          data_source(false),
          initialized(false)
    {
        Clean();
    }

    void Check()
    {
        if (!initialized) {
            initialized = true;
            if (data_source)
                InitJson();
            else
                InitData();
        }
    }

private:
    template <TLocation> CTime GetTime();
    void InitData();
    void InitExtra();
    void InitJson();
    void Clean();

    bool data_source;
    bool initialized;
};


struct SNetStorageObjectInfoImpl : CObject
{
    SNetStorageObjectInfoImpl(const SLazyInitData& data) : m_Data(data) {}

    TLocation GetLocation()            const { Check(); return m_Data.location; }
    CJsonNode GetObjectLocInfo()       const { Check(); return m_Data.object_loc_info; }
    CTime     GetCreationTime()        const { Check(); return m_Data.time; }
    Uint8     GetSize()                const { Check(); return m_Data.file_size; }
    CJsonNode GetStorageSpecificInfo() const { Check(); return m_Data.st_info; }
    CJsonNode GetJSON()                const { Check(); return m_Data.json; }

private:
    void Check() const { m_Data.Check(); }

    mutable SLazyInitData m_Data;
};


template <>
CTime SLazyInitData::GetTime<eNFL_FileTrack>()
{
    const char* const kISO8601TimeFormat = "Y-M-DTh:m:s.ro";

    if (st_info) {
        if (CJsonNode ctime = st_info.GetByKeyOrNull("ctime")) {
            return CTime(ctime.AsString(), kISO8601TimeFormat).ToLocalTime();
        }
    }

    return CTime();
}

template <>
CTime SLazyInitData::GetTime<eNFL_NetCache>()
{
    const char* const kNCTimeFormat = "M/D/Y h:m:s.r";

    if (st_info) {
        if (CJsonNode ctime = st_info.GetByKeyOrNull("Write time")) {
            return CTime(ctime.AsString(), kNCTimeFormat);
        }
    }

    return CTime();
}

void SLazyInitData::InitData()
{
    // Init data from JSON

    const string l(json.GetString("Location"));
    CJsonNode ol(json.GetByKeyOrNull("ObjectLoc"));
    CJsonNode ok(json.GetByKeyOrNull("ObjectKey"));
    CJsonNode size(json.GetByKeyOrNull("Size"));

    location =
        l == "NetCache"  ? eNFL_NetCache :
        l == "FileTrack" ? eNFL_FileTrack :
        l == "NotFound"  ? eNFL_NotFound : eNFL_Unknown;
    object_loc = ol ? ol.AsString() : kEmptyStr;
    object_key = ok ? ol.AsString() : kEmptyStr;
    object_loc_info = json.GetByKey("ObjectLocInfo");
    file_size = size ? (Uint8) size.AsInteger() : 0;
    st_info = json.GetByKeyOrNull("StorageSpecificInfo");
    InitExtra();
}

void SLazyInitData::InitExtra()
{
    if (location == eNFL_FileTrack) {
        time = GetTime<eNFL_FileTrack>();
    } else if (location == eNFL_NetCache) {
        time = GetTime<eNFL_NetCache>();
    }
}

void SLazyInitData::Clean()
{
    // Remove server reply
    json.DeleteByKey("Type");
    json.DeleteByKey("Status");
    json.DeleteByKey("RE");
}

void SLazyInitData::InitJson()
{
    // Init JSON from data

    const char* const kOutputTimeFormat = "M/D/Y h:m:s";
    json = CJsonNode::NewObjectNode();

    switch (location) {
    case eNFL_NetCache:
        json.SetByKey("CreationTime", CJsonNode::NewStringNode(
                GetTime<eNFL_NetCache>().AsString(kOutputTimeFormat)));
        json.SetString("Location", "NetCache");
        json.SetInteger("Size", file_size);
        break;
    case eNFL_FileTrack:
        json.SetByKey("CreationTime", CJsonNode::NewStringNode(
                GetTime<eNFL_FileTrack>().AsString(kOutputTimeFormat)));
        json.SetString("Location", "FileTrack");
        json.SetInteger("Size", file_size);
        break;
    default:
        json.SetString("Location", "NotFound");
    }

    json.SetString("ObjectLoc", object_loc);

    if (!object_key.empty())
        json.SetString("ObjectKey", object_key);

    if (object_loc_info)
        json.SetByKey("ObjectLocInfo", object_loc_info);

    if (st_info)
        json.SetByKey("StorageSpecificInfo", st_info);
}

CNetStorageObjectInfo g_CreateNetStorageObjectInfo(const string& object_loc,
        ENetStorageObjectLocation location,
        const CNetStorageObjectLoc* object_loc_struct,
        Uint8 file_size, CJsonNode::TInstance storage_specific_info)
{
    return new SNetStorageObjectInfoImpl(SData(location, object_loc,
            object_loc_struct ? object_loc_struct->GetUniqueKey() : kEmptyStr,
            object_loc_struct ? object_loc_struct->ToJSON() : NULL,
            file_size, storage_specific_info));
}

CNetStorageObjectInfo g_CreateNetStorageObjectInfo(
        const CJsonNode& object_info_node)
{
    return new SNetStorageObjectInfoImpl(object_info_node);
}

TLocation CNetStorageObjectInfo::GetLocation() const
{
    return m_Impl->GetLocation();
}

CJsonNode CNetStorageObjectInfo::GetObjectLocInfo() const
{
    return m_Impl->GetObjectLocInfo();
}

CTime CNetStorageObjectInfo::GetCreationTime() const
{
    return m_Impl->GetCreationTime();
}

Uint8 CNetStorageObjectInfo::GetSize() const
{
    return m_Impl->GetSize();
}

CJsonNode CNetStorageObjectInfo::GetStorageSpecificInfo() const
{
    return m_Impl->GetStorageSpecificInfo();
}

CJsonNode CNetStorageObjectInfo::ToJSON()
{
    return m_Impl->GetJSON();
}

END_NCBI_SCOPE
