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

#include <objmgr/annot_selector.hpp> // for CAnnotName
#include <objmgr/seq_id_handle.hpp>

#include <memory>
#include <map>
#include <vector>

#include <objmgr/split/id_range.hpp>
#include <objmgr/split/size.hpp>

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
class CID2S_Split_Info;
class CID2S_Chunk_Id;
class CID2S_Chunk;
class CBlobSplitter;
class CBlobSplitterImpl;
struct SSplitterParams;

class CAnnotObject_SplitInfo
{
public:
    CAnnotObject_SplitInfo(void)
        : m_ObjectType(0)
        {
        }
    CAnnotObject_SplitInfo(const CSeq_feat& obj, double ratio);
    CAnnotObject_SplitInfo(const CSeq_align& obj, double ratio);
    CAnnotObject_SplitInfo(const CSeq_graph& obj, double ratio);

    int m_ObjectType;
    CConstRef<CObject> m_Object;

    CSize m_Size;
    CSeqsRange m_Location;
};


class CLocObjects_SplitInfo
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

    TObjects m_Objects;

    CSize m_Size;
    CSeqsRange m_Location;
};


inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CLocObjects_SplitInfo& info)
{
    return info.Print(out);
}


class CSeq_annot_SplitInfo
{
public:
    typedef map<CSeq_id_Handle, CLocObjects_SplitInfo> TSimpleLocObjects;

    CSeq_annot_SplitInfo(void);
    CSeq_annot_SplitInfo(const CSeq_annot_SplitInfo& base,
                         const CLocObjects_SplitInfo& objs);
    
    void SetSeq_annot(int id, const CSeq_annot& annot,
                      const SSplitterParams& params);
    void Add(const CAnnotObject_SplitInfo& obj);

    bool IsLandmark(const CAnnotObject_SplitInfo& obj) const;

    CNcbiOstream& Print(CNcbiOstream& out) const;

    static CAnnotName GetName(const CSeq_annot& annot);
    static size_t CountAnnotObjects(const CSeq_annot& annot);

    int m_Id;
    CConstRef<CSeq_annot> m_Src_annot;
    CAnnotName m_Name;

    TSimpleLocObjects m_SimpleLocObjects;
    CLocObjects_SplitInfo m_ComplexLocObjects;
    CLocObjects_SplitInfo m_LandmarkObjects;

    CSize m_Size;
    CSeqsRange m_Location;
};


class CSeq_data_SplitInfo
{
public:
    typedef CRange<TSeqPos> TRange;
    void SetSeq_data(int gi, const TRange& range,
                     const CSeq_data& data, const SSplitterParams& params);

    int GetGi(void) const;
    TRange GetRange(void) const;

    CSeqsRange m_Location;
    CConstRef<CSeq_data> m_Data;
    CSize m_Size;
};


class CSeq_inst_SplitInfo : public CObject
{
public:
    typedef vector<CSeq_data_SplitInfo> TSeq_data;

    void Add(const CSeq_data_SplitInfo& data);

    CConstRef<CSeq_inst> m_Seq_inst;
    TSeq_data m_Seq_data;
};


typedef CSeq_annot_SplitInfo::TSimpleLocObjects TSimpleLocObjects;


inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CSeq_annot_SplitInfo& info)
{
    return info.Print(out);
}


class CBioseq_SplitInfo
{
public:
    typedef map<CConstRef<CSeq_annot>, CSeq_annot_SplitInfo> TSeq_annots;

    CBioseq_SplitInfo(void);
    ~CBioseq_SplitInfo(void);

    int m_Id;
    CRef<CBioseq> m_Bioseq;
    CRef<CBioseq_set> m_Bioseq_set;
    TSeq_annots m_Seq_annots;
    CRef<CSeq_inst_SplitInfo> m_Seq_inst;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2004/06/15 14:05:49  vasilche
* Added splitting of sequence.
*
* Revision 1.3  2004/01/07 17:36:20  vasilche
* Moved id2_split headers to include/objmgr/split.
* Fixed include path to genbank.
*
* Revision 1.2  2003/11/26 23:04:59  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.1  2003/11/12 16:18:31  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
#endif//NCBI_OBJMGR_SPLIT_OBJECT_SPLITINFO__HPP
