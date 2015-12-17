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

#include <connect/ncbi_socket.h>

#include <corelib/ncbi_base64.h>
#include <corelib/ncbiargs.hpp>

#include <time.h>
#include <string.h>


#define NCBI_USE_ERRCODE_X  NetStorage_Common

#define FILETRACK_STORAGE_CODE "FT"
#define NETCACHE_STORAGE_CODE "NC"

#define STORAGE_INFO_CUE 0

BEGIN_NCBI_SCOPE

CNetStorageObjectLoc::CNetStorageObjectLoc(CCompoundIDPool::TInstance cid_pool,
        TNetStorageAttrFlags flags,
        const string& app_domain,
        Uint8 random_number,
        EFileTrackSite ft_site) :
    m_CompoundIDPool(cid_pool),
    m_LocatorFlags(x_StorageFlagsToLocatorFlags(flags, ft_site)),
    m_ObjectID(0),
    m_Location(eNFL_Unknown),
    m_AppDomain(app_domain),
    m_Timestamp(time(NULL)),
    m_Random(random_number),
    m_ShortUniqueKey(MakeShortUniqueKey()),
    m_UniqueKey(MakeUniqueKey()),
    m_NCFlags(0),
    m_Dirty(true)
{

}

CNetStorageObjectLoc::CNetStorageObjectLoc(CCompoundIDPool::TInstance cid_pool,
        TNetStorageAttrFlags flags,
        const string& app_domain,
        const string& unique_key,
        EFileTrackSite ft_site) :
    m_CompoundIDPool(cid_pool),
    m_LocatorFlags(x_StorageFlagsToLocatorFlags(flags, ft_site) | fLF_HasUserKey),
    m_ObjectID(0),
    m_Location(eNFL_Unknown),
    m_AppDomain(app_domain),
    m_ShortUniqueKey(unique_key),
    m_UniqueKey(MakeUniqueKey()),
    m_NCFlags(0),
    m_Dirty(true)
{
}

#define INVALID_LOC_ERROR_MSG "Invalid NetStorage object locator"

#define THROW_INVALID_LOC_ERROR(object_loc, msg) \
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg, \
                msg " '" << (object_loc) << '\'')

#define VERIFY_FIELD_EXISTS(field) \
        if (!(field)) { \
            THROW_INVALID_LOC_ERROR(object_loc, INVALID_LOC_ERROR_MSG); \
        }

ENetStorageObjectLocation s_LocationCodeToLocation(const string& location)
{
    if (location.length() == 2) {
        if (location.data()[0] == FILETRACK_STORAGE_CODE[0] &&
                location.data()[1] == FILETRACK_STORAGE_CODE[1])
            return eNFL_FileTrack;
        if (location.data()[0] == NETCACHE_STORAGE_CODE[0] &&
                location.data()[1] == NETCACHE_STORAGE_CODE[1])
            return eNFL_NetCache;
    }
    return eNFL_Unknown;
}

CNetStorageObjectLoc::CNetStorageObjectLoc(CCompoundIDPool::TInstance cid_pool,
        const string& object_loc) :
    m_CompoundIDPool(cid_pool),
    m_ObjectID(0),
    m_Location(eNFL_Unknown),
    m_NCFlags(0),
    m_Dirty(false),
    m_Locator(object_loc)
{
    Parse(object_loc);
}

CNetStorageObjectLoc::CNetStorageObjectLoc(CCompoundIDPool::TInstance cid_pool,
        const string& object_loc, TNetStorageAttrFlags flags) :
    m_CompoundIDPool(cid_pool),
    m_ObjectID(0),
    m_Location(eNFL_Unknown),
    m_NCFlags(0),
    m_Dirty(false),
    m_Locator(object_loc)
{
    Parse(object_loc);

    if (flags != GetStorageAttrFlags()) {
        m_Dirty = true;
        m_LocatorFlags = (m_LocatorFlags & eLF_FieldFlags) |
            x_StorageFlagsToLocatorFlags(flags);
    }
}

void CNetStorageObjectLoc::Parse(const string& object_loc)
{
    CCompoundID cid = m_CompoundIDPool.FromString(object_loc);

    // Check the ID class.
    switch (cid.GetClass()) {
    case eCIC_NetStorageObjectLocV1:
        THROW_INVALID_LOC_ERROR(object_loc,
                "Unsupported NetStorage object locator version");
    case eCIC_NetStorageObjectLoc:
        break;
    default:
        THROW_INVALID_LOC_ERROR(object_loc, INVALID_LOC_ERROR_MSG);
    }

    // Get locator flags.
    CCompoundIDField field = cid.GetFirst(eCIT_Flags);
    VERIFY_FIELD_EXISTS(field);
    m_LocatorFlags = (TLocatorFlags) field.GetFlags();

    // Restore NetStorage service name.
    if (m_LocatorFlags & fLF_NetStorageService) {
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_ServiceName = field.GetServiceName();
    }

    // Restore object ID.
    if (m_LocatorFlags & fLF_HasObjectID) {
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_ObjectID = field.GetID();
    }

    // Get the domain name.
    VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
    m_AppDomain = field.GetString();

    // Restore object identification.
    if (m_LocatorFlags & fLF_HasUserKey) {
        // Get the unique object key.
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_ShortUniqueKey = field.GetString();
    } else {
        // Get object creation timestamp.
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_Timestamp = field.GetTimestamp();
        // Get the random ID.
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_Random = (Uint8) field.GetRandom() << (sizeof(Uint4) * 8);
        VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
        m_Random |= field.GetRandom();
        m_ShortUniqueKey = MakeShortUniqueKey();
    }

    m_UniqueKey = MakeUniqueKey();

    // Not used, though has to be read to be backward-compatible
    if (m_LocatorFlags & fLF_Cacheable) {
        field.GetNextNeighbor();
    }

    // Find storage info (optional).
    for (field = cid.GetFirst(eCIT_Cue); field; field = field.GetNextHomogeneous()) {
        if (field.GetCue() == STORAGE_INFO_CUE) {
            // Restore object location.
            VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
            m_LocationCode = field.GetDatabaseName();
            m_Location = s_LocationCodeToLocation(m_LocationCode);

            // Restore storage-specific info.
            if (m_Location == eNFL_NetCache) {
                // Get NetCache flags.
                VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
                m_NCFlags = (TNetCacheFlags) field.GetFlags();
                // Get the service name.
                VERIFY_FIELD_EXISTS(field = field.GetNextNeighbor());
                m_NCServiceName = field.GetServiceName();
            }

            break;
        }
    }
}

void CNetStorageObjectLoc::SetLocation_NetCache(
        const string& service_name,
        bool allow_xsite_conn)
{
    m_Dirty = true;

    m_LocationCode = NETCACHE_STORAGE_CODE;
    m_Location = eNFL_NetCache;

    m_NCServiceName = service_name;

    if (allow_xsite_conn)
        m_NCFlags |= fNCF_AllowXSiteConn;
    else
        m_NCFlags &= ~(TNetCacheFlags) fNCF_AllowXSiteConn;

    // NB: FileTrack site must not be reset.
}

void CNetStorageObjectLoc::SetLocation_FileTrack(EFileTrackSite ft_site)
{
    m_Dirty = true;

    m_LocationCode = FILETRACK_STORAGE_CODE;
    m_Location = eNFL_FileTrack;

    m_NCServiceName.clear();
    m_NCFlags = 0;

    m_LocatorFlags &= ~(TLocatorFlags) (fLF_DevEnv | fLF_QAEnv);

    if (ft_site == eFileTrack_DevSite)
        m_LocatorFlags |= fLF_DevEnv;
    else if (ft_site == eFileTrack_QASite)
        m_LocatorFlags |= fLF_QAEnv;
}

CNetStorageObjectLoc::EFileTrackSite CNetStorageObjectLoc::GetFileTrackSite() const
{
    return m_LocatorFlags & fLF_DevEnv ? eFileTrack_DevSite :
            m_LocatorFlags & fLF_QAEnv ? eFileTrack_QASite :
                    eFileTrack_ProdSite;
}

string CNetStorageObjectLoc::MakeShortUniqueKey() const
{
    string result = NStr::NumericToString(m_Timestamp);
    result += '-';
    result += NStr::NumericToString(m_Random);

    if (m_LocatorFlags & fLF_HasObjectID) {
        result += '-';
        result += NStr::NumericToString(m_ObjectID);
    }

    return result;
}

void CNetStorageObjectLoc::x_Pack() const
{
    // Allocate a new CompoundID object.
    CCompoundID cid = m_CompoundIDPool.NewID(eCIC_NetStorageObjectLoc);

    // Save locator flags.
    // If its location is not set yet, add "Movable", so the locator can be used
    cid.AppendFlags(m_Location != eNFL_Unknown ?
            m_LocatorFlags : (m_LocatorFlags | fLF_Movable));

    // Save NetStorage service name.
    if (m_LocatorFlags & fLF_NetStorageService)
        cid.AppendServiceName(m_ServiceName);

    // Save object ID.
    if (m_LocatorFlags & fLF_HasObjectID)
        cid.AppendID(m_ObjectID);

    // Save the domain name.
    cid.AppendString(m_AppDomain);

    // Save object identification
    if (m_LocatorFlags & fLF_HasUserKey)
        // Save the unique object key.
        cid.AppendString(m_ShortUniqueKey);
    else {
        // Save object creation timestamp.
        cid.AppendTimestamp(m_Timestamp);
        // Save the random ID.
        cid.AppendRandom(m_Random >> (sizeof(Uint4) * 8));
        cid.AppendRandom((Uint4) m_Random);
    }

    // Not used, though has to be written to be backward-compatible
    if (m_LocatorFlags & fLF_Cacheable)
        cid.AppendInteger(0);

    // Save storage info (optional).
    if (m_Location != eNFL_Unknown) {
        cid.AppendCue(STORAGE_INFO_CUE);

        // Save object location.
        cid.AppendDatabaseName(m_LocationCode);

        switch (m_Location) {
        case eNFL_NetCache:
            // Save NetCache flags.
            cid.AppendFlags(m_NCFlags);
            // Save the service name.
            cid.AppendServiceName(m_NCServiceName);
            break;
        default:
            break;
        }
    }

    // Now pack it all up.
    m_Locator = cid.ToString();

    m_Dirty = false;
}

TNetStorageAttrFlags CNetStorageObjectLoc::GetStorageAttrFlags() const
{
    TNetStorageAttrFlags flags = 0;

    if (m_LocatorFlags & fLF_Movable)
        flags |= fNST_Movable;
    if (m_LocatorFlags & fLF_Cacheable)
        flags |= fNST_Cacheable;
    if (m_LocatorFlags & fLF_NoMetaData)
        flags |= fNST_NoMetaData;

    return flags;
}

CNetStorageObjectLoc::TLocatorFlags
CNetStorageObjectLoc::x_StorageFlagsToLocatorFlags(
        TNetStorageAttrFlags storage_flags,
        EFileTrackSite ft_site)
{
    TLocatorFlags locator_flags = 0;

    if (storage_flags & fNST_Movable)
        locator_flags |= fLF_Movable;
    if (storage_flags & fNST_Cacheable)
        locator_flags |= fLF_Cacheable;
    if (storage_flags & fNST_NoMetaData)
        locator_flags |= fLF_NoMetaData;

    if (ft_site == eFileTrack_DevSite)
        locator_flags |= fLF_DevEnv;
    else if (ft_site == eFileTrack_QASite)
        locator_flags |= fLF_QAEnv;

    return locator_flags;
}

CJsonNode CNetStorageObjectLoc::ToJSON() const
{
    CJsonNode root(CJsonNode::NewObjectNode());
    ToJSON(root);
    return root;
}

void CNetStorageObjectLoc::ToJSON(CJsonNode& root) const
{
    root.SetInteger("Version", 1 +
            eCIC_NetStorageObjectLoc - eCIC_NetStorageObjectLocV1);

    root.SetString("Environment",
            m_LocatorFlags & fLF_DevEnv ? "dev/test" :
            m_LocatorFlags & fLF_QAEnv ? "QA" : "production");

    if (m_LocatorFlags & fLF_NetStorageService)
        root.SetString("ServiceName", m_ServiceName);

    CJsonNode storage_flags(CJsonNode::NewObjectNode());

    storage_flags.SetBoolean("Movable",
            (m_LocatorFlags & fLF_Movable) != 0);
    storage_flags.SetBoolean("Cacheable",
            (m_LocatorFlags & fLF_Cacheable) != 0);
    storage_flags.SetBoolean("NoMetaData",
            (m_LocatorFlags & fLF_NoMetaData) != 0);

    root.SetByKey("StorageFlags", storage_flags);
    root.SetString("Namespace", m_AppDomain);
    root.SetString("ShortUniqueKey", m_ShortUniqueKey);

    if (!m_LocationCode.empty())
        root.SetString("DefaultLocation", m_LocationCode);

    CJsonNode storage_info(CJsonNode::NewObjectNode());

    switch (m_Location) {
    case eNFL_NetCache:
        storage_info.SetString("ServiceName", m_NCServiceName);
        storage_info.SetBoolean("AllowXSiteConn", IsXSiteProxyAllowed());

        root.SetByKey("NetCache", storage_info);
        break;
    default:
        break;
    }
}

END_NCBI_SCOPE
