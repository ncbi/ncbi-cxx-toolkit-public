#ifndef CONNECT_SERVICES__COMPOUND_ID_IMPL__HPP
#define CONNECT_SERVICES__COMPOUND_ID_IMPL__HPP

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
 *   Compound IDs are Base64-encoded string that contain application-specific
 *   information to identify and/or locate objects.
 *
 */

/// @file compound_id_impl.hpp
/// Internal declarations of the CompoundID classes.

#include <connect/services/compound_id.hpp>

#include <util/random_gen.hpp>

#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE

struct SFieldListLink
{
    SCompoundIDFieldImpl* m_Prev;
    SCompoundIDFieldImpl* m_Next;
};

struct SNeighborListLink : SFieldListLink
{
};

struct SSameTypeListLink : SFieldListLink
{
};

struct SCompoundIDFieldImpl : public CObject,
    public SNeighborListLink, public SSameTypeListLink
{
    virtual void DeleteThis();

    CCompoundID m_CID;

    ECompoundIDFieldType m_Type;

    union {
        Uint8 m_Uint8Value;
        Int8 m_Int8Value;
        Uint4 m_Uint4Value;
        struct {
            Uint4 m_IPv4Addr;
            Uint2 m_Port;
        } m_IPv4SockAddr;
        bool m_BoolValue;

        SCompoundIDFieldImpl* m_NextObjectInPool;
    };
    string m_StringValue;
    CCompoundID m_NestedCID;
};

template <typename Link>
struct SFieldList
{
    void Clear()
    {
        m_Tail = m_Head = NULL;
    }

    bool IsEmpty() const {return m_Head == NULL;}

    static Link* GetLink(SCompoundIDFieldImpl* entry)
    {
        return static_cast<Link*>(entry);
    }

    static SCompoundIDFieldImpl* GetPrev(SCompoundIDFieldImpl* entry)
    {
        return GetLink(entry)->m_Prev;
    }

    static SCompoundIDFieldImpl* GetNext(SCompoundIDFieldImpl* entry)
    {
        return GetLink(entry)->m_Next;
    }

    static void SetPrev(SCompoundIDFieldImpl* entry, SCompoundIDFieldImpl* prev)
    {
        GetLink(entry)->m_Prev = prev;
    }

    static void SetNext(SCompoundIDFieldImpl* entry, SCompoundIDFieldImpl* next)
    {
        GetLink(entry)->m_Next = next;
    }

    void Append(SCompoundIDFieldImpl* new_entry)
    {
        SetNext(new_entry, NULL);
        SetPrev(new_entry, m_Tail);
        if (m_Tail != NULL)
            SetNext(m_Tail, new_entry);
        else
            m_Head = new_entry;
        m_Tail = new_entry;
    }

    void Remove(SCompoundIDFieldImpl* entry)
    {
        if (GetPrev(entry) == NULL)
            if ((m_Head = GetNext(entry)) == NULL)
                m_Tail = NULL;
            else
                SetPrev(GetNext(entry), NULL);
        else if (GetNext(entry) == NULL)
            SetNext(m_Tail = GetPrev(entry), NULL);
        else {
            SetNext(GetPrev(entry), GetNext(entry));
            SetPrev(GetNext(entry), GetPrev(entry));
        }
    }

    SCompoundIDFieldImpl* m_Head;
    SCompoundIDFieldImpl* m_Tail;
};

typedef SFieldList<SNeighborListLink> TFieldList;
typedef SFieldList<SSameTypeListLink> THomogeneousFieldList;

struct SCompoundIDImpl : public CObject
{
    void Reset(SCompoundIDPoolImpl* pool, ECompoundIDClass id_class)
    {
        m_Class = id_class;
        m_Pool = pool;
        m_Length = 0;
        m_Dirty = true;
        m_FieldList.Clear();
        for (unsigned i = 0; i < eCIT_NumberOfTypes; ++i)
            m_HomogeneousFields[i].Clear();
    }

    void Remove(SCompoundIDFieldImpl* field);

    string PackV0();

    SCompoundIDFieldImpl* AppendField(ECompoundIDFieldType field_type);

    virtual void DeleteThis();

    ECompoundIDClass m_Class;

    TFieldList m_FieldList;
    THomogeneousFieldList m_HomogeneousFields[eCIT_NumberOfTypes];

    unsigned m_Length;

    CCompoundIDPool m_Pool;
    SCompoundIDImpl* m_NextObjectInPool;

    string m_PackedID;
    bool m_Dirty;
};

template <typename Poolable, typename FieldTypeOrIDClass>
struct SCompoundIDObjectPool
{
    SCompoundIDObjectPool() : m_Head(NULL)
    {
    }

    CFastMutex m_Mutex;
    Poolable* m_Head;

    Poolable* Alloc()
    {
        CFastMutexGuard guard(m_Mutex);

        if (m_Head == NULL)
            return new Poolable;
        else {
            Poolable* element = m_Head;
            m_Head = element->m_NextObjectInPool;
            return element;
        }
    }

    void ReturnToPool(Poolable* element)
    {
        CFastMutexGuard guard(m_Mutex);

        element->m_NextObjectInPool = m_Head;
        m_Head = element;
    }

    ~SCompoundIDObjectPool()
    {
        Poolable* element = m_Head;
        while (element != NULL) {
            Poolable* next_element = element->m_NextObjectInPool;
            delete element;
            element = next_element;
        }
    }

};

struct SCompoundIDPoolImpl : public CObject
{
    Uint4 GetRand()
    {
        CFastMutexGuard guard(m_RandomGenMutex);
        return m_RandomGen.GetRand();
    }

    CCompoundID UnpackV0(const string& packed_id);

    SCompoundIDObjectPool<SCompoundIDFieldImpl,
            ECompoundIDFieldType> m_FieldPool;
    SCompoundIDObjectPool<SCompoundIDImpl, ECompoundIDClass> m_CompoundIDPool;
    CFastMutex m_RandomGenMutex;
    CRandom m_RandomGen;
};

inline void SCompoundIDImpl::Remove(SCompoundIDFieldImpl* field)
{
    m_FieldList.Remove(field);
    m_HomogeneousFields[field->m_Type].Remove(field);
    m_Pool->m_FieldPool.ReturnToPool(field);
    --m_Length;
    m_Dirty = true;
}

extern void g_PackID(void* binary_id, size_t binary_id_len, string& packed_id);

extern bool g_UnpackID(const string& packed_id, string& binary_id);

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES__COMPOUND_ID_IMPL__HPP */
