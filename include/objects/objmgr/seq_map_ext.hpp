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

#include <objects/objmgr/seq_map.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDelta_ext;
class CSeg_ext;

class CSeqMap_Delta_seqs : public CSeqMap
{
public:
    typedef CDelta_ext TObject;
    typedef list< CRef<CDelta_seq> > TList;
    typedef TList::iterator TList_I;

    CSeqMap_Delta_seqs(TObject& obj, CDataSource* source = 0);
    CSeqMap_Delta_seqs(TObject& obj, CSeqMap_Delta_seqs* parent, size_t index);

    ~CSeqMap_Delta_seqs(void);

protected:
    void x_Index(TList& seq);
    void x_IndexAll(TList& seq);
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
    CRef<TObject> m_Object;
    TList*        m_List;
};


class CSeqMap_Seq_locs : public CSeqMap
{
public:
    typedef CObject TObject;
    typedef list< CRef<CSeq_loc> > TList;
    typedef TList::iterator TList_I;

    CSeqMap_Seq_locs(CSeg_ext& obj, TList& seq, CDataSource* source = 0);
    CSeqMap_Seq_locs(CSeq_loc_mix& obj, TList& seq, CDataSource* source = 0);
    CSeqMap_Seq_locs(CSeq_loc_equiv& obj, TList& seq, CDataSource* source = 0);

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
    CRef<TObject> m_Object;
    TList*        m_List;
};


class CSeqMap_Seq_intervals : public CSeqMap
{
public:
    typedef CPacked_seqint TObject;
    typedef list< CRef<CSeq_interval> > TList;
    typedef TList::iterator TList_I;

    CSeqMap_Seq_intervals(TObject& obj, CDataSource* source = 0);
    CSeqMap_Seq_intervals(TObject& obj, CSeqMap* parent, size_t index);

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
    CRef<TObject> m_Object;
    TList*        m_List;
};


class CSeqMap_SeqPoss : public CSeqMap
{
public:
    typedef CPacked_seqpnt TObject;
    typedef list< TSeqPos > TList;
    typedef TList::iterator TList_I;

    CSeqMap_SeqPoss(TObject& obj, CDataSource* source = 0);
    CSeqMap_SeqPoss(TObject& obj, CSeqMap* parent, size_t index);

    ~CSeqMap_SeqPoss(void);

protected:
    virtual const CSeq_id& x_GetRefSeqid(const CSegment& seg) const;
    virtual TSeqPos x_GetRefPosition(const CSegment& seg) const;
    virtual bool x_GetRefMinusStrand(const CSegment& seg) const;

    void x_IndexAll(void);

    CSegment& x_AddPos(TSeqPos pos);

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
    CRef<TObject> m_Object;
    TList*        m_List;
};

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/12/26 16:39:22  vasilche
* Object manager class CSeqMap rewritten.
*
*
* ===========================================================================
*/

#endif  // OBJECTS_OBJMGR___SEQ_MAP_EXT__HPP
