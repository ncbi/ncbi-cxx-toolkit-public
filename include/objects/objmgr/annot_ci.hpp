#ifndef ANNOT_CI__HPP
#define ANNOT_CI__HPP

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
*   Object manager iterators
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2002/01/16 16:26:36  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:03:59  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <util/rangemap.hpp>

#include <objects/objmgr1/bioseq_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CAnnotObject;
class CDataSource;

// Structure to select type of Seq-annot
struct SAnnotSelector
{
    typedef CSeq_annot::C_Data::E_Choice TAnnotChoice;
    typedef CSeqFeatData::E_Choice       TFeatChoice;

    SAnnotSelector(TAnnotChoice annot = CSeq_annot::C_Data::e_not_set,
                   TFeatChoice  feat  = CSeqFeatData::e_not_set)
        : m_AnnotChoice(annot), m_FeatChoice(feat) {}

    TAnnotChoice m_AnnotChoice;  // Annotation type
    TFeatChoice  m_FeatChoice;   // Seq-feat subtype
};


class CHandleRangeMap;


// General annotation iterator. Public interfaces should use
// CFeat_CI, CGraph_CI and CAlign_CI instead.
class CAnnot_CI
{
public:
    typedef CRange<int>                                              TRange;
    typedef CRangeMultimap<CRef<CAnnotObject>,TRange::position_type> TRangeMap;
    typedef map<CBioseqHandle::THandle, TRangeMap>                   TAnnotMap;

    CAnnot_CI(void);
    CAnnot_CI(CDataSource& datasource,
              CHandleRangeMap& loc,
              SAnnotSelector selector);
    CAnnot_CI(const CAnnot_CI& iter);
    ~CAnnot_CI(void);
    CAnnot_CI& operator= (const CAnnot_CI& iter);

    CAnnot_CI& operator++ (void);
    CAnnot_CI& operator++ (int);
    operator bool (void) const;
    CAnnotObject& operator* (void) const;
    CAnnotObject* operator-> (void) const;

private:
    typedef TRangeMap::iterator     TRangeIter;

    // Check if the location intersects with the current m_Location.
    bool x_ValidLocation(const CHandleRangeMap& loc) const;

    // FALSE if the selected item does not fit m_Selector and m_Location
    // TRUE will also be returned at the end of annotation list
    bool x_IsValid(void) const;

    // Move to the next valid seq-annot
    void x_Walk(void);

    CRef<CDataSource> m_DataSource;
    SAnnotSelector    m_Selector;
    TRangeMap*        m_RangeMap;
    TRange            m_CoreRange;
    TRangeIter        m_Current;
    CHandleRangeMap*  m_HandleRangeMap;
    CBioseqHandle     m_CurrentHandle;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ANNOT_CI__HPP
