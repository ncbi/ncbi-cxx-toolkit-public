#ifndef CONNECT_SERVICES__NET_OBJECT_HPP
#define CONNECT_SERVICES__NET_OBJECT_HPP

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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <connect/connect_export.h>

#include <corelib/ncbicntr.hpp>

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////
//
#define NET_COMPONENT(component_name) \
    protected: \
    CNetObjectRef<S##component_name##Impl> m_Impl; \
    public: \
    C##component_name() {} \
    explicit C##component_name(S##component_name##Impl* impl) : \
        m_Impl(impl) {} \
    operator S##component_name##Impl*() const {return m_Impl.GetPtr();} \
    S##component_name##Impl* operator ->() const {return m_Impl.GetPtr();}

class NCBI_XCONNECT_EXPORT CNetObject
{
public:
    CNetObject() {m_Refs.Set(0);}

    void AddRef() {m_Refs.Add(1);}
    void Release() {if (m_Refs.Add(-1) == 0) Delete();}

protected:
    virtual ~CNetObject();

    virtual void Delete();

private:
    CAtomicCounter m_Refs;
};

template <class TYPE>
class CNetObjectRef
{
public:
    CNetObjectRef() : m_Impl(NULL) {}

    CNetObjectRef(TYPE* impl) {
        if ((m_Impl =
                reinterpret_cast<CNetObject*>(impl)) != NULL)
            m_Impl->AddRef();
    }

    CNetObjectRef(const CNetObjectRef<TYPE>& src) {
        if ((m_Impl = src.m_Impl) != NULL)
            m_Impl->AddRef();
    }

    CNetObjectRef<TYPE>& operator =(const CNetObjectRef<TYPE>& src) {
        if (src.m_Impl != NULL)
            src.m_Impl->AddRef();

        CNetObject* old_ptr = m_Impl;

        m_Impl = src.m_Impl;

        if (old_ptr != NULL)
            old_ptr->Release();

        return *this;
    }

    ~CNetObjectRef() {
        if (m_Impl != NULL)
            m_Impl->Release();
    }

    TYPE* GetPtr() const {return reinterpret_cast<TYPE*>(m_Impl);}

    operator TYPE*() const {return reinterpret_cast<TYPE*>(m_Impl);}

    TYPE* operator ->() const {return reinterpret_cast<TYPE*>(m_Impl);}

private:
    CNetObject* m_Impl;
};


END_NCBI_SCOPE

#endif // CONNECT_SERVICES__SERVER_CONN_HPP
