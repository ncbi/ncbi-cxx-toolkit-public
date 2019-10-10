#ifndef NCBI_OBJMGR_SPLIT_OBJECT_SPLITINFO__HPP
#define NCBI_OBJMGR_SPLIT_OBJECT_SPLITINFO__HPP

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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Application for splitting blobs withing ID1 cache
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <objmgr/annot_name.hpp>
#include <objects/seq/seq_id_handle.hpp>

#include <memory>
#include <map>
#include <vector>

#include <objmgr/split/id_range.hpp>
#include <objmgr/split/size.hpp>
#include <objmgr/split/place_id.hpp>

BEGIN_NCBI_SCOPE

class CObjectOStream;

BEGIN_SCOPE(objects)

class CSeq_entry;
class CBioseq;
class CBioseq_set;
class CSeq_annot;
class CSeq_feat;
class CSeq_align;
class CSeq_graph;
class CSeq_data;
class CSeq_inst;
class CSeq_descr;
class CID2S_Split_Info;
class CID2S_Chunk_Id;
class CID2S_Chunk;
class CBlobSplitter;
class CBlobSplitterImpl;
struct SSplitterParams;


enum EAnnotPriority
{
    eAnnotPriority_skeleton = 0,
    eAnnotPriority_landmark,
    eAnnotPriority_regular,
    eAnnotPriority_low,
    eAnnotPriority_lowest,
    eAnnotPriority_zoomed,
    eAnnotPriority_max = kMax_Int
};
typedef unsigned TAnnotPriority;


class CAnnotObject_SplitInfo
{
public:
    CAnnotObject_SplitInfo(void)
        : m_ObjectType(0)
        {
        }
    CAnnotObject_SplitInfo(const CSeq_feat& obj,
                           const CBlobSplitterImpl& impl,
                           CSize::TSizeRatio ratio);
    CAnnotObject_SplitInfo(const CSeq_align& obj,
                           const CBlobSplitterImpl& impl,
                           CSize::TSizeRatio ratio);
    CAnnotObject_SplitInfo(const CSeq_graph& obj,
                           const CBlobSplitterImpl& impl,
                           CSize::TSizeRatio ratio);
    CAnnotObject_SplitInfo(const CSeq_table& obj,
                           const CBlobSplitterImpl& impl,
                           CSize::TSizeRatio ratio);

    TAnnotPriority GetPriority(void) const;
    TAnnotPriority CalcPriority(void) const;

    int Compare(const CAnnotObject_SplitInfo& other) const;

    int         m_ObjectType;
    CConstRef<CObject> m_Object;

    TAnnotPriority m_Priority;

    CSize       m_Size;
    CSeqsRange  m_Location;
};


class CLocObjects_SplitInfo : public CObject
{
public:
    typedef vector<CAnnotObject_SplitInfo> TObjects;
    typedef TObjects::const_iterator const_iterator;

    void Add(const CAnnotObject_SplitInfo& obj);
    CNcbiOstream& Print(CNcbiOstream& out) const;

    bool empty(void) const
        {
            return m_Objects.empty();
        }
    size_t size(void) const
        {
            return m_Objects.size();
        }
    void clear(void)
        {
            m_Objects.clear();
            m_Size.clear();
            m_Location.clear();
        }
    const_iterator begin(void) const
        {
            return m_Objects.begin();
        }
    const_iterator end(void) const
        {
            return m_Objects.end();
        }

    TObjects    m_Objects;

    CSize       m_Size;
    CSeqsRange  m_Location;
};


inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CLocObjects_SplitInfo& info)
{
    return info.Print(out);
}


class CSeq_annot_SplitInfo : public CObject
{
public:
    typedef vector< CRef<CLocObjects_SplitInfo> > TObjects;

    CSeq_annot_SplitInfo(void);

    void SetSeq_annot(const CSeq_annot& annot,
                      const SSplitterParams& params,
                      const CBlobSplitterImpl& impl);
    void Add(const CAnnotObject_SplitInfo& obj);

    CNcbiOstream& Print(CNcbiOstream& out) const;

    static CAnnotName GetName(const CSeq_annot& annot);
    static size_t CountAnnotObjects(const CSeq_annot& annot);

    TAnnotPriority GetPriority(void) const;
    TAnnotPriority GetPriority(const CAnnotObject_SplitInfo& obj) const;

    int Compare(const CSeq_annot_SplitInfo& other) const;
    
    CConstRef<CSeq_annot> m_Src_annot;
    CAnnotName      m_Name;

    TAnnotPriority  m_TopPriority;
    TAnnotPriority  m_NamePriority;
    TObjects        m_Objects;

    CSize           m_Size;
    CSeqsRange      m_Location;
};


inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CSeq_annot_SplitInfo& info)
{
    return info.Print(out);
}


class CSeq_descr_SplitInfo : public CObject
{
public:
    CSeq_descr_SplitInfo(const CPlaceId& place_id,
                         TSeqPos seq_length,
                         const CSeq_descr& descr,
                         const SSplitterParams& params);

    TAnnotPriority GetPriority(void) const;

    int Compare(const CSeq_descr_SplitInfo& other) const;

    CConstRef<CSeq_descr> m_Descr;

    TAnnotPriority m_Priority;

    CSize       m_Size;
    CSeqsRange  m_Location;
    mutable vector< CRef<CSeq_descr_SplitInfo> > m_SubPieces;
};


class CSeq_hist_SplitInfo : public CObject
{
public:
    CSeq_hist_SplitInfo(const CPlaceId& place_id,
                        const CSeq_hist& hist,
                        const SSplitterParams& params);
    CSeq_hist_SplitInfo(const CPlaceId& place_id,
                        const CSeq_align& align,
                        const SSplitterParams& params);

    TAnnotPriority GetPriority(void) const;

    typedef CSeq_hist::TAssembly TAssembly;

    TAssembly      m_Assembly;
    TAnnotPriority m_Priority;
    CSize          m_Size;
    CSeqsRange     m_Location;
};


class CSeq_data_SplitInfo : public CObject
{
public:
    typedef CRange<TSeqPos> TRange;
    void SetSeq_data(const CPlaceId& place_id, const TRange& range,
                     TSeqPos seq_length,
                     const CSeq_data& data,
                     const SSplitterParams& params);

    TAnnotPriority GetPriority(void) const;

    TRange GetRange(void) const;

    CConstRef<CSeq_data> m_Data;

    TAnnotPriority m_Priority;

    CSize       m_Size;
    CSeqsRange  m_Location;
};


class CSeq_inst_SplitInfo : public CObject
{
public:
    typedef vector<CSeq_data_SplitInfo> TSeq_data;

    void Add(const CSeq_data_SplitInfo& data);

    CConstRef<CSeq_inst> m_Seq_inst;

    TSeq_data m_Seq_data;
};


class CBioseq_SplitInfo : public CObject
{
public:
    CBioseq_SplitInfo(const CBioseq& bioseq, const SSplitterParams& params);
    
    bool CanSplit(void) const;
    TAnnotPriority GetPriority(void) const;

    CConstRef<CBioseq> m_Bioseq;

    TAnnotPriority m_Priority;

    CSize              m_Size;
    CSeqsRange         m_Location;
};


class CPlace_SplitInfo
{
public:
    typedef map<CConstRef<CSeq_annot>, CSeq_annot_SplitInfo> TSeq_annots;
    typedef vector<CBioseq_SplitInfo> TBioseqs;

    CPlace_SplitInfo(void);
    ~CPlace_SplitInfo(void);

    CRef<CBioseq> m_Bioseq;
    CRef<CBioseq_set> m_Bioseq_set;

    CPlaceId                    m_PlaceId;
    CRef<CSeq_descr_SplitInfo>  m_Descr;
    TSeq_annots                 m_Annots;
    CRef<CSeq_inst_SplitInfo>   m_Inst;
    CRef<CSeq_hist_SplitInfo>   m_Hist;
    TBioseqs                    m_Bioseqs;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//NCBI_OBJMGR_SPLIT_OBJECT_SPLITINFO__HPP
