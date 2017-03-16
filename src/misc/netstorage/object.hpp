#ifndef MISC_NETSTORAGE___OBJECT__HPP
#define MISC_NETSTORAGE___OBJECT__HPP

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

#include <connect/services/impl/netstorage_impl.hpp>
#include "state.hpp"


BEGIN_NCBI_SCOPE


namespace NDirectNetStorageImpl
{

class CObj : public INetStorageObjectState
{
public:
    CObj(SNetStorageObjectImpl& fsm, SContext* context, const CNetStorageObjectLoc& loc, TNetStorageFlags flags, bool is_opened = false, ENetStorageObjectLocation location = eNFL_Unknown);

    ERW_Result Read(void*, size_t, size_t*) override;
    ERW_Result PendingCount(size_t* count) override { *count = 0; return eRW_Success; }
    bool Eof() override;

    ERW_Result Write(const void*, size_t, size_t*) override;
    ERW_Result Flush() override { return eRW_Success; }

    void Close() override;
    void Abort() override;

    string GetLoc() const override;
    Uint8 GetSize() override;
    list<string> GetAttributeList() const override;
    string GetAttribute(const string&) const override;
    void SetAttribute(const string&, const string&) override;
    CNetStorageObjectInfo GetInfo() override;
    void SetExpiration(const CTimeout&) override;

    string FileTrack_Path() override;
    pair<string, string> GetUserInfo() override;

    CNetStorageObjectLoc& Locator() override;
    string Relocate(TNetStorageFlags, TNetStorageProgressCb cb) override;
    void CancelRelocate() override;
    bool Exists() override;
    ENetStorageRemoveResult Remove() override;

    void SetLocator();

private:
    template <class TCaller>
    auto Meta(TCaller caller)                   -> decltype(caller(nullptr));

    INetStorageObjectState* StartRead(void*, size_t, size_t*, ERW_Result*);

    void RemoveOldCopyIfExists(ILocation* current);
    SNetStorageObjectImpl* Clone(TNetStorageFlags flags, CObj** copy);

    bool m_CancelRelocate = false;
    CNetStorageObjectLoc m_ObjectLoc;
    CRef<SContext> m_Context;
    list<unique_ptr<ILocation>> m_Locations;
    CNotFound m_NotFound;
    bool m_IsOpened;
};

}

END_NCBI_SCOPE

#endif
