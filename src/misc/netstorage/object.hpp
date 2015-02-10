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

#include <corelib/ncbiobj.hpp>
#include <connect/services/netcomponent.hpp>
#include <connect/services/neticache_client.hpp>
#include <connect/services/netstorage.hpp>

#include "filetrack.hpp"
#include "state.hpp"

BEGIN_NCBI_SCOPE

namespace NImpl
{

class CObj : public SNetStorageObjectImpl, private IState, private ILocation
{
public:
    CObj(ISelector::Ptr selector)
        : m_Selector(selector),
          m_State(this),
          m_Location(this)
    {
        _ASSERT(m_Selector.get());
    }

    virtual ~CObj();

    ERW_Result Read(void*, size_t, size_t*);
    ERW_Result Write(const void*, size_t, size_t*);
    void Close();
    void Abort();

    string GetLoc();
    bool Eof();
    Uint8 GetSize();
    string GetAttribute(const string&);
    void SetAttribute(const string&, const string&);
    CNetStorageObjectInfo GetInfo();

    bool Exists();
    void Remove();

    string MoveTo(ISelector::Ptr);

private:
    ERW_Result ReadImpl(void*, size_t, size_t*);
    bool EofImpl();
    ERW_Result WriteImpl(const void*, size_t, size_t*);
    void CloseImpl() {}
    void AbortImpl() {}

    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    void RemoveImpl();

    ISelector::Ptr m_Selector;
    IState* m_State;
    ILocation* m_Location;
};

}

END_NCBI_SCOPE

#endif
