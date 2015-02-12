#ifndef MISC_NETSTORAGE___NETSTORAGEIMPL__HPP
#define MISC_NETSTORAGE___NETSTORAGEIMPL__HPP

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
 * Author: Rafael Sadyrov
 *
 */

// TODO: Check all my files for unnecessary includes

#include <connect/services/compound_id.hpp>
#include <connect/services/neticache_client.hpp>
#include <connect/services/netstorage.hpp>

#include "filetrack.hpp"

// TODO: Move internal classes to unnamed namespaces to avoid clashing

BEGIN_NCBI_SCOPE

namespace NImpl
{

struct SContext : CObject
{
    CNetICacheClient icache_client;
    SFileTrackAPI filetrack_api;
    CCompoundIDPool compound_id_pool;
    TNetStorageFlags default_flags;
    TNetStorageFlags valid_flags_mask;
    string app_domain;

    SContext(const string&, CNetICacheClient, TNetStorageFlags);
    Uint8 GetRandomNumber() { return filetrack_api.m_Random.GetRandUint8(); }
};

// TODO: Add constness to methods where applicable

class IState
{
public:
    virtual ~IState() {}

    virtual ERW_Result ReadImpl(void*, size_t, size_t*) = 0;
    virtual bool EofImpl() = 0;
    virtual ERW_Result WriteImpl(const void*, size_t, size_t*) = 0;
    virtual void CloseImpl() {}
    virtual void AbortImpl() { CloseImpl(); }
};

class ILocation
{
public:
    virtual ~ILocation() {}

    virtual IState* StartRead(void*, size_t, size_t*, ERW_Result*) = 0;
    virtual IState* StartWrite(const void*, size_t, size_t*, ERW_Result*) = 0;
    virtual Uint8 GetSizeImpl() = 0;
    virtual CNetStorageObjectInfo GetInfoImpl() = 0;
    virtual bool ExistsImpl() = 0;
    virtual void RemoveImpl() = 0;
};

class ISelector
{
public:
    virtual ~ISelector() {}

    virtual ILocation* First() = 0;
    virtual ILocation* Next() = 0;
    virtual string Locator() = 0;


    typedef auto_ptr<ISelector> Ptr;

    // TODO: It should be possible to reduce number of these methods
    // TODO: It might be possible to use the same name for all overloads
    static Ptr Create(SContext*, TNetStorageFlags);
    static Ptr CreateFromLoc(SContext*, const string&, TNetStorageFlags = 0);
    static Ptr CreateFromKey(SContext*, const string&, TNetStorageFlags);
    static Ptr Create(SContext*, TNetStorageFlags, const string&, Int8);
    static Ptr Create(SContext*, TNetStorageFlags, const string&);
};

}

NCBI_PARAM_DECL(string, netstorage_api, backend_storage);
typedef NCBI_PARAM_TYPE(netstorage_api, \
        backend_storage) TNetStorageAPI_BackendStorage;

END_NCBI_SCOPE

#endif
