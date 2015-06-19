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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write bed file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>

#include <objtools/writers/write_util.hpp>
#include <objtools/writers/bed_track_record.hpp>
#include <objtools/writers/bed_feature_record.hpp>
#include <objtools/writers/bed_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CBedWriter::CBedWriter(
    CScope& scope,
    CNcbiOstream& ostr,
    unsigned int colCount,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CWriterBase(ostr, uFlags),
    m_Scope(scope),
    m_colCount(colCount)
{
    // the first three columns are mandatory
    if (m_colCount < 3) {
        m_colCount = 3;
    }
};

//  ----------------------------------------------------------------------------
CBedWriter::~CBedWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CBedWriter::WriteAnnot( 
    const CSeq_annot& annot,
    const string&,
    const string& )
//  ----------------------------------------------------------------------------
{
    if( annot.CanGetDesc() ) {
        ITERATE(CAnnot_descr::Tdata, DescIter, annot.GetDesc().Get()) {
            const CAnnotdesc& desc = **DescIter;
            if(desc.IsUser()) {
                if(desc.GetUser().HasField("NCBI_BED_COLUMN_COUNT")) {
                    CConstRef< CUser_field > field = desc.GetUser().GetFieldRef("NCBI_BED_COLUMN_COUNT");
                    if(field && field->CanGetData() && field->GetData().IsInt()) {
                        m_colCount = field->GetData().GetInt();
                    }
                }
            }
        }
    }
    
    CBedTrackRecord track;
    if ( ! track.Assign(annot) ) {
        return false;
    }
    track.Write(m_Os);

    SAnnotSelector sel = xGetAnnotSelector();
    CSeq_annot_Handle sah = m_Scope.AddSeq_annot(annot);
    for (CFeat_CI pMf(sah, sel); pMf; ++pMf ) {
        if (!xWriteFeature(track, *pMf)) {
            m_Scope.RemoveSeq_annot(sah);
            return false;
        }
    }
    m_Scope.RemoveSeq_annot(sah);
    return true;
}


//  ----------------------------------------------------------------------------
bool CBedWriter::xWriteFeature(
    const CBedTrackRecord& track,
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    CBedFeatureRecord record;
    if (!record.AssignDisplayData( mf, track.UseScore())) {
//          feature did not contain display data ---
//          Is there any alternative way to populate some of the bed columns?
//          For now, keep going, emit at least the locations ...
    }

    CRef<CSeq_loc> pPackedInt(new CSeq_loc(CSeq_loc::e_Mix));
    pPackedInt->Add(mf.GetLocation());
    CWriteUtil::ChangeToPackedInt(*pPackedInt);

    if (!pPackedInt->IsPacked_int() || !pPackedInt->GetPacked_int().CanGet()) {
        // nothing to do
        return true;
    }

    const list<CRef<CSeq_interval> >& sublocs = pPackedInt->GetPacked_int().Get();
    list<CRef<CSeq_interval> >::const_iterator it;
    for (it = sublocs.begin(); it != sublocs.end(); ++it ) {
        if (!record.AssignLocation(**it)  ||  !record.Write(m_Os, m_colCount)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
SAnnotSelector CBedWriter::xGetAnnotSelector()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
    return sel;
}

END_NCBI_SCOPE
