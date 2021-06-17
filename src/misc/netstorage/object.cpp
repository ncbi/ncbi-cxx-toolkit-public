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
#include <typeinfo>


BEGIN_NCBI_SCOPE


namespace NDirectNetStorageImpl
{

CObj::CObj(SNetStorageObjectImpl& fsm, SContext* context, const CNetStorageObjectLoc& loc, TNetStorageFlags flags, bool is_opened, ENetStorageObjectLocation location) :
    m_ObjectLoc(loc),
    m_Context(context),
    m_NotFound(m_ObjectLoc, fsm),
    m_IsOpened(is_opened)
{
    const bool primary_nc = location == eNFL_NetCache;
    const bool primary_ft = location == eNFL_FileTrack;
    const bool secondary_nc = flags & (fNST_NetCache | fNST_Fast);
    const bool secondary_ft = flags & (fNST_FileTrack | fNST_Persistent);
    const bool movable = flags & fNST_Movable;

    if (primary_ft || secondary_ft || movable) {
        unique_ptr<CFileTrack> ft(new CFileTrack(m_ObjectLoc, fsm, m_Context, &m_CancelRelocate));
        if (ft->Init()) m_Locations.emplace_back(ft.release());
    }

    if (primary_nc || secondary_nc || movable) {
        unique_ptr<CNetCache> nc(new CNetCache(m_ObjectLoc, fsm, m_Context, &m_CancelRelocate));
        if (nc->Init()) m_Locations.emplace_back(nc.release());
    }

    if (primary_nc || (!primary_ft && !secondary_ft && secondary_nc)) {
        m_Locations.reverse();
    }

    if (m_Locations.size()) return;

    // No locations
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


template <class TCaller>
auto CObj::Meta(TCaller caller) -> decltype(caller(nullptr))
{
    auto i = m_Locations.begin();
    auto last = prev(m_Locations.end());

    for (;;) {
        try {
            return caller(i->get());
        }
        catch (CNetStorageException& e) {
            if (e.GetErrCode() != CNetStorageException::eNotExists) throw;
        }
        catch (int) {
            // Just a signal to try next location
        }

        if (i++ == last) break;

        // Move failed location to the end
        m_Locations.splice(m_Locations.end(), m_Locations, m_Locations.begin());
    }

    return caller(&m_NotFound);
}


Uint8 CObj::GetSize()
{
    return Meta([](ILocation* l) { return l->GetSize(); });
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
    return Meta([](ILocation* l) { return l->GetInfo(); });
}


void CObj::SetExpiration(const CTimeout& ttl)
{
    return Meta([&](ILocation* l) { return l->SetExpiration(ttl); });
}


string CObj::FileTrack_Path()
{
    return Meta([&](ILocation* l) { return l->FileTrack_Path(); });
}


pair<string, string> CObj::GetUserInfo()
{
    return Meta([&](ILocation* l) { return l->GetUserInfo(); });
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
    const size_t max = m_Context->relocate_chunk;
    size_t current = 0;
    size_t total = 0;
    size_t bytes_read;
    array<char, 128 * 1024> buffer;

    // Use Read() to detect the current location
    ERW_Result result = eRW_Error;
    auto rw_state = StartRead(buffer.data(), buffer.size(), &bytes_read, &result);
    s_Check(result, "reading");

    _ASSERT(rw_state); // Cannot be a nullptr if result check passed

    // Selector can only be cloned after location is detected
    CObj* new_obj;
    CNetStorageObject new_file(Clone(flags, &new_obj));

    auto& src = *m_Locations.front();
    auto& dst = *new_obj->m_Locations.front();

    if (typeid(src) == typeid(dst)) {
        rw_state->Close();
        return new_obj->m_ObjectLoc.GetLocator();
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
    return Meta([&](ILocation* l) { return l->Exists(); });
}


ENetStorageRemoveResult CObj::Remove()
{
    return Meta([&](ILocation* l) { return l->Remove(); });
}


void CObj::Close()
{
}


void CObj::Abort()
{
}


string CObj::GetLoc() const
{
    return m_ObjectLoc.GetLocator();
}


ERW_Result CObj::Read(void* buf, size_t count, size_t* bytes_read)
{
    ERW_Result result = eRW_Error;
    StartRead(buf, count, bytes_read, &result);
    return result;
}


bool CObj::Eof()
{
    return false;
}


ERW_Result CObj::Write(const void* buf, size_t count, size_t* bytes_written)
{
    // If object already exists, it will be overwritten in the same backend
    if (m_IsOpened) Exists();

    ERW_Result result = eRW_Error;
    EnterState(m_Locations.front()->StartWrite(buf, count, bytes_written, &result));

    if (m_IsOpened) {
        CObj* old_obj;
        CNetStorageObject guard(Clone(fNST_Movable, &old_obj));
        old_obj->RemoveOldCopyIfExists(m_Locations.front().get());
    }

    m_IsOpened = true;
    return result;
}


INetStorageObjectState* CObj::StartRead(void* buf, size_t count, size_t* bytes_read, ERW_Result* result)
{
    INetStorageObjectState* rw_state = nullptr;

    auto f = [&](ILocation* l)
    {
        rw_state = l->StartRead(buf, count, bytes_read, result);

        if (!rw_state) throw 0;

        EnterState(rw_state);
        return rw_state;
    };

    return Meta(f);
}


void CObj::RemoveOldCopyIfExists(ILocation* current)
{
    auto f = [&current](ILocation* l)
    {
        // Do not remove object from current location
        if (typeid(*l) == typeid(*current)) throw 0;

        return l->Remove();
    };

    Meta(f);
}


CNetStorageObjectLoc& CObj::Locator()
{
    return m_ObjectLoc;
}


void CObj::SetLocator()
{
    m_Locations.front()->SetLocator();
}


SNetStorageObjectImpl* CObj::Clone(TNetStorageFlags flags, CObj** copy)
{
    _ASSERT(copy);

    if (!flags) flags = m_Context->default_flags;

    CNetStorageObjectLoc loc = Locator();
    loc.SetStorageAttrFlags(flags);

    auto l = [&](CObj& state) { *copy = &state; };
    return SNetStorageObjectImpl::CreateAndStart<CObj>(l, m_Context, loc, flags);
}


}

END_NCBI_SCOPE
