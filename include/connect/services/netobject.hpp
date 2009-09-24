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
#define NET_COMPONENT_ALLOW_DEFAULT_CONSTRUCTOR(component_name) \
    protected: \
    CNetObjectRef<S##component_name##Impl> m_Impl; \
    public: \
    typedef S##component_name##Impl* TInstance; \
    C##component_name(TInstance impl) : m_Impl(impl) {} \
    C##component_name& operator =(TInstance impl) \
        {m_Impl = impl; return *this;} \
    operator TInstance() const {return m_Impl.GetPtr();} \
    TInstance operator ->() const {return m_Impl.GetPtr();}

#define NET_COMPONENT(component_name) \
    protected: \
    CNetObjectRef<S##component_name##Impl> m_Impl; \
    public: \
    typedef S##component_name##Impl* TInstance; \
    C##component_name(TInstance impl) : m_Impl(impl) {} \
    C##component_name& operator =(TInstance impl) \
        {m_Impl = impl; return *this;} \
    operator TInstance() const {return m_Impl.GetPtr();} \
    TInstance operator ->() const {return m_Impl.GetPtr();} \
    C##component_name() {}

class NCBI_XCONNECT_EXPORT CNetObject
{
public:
    CNetObject() { }

    void AddRef() {m_Refs.Add(1);}
    void Release() {if (m_Refs.Add(-1) == 0) Delete();}

    CAtomicCounter::TValue GetRefCount() const {return m_Refs.Get();}

protected:
    virtual ~CNetObject();

    virtual void Delete();

protected:
    CAtomicCounter_WithAutoInit m_Refs;
};

class CNetObjectRef_Base
{
public:
    CNetObjectRef_Base() : m_Impl(NULL) {}

    CNetObjectRef_Base(CNetObject* impl)
    {
        if ((m_Impl = impl) != NULL)
            m_Impl->AddRef();
    }

    CNetObjectRef_Base(const CNetObjectRef_Base& src)
    {
        if ((m_Impl = src.m_Impl) != NULL)
            m_Impl->AddRef();
    }

    CNetObjectRef_Base& operator =(const CNetObjectRef_Base& src) {
        Assign(src.m_Impl);

        return *this;
    }

    void Assign(CNetObject* impl) {
        if (impl != NULL)
            impl->AddRef();

        CNetObject* old_ptr = m_Impl;

        m_Impl = impl;

        if (old_ptr != NULL)
            old_ptr->Release();
    }

    ~CNetObjectRef_Base() {
        if (m_Impl != NULL)
            m_Impl->Release();
    }

protected:
    CNetObject* m_Impl;
};

template <class TYPE>
class CNetObjectRef : public CNetObjectRef_Base
{
public:
    CNetObjectRef() : CNetObjectRef_Base() {}

    CNetObjectRef(TYPE* impl) :
        CNetObjectRef_Base(reinterpret_cast<CNetObject*>(impl)) {}

    CNetObjectRef<TYPE>& operator =(TYPE* impl) {
        Assign(reinterpret_cast<CNetObject*>(impl));

        return *this;
    }

    TYPE* GetPtr() const {return reinterpret_cast<TYPE*>(m_Impl);}

    operator TYPE*() const {return reinterpret_cast<TYPE*>(m_Impl);}

    TYPE* operator ->() const {return reinterpret_cast<TYPE*>(m_Impl);}
};


END_NCBI_SCOPE

#endif // CONNECT_SERVICES__SERVER_CONN_HPP
