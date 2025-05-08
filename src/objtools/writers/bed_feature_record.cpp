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

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>

#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/writers/bed_feature_record.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/genbank_id_resolve.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define BUMP( x, y ) if ( x < (y) ) x = (y)

//  ----------------------------------------------------------------------------
CBedFeatureRecord::CBedFeatureRecord(CGenbankIdResolve& id_resolve):
//  ----------------------------------------------------------------------------
    m_uColumnCount( 0 ),
    m_strChrom( "." ),
    m_strChromStart( "." ),
    m_strChromEnd( "." ),
    m_strName( "." ),
    m_strScore( "0" ),
    m_strStrand( "." ),
    m_strThickStart( "." ),
    m_strThickEnd( "." ),
    m_strItemRgb( "." ),
    m_strBlockCount( "." ),
    m_strBlockSizes( "." ),
    m_strBlockStarts( "." ),
    mIdResolve(id_resolve)
{
}

//  ----------------------------------------------------------------------------
CBedFeatureRecord::~CBedFeatureRecord()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
const CGene_ref&
sGetClosestGeneRef(
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    static const CGene_ref noRef;
    if (mf.GetData().IsGene()) {
        return mf.GetData().GetGene();
    }
    if (mf.IsSetXref()) {
        const auto& xrefs = mf.GetXref();
        for (auto it = xrefs.begin(); it != xrefs.end(); ++it) {
            const auto& xref = **it;
            if (xref.CanGetData() && xref.GetData().IsGene()) {
                return xref.GetData().GetGene();
            }
        }
    }
    CMappedFeat gene = feature::GetBestGeneForFeat(mf);
    if (gene  &&  gene.IsSetData()  &&  gene.GetData().IsGene()) {
        return gene.GetData().GetGene();
    }
    return noRef;
}


//  ----------------------------------------------------------------------------
bool CBedFeatureRecord::AssignName(
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    //auto featType = mf.GetFeatSubtype();
    //if (featType == CSeqFeatData::eSubtype_gene) {
    //    cerr << "";
    //}
    if (mf.GetData().IsRegion()) {
        m_strName = mf.GetData().GetRegion();
        std::transform(m_strName.begin(), m_strName.end(), m_strName.begin(),
            [](char c) -> char { return std::isspace(c) ? '_' : c; });
        return true;
    }
    const auto& geneRef = sGetClosestGeneRef(mf);
    if (geneRef.IsSetLocus()) {
        m_strName = geneRef.GetLocus();
        return true;
    }
    if (geneRef.IsSetLocus_tag()) {
        m_strName = geneRef.GetLocus_tag();
        return true;
    }

    // try harder only if the given feature itself is a gene:
    if (!mf.GetData().IsGene()) {
        return true;
    }
    if (geneRef.IsSetDesc()) {
        m_strName = geneRef.GetDesc();
    }
    if (geneRef.IsSetSyn()) {
        m_strName = geneRef.GetSyn().front();
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedFeatureRecord::AssignDisplayData(
    const CMappedFeat& mf,
    bool bUseScore )
//  ----------------------------------------------------------------------------
{
    if ( ! mf.GetData().IsUser() ) {
        return false;
    }
    const CUser_object& uo = mf.GetData().GetUser();
    if ( ! uo.IsSetType() || ! uo.GetType().IsStr() ||
        uo.GetType().GetStr() != "Display Data" ) {
        return false;
    }
    const vector< CRef< CUser_field > >& fields = uo.GetData();
    vector< CRef< CUser_field > >::const_iterator it = fields.begin();
    for ( ; it != fields.end(); ++it ) {
        if ( ! (*it)->CanGetLabel() || ! (*it)->GetLabel().IsStr() ) {
            continue;
        }
        string strLabel = (*it)->GetLabel().GetStr();
        if ( strLabel == "name" ) {
            BUMP( m_uColumnCount, 4 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsStr() ) {
                m_strName = (*it)->GetData().GetStr();
            }
            continue;
        }
        if ( strLabel == "score" && ! bUseScore ) {
            BUMP( m_uColumnCount, 5 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                m_strScore = NStr::UIntToString( (*it)->GetData().GetInt() );
            }
            continue;
        }
        if ( strLabel == "greylevel" && bUseScore ) {
            BUMP( m_uColumnCount, 5 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                m_strScore = NStr::UIntToString( (*it)->GetData().GetInt() );
            }
            continue;
        }
        if ( strLabel == "thickStart" ) {
            BUMP( m_uColumnCount, 7 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                m_strThickStart = NStr::UIntToString( (*it)->GetData().GetInt());
            }
            continue;
        }
        if ( strLabel == "thickEnd" ) {
            BUMP( m_uColumnCount, 8 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                m_strThickEnd = NStr::UIntToString( (*it)->GetData().GetInt()+1);
            }
            continue;
        }
        if ( strLabel == "itemRGB" ) {
            BUMP( m_uColumnCount, 9 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                m_strItemRgb = NStr::UIntToString( (*it)->GetData().GetInt() );
                continue;
            }
            if ( (*it)->IsSetData() && (*it)->GetData().IsStr() ) {
                m_strItemRgb = (*it)->GetData().GetStr();
                continue;
            }
            continue;
        }
        if ( strLabel == "blockCount" ) {
            BUMP( m_uColumnCount, 10 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                m_strBlockCount = NStr::UIntToString( (*it)->GetData().GetInt() );
            }
            continue;
        }
        if ( strLabel == "blockSizes" ) {
            BUMP( m_uColumnCount, 11 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsStr() ) {
                m_strBlockSizes = (*it)->GetData().GetStr();
            }
            continue;
        }
        if ( strLabel == "blockStarts" ) {
            BUMP( m_uColumnCount, 12 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsStr() ) {
                m_strBlockStarts = (*it)->GetData().GetStr();
            }
            continue;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedFeatureRecord::AssignLocation(
    CScope& scope,
    const CSeq_interval& interval )
//  ----------------------------------------------------------------------------
{
    if ( interval.CanGetId() ) {
        m_strChrom = interval.GetId().GetSeqIdString(true);
        try {
            string bestId;
            mIdResolve.GetBestId(
                CSeq_id_Handle::GetHandle(interval.GetId()), scope, bestId);
            m_strChrom = bestId;
            // its OK for this to silently fail.
            // Seq-ids that dont look up are just meant to be output directly
        } catch(...) { ; }
    }
    if ( interval.IsSetFrom() ) {
        m_strChromStart = NStr::UIntToString( interval.GetFrom() );
    }
    if ( interval.IsSetTo() ) {
        m_strChromEnd = NStr::UIntToString( interval.GetTo() + 1 );
    }
    BUMP( m_uColumnCount, 3 );
    m_strStrand = "+";
    if (interval.IsSetStrand()) {
        ENa_strand strand = interval.GetStrand();
        if (strand == eNa_strand_minus) {
            m_strStrand = "-";
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedFeatureRecord::Write(
    CNcbiOstream& ostr,
    unsigned int columnCount)
//  ----------------------------------------------------------------------------
{
    ostr << Chrom();
    ostr << "\t" << ChromStart();
    ostr << "\t" << ChromEnd();
    if ( columnCount >= 4 ) {
        ostr << "\t" << Name();
    }
    if ( columnCount >= 5 ) {
        ostr << "\t" << Score();
    }
    if ( columnCount >= 6 ) {
        ostr << "\t" << Strand();
    }
    if ( columnCount >= 7 ) {
        ostr << "\t" << ThickStart();
    }
    if ( columnCount >= 8 ) {
        ostr << "\t" << ThickEnd();
    }
    if ( columnCount >= 9 ) {
        ostr << "\t" << ItemRgb();
    }
    if ( columnCount >= 10 ) {
        ostr << "\t" << BlockCount();
    }
    if ( columnCount >= 11 ) {
        ostr << "\t" << BlockSizes();
    }
    if ( columnCount >= 12 ) {
        ostr << "\t" << BlockStarts();
    }
    ostr << '\n';
    return true;
}


//  ----------------------------------------------------------------------------
bool CBedFeatureRecord::SetLocation(
    const CSeq_loc& loc)
//  ----------------------------------------------------------------------------
{
    string bestId;
    if (!mIdResolve.GetBestId(loc, bestId)) {
        return false;
    }
    if (loc.IsInt()) {
        const CSeq_interval& chromInt = loc.GetInt();
        m_strChrom = bestId;
        m_strChromStart = NStr::IntToString(chromInt.GetFrom());
        m_strChromEnd = NStr::IntToString(chromInt.GetTo()+1);
        m_strStrand = "+";
        if (chromInt.IsSetStrand()) {
            ENa_strand strand = chromInt.GetStrand();
            if (strand == eNa_strand_minus) {
                m_strStrand = "-";
            }
        }
        return true;
    }

    if (loc.IsPnt()) {
        const CSeq_point& chromPt = loc.GetPnt();
        m_strChrom = bestId;
        m_strChromStart = NStr::IntToString(chromPt.GetPoint());
        m_strChromEnd = NStr::IntToString(chromPt.GetPoint()+1);
        m_strStrand = "+";
        if (chromPt.IsSetStrand()) {
            ENa_strand strand = chromPt.GetStrand();
            if (strand == eNa_strand_minus) {
                m_strStrand = "-";
            }
        }
        return true;
    }
    return false;
}

//  -----------------------------------------------------------------------------
bool CBedFeatureRecord::SetName(
    const CSeqFeatData& data)
//  -----------------------------------------------------------------------------
{
    if (data.IsRegion()) {
        m_strName = data.GetRegion();
        return true;
    }
    if (data.IsGene()) {
    }
    return false;
}

//  -----------------------------------------------------------------------------
bool CBedFeatureRecord::SetScore(
    int score)
//  -----------------------------------------------------------------------------
{
    m_strScore = NStr::IntToString(score);
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedFeatureRecord::SetThick(
    const CSeq_loc& loc)
//  ----------------------------------------------------------------------------
{
    if (loc.IsInt()) {
        const CSeq_interval& thickInt = loc.GetInt();
        m_strThickStart = NStr::IntToString(thickInt.GetFrom());
        m_strThickEnd = NStr::IntToString(thickInt.GetTo()+1);
        return true;
    }
    if (loc.IsPnt()) {
        const CSeq_point& thickPoint = loc.GetPnt();
        m_strThickStart = NStr::IntToString(thickPoint.GetPoint());
        m_strThickEnd = NStr::IntToString(thickPoint.GetPoint()+1);
        return true;
    }
    return false;
}

//  -----------------------------------------------------------------------------
bool CBedFeatureRecord::SetBlocks(
    const CSeq_loc& chrom,
    const CSeq_loc& blocks)
//  -----------------------------------------------------------------------------
{
    if (blocks.IsInt()) {
        return true;
    }
    if (blocks.IsPacked_int()) {
        const list<CRef<CSeq_interval> >& intervals = blocks.GetPacked_int().Get();
        ENa_strand strand = blocks.GetStrand();
        int offset = chrom.GetStart(eExtreme_Positional);
        list<string> blockStarts;
        list<string> blockSizes;

        for (auto pInterval: intervals) {
            const CSeq_interval& interval = *pInterval;
            if (strand == eNa_strand_minus) {
                blockStarts.push_front(NStr::NumericToString(interval.GetFrom()-offset));
                blockSizes.push_front(NStr::NumericToString(interval.GetLength()));
            }
            else {
                blockStarts.push_back(NStr::NumericToString(interval.GetFrom()-offset));
                blockSizes.push_back(NStr::NumericToString(interval.GetLength()));
            }
        }
        m_strBlockCount  = NStr::NumericToString(intervals.size());
        m_strBlockStarts = NStr::Join(blockStarts, ",");
        m_strBlockSizes  = NStr::Join(blockSizes, ",");
        return true;
    }
    return false;
}

//  -----------------------------------------------------------------------------
bool CBedFeatureRecord::SetNoThick(
    const CSeq_loc& loc)
//  -----------------------------------------------------------------------------
{
    if (loc.IsInt()) {
        const CSeq_interval& chromInt = loc.GetInt();
        m_strThickStart = NStr::IntToString(chromInt.GetFrom());
        m_strThickEnd = NStr::IntToString(chromInt.GetFrom());
        return true;
    }

    if (loc.IsPnt()) {
        const CSeq_point& chromPt = loc.GetPnt();
        m_strThickStart = NStr::IntToString(chromPt.GetPoint());
        m_strThickEnd = NStr::IntToString(chromPt.GetPoint());
        return true;
    }
    return false;
}

//  -----------------------------------------------------------------------------
bool CBedFeatureRecord::SetRgb(
    const string& color)
//  -----------------------------------------------------------------------------
{
    if (color == "0 0 0") {
        m_strItemRgb = "0";
        return true;
    }
    vector<string> rgb;
    NStr::Split(color, " ", rgb);
    m_strItemRgb = NStr::Join(rgb, ",");
    return true;
}

//  -----------------------------------------------------------------------------

END_NCBI_SCOPE
