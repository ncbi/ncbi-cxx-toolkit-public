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

struct SAnnotObject_Key
{
    SAnnotObject_Key(void)
        : m_AnnotObject_Info(0)
        {
        }

    CAnnotObject_Info*      m_AnnotObject_Info;
    CSeq_id_Handle          m_Handle;
    CRange<TSeqPos>         m_Range;
};

struct SAnnotObject_Index
{
    SAnnotObject_Index(void)
        : m_AnnotObject_Info(0),
          m_AnnotLocationIndex(0),
          m_StrandIndex(0x3) // both strands are in this index
        {
        }

    CAnnotObject_Info*                  m_AnnotObject_Info;
    CRef< CObjectFor<CHandleRange> >    m_HandleRange;
    unsigned short                      m_AnnotLocationIndex;
    // bit 0 = plus strand; bit 1 = minus strand
    unsigned short                      m_StrandIndex;
};


struct NCBI_XOBJMGR_EXPORT SAnnotObjects_Info
{
    SAnnotObjects_Info(void);
    SAnnotObjects_Info(const CAnnotName& name);
    SAnnotObjects_Info(const SAnnotObjects_Info&);
    ~SAnnotObjects_Info(void);

    typedef vector<SAnnotObject_Key>           TObjectKeys;
    typedef vector<CAnnotObject_Info>          TObjectInfos;

    void SetName(const CAnnotName& name);
    const CAnnotName& GetName(void) const;

    // reserve space for size annot objects
    // keys will be reserved for size*keys_factor objects
    // this is done to avoid reallocation and invalidation
    // of m_Infos in AddInfo() method
    void Reserve(size_t size, double keys_factor = 1.0);
    void Clear(void);

    void AddKey(const SAnnotObject_Key& key);
    CAnnotObject_Info* AddInfo(const CAnnotObject_Info& info);

    const TObjectKeys& GetKeys(void) const;
    const TObjectInfos& GetInfos(void) const;
    const CAnnotObject_Info& GetInfo(size_t index) const;
    size_t GetIndex(const CAnnotObject_Info& info) const;

private:    
    CAnnotName      m_Name;
    TObjectKeys     m_Keys;
    TObjectInfos    m_Infos;

    SAnnotObjects_Info& operator=(const SAnnotObjects_Info&);
};


inline
const CAnnotName& SAnnotObjects_Info::GetName(void) const
{
    return m_Name;
}


inline
const SAnnotObjects_Info::TObjectKeys& SAnnotObjects_Info::GetKeys(void) const
{
    return m_Keys;
}


inline
const SAnnotObjects_Info::TObjectInfos& SAnnotObjects_Info::GetInfos(void) const
{
    return m_Infos;
}


inline
const CAnnotObject_Info& SAnnotObjects_Info::GetInfo(size_t index) const
{
    _ASSERT(index < m_Infos.size());
    return m_Infos[index];
}


inline
size_t SAnnotObjects_Info::GetIndex(const CAnnotObject_Info& info) const
{
    _ASSERT(!m_Infos.empty());
    _ASSERT(&info >= &m_Infos.front() && &info <= &m_Infos.back());
    return &info - &m_Infos.front();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2004/08/16 18:00:40  grichenk
* Added detection of circular locations, improved annotation
* indexing by strand.
*
* Revision 1.7  2004/08/05 18:24:52  vasilche
* CAnnotName and CAnnotTypeSelector are moved in separate headers.
*
* Revision 1.6  2004/07/12 15:05:31  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.5  2004/05/26 14:29:20  grichenk
* Redesigned CSeq_align_Mapper: preserve non-mapping intervals,
* fixed strands handling, improved performance.
*
* Revision 1.4  2004/01/23 16:14:46  grichenk
* Implemented alignment mapping
*
* Revision 1.3  2003/11/26 17:55:55  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.2  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.1  2003/09/30 16:22:00  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* ===========================================================================
*/

#endif// OBJECTS_OBJMGR_IMPL___ANNOT_OBJECT_INDEX__HPP
