#ifndef OBJECTS_OBJMGR___SEQ_MAP_EXT__HPP
#define OBJECTS_OBJMGR___SEQ_MAP_EXT__HPP

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
* Authors:
*           Eugene Vasilchenko
*
* File Description:
*   CSeqMap -- formal sequence map to describe sequence parts in general,
*   i.e. location and type only, without providing real data
*
*/
#include <objmgr/seq_map.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeg_ext;

class NCBI_XOBJMGR_EXPORT CSeqMap_Delta_seqs : public CSeqMap
{
public:
    typedef CDelta_ext            TObject;
    typedef TObject::Tdata        TList;
    typedef TList::const_iterator TList_I;

    CSeqMap_Delta_seqs(const TObject& obj);
    CSeqMap_Delta_seqs(const TObject& obj, CSeqMap_Delta_seqs* parent, size_t index);

    ~CSeqMap_Delta_seqs(void);

protected:
    void x_Index(const TList& seq);
    void x_IndexAll(const TList& seq);
    void x_IndexUnloadedSubMap(TSeqPos len);
    TList* x_Splice(size_t index, TList& seq);

    TList_I x_GetSegmentList_I(size_t index) const
        {
            return x_GetSegmentList_I(x_GetSegment(index));
        }
    void x_SetSegmentList_I(size_t index, TList_I iter)
        {
            x_SetSegmentList_I(x_SetSegment(index), iter);
        }

    TList_I x_GetSegmentList_I(const CSegment& seg) const
        {
            return reinterpret_cast<const TList_I&>(seg.m_Iterator);
        }
    void x_SetSegmentList_I(CSegment& seg, TList_I iter)
        {
            reinterpret_cast<TList_I&>(seg.m_Iterator) = iter;
        }
    
    TList_I x_FindInsertList_I(size_t index) const;
    virtual void x_SetSeq_data(size_t index, CSeq_data& data);
    virtual void x_SetSubSeqMap(size_t index, CSeqMap_Delta_seqs* subMap);

private:
    CConstRef<TObject> m_Object;
    const TList*       m_List;
};


class NCBI_XOBJMGR_EXPORT CSeqMap_Seq_data : public CSeqMap
{
public:
    typedef CSeq_inst             TObject;

    CSeqMap_Seq_data(const TObject& obj);
    ~CSeqMap_Seq_data(void);

protected:
    virtual void x_SetSeq_data(size_t index, CSeq_data& data);

private:
    CConstRef<TObject> m_Object;
};


class NCBI_XOBJMGR_EXPORT CSeqMap_Seq_locs : public CSeqMap
{
public:
    typedef CObject                 TObject;
    typedef list< CRef<CSeq_loc> >  TList;
    typedef TList::const_iterator   TList_I;

    CSeqMap_Seq_locs(const CSeg_ext& obj, const TList& seq);
    CSeqMap_Seq_locs(const CSeq_loc_mix& obj, const TList& seq);
    CSeqMap_Seq_locs(const CSeq_loc_equiv& obj, const TList& seq);

    ~CSeqMap_Seq_locs(void);

protected:
    void x_IndexAll(void);

    TList_I x_GetSegmentList_I(size_t index) const
        {
            return x_GetSegmentList_I(x_GetSegment(index));
        }
    void x_SetSegmentList_I(size_t index, TList_I iter)
        {
            x_SetSegmentList_I(x_SetSegment(index), iter);
        }

    TList_I x_GetSegmentList_I(const CSegment& seg) const
        {
            return reinterpret_cast<const TList_I&>(seg.m_Iterator);
        }
    void x_SetSegmentList_I(CSegment& seg, TList_I iter)
        {
            reinterpret_cast<TList_I&>(seg.m_Iterator) = iter;
        }
    
private:
    CConstRef<TObject> m_Object;
    const TList*       m_List;
};


class NCBI_XOBJMGR_EXPORT CSeqMap_Seq_intervals : public CSeqMap
{
public:
    typedef CPacked_seqint        TObject;
    typedef TObject::Tdata        TList;
    typedef TList::const_iterator TList_I;

    CSeqMap_Seq_intervals(const TObject& obj);
    CSeqMap_Seq_intervals(const TObject& obj, CSeqMap* parent, size_t index);

    ~CSeqMap_Seq_intervals(void);

protected:
    void x_IndexAll(void);

    TList_I x_GetSegmentList_I(size_t index) const
        {
            return x_GetSegmentList_I(x_GetSegment(index));
        }
    void x_SetSegmentList_I(size_t index, TList_I iter)
        {
            x_SetSegmentList_I(x_SetSegment(index), iter);
        }

    TList_I x_GetSegmentList_I(const CSegment& seg) const
        {
            return reinterpret_cast<const TList_I&>(seg.m_Iterator);
        }
    void x_SetSegmentList_I(CSegment& seg, TList_I iter)
        {
            reinterpret_cast<TList_I&>(seg.m_Iterator) = iter;
        }
    
private:
    CConstRef<TObject> m_Object;
    const TList*       m_List;
};


class NCBI_XOBJMGR_EXPORT CSeqMap_SeqPoss : public CSeqMap
{
public:
    typedef CPacked_seqpnt        TObject;
    typedef TObject::TPoints      TList;
    typedef TList::const_iterator TList_I;

    CSeqMap_SeqPoss(const TObject& obj);
    CSeqMap_SeqPoss(const TObject& obj, CSeqMap* parent, size_t index);

    ~CSeqMap_SeqPoss(void);

protected:
    void x_IndexAll(void);

    CSegment& x_AddPos(const CSeq_id* id, TSeqPos pos, ENa_strand strand);

    TList_I x_GetSegmentList_I(size_t index) const
        {
            return x_GetSegmentList_I(x_GetSegment(index));
        }
    void x_SetSegmentList_I(size_t index, TList_I iter)
        {
            x_SetSegmentList_I(x_SetSegment(index), iter);
        }

    TList_I x_GetSegmentList_I(const CSegment& seg) const
        {
            return reinterpret_cast<const TList_I&>(seg.m_Iterator);
        }
    void x_SetSegmentList_I(CSegment& seg, TList_I iter)
        {
            reinterpret_cast<TList_I&>(seg.m_Iterator) = iter;
        }
    
private:
    // Prohibit copy operator and constructor
    CSeqMap_SeqPoss(const CSeqMap_SeqPoss&);
    CSeqMap_SeqPoss& operator= (const CSeqMap_SeqPoss&);
    
    CConstRef<TObject> m_Object;
    const TList*       m_List;
};

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2004/07/12 16:53:28  vasilche
* Fixed loading of split Seq-data when sequence is not delta.
*
* Revision 1.9  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.8  2004/01/22 22:11:59  shomrat
* using typedefs instead of concrete types
*
* Revision 1.7  2003/11/12 16:53:16  grichenk
* Modified CSeqMap to work with const objects (CBioseq, CSeq_loc etc.)
*
* Revision 1.6  2003/09/30 16:21:59  vasilche
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
* Revision 1.5  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.4  2003/05/21 16:03:07  vasilche
* Fixed access to uninitialized optional members.
* Added initialization of mandatory members.
*
* Revision 1.3  2003/01/22 20:11:53  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.2  2002/12/26 20:51:36  dicuccio
* Added Win32 export specifier
*
* Revision 1.1  2002/12/26 16:39:22  vasilche
* Object manager class CSeqMap rewritten.
*
*
* ===========================================================================
*/

#endif  // OBJECTS_OBJMGR___SEQ_MAP_EXT__HPP
