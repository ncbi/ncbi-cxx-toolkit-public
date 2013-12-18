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
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gff3_source_data.hpp>
#include <objtools/writers/gff3_alignment_data.hpp>
#include <objtools/writers/gff3_denseg_record.hpp>
#include <objtools/writers/gff3_splicedseg_record.hpp>
#include <objtools/writers/gff3_writer.hpp>
#include <objtools/alnmgr/alnmap.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define INSERTION(sf, tf) ( ((sf) &  CAlnMap::fSeq) && !((tf) &  CAlnMap::fSeq) )
#define DELETION(sf, tf) ( !((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )
#define MATCH(sf, tf) ( ((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )

//  ----------------------------------------------------------------------------
bool sIsTransspliced(const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    return (mf.IsSetExcept_text()  &&  mf.GetExcept_text() == "trans-splicing");
}

//  ----------------------------------------------------------------------------
static CConstRef<CSeq_id> sGetSourceId(
    const CSeq_id& id, CScope& scope )
//  ----------------------------------------------------------------------------
{
    try {
        return sequence::GetId(id, scope, sequence::eGetId_ForceAcc).GetSeqId();
    }
    catch (CException&) {
    }
    return CConstRef<CSeq_id>(&id);
}

//  ----------------------------------------------------------------------------
bool
sInheritScores(
    const CSeq_align& alignFrom,
    CSeq_align& alignTo)
//  Idea: Inherit down, but only in a score of the same key/id does not already
//    exist.
//  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CScore> > SCORES;

    if (!alignFrom.IsSetScore()) {
        return true;
    }
    const SCORES& scoresFrom = alignFrom.GetScore();
    for (SCORES::const_iterator itFrom = scoresFrom.begin(); 
            itFrom != scoresFrom.end(); ++itFrom) {

        const CScore& scoreFrom = **itFrom;

        if (scoreFrom.GetId().IsStr()) {
            const string& keyFrom = scoreFrom.GetId().GetStr(); 
            const SCORES& scoresTo = alignTo.GetScore();
            SCORES::const_iterator itTo;
            for (itTo = scoresTo.begin(); itTo != scoresTo.end(); ++itTo) {
                const CScore& scoreTo = **itTo;
                if (scoreTo.GetId().IsStr()) {
                    const string& keyTo = scoreTo.GetId().GetStr();
                    if (keyTo == keyFrom) {
                        break;
                    }
                }
            }
            if (itTo == scoresTo.end()) {
                alignTo.SetScore().push_back(*itFrom);
            }
        }

        if (scoreFrom.GetId().IsId()) {
            const CObject_id& idFrom = scoreFrom.GetId();
            const SCORES& scoresTo = alignFrom.GetScore();
            SCORES::const_iterator itTo;
            for (itTo = scoresTo.begin(); itTo != scoresTo.end(); ++itTo) {
                const CScore& scoreTo = **itTo;
                if (scoreTo.GetId().IsId()) {
                    const CObject_id& idTo = scoreTo.GetId();
                    if (idTo.Match(idFrom)) {
                        break;
                    }
                }
            }
            if (itTo == scoresTo.end()) {
                alignTo.SetScore().push_back(*itFrom);
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
CGff3Writer::CGff3Writer(
    CScope& scope, 
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CGff2Writer( scope, ostr, uFlags )
{
    m_uRecordId = 1;
    m_uPendingGeneId = 0;
    m_uPendingMrnaId = 0;
    m_uPendingTrnaId = 0;
    m_uPendingCdsId = 0;
    m_uPendingGenericId = 0;
};

//  ----------------------------------------------------------------------------
CGff3Writer::CGff3Writer(
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CGff2Writer( ostr, uFlags )
{
    m_uRecordId = 1;
    m_uPendingGeneId = 0;
    m_uPendingMrnaId = 0;
    m_uPendingCdsId = 0;
    m_uPendingTrnaId = 0;
    m_uPendingGenericId = 0;
};

//  ----------------------------------------------------------------------------
CGff3Writer::~CGff3Writer()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGff3Writer::WriteAlign(
    const CSeq_align& align,
    const string& strAssName,
    const string& strAssAcc )
//  ----------------------------------------------------------------------------
{
    try {
        align.Validate(true);
    }
    catch(CException& e) {
        string msg("Inconsistent alignment data ");
        msg += ("\"\"\"" + e.GetMsg() + "\"\"\"");
        NCBI_THROW(CObjWriterException, eBadInput, msg);
    }
    if (!CGff2Writer::WriteAlign(align, strAssName, strAssAcc)) {
        return false;
    }
    m_uRecordId++;
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteSeqAnnotHandle(
    CSeq_annot_Handle sah )
//  ----------------------------------------------------------------------------
{
    CConstRef<CSeq_annot> pAnnot = sah.GetCompleteSeq_annot();

    if ( pAnnot->IsAlign() ) {
        for ( CAlign_CI it( sah ); it; ++it ) {
            if ( ! x_WriteAlign( *it ) ) {
                return false;
            }
        }
		m_uRecordId++;
        return true;
    }

    SAnnotSelector sel = GetAnnotSelector();
    CFeat_CI feat_iter(sah, sel);
    CGffFeatureContext fc(feat_iter, CBioseq_Handle(), sah);
    for ( /*0*/; feat_iter; ++feat_iter ) {
        if ( ! x_WriteFeature( fc, *feat_iter ) ) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteAlign( 
    const CSeq_align& align )
//  ----------------------------------------------------------------------------
{
    if ( ! align.IsSetSegs() ) {
        cerr << "Object type not supported." << endl;
        return true;
    }

    switch( align.GetSegs().Which() ) {
        default:
            break;
        case CSeq_align::TSegs::e_Denseg:
            return x_WriteAlignDenseg( align );
        case CSeq_align::TSegs::e_Spliced:
            return x_WriteAlignSpliced( align );
        case CSeq_align::TSegs::e_Disc:
            return x_WriteAlignDisc( align );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteAlignDisc( 
    const CSeq_align& align)
//  ----------------------------------------------------------------------------
{
    typedef CSeq_align_set::Tdata::const_iterator CASCIT;

    const CSeq_align_set::Tdata& data = align.GetSegs().GetDisc().Get();
    for ( CASCIT cit = data.begin(); cit != data.end(); ++cit ) {

        CRef<CSeq_align> pA(new CSeq_align);
        pA->Assign(**cit);
        if (!sInheritScores(align, *pA)) {
            return false;
        }
        if (!x_WriteAlign(*pA)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteAlignSpliced( 
    const CSeq_align& align)
//  ----------------------------------------------------------------------------
{
    _ASSERT(align.IsSetSegs() && align.GetSegs().IsSpliced());

    typedef list<CRef<CSpliced_exon> > EXONS;
    const EXONS& exons = align.GetSegs().GetSpliced().GetExons();

    for (EXONS::const_iterator cit = exons.begin(); cit != exons.end(); ++cit) {
        CGffSplicedSegRecord record(m_uFlags, m_uRecordId);
        const CSpliced_exon& exon = **cit;
        if (!record.Initialize(*m_pScope, align, exon)) {
            return false;
        }
        x_WriteAlignment(record);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteAlignDenseg( 
    const CSeq_align& align)
//  ----------------------------------------------------------------------------
{
    CRef<CDense_seg> dsFilled = align.GetSegs().GetDenseg().FillUnaligned();
    CAlnMap alnMap(*dsFilled);

    unsigned int numRows = alnMap.GetNumRows();
    for (int sourceRow = 1; sourceRow < numRows; ++sourceRow) {

        CGffDenseSegRecord record(m_uFlags, m_uRecordId);
        if (!record.Initialize(*m_pScope, align, alnMap, sourceRow)) {
            return false;
        }
        x_WriteAlignment(record);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::WriteHeader()
//  ----------------------------------------------------------------------------
{
    if (!m_bHeaderWritten) {
        m_Os << "##gff-version 3" << endl;
        m_Os << "#!gff-spec-version 1.20" << endl;
        m_Os << "#!processor NCBI annotwriter" << endl;
        m_bHeaderWritten = true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteSequenceHeader(
    CSeq_id_Handle idh)
//  ----------------------------------------------------------------------------
{
    string id;
    if (!CWriteUtil::GetBestId(idh, *m_pScope, id)) {
        id = "<unknown>";
    }
    m_Os << "##sequence-region " << id << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteSequenceHeader(
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    //sequence-region
    string id;
    CConstRef<CSeq_id> pId = bsh.GetNonLocalIdOrNull();
    if ( pId ) {
        if (!CWriteUtil::GetBestId(CSeq_id_Handle::GetHandle(*pId), bsh.GetScope(), id)) {
            id = "<unknown>";
        }
    }

    string start = "1";
    string stop = NStr::IntToString(bsh.GetBioseqLength());
    m_Os << "##sequence-region " << id << " " << start << " " << stop << endl;

    //species
    const string base_url = 
        "http://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?";
    CSeqdesc_CI sdi( bsh.GetParentEntry(), CSeqdesc::e_Source, 0 );
    if (sdi) {
        const CBioSource& bs = sdi->GetSource();
        if (bs.IsSetOrg()) {
            string tax_id = NStr::IntToString(bs.GetOrg().GetTaxId());
            m_Os << "##species " << base_url << "id=" << tax_id << endl;
        }
        else if (bs.IsSetOrgname()) {
            string orgname = NStr::URLEncode(bs.GetTaxname());
            m_Os << "##species " << base_url << "name=" << orgname << endl;        
        }
    }

    //genome build
//    for(CSeqdesc_CI udi(bsh.GetParentEntry(), CSeqdesc::e_User, 0); udi; ++udi) {
//        const CUser_object& uo = udi->GetUser();
//        if (!uo.IsSetType()  ||  uo.GetType().IsStr()  ||  
//                uo.GetType().GetStr() != "GenomeBuild" ) {
//            continue;
//        }
//        //awaiting specific instructions here ...
//        break;
//    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteBioseqHandle(
    CBioseq_Handle bsh ) 
//  ----------------------------------------------------------------------------
{
    if ( ! x_WriteSequenceHeader(bsh) ) {
        return false;
    }

    SAnnotSelector sel = GetAnnotSelector();
    CFeat_CI feat_iter(bsh, sel);
    CGffFeatureContext fc(feat_iter, bsh);

    if (!x_WriteSource(fc)) {
        return false;
    }
    for ( ; feat_iter; ++feat_iter) {
        if (!x_WriteFeature(fc, *feat_iter)) {
            return false;
        }
    }
    for (CAlign_CI align_it(bsh, sel);  align_it;  ++ align_it) {
        x_WriteAlign(*align_it);
        m_uRecordId++;
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteSource(
    CGffFeatureContext& fc )
//  ----------------------------------------------------------------------------
{
    CBioseq_Handle bsh = fc.BioseqHandle();
    CSeqdesc_CI sdi(bsh.GetParentEntry(), CSeqdesc::e_Source, 0);
    if (!sdi) {
        return true; 
    }
    CRef<CGff3SourceRecord> pSource( 
        new CGff3SourceRecord( 
            fc, 
            string("id") + NStr::UIntToString(m_uPendingGenericId++)));
    pSource->AssignData(*sdi);
    x_WriteRecord(pSource);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteFeature(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    //CSeqFeatData::ESubtype s = mf.GetFeatSubtype();
    try {
        switch( mf.GetFeatSubtype() ) {
            default:
                if (mf.GetFeatType() == CSeqFeatData::e_Rna) {
                    return x_WriteFeatureRna( fc, mf );
                }
                return x_WriteFeatureGeneric( fc, mf );
            case CSeqFeatData::eSubtype_gene: 
                return x_WriteFeatureGene( fc, mf );
            case CSeqFeatData::eSubtype_cdregion:
                return x_WriteFeatureCds( fc, mf );
            case CSeqFeatData::eSubtype_tRNA:
                return x_WriteFeatureTrna( fc, mf );

            case CSeqFeatData::eSubtype_pub:
                return true; //ignore
        }
    }
    catch (CException& e) {
        cerr << "CGff3Writer: Unsupported feature type encountered: Removed." << endl;
        cerr << mf.GetFeatType() << "\t" << mf.GetFeatSubtype() << endl;
        cerr << "  exc: " << e.ReportAll() << endl;
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteFeatureTrna(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGff3WriteRecordFeature> pParent( new CGff3WriteRecordFeature( fc ) );
    if ( ! pParent->AssignFromAsn( mf, m_uFlags ) ) {
        return false;
    }
    xTryAssignGeneParent(*pParent, fc, mf);
    vector<string> ids;
    if ( ! pParent->GetAttribute( "ID", ids ) ) {
        pParent->ForceAttributeID( 
            string( "rna" ) + NStr::UIntToString( m_uPendingTrnaId ) );
    }
    TSeqPos seqlength = 0;
    if(fc.BioseqHandle() && fc.BioseqHandle().CanGetInst())
        seqlength = fc.BioseqHandle().GetInst().GetLength();
    if (!x_WriteFeatureRecords( *pParent, mf.GetLocation(), seqlength ) ) {
        return false;
    }

    CRef<CGff3WriteRecordFeature> pRna( 
        new CGff3WriteRecordFeature( 
            fc,
            string( "rna" ) + NStr::UIntToString( m_uPendingTrnaId++ ) ) );
    if ( ! pRna->AssignFromAsn( mf, m_uFlags ) ) {
        return false;
    }
    const CSeq_loc& PackedInt = *pRna->GetCircularLocation();
    unsigned int seqLength = 0;
    if (CWriteUtil::IsSequenceCircular(fc.BioseqHandle()) &&
        fc.BioseqHandle( )) {
        seqLength = fc.BioseqHandle().GetInst().GetLength();
    }

    if ( PackedInt.IsPacked_int() && PackedInt.GetPacked_int().CanGet() ) {
        const list< CRef< CSeq_interval > >& sublocs = PackedInt.GetPacked_int().Get();
        list< CRef< CSeq_interval > >::const_iterator it;
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CRef<CGff3WriteRecordFeature> pChild( 
                new CGff3WriteRecordFeature( *pRna ) );
            pChild->CorrectType("exon");
            pChild->AssignParent(*pRna);
            pChild->CorrectLocation( *pRna, subint, seqLength );
            pChild->ForceAttributeID(
                string("id") + NStr::UIntToString(m_uPendingGenericId++));
            if ( ! x_WriteRecord( pChild ) ) {
                return false;
            }
        }
    }
    return true;    
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteFeatureGene(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGff3WriteRecordFeature> pRecord( 
        new CGff3WriteRecordFeature( 
            fc,
            string("gene") + NStr::UIntToString(m_uPendingGeneId++)));

    if (!pRecord->AssignFromAsn(mf, m_uFlags)) {
        return false;
    }
    m_GeneMap[mf] = pRecord;

    return x_WriteFeatureRecords(*pRecord, *pRecord->GetCircularLocation(), 0);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteFeatureCds(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef< CGff3WriteRecordFeature > pCds( new CGff3WriteRecordFeature( fc ) );

    if ( ! pCds->AssignFromAsn( mf, m_uFlags ) ) {
        return false;
    }
    if (!xTryAssignMrnaParent(*pCds, fc, mf)) {
        xTryAssignGeneParent(*pCds, fc, mf);
    }

    const CSeq_loc& PackedInt = *pCds->GetCircularLocation();
    bool bStrandAdjust = PackedInt.IsSetStrand()  
        &&  (PackedInt.GetStrand()  ==  eNa_strand_minus);
    int /*CCdregion::EFrame*/ iPhase = 0;
    if (mf.GetData().GetCdregion().IsSetFrame()) {
        iPhase = max(mf.GetData().GetCdregion().GetFrame()-1, 0);
    }
    int iTotSize = -iPhase;
    if (bStrandAdjust && iPhase) {
        iPhase = 3-iPhase;
    }
    unsigned int seqLength = 0;
    if (CWriteUtil::IsSequenceCircular(fc.BioseqHandle()) &&
        fc.BioseqHandle() ) {
        seqLength = fc.BioseqHandle().GetInst().GetLength();
    }
    if ( PackedInt.IsPacked_int() && PackedInt.GetPacked_int().CanGet() ) {
        list< CRef< CSeq_interval > > sublocs( PackedInt.GetPacked_int().Get() );
        list< CRef< CSeq_interval > >::const_iterator it;
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CRef<CGff3WriteRecordFeature> pExon( new CGff3WriteRecordFeature( *pCds ) );
            pExon->CorrectType( "CDS" );
            pExon->CorrectLocation(*pCds, subint, seqLength);
            pExon->CorrectPhase( bStrandAdjust ? (3-iPhase) : iPhase );
            pExon->ForceAttributeID( string( "cds" ) + NStr::UIntToString( m_uPendingCdsId ) );
            if ( ! x_WriteRecord( pExon ) ) {
                return false;
            }
            iTotSize = (iTotSize + subint.GetLength());
            if (!bStrandAdjust) {
                iPhase = (3-iTotSize)%3; 
            }
            else {
                iPhase = iTotSize%3;
            }
        }
    }
    ++m_uPendingCdsId;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignRecordFromAsn(
    const CMappedFeat& mf,
    CGffFeatureContext& context,
    CGff3WriteRecordFeature& record)
//  ----------------------------------------------------------------------------
{
    if (sIsTransspliced(mf)) {
        cerr << "";
    }
    if (!record.AssignFromAsn(mf, m_uFlags)) {
        return false;
    }
    if (mf.GetFeatType() == CSeqFeatData::e_Rna) {
        if (!xTryAssignGeneParent(record, context, mf)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteFeatureRna(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGff3WriteRecordFeature> pRna( 
        new CGff3WriteRecordFeature( 
            fc,
            string( "rna" ) + NStr::UIntToString( m_uPendingTrnaId++ ) ) );
    if (!xAssignRecordFromAsn(mf, fc, *pRna)) {
        return false;
    }
    if ( ! x_WriteRecord( pRna ) ) {
        return false;
    }
    if (mf.GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA) {
        m_MrnaMap[ mf ] = pRna;
    }    

    const CSeq_loc& PackedInt = *pRna->GetCircularLocation();
    unsigned int seqLength = 0;
    if (CWriteUtil::IsSequenceCircular(fc.BioseqHandle()) &&
        fc.BioseqHandle() ) {
        seqLength = fc.BioseqHandle().GetInst().GetLength();
    }
    if ( PackedInt.IsPacked_int() && PackedInt.GetPacked_int().CanGet() ) {
        const list< CRef< CSeq_interval > >& sublocs = PackedInt.GetPacked_int().Get();
        list< CRef< CSeq_interval > >::const_iterator it;
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CRef<CGff3WriteRecordFeature> pChild( 
                new CGff3WriteRecordFeature( *pRna ) );
            pChild->CorrectType("exon");
            pChild->AssignParent(*pRna);
            pChild->CorrectLocation( *pRna, subint, seqLength );
            pChild->ForceAttributeID(
                string("id") + NStr::UIntToString(m_uPendingGenericId++));
            pChild->DropAttribute( "Name" ); //explicitely not inherited
            if ( ! x_WriteRecord( pChild ) ) {
                return false;
            }
        }
        return true;
    }
    return true;    
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteFeatureGeneric(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGff3WriteRecordFeature> pParent( new CGff3WriteRecordFeature( fc ) );
    if ( ! pParent->AssignFromAsn( mf, m_uFlags ) ) {
        return false;
    }

    vector<string> ids;
    if ( ! pParent->GetAttribute("ID", ids) ) {
        pParent->ForceAttributeID( 
            string("id") + NStr::UIntToString(m_uPendingGenericId++));
    }

    //  Even for generic features, there are special case to consider ...
    switch (mf.GetFeatSubtype()) {
        default:
            break;
        case CSeqFeatData::eSubtype_C_region:
        case CSeqFeatData::eSubtype_D_segment:
        case CSeqFeatData::eSubtype_J_segment:
        case CSeqFeatData::eSubtype_V_segment:
            xTryAssignGeneParent(*pParent, fc, mf);
            break;
        case CSeqFeatData::eSubtype_exon:
            if (!xTryAssignMrnaParent(*pParent, fc, mf)) {
                xTryAssignGeneParent(*pParent, fc, mf);
            }
            break;
    }

    // Finally, actually write out that feature
    TSeqPos seqlength = 0;
    if(fc.BioseqHandle() && fc.BioseqHandle().CanGetInst())
        seqlength = fc.BioseqHandle().GetInst().GetLength();
    return x_WriteFeatureRecords( *pParent, mf.GetLocation(), seqlength );
}

//  ============================================================================
bool CGff3Writer::x_WriteFeatureRecords(
    const CGff3WriteRecordFeature& record,
    const CSeq_loc& location,
    unsigned int seqLength )
//  ============================================================================
{
    CRef< CSeq_loc > pPackedInt( new CSeq_loc( CSeq_loc::e_Mix ) );
    pPackedInt->Add( location );
    CWriteUtil::ChangeToPackedInt(*pPackedInt);

    if ( pPackedInt->IsPacked_int() && pPackedInt->GetPacked_int().CanGet() ) {
        const list< CRef< CSeq_interval > >& sublocs = pPackedInt->GetPacked_int().Get();
        if (sublocs.size() > 1) {
            list< CRef< CSeq_interval > >::const_iterator it;
            string totIntervals = string("/") + NStr::IntToString(sublocs.size());
            unsigned int curInterval = 1;
            for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
                const CSeq_interval& subint = **it;
                CRef<CGff3WriteRecordFeature> pChild( 
                    new CGff3WriteRecordFeature( record ) );
                pChild->CorrectLocation( record, subint, seqLength );
                string part = NStr::IntToString(curInterval++) + totIntervals;
                pChild->SetAttribute("part", part);
                if ( ! x_WriteRecord( pChild ) ) {
                    return false;
                }
            }
            return true;
        }
    }
    
    // default behavior:
    return x_WriteRecord( &record );    
}

//  ============================================================================
void CGff3Writer::x_WriteAlignment( 
    const CGffAlignmentRecord& record )
//  ============================================================================
{
    m_Os << record.StrId() << '\t';
    m_Os << record.StrSource() << '\t';
    m_Os << record.StrType() << '\t';
    m_Os << record.StrSeqStart() << '\t';
    m_Os << record.StrSeqStop() << '\t';
    m_Os << record.StrScore() << '\t';
    m_Os << record.StrStrand() << '\t';
    m_Os << record.StrPhase() << '\t';
    m_Os << record.StrAttributes() << endl;
}

//  ============================================================================
bool CGff3Writer::xTryAssignGeneParent(
    CGff3WriteRecordFeature& feat,
    CGffFeatureContext& fc,
    CMappedFeat mf)
//  ============================================================================
{
    CMappedFeat gene = fc.FindBestGeneParent(mf);
    if (!gene) {
        return false;
    }
    TGeneMap::iterator it = m_GeneMap.find(gene);
    if (it == m_GeneMap.end()) {
        return false;
    }
    feat.AssignParent(*( it->second));
    return true;
}

//  ============================================================================
bool CGff3Writer::xTryAssignMrnaParent(
    CGff3WriteRecordFeature& feat,
    CGffFeatureContext& fc,
    CMappedFeat mf)
//  ============================================================================
{
    CMappedFeat mrna;
    switch (mf.GetFeatSubtype()) {
        default:
            mrna = feature::GetBestParentForFeat(
                mf, CSeqFeatData::eSubtype_mRNA, &fc.FeatTree());
            break;
        case CSeqFeatData::eSubtype_cdregion:
            mrna = feature::GetBestMrnaForCds(mf, &fc.FeatTree());
            break;
    }
    TMrnaMap::iterator it = m_MrnaMap.find(mrna);
    if (it != m_MrnaMap.end()) {
        feat.AssignParent(*(it->second));
        return true;
    }
    return false;
}

END_NCBI_SCOPE
