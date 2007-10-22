#ifndef OBJECTS_OBJMGR_IMPL___ANNOT_OBJECT_INDEX__HPP
#define OBJECTS_OBJMGR_IMPL___ANNOT_OBJECT_INDEX__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   Annot objecty index structures
*
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objmgr/annot_name.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/impl/handle_range.hpp>

#include <util/rangemap.hpp>

#include <vector>
#include <deque>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CHandleRange;
class CAnnotObject_Info;

////////////////////////////////////////////////////////////////////
//
//  CTSE_Info::
//
//    General information and indexes for top level seq-entries
//


// forward declaration

struct SAnnotObject_Index
{
    SAnnotObject_Index(void)
        : m_AnnotObject_Info(0),
          m_AnnotLocationIndex(0),
          m_Flags(fStrand_both)
        {
        }

    enum EFlags {
        fStrand_mask        = 3,
        fStrand_none        = 0,
        fStrand_plus        = 1,
        fStrand_minus       = 2,
        fStrand_both        = 3,
        fMultiId            = 1 << 2,
        fPartial            = 1 << 3,
        fSimpleLocation_Mask= 3 << 4,
        fLocation_Point     = 1 << 4,
        fLocation_Interval  = 2 << 4,
        fLocation_Whole     = 3 << 4
    };
    typedef Uint1 TFlags;
    
    bool GetMultiIdFlag(void) const
        {
            return (m_Flags & fMultiId) != 0;
        }
    void SetMultiIdFlag(void)
        {
            m_Flags |= fMultiId;
        }
    bool IsPartial(void) const
        {
            return (m_Flags & fPartial) != 0;
        }
    void SetPartial(bool partial)
        {
            if ( partial ) {
                m_Flags |= fPartial;
            }
        }
    bool LocationIsSimple(void) const
        {
            return (m_Flags & fSimpleLocation_Mask) != 0;
        }
    bool LocationIsPoint(void) const
        {
            return (m_Flags & fSimpleLocation_Mask) == fLocation_Point;
        }
    bool LocationIsInterval(void) const
        {
            return (m_Flags & fSimpleLocation_Mask) == fLocation_Interval;
        }
    bool LocationIsWhole(void) const
        {
            return (m_Flags & fSimpleLocation_Mask) == fLocation_Whole;
        }
    void SetLocationIsPoint(void)
        {
            m_Flags = (m_Flags & ~fSimpleLocation_Mask) | fLocation_Point;
        }
    void SetLocationIsInterval(void)
        {
            m_Flags = (m_Flags & ~fSimpleLocation_Mask) | fLocation_Interval;
        }
    void SetLocationIsWhole(void)
        {
            m_Flags = (m_Flags & ~fSimpleLocation_Mask) | fLocation_Whole;
        }

    CAnnotObject_Info*                  m_AnnotObject_Info;
    CRef< CObjectFor<CHandleRange> >    m_HandleRange;
    Uint2                               m_AnnotLocationIndex;
    Uint1                               m_Flags;
};


struct NCBI_XOBJMGR_EXPORT SAnnotObjectsIndex
{
    SAnnotObjectsIndex(void);
    SAnnotObjectsIndex(const CAnnotName& name);
    SAnnotObjectsIndex(const SAnnotObjectsIndex&);
    ~SAnnotObjectsIndex(void);

    typedef deque<CAnnotObject_Info>           TObjectInfos;
    typedef vector<SAnnotObject_Key>           TObjectKeys;

    void SetName(const CAnnotName& name);
    const CAnnotName& GetName(void) const;

    bool IsIndexed(void) const;
    void SetIndexed(void);

    bool IsEmpty(void) const;
    // reserve space for size annot objects
    // keys will be reserved for size*keys_factor objects
    // this is done to avoid reallocation and invalidation
    // of m_Infos in AddInfo() method
    void Clear(void);

    void ReserveInfoSize(size_t size);
    void AddInfo(const CAnnotObject_Info& info);

    TObjectInfos& GetInfos(void);
    const TObjectInfos& GetInfos(void) const;

    void ReserveMapSize(size_t size);
    void AddMap(const SAnnotObject_Key& key, const SAnnotObject_Index& index);
    void RemoveLastMap(void);

    void PackKeys(void);

    const TObjectKeys& GetKeys(void) const;
    const SAnnotObject_Key& GetKey(size_t i) const;

private:    
    CAnnotName      m_Name;
    TObjectInfos    m_Infos;
    bool            m_Indexed;
    TObjectKeys     m_Keys;

    SAnnotObjectsIndex& operator=(const SAnnotObjectsIndex&);
};


inline
const CAnnotName& SAnnotObjectsIndex::GetName(void) const
{
    return m_Name;
}


inline
bool SAnnotObjectsIndex::IsIndexed(void) const
{
    return m_Indexed;
}


inline
void SAnnotObjectsIndex::SetIndexed(void)
{
    _ASSERT(!IsIndexed());
    m_Indexed = true;
}


inline
bool SAnnotObjectsIndex::IsEmpty(void) const
{
    return m_Infos.empty();
}


inline
const SAnnotObjectsIndex::TObjectInfos&
SAnnotObjectsIndex::GetInfos(void) const
{
    return m_Infos;
}


inline
SAnnotObjectsIndex::TObjectInfos&
SAnnotObjectsIndex::GetInfos(void)
{
    return m_Infos;
}


inline
const SAnnotObjectsIndex::TObjectKeys&
SAnnotObjectsIndex::GetKeys(void) const
{
    return m_Keys;
}


inline
const SAnnotObject_Key&
SAnnotObjectsIndex::GetKey(size_t i) const
{
    _ASSERT(i < m_Keys.size());
    return m_Keys[i];
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif// OBJECTS_OBJMGR_IMPL___ANNOT_OBJECT_INDEX__HPP
