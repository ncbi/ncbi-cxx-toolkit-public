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

#include <ncbi_pch.hpp>
#include "object.hpp"

#include <array>
#include <sstream>


BEGIN_NCBI_SCOPE


namespace NDirectNetStorageImpl
{

TNetStorageFlags s_DefaultFlags(const SContext* context, TNetStorageFlags flags)
{
    return flags ? flags : context->default_flags;
}


CSelector* s_Create(SContext* context, SNetStorageObjectImpl& fsm, const TObjLoc& object_loc, TNetStorageFlags flags)
{
    flags = s_DefaultFlags(context, flags);
    TObjLoc loc(context->compound_id_pool, object_loc.GetLocator());
    loc.SetStorageAttrFlags(flags);
    return new CSelector(fsm, loc, context, nullptr, flags);
}


CSelector* s_Create(SContext* context, SNetStorageObjectImpl& fsm, bool* cancel_relocate, const string& object_loc)
{
    TObjLoc loc(context->compound_id_pool, object_loc);
    return new CSelector(fsm, loc, context, cancel_relocate, loc.GetStorageAttrFlags(), loc.GetLocation());
}


CSelector* s_Create(SContext* context, SNetStorageObjectImpl& fsm, TNetStorageFlags flags, const string& service = kEmptyStr)
{
    flags = s_DefaultFlags(context, flags);
    TObjLoc loc(context->compound_id_pool, flags, context->app_domain,
            context->random.GetRandUint8(), context->filetrack_api.config.site);

    // Non empty service name means this is called by NetStorage server
    if (!service.empty()) {
        // Do not set fake service name used by NST health check script
        if (NStr::CompareNocase(service, "LBSMDNSTTestService")) {
            loc.SetServiceName(service);
        }
    }

    return new CSelector(fsm, loc, context, nullptr, flags);
}


CSelector* s_Create(SContext* context, SNetStorageObjectImpl& fsm, bool* cancel_relocate, const string& key, TNetStorageFlags flags,
        const string& service)
{
    flags = s_DefaultFlags(context, flags);
    TObjLoc loc(context->compound_id_pool, flags, context->app_domain,
            key, context->filetrack_api.config.site);

    // Non empty service name means this is called by NetStorage server
    if (!service.empty()) loc.SetServiceName(service);
    return new CSelector(fsm, loc, context, cancel_relocate, flags);
}


CObj::CObj(SNetStorageObjectImpl& fsm, CObj* source, TNetStorageFlags flags) :
    m_Selector(s_Create(&source->m_Selector->GetContext(), fsm, source->Locator(), flags)),
    m_Location(this),
    m_IsOpened(false)
{
}


CObj::CObj(SNetStorageObjectImpl& fsm, SContext* context, TNetStorageFlags flags) :
    m_Selector(s_Create(context, fsm, flags)),
    m_Location(this),
    m_IsOpened(false)
{
}


CObj::CObj(SNetStorageObjectImpl& fsm, SContext* context, TNetStorageFlags flags, const string& service) :
    m_Selector(s_Create(context, fsm, flags, service)),
    m_Location(this),
    m_IsOpened(false)
{
    // Server reports locator to the client before writing anything
    // So, object must choose location for writing here to make locator valid
    m_Selector->SetLocator();
}


CObj::CObj(SNetStorageObjectImpl& fsm, SContext* context, const string& object_loc) :
    m_Selector(s_Create(context, fsm, &m_CancelRelocate, object_loc)),
    m_Location(this),
    m_IsOpened(true)
{
}


CObj::CObj(SNetStorageObjectImpl& fsm, SContext* context, const string& key, TNetStorageFlags flags, const string& service) :
    m_Selector(s_Create(context, fsm, &m_CancelRelocate, key, flags, service)),
    m_Location(this),
    m_IsOpened(true)
{
}


template <class TCaller>
auto CObj::Meta(TCaller caller) -> decltype(caller(nullptr))
{
    return Meta(caller, m_Location != static_cast<ILocation*>(this));
}


template <class TCaller>
auto CObj::Meta(TCaller caller, bool restartable) -> decltype(caller(nullptr))
{
    try {
        return caller(m_Location);
    }
    catch (CNetStorageException& e) {
        if (e.GetErrCode() != CNetStorageException::eNotExists) throw;
        if (!restartable) throw;
    }

    // Restarting location search,
    // as object location might be changed while we were using it.
    m_Location = this;
    m_Selector->Restart();
    return caller(m_Location);
}


Uint8 CObj::GetSize()
{
    return Meta([](ILocation* l) { return l->GetSizeImpl(); });
}


list<string> CObj::GetAttributeList() const
{
    NCBI_THROW(CNetStorageException, eNotSupported,
        "Attribute support is only implemented in NetStorage server.");
}


string CObj::GetAttribute(const string&) const
{
    NCBI_THROW(CNetStorageException, eNotSupported,
        "Attribute support is only implemented in NetStorage server.");
}


void CObj::SetAttribute(const string&, const string&)
{
    NCBI_THROW(CNetStorageException, eNotSupported,
        "Attribute support is only implemented in NetStorage server.");
}


CNetStorageObjectInfo CObj::GetInfo()
{
    return Meta([](ILocation* l) { return l->GetInfoImpl(); });
}


void CObj::SetExpiration(const CTimeout& ttl)
{
    return Meta([&](ILocation* l) { return l->SetExpirationImpl(ttl); });
}


string CObj::FileTrack_Path()
{
    return Meta([&](ILocation* l) { return l->FileTrack_PathImpl(); });
}


ILocation::TUserInfo CObj::GetUserInfo()
{
    return Meta([&](ILocation* l) { return l->GetUserInfoImpl(); });
}


CNetStorageObjectLoc& CObj::Locator()
{
    return m_Selector->Locator();
}


void s_Check(ERW_Result result, const char* what)
{
    switch (result) {
    case eRW_Success:
    case eRW_Eof:
        return;

    case eRW_Timeout:
        NCBI_THROW_FMT(CNetStorageException, eTimeout,
            "Timeout during " << what << " on relocate");

    case eRW_Error:
        NCBI_THROW_FMT(CNetStorageException, eIOError,
            "IO error during " << what << " on relocate");

    case eRW_NotImplemented:
        NCBI_THROW_FMT(CNetStorageException, eNotSupported,
            "Non-implemented operation called during " << what << " on relocate");
    }
}


string CObj::Relocate(TNetStorageFlags flags, TNetStorageProgressCb cb)
{
    const size_t max = m_Selector->GetContext().relocate_chunk;
    size_t current = 0;
    size_t total = 0;
    size_t bytes_read;
    array<char, 128 * 1024> buffer;

    // Use Read() to detect the current location
    m_Location = this;
    m_Selector->Restart();
    ERW_Result result = eRW_Error;
    auto rw_state = StartRead(buffer.data(), buffer.size(), &bytes_read, &result);
    s_Check(result, "reading");

    _ASSERT(rw_state); // Cannot be a nullptr if result check passed

    // Selector can only be cloned after location is detected
    CObj* new_obj;
    CNetStorageObject new_file(Clone(flags, &new_obj));

    if (m_Location->IsSame(new_obj->m_Selector->First())) {
        rw_state->Close();
        return new_obj->m_Selector->Locator().GetLocator();
    }

    for (;;) {
        current += bytes_read;

        const char *data = buffer.data();
        size_t bytes_written;

        do {
            s_Check(new_file--->Write(data, bytes_read, &bytes_written), "writing");
            data += bytes_written;
            bytes_read -= bytes_written;
        }
        while (bytes_read);

        if (rw_state->Eof()) break;

        if (current >= max) {
            total += current;
            current = 0;

            if (cb) {
                CJsonNode progress(CJsonNode::NewObjectNode());
                progress.SetInteger("BytesRelocated", total);
                progress.SetString("Message", "Relocating");

                // m_CancelRelocate may only be set to true inside the callback
                if (cb(progress), m_CancelRelocate) {
                    m_CancelRelocate = false;
                    m_Location = this;
                    new_file--->Abort();
                    rw_state->Abort();
                    NCBI_THROW(CNetStorageException, eInterrupted,
                        "Request to interrupt Relocate has been received.");
                }
            }
        }

        s_Check(rw_state->Read(buffer.data(), buffer.size(), &bytes_read), "reading");
    }

    new_file--->Close();
    rw_state->Close();
    Remove();
    return new_file--->GetLoc();
}


void CObj::CancelRelocate()
{
}


bool CObj::Exists()
{
    return Meta([&](ILocation* l) { return l->ExistsImpl(); });
}


ENetStorageRemoveResult CObj::Remove()
{
    return Meta([&](ILocation* l) { return l->RemoveImpl(); });
}


void CObj::Close()
{
}


void CObj::Abort()
{
}


string CObj::GetLoc() const
{
    return m_Selector->Locator().GetLocator();
}


ERW_Result CObj::Read(void* buf, size_t count, size_t* bytes_read)
{
    auto f = [&](ILocation*)
    {
        ERW_Result result = eRW_Error;
        StartRead(buf, count, bytes_read, &result);
        return result;
    };

    return Meta(f, m_Selector->InProgress());
}


bool CObj::Eof()
{
    return false;
}


ERW_Result CObj::Write(const void* buf, size_t count, size_t* bytes_written)
{
    // If object already exists, it will be overwritten in the same backend
    if (m_IsOpened && !Exists()) {
        m_Selector->Restart();
    }

    ERW_Result result = eRW_Error;
    ILocation* l = m_Selector->First();
    EnterState(l->StartWrite(buf, count, bytes_written, &result));
    m_Location = l;
    if (m_IsOpened) RemoveOldCopyIfExists();
    m_IsOpened = true;
    return result;
}


INetStorageObjectState* CObj::StartRead(void* buf, size_t count, size_t* bytes_read,
        ERW_Result* result)
{
    for (ILocation* l = m_Selector->First(); l; l = m_Selector->Next()) {
        auto rw_state = l->StartRead(buf, count, bytes_read, result);
        if (rw_state) {
            m_Location = l;
            EnterState(rw_state);
            return rw_state;
        }
    }

    *result = eRW_Error;
    return nullptr;
}


INetStorageObjectState* CObj::StartWrite(const void*, size_t, size_t*, ERW_Result*)
{
    // This just cannot happen
    _TROUBLE;
    return NULL;
}


template <class TCaller>
auto CObj::MetaImpl(TCaller caller) -> decltype(caller(nullptr))
{
    ILocation* l = m_Selector->First();

    for (;;) {
        m_Location = l;

        try {
            return caller(m_Location);
        }
        catch (CNetStorageException& e) {
            if (e.GetErrCode() != CNetStorageException::eNotExists) throw;

            l = m_Selector->Next();

            if (!l) throw;
        }
    }
}


Uint8 CObj::GetSizeImpl()
{
    return MetaImpl([&](ILocation* l) { return l->GetSizeImpl(); });
}


CNetStorageObjectInfo CObj::GetInfoImpl()
{
    return MetaImpl([&](ILocation* l) { return l->GetInfoImpl(); });
}


bool CObj::ExistsImpl()
{
    return MetaImpl([&](ILocation* l) { return l->ExistsImpl(); });
}


ENetStorageRemoveResult CObj::RemoveImpl()
{
    return MetaImpl([&](ILocation* l) { return l->RemoveImpl(); });
}


void CObj::SetExpirationImpl(const CTimeout& ttl)
{
    return MetaImpl([&](ILocation* l) { return l->SetExpirationImpl(ttl); });
}


string CObj::FileTrack_PathImpl()
{
    return MetaImpl([&](ILocation* l) { return l->FileTrack_PathImpl(); });
}


ILocation::TUserInfo CObj::GetUserInfoImpl()
{
    return MetaImpl([&](ILocation* l) { return l->GetUserInfoImpl(); });
}


void CObj::RemoveOldCopyIfExists()
{
    CObj* old_obj;
    CNetStorageObject guard(Clone(fNST_Movable, &old_obj));

    for (ILocation* l = old_obj->m_Selector->First(); l; l = old_obj->m_Selector->Next()) {
        if (l->IsSame(m_Location)) continue;

        try {
            if (l->RemoveImpl() == eNSTRR_Removed) return;
        }
        catch (CNetStorageException& e) {
            if (e.GetErrCode() != CNetStorageException::eNotExists) throw;
        }
    }
}


CSelector::CSelector(SNetStorageObjectImpl& fsm, const TObjLoc& loc, SContext* context, bool* cancel_relocate,
        TNetStorageFlags flags, ENetStorageObjectLocation location)
    : m_ObjectLoc(loc),
        m_Context(context),
        m_NotFound(m_ObjectLoc, fsm),
        m_NetCache(m_ObjectLoc, fsm, m_Context, cancel_relocate),
        m_FileTrack(m_ObjectLoc, fsm, m_Context, cancel_relocate)
{
    InitLocations(location, flags);
}

void CSelector::InitLocations(ENetStorageObjectLocation location,
        TNetStorageFlags flags)
{
    // The order does matter:
    // First, primary locations
    // Then, secondary locations
    // After, all other locations that have not yet been used
    // And finally, the 'not found' location

    const bool primary_nc = location == eNFL_NetCache;
    const bool primary_ft = location == eNFL_FileTrack;
    const bool secondary_nc = flags & (fNST_NetCache | fNST_Fast);
    const bool secondary_ft = flags & (fNST_FileTrack | fNST_Persistent);
    const bool movable = flags & fNST_Movable;

    m_Locations.push_back(&m_NotFound);

    if (!primary_nc && !secondary_nc && movable) {
        if (m_NetCache.Init()) {
            m_Locations.push_back(&m_NetCache);
        }
    }

    if (!primary_ft && !secondary_ft && movable) {
        if (m_FileTrack.Init()) {
            m_Locations.push_back(&m_FileTrack);
        }
    }

    if (!primary_nc && secondary_nc) {
        if (m_NetCache.Init()) {
            m_Locations.push_back(&m_NetCache);
        }
    }

    if (!primary_ft && secondary_ft) {
        if (m_FileTrack.Init()) {
            m_Locations.push_back(&m_FileTrack);
        }
    }

    if (primary_nc) {
        if (m_NetCache.Init()) {
            m_Locations.push_back(&m_NetCache);
        }
    }
    
    if (primary_ft) {
        if (m_FileTrack.Init()) {
            m_Locations.push_back(&m_FileTrack);
        }
    }

    // No real locations, only CNotFound
    if (m_Locations.size() == 1) {
        const bool ft = m_Context->filetrack_api;
        const bool nc = m_Context->icache_client;

        ostringstream os;

        os << "No backends to use for locator=\"" << m_ObjectLoc.GetLocator() << "\"";

        if (secondary_ft && secondary_nc) {
            os << ", requested FileTrack+NetCache";
        } else if (secondary_ft) {
            os << ", requested FileTrack";
        } else if (secondary_nc) {
            os << ", requested NetCache";
        } else if (flags && !movable) {
            os << ", zero requested backends";
        }

        os << " and ";

        if (ft && nc) {
            os << "configured FileTrack+NetCache";
        } else if (ft) {
            os << "configured FileTrack";
        } else if (nc) {
            os << "configured NetCache";
        } else {
            os << "no configured backends";
        }

        NCBI_THROW(CNetStorageException, eInvalidArg, os.str());
    }

    Restart();
}


ILocation* CSelector::First()
{
    return Top();
}


ILocation* CSelector::Next()
{
    if (m_CurrentLocation) {
        --m_CurrentLocation;
        return Top();
    }
   
    return NULL;
}


CLocation* CSelector::Top()
{
    _ASSERT(m_Locations.size() > m_CurrentLocation);
    CLocation* location = m_Locations[m_CurrentLocation];
    _ASSERT(location);
    return location;
}


bool CSelector::InProgress() const
{
    return m_CurrentLocation != m_Locations.size() - 1;
}


void CSelector::Restart()
{
    m_CurrentLocation = m_Locations.size() - 1;
}


TObjLoc& CSelector::Locator()
{
    return m_ObjectLoc;
}


void CSelector::SetLocator()
{
    if (CLocation* l = Top()) {
        l->SetLocator();
    } else {
        NCBI_THROW_FMT(CNetStorageException, eNotExists,
                "Cannot open \"" << m_ObjectLoc.GetLocator() << "\" for writing.");
    }
}


SNetStorageObjectImpl* CObj::Clone(TNetStorageFlags flags, CObj** copy)
{
    _ASSERT(copy);
    auto l = [&](CObj& state) { *copy = &state; };
    return SNetStorageObjectImpl::CreateAndStart<CObj>(l, this, flags);
}


SContext& CSelector::GetContext()
{
    return *m_Context;
}


}

END_NCBI_SCOPE
