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
 *   Implementation of CNetStorageObjectInfo.
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netstorage.hpp>


BEGIN_NCBI_SCOPE

struct SNetStorageObjectInfoImpl : public CObject
{
    SNetStorageObjectInfoImpl(const string& object_id,
            ENetStorageObjectLocation location,
            CJsonNode::TInstance object_id_info,
            Uint8 file_size,
            CJsonNode::TInstance storage_specific_info);

    string m_ObjectID;
    ENetStorageObjectLocation m_Location;
    CJsonNode m_ObjectIDInfo;
    Uint8 m_FileSize;
    CJsonNode m_StorageSpecificInfo;
};

static const char* s_NCTimeFormat = "M/D/Y h:m:s.r";
static const char* s_ISO8601TimeFormat = "Y-M-DTh:m:s.r";

SNetStorageObjectInfoImpl::SNetStorageObjectInfoImpl(const string& object_id,
        ENetStorageObjectLocation location,
        CJsonNode::TInstance object_id_info,
        Uint8 file_size,
        CJsonNode::TInstance storage_specific_info) :
    m_ObjectID(object_id),
    m_Location(location),
    m_ObjectIDInfo(object_id_info),
    m_FileSize(file_size),
    m_StorageSpecificInfo(storage_specific_info)
{
}

CNetStorageObjectInfo::CNetStorageObjectInfo(const string& object_id,
        ENetStorageObjectLocation location,
        const CNetStorageObjectID& object_id_struct,
        Uint8 file_size, CJsonNode::TInstance storage_specific_info) :
    m_Impl(new SNetStorageObjectInfoImpl(object_id, location,
            object_id_struct.ToJSON(), file_size, storage_specific_info))
{
}

CNetStorageObjectInfo::CNetStorageObjectInfo(const string& object_id,
        const CJsonNode& file_info_node)
{
    const string location_str(file_info_node.GetString("Location"));

    ENetStorageObjectLocation location =
        location_str == "NetCache" ? eNFL_NetCache :
        location_str == "FileTrack" ? eNFL_FileTrack :
        location_str == "NotFound" ? eNFL_NotFound : eNFL_Unknown;

    CJsonNode file_size_node(file_info_node.GetByKeyOrNull("Size"));

    Uint8 file_size = file_size_node ? (Uint8) file_size_node.AsInteger() : 0;

    m_Impl = new SNetStorageObjectInfoImpl(object_id, location,
            file_info_node.GetByKey("ObjectIDInfo"), file_size,
            file_info_node.GetByKeyOrNull("StorageSpecificInfo"));
}

ENetStorageObjectLocation CNetStorageObjectInfo::GetLocation() const
{
    return m_Impl->m_Location;
}

CJsonNode CNetStorageObjectInfo::GetObjectIDInfo() const
{
    return m_Impl->m_ObjectIDInfo;
}

CTime CNetStorageObjectInfo::GetCreationTime() const
{
    // TODO return a valid creation time
    return CTime();
}

Uint8 CNetStorageObjectInfo::GetSize()
{
    return m_Impl->m_FileSize;
}

CJsonNode CNetStorageObjectInfo::GetStorageSpecificInfo() const
{
    return m_Impl->m_StorageSpecificInfo;
}

string CNetStorageObjectInfo::GetNFSPathname() const
{
    try {
        return m_Impl->m_StorageSpecificInfo.GetString("path");
    }
    catch (CJsonException& e) {
        if (e.GetErrCode() != CJsonException::eKeyNotFound)
            throw;
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Cannot retrieve NFS path information for '" <<
                m_Impl->m_ObjectID << '\'');
    }
}

CJsonNode CNetStorageObjectInfo::ToJSON()
{
    CJsonNode root(CJsonNode::NewObjectNode());

    CJsonNode ctime;

    switch (m_Impl->m_Location) {
    case eNFL_Unknown:
    case eNFL_NotFound:
        root.SetString("Location", "NotFound");
        break;
    case eNFL_NetCache:
        root.SetString("Location", "NetCache");
        root.SetInteger("Size", (Int8) m_Impl->m_FileSize);
        if (m_Impl->m_StorageSpecificInfo) {
            ctime = m_Impl->m_StorageSpecificInfo.GetByKeyOrNull("Write time");
            if (ctime)
                ctime = CJsonNode::NewStringNode(CTime(ctime.AsString(),
                        s_NCTimeFormat).AsString(s_ISO8601TimeFormat));
        }
        break;
    case eNFL_FileTrack:
        root.SetString("Location", "FileTrack");
        root.SetInteger("Size", (Int8) m_Impl->m_FileSize);
        if (m_Impl->m_StorageSpecificInfo)
            // No time format conversion required for FileTrack.
            ctime = m_Impl->m_StorageSpecificInfo.GetByKeyOrNull("ctime");
    }

    if (ctime)
        root.SetByKey("CreationTime", ctime);

    if (m_Impl->m_ObjectIDInfo)
        root.SetByKey("ObjectIDInfo", m_Impl->m_ObjectIDInfo);

    if (m_Impl->m_StorageSpecificInfo)
        root.SetByKey("StorageSpecificInfo", m_Impl->m_StorageSpecificInfo);

    return root;
}

END_NCBI_SCOPE
