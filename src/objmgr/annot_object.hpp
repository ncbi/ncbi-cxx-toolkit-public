#ifndef ANNOT_OBJECT__HPP
#define ANNOT_OBJECT__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Annotation object wrapper
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.5  2002/03/07 21:25:33  grichenk
* +GetSeq_annot() in annotation iterators
*
* Revision 1.4  2002/02/21 19:27:04  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:16  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include "data_source.hpp"
#include "handle_range_map.hpp"
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// General Seq-annot object used by CAnnot_CI to store data
class CAnnotObject : public CObject
{
public:
    CAnnotObject(CDataSource& data_source,
                 const CSeq_feat&  feat,
                 const CSeq_annot& annot);
    CAnnotObject(CDataSource& data_source,
                 const CSeq_align& align,
                 const CSeq_annot& annot);
    CAnnotObject(CDataSource& data_source,
                 const CSeq_graph& graph,
                 const CSeq_annot& annot);
    virtual ~CAnnotObject(void);

    SAnnotSelector::TAnnotChoice Which(void) const;

    bool IsFeat(void) const;
    const CSeq_feat& GetFeat(void) const;

    bool IsAlign(void) const;
    const CSeq_align& GetAlign(void) const;

    bool IsGraph(void) const;
    const CSeq_graph& GetGraph(void) const;

    const CHandleRangeMap& GetRangeMap(void) const;

    // Get Seq-annot, containing the element
    const CSeq_annot& GetSeq_annot(void) const;

private:
    // Constructors used by CAnnotTypes_CI only to create fake annotations
    // for sequence segments. The annot object points to the seq-annot
    // containing the original annotation object.
    friend class CAnnotTypes_CI;
    CAnnotObject(const CSeq_feat&  feat,
                 const CSeq_annot& annot);
    CAnnotObject(const CSeq_align& align,
                 const CSeq_annot& annot);
    CAnnotObject(const CSeq_graph& graph,
                 const CSeq_annot& annot);

    void x_ProcessAlign(const CSeq_align& align);

    CDataSource*                 m_DataSource;
    SAnnotSelector::TAnnotChoice m_Choice;
    CConstRef<CObject>           m_Object;
    CConstRef<CSeq_annot>        m_Annot;
    auto_ptr<CHandleRangeMap>    m_RangeMap; // may be null for fake objects
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CAnnotObject::CAnnotObject(CDataSource& data_source,
                           const CSeq_feat& feat,
                           const CSeq_annot& annot)
    : m_DataSource(&data_source),
      m_Choice(CSeq_annot::C_Data::e_Ftable),
      m_Object(dynamic_cast<const CObject*>(&feat)),
      m_Annot(&annot),
      m_RangeMap(new CHandleRangeMap(data_source.GetIdMapper()))
{
    m_RangeMap->AddLocation(feat.GetLocation());
    if ( feat.IsSetProduct() )
        m_RangeMap->AddLocation(feat.GetProduct());
    return;
}

inline
CAnnotObject::CAnnotObject(CDataSource& data_source,
                           const CSeq_align& align,
                           const CSeq_annot& annot)
    : m_DataSource(&data_source),
      m_Choice(CSeq_annot::C_Data::e_Align),
      m_Object(dynamic_cast<const CObject*>(&align)),
      m_Annot(&annot),
      m_RangeMap(new CHandleRangeMap(data_source.GetIdMapper()))
{
    x_ProcessAlign(align);
    return;
}

inline
CAnnotObject::CAnnotObject(CDataSource& data_source,
                           const CSeq_graph& graph,
                           const CSeq_annot& annot)
    : m_DataSource(&data_source),
      m_Choice(CSeq_annot::C_Data::e_Graph),
      m_Object(dynamic_cast<const CObject*>(&graph)),
      m_Annot(&annot),
      m_RangeMap(new CHandleRangeMap(data_source.GetIdMapper()))
{
    m_RangeMap->AddLocation(graph.GetLoc());
    return;
}

inline
CAnnotObject::~CAnnotObject(void)
{
    return;
}

inline
SAnnotSelector::TAnnotChoice CAnnotObject::Which(void) const
{
    return m_Choice;
}

inline
bool CAnnotObject::IsFeat(void) const
{
    return m_Choice == CSeq_annot::C_Data::e_Ftable;
}

inline
const CSeq_feat& CAnnotObject::GetFeat(void) const
{
    return dynamic_cast<const CSeq_feat&>(*m_Object);
}

inline
bool CAnnotObject::IsAlign(void) const
{
    return m_Choice == CSeq_annot::C_Data::e_Align;
}

inline
const CSeq_align& CAnnotObject::GetAlign(void) const
{
    return dynamic_cast<const CSeq_align&>(*m_Object);
}

inline
bool CAnnotObject::IsGraph(void) const
{
    return m_Choice == CSeq_annot::C_Data::e_Graph;
}

inline
const CSeq_graph& CAnnotObject::GetGraph(void) const
{
    return dynamic_cast<const CSeq_graph&>(*m_Object);
}

inline
const CHandleRangeMap& CAnnotObject::GetRangeMap(void) const
{
    return *m_RangeMap;
}

inline
const CSeq_annot& CAnnotObject::GetSeq_annot(void) const
{
    return *m_Annot;
}


inline
CAnnotObject::CAnnotObject(const CSeq_feat& feat,
                           const CSeq_annot& annot)
    : m_DataSource(0),
      m_Choice(CSeq_annot::C_Data::e_Ftable),
      m_Object(dynamic_cast<const CObject*>(&feat)),
      m_Annot(&annot),
      m_RangeMap(0)
{
    return;
}

inline
CAnnotObject::CAnnotObject(const CSeq_align& align,
                           const CSeq_annot& annot)
    : m_DataSource(0),
      m_Choice(CSeq_annot::C_Data::e_Align),
      m_Object(dynamic_cast<const CObject*>(&align)),
      m_Annot(&annot),
      m_RangeMap(0)
{
    return;
}

inline
CAnnotObject::CAnnotObject(const CSeq_graph& graph,
                           const CSeq_annot& annot)
    : m_DataSource(0),
      m_Choice(CSeq_annot::C_Data::e_Graph),
      m_Object(dynamic_cast<const CObject*>(&graph)),
      m_Annot(&annot),
      m_RangeMap(0)
{
    return;
}



END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ANNOT_OBJECT__HPP
