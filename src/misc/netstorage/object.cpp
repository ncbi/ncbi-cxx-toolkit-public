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


BEGIN_NCBI_SCOPE


namespace NDirectNetStorageImpl
{

#define RELOCATION_BUFFER_SIZE (128 * 1024)

CObj::~CObj()
{
    try {
        m_State->CloseImpl();
    }
    NCBI_CATCH_ALL("Error while implicitly closing a NetStorage file.");
}


ERW_Result CObj::Read(void* buf, size_t count, size_t* bytes_read)
{
    return m_State->ReadImpl(buf, count, bytes_read);
}


bool CObj::Eof()
{
    return m_State->EofImpl();
}


ERW_Result CObj::Write(const void* buf, size_t count, size_t* bytes_written)
{
    return m_State->WriteImpl(buf, count, bytes_written);
}


template <class TR, TR (ILocation::*TMethod)()>
struct TCaller
{
    typedef TR TReturn;
    TR operator()(ILocation* l) const { return (l->*TMethod)(); }
};


struct TSetExpirationCaller
{
    typedef void TReturn;
    const CTimeout& ttl;
    TSetExpirationCaller(const CTimeout& t) : ttl(t) {}
    void operator()(ILocation* l) const { return l->SetExpirationImpl(ttl); }
};


template <class TCaller>
inline typename TCaller::TReturn CObj::Meta(const TCaller& caller)
{
    const bool restartable = m_Location != static_cast<ILocation*>(this);

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
    return Meta(TCaller<Uint8, &ILocation::GetSizeImpl>());
}


list<string> CObj::GetAttributeList() const
{
    NCBI_THROW(CNetStorageException, eNotSupported,
        "Attribute support is only implemented in NetStorage server.");
}


string CObj::GetAttribute(const string& attr_name) const
{
    NCBI_THROW(CNetStorageException, eNotSupported,
        "Attribute support is only implemented in NetStorage server.");
}


void CObj::SetAttribute(const string& attr_name, const string& attr_value)
{
    NCBI_THROW(CNetStorageException, eNotSupported,
        "Attribute support is only implemented in NetStorage server.");
}


CNetStorageObjectInfo CObj::GetInfo()
{
    return Meta(TCaller<CNetStorageObjectInfo, &ILocation::GetInfoImpl>());
}


void CObj::SetExpiration(const CTimeout& ttl)
{
    return Meta(TSetExpirationCaller(ttl));
}


string CObj::FileTrack_Path()
{
    return Meta(TCaller<string, &ILocation::FileTrack_PathImpl>());
}


ILocation::TUserInfo CObj::GetUserInfo()
{
    return Meta(TCaller<TUserInfo, &ILocation::GetUserInfoImpl>());
}


const TObjLoc& CObj::Locator() const
{
    return m_Selector->Locator();
}


string CObj::Relocate(TNetStorageFlags flags)
{
    // Use Read() to detect the current location
    char buffer[RELOCATION_BUFFER_SIZE];
    size_t bytes_read;
    Read(buffer, sizeof(buffer), &bytes_read);

    // Selector can only be cloned after location is detected
    ISelector::Ptr selector(m_Selector->Clone(flags));

    if (m_Location->IsSame(selector->First())) {
        LOG_POST(Trace << "locations are the same");
        return selector->Locator().GetLocator();
    }

    LOG_POST(Trace << "locations are different");
    CRef<CObj> new_file(new CObj(selector));

    for (;;) {
        new_file->Write(buffer, bytes_read, NULL);
        if (Eof()) break;
        Read(buffer, sizeof(buffer), &bytes_read);
    }

    new_file->Close();
    Close();
    Remove();
    return new_file->GetLoc();
}


bool CObj::Exists()
{
    return Meta(TCaller<bool, &ILocation::ExistsImpl>());
}


ENetStorageRemoveResult CObj::Remove()
{
    return Meta(TCaller<ENetStorageRemoveResult, &ILocation::RemoveImpl>());
}


void CObj::Close()
{
    IState* rw_state = m_State;
    m_State = this;
    rw_state->CloseImpl();
}


void CObj::Abort()
{
    IState* rw_state = m_State;
    m_State = this;
    rw_state->AbortImpl();
}


string CObj::GetLoc()
{
    return m_Selector->Locator().GetLocator();
}


ERW_Result CObj::ReadImpl(void* buf, size_t count, size_t* bytes_read)
{
    const bool restartable = m_Selector->InProgress();
    ERW_Result result = eRW_Error;

    try {
        StartRead(buf, count, bytes_read, &result);
        return result;
    }
    catch (CNetStorageException& e) {
        if (e.GetErrCode() != CNetStorageException::eNotExists) throw;
        if (!restartable) throw;
    }

    // Restarting location search,
    // as object location might be changed while we were using it.
    m_Selector->Restart();
    StartRead(buf, count, bytes_read, &result);
    return result;
}


bool CObj::EofImpl()
{
    return false;
}


ERW_Result CObj::WriteImpl(const void* buf, size_t count, size_t* bytes_written)
{
    // If object already exists, it will be overwritten in the same backend
    if (!Exists()) {
        m_Selector->Restart();
    }

    ERW_Result result = eRW_Error;
    if (ILocation* l = m_Selector->First()) {
        IState* rw_state = l->StartWrite(buf, count, bytes_written, &result);
        if (rw_state) {
            m_Location = l;
            m_State = rw_state;
            if (m_RemoveOldCopy) RemoveOldCopyIfExists();
            return result;
        }
    }

    // Not reached (CNotFound location always throws exception)
    return result;
}


IState* CObj::StartRead(void* buf, size_t count, size_t* bytes_read,
        ERW_Result* result)
{
    for (ILocation* l = m_Selector->First(); l; l = m_Selector->Next()) {
        IState* rw_state = l->StartRead(buf, count, bytes_read, result);
        if (rw_state) {
            m_Location = l;
            m_State = rw_state;
            break;
        }
    }

    // Not used
    return nullptr;
}


IState* CObj::StartWrite(const void*, size_t, size_t*, ERW_Result*)
{
    // This just cannot happen
    _TROUBLE;
    return NULL;
}


template <class TCaller>
inline typename TCaller::TReturn CObj::MetaImpl(const TCaller& caller)
{
    if (ILocation* l = m_Selector->First()) {
        for (;;) {
            m_Location = l;

            try {
                return caller(m_Location);
            }
            catch (CNetStorageException& e) {
                if (e.GetErrCode() != CNetStorageException::eNotExists) throw;

                l = m_Selector->Next();

                if (!l) throw;

                LOG_POST(Trace << "Exception: " << e);
            }
        }
    } else {
        return caller(m_Location);
    }
}                                                                    


Uint8 CObj::GetSizeImpl() {
    return MetaImpl(TCaller<Uint8, &ILocation::GetSizeImpl>());
}


CNetStorageObjectInfo CObj::GetInfoImpl()
{
    return MetaImpl(TCaller<CNetStorageObjectInfo, &ILocation::GetInfoImpl>());
}


bool CObj::ExistsImpl()
{
    return MetaImpl(TCaller<bool, &ILocation::ExistsImpl>());
}


ENetStorageRemoveResult CObj::RemoveImpl()
{
    return MetaImpl(TCaller<ENetStorageRemoveResult, &ILocation::RemoveImpl>());
}


void CObj::SetExpirationImpl(const CTimeout& ttl)
{
    return MetaImpl(TSetExpirationCaller(ttl));
}


string CObj::FileTrack_PathImpl()
{
    return MetaImpl(TCaller<string, &ILocation::FileTrack_PathImpl>());
}


ILocation::TUserInfo CObj::GetUserInfoImpl()
{
    return MetaImpl(TCaller<TUserInfo, &ILocation::GetUserInfoImpl>());
}


void CObj::RemoveOldCopyIfExists() const
{
    ISelector::Ptr selector(m_Selector->Clone(fNST_Movable));

    for (ILocation* l = selector->First(); l; l = selector->Next()) {
        if (l->IsSame(m_Location)) continue;

        try {
            if (l->RemoveImpl() == eNSTRR_Removed) return;
        }
        catch (CNetStorageException& e) {
            if (e.GetErrCode() != CNetStorageException::eNotExists) throw;
        }
    }
}


}

END_NCBI_SCOPE
