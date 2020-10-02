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
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seq/so_map.hpp>

#include <objmgr/annot_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/writers/gff2_write_data.hpp>
#include <objtools/writers/gtf_write_data.hpp>
#include <objtools/writers/gff_writer.hpp>
#include <objtools/writers/gtf_writer.hpp>
#include <objtools/writers/genbank_id_resolve.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ============================================================================
class CGtfIdGenerator
//  ============================================================================
{
public:
    static void Reset()
    {
        mLastSuffixes.clear();
    }
    static string NextId(
        const string prefix)
    {
        auto mapIt = mLastSuffixes.find(prefix);
        if (mapIt != mLastSuffixes.end()) {
            ++mapIt->second;
            return prefix + "_" + NStr::NumericToString(mapIt->second);
        }
        mLastSuffixes[prefix] = 1;
        return prefix + "_1";
    }
    
private:
    static map<string, int> mLastSuffixes;
};
map<string, int> CGtfIdGenerator::mLastSuffixes;

//  ----------------------------------------------------------------------------
CConstRef<CUser_object> sGetUserObjectByType(
    const CUser_object& uo,
    const string& strType )
    //  ----------------------------------------------------------------------------
{
    if ( uo.IsSetType() && uo.GetType().IsStr() && 
        uo.GetType().GetStr() == strType ) {
        return CConstRef<CUser_object>( &uo );
    }
    const CUser_object::TData& fields = uo.GetData();
    for ( CUser_object::TData::const_iterator it = fields.begin(); 
        it != fields.end(); 
        ++it ) {
        const CUser_field& field = **it;
        if ( field.IsSetData() ) {
            const CUser_field::TData& data = field.GetData();
            if ( data.Which() == CUser_field::TData::e_Object ) {
                CConstRef<CUser_object> recur = sGetUserObjectByType( 
                    data.GetObject(), strType );
                if ( recur ) {
                    return recur;
                }
            }
        }
    }
    return CConstRef<CUser_object>();
}

//  ----------------------------------------------------------------------------
CConstRef<CUser_object> sGetUserObjectByType(
    const list<CRef<CUser_object > >& uos,
    const string& strType)
    //  ----------------------------------------------------------------------------
{
    CConstRef<CUser_object> pResult;
    typedef list<CRef<CUser_object > >::const_iterator CIT;
    for (CIT cit=uos.begin(); cit != uos.end(); ++cit) {
        const CUser_object& uo = **cit;
        pResult = sGetUserObjectByType(uo, strType);
        if (pResult) {
            return pResult;
        }
    }
    return CConstRef<CUser_object>();
}

//  ----------------------------------------------------------------------------
bool sIsTrancriptType(
    const CMappedFeat mf)
//  ----------------------------------------------------------------------------
{
    static list<CSeqFeatData::ESubtype> acceptableTranscriptTypes = {
        CSeqFeatData::eSubtype_mRNA,
        CSeqFeatData::eSubtype_otherRNA,
        CSeqFeatData::eSubtype_C_region,
        CSeqFeatData::eSubtype_D_segment,
        CSeqFeatData::eSubtype_J_segment,
        CSeqFeatData::eSubtype_V_segment
    };
   auto itType = std::find(
        acceptableTranscriptTypes.begin(), acceptableTranscriptTypes.end(), 
        mf.GetFeatSubtype());
    return (itType != acceptableTranscriptTypes.end());
}

//  ----------------------------------------------------------------------------
bool sHasAccaptableTranscriptParent(
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    static list<CSeqFeatData::ESubtype> acceptableTranscriptTypes = {
        CSeqFeatData::eSubtype_mRNA,
        CSeqFeatData::eSubtype_otherRNA,
        CSeqFeatData::eSubtype_C_region,
        CSeqFeatData::eSubtype_D_segment,
        CSeqFeatData::eSubtype_J_segment,
        CSeqFeatData::eSubtype_V_segment
    };
    CMappedFeat parent = feature::GetParentFeature(mf);
    if (!parent) {
        return false;
    }
    return sIsTrancriptType(parent);
}    

//  ----------------------------------------------------------------------------
CGtfWriter::CGtfWriter(
    CScope&scope,
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CGff2Writer( scope, ostr, uFlags )
{
    CGtfIdGenerator::Reset(); //may be flag dependent some day
};

//  ----------------------------------------------------------------------------
CGtfWriter::CGtfWriter(
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CGff2Writer( ostr, uFlags )
{
    CGtfIdGenerator::Reset(); //may be flag dependent some day
};

//  ----------------------------------------------------------------------------
CGtfWriter::~CGtfWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_WriteBioseqHandle(
    CBioseq_Handle bsh ) 
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel = SetAnnotSelector();
    const auto& display_range = GetRange();
    CFeat_CI feat_iter(bsh, display_range, sel);
    CGffFeatureContext fc(feat_iter, bsh);

    vector<CMappedFeat> vRoots = fc.FeatTree().GetRootFeatures();
    std::sort(vRoots.begin(), vRoots.end(), CWriteUtil::CompareLocations);
    for (auto pit = vRoots.begin(); pit != vRoots.end(); ++pit) {
        CMappedFeat mRoot = *pit;
        fc.AssignShouldInheritPseudo(false);
        if (!xWriteFeature(fc, mRoot)) {
            // error!
            continue;
        }
        xWriteAllChildren(fc, mRoot);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::WriteHeader()
//  ----------------------------------------------------------------------------
{
    if (!m_bHeaderWritten) {
        m_Os << "#gtf-version 2.2" << '\n';
        m_bHeaderWritten = true;
    }
    return true;
};

//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteRecord( 
    const CGffWriteRecord* pRecord )
//  ----------------------------------------------------------------------------
{
    m_Os << pRecord->StrSeqId() << '\t';
    m_Os << pRecord->StrMethod() << '\t';
    m_Os << pRecord->StrType() << '\t';
    m_Os << pRecord->StrSeqStart() << '\t';
    m_Os << pRecord->StrSeqStop() << '\t';
    m_Os << pRecord->StrScore() << '\t';
    m_Os << pRecord->StrStrand() << '\t';
    m_Os << pRecord->StrPhase() << '\t';

    if ( m_uFlags & fStructibutes ) {
        m_Os << pRecord->StrStructibutes() << '\n';
    }
    else {
        m_Os << pRecord->StrAttributes() << '\n';
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteFeature(
    CGffFeatureContext& context,
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    auto subtype = mf.GetFeatSubtype();
    const auto& feat = mf.GetMappedFeature();
    switch(subtype) {
        default:
            if (mf.GetFeatType() == CSeqFeatData::e_Rna) {
                return xWriteFeatureTranscript(context, mf);
            }
            // GTF is not interested --- ignore
            return true;
        case CSeqFeatData::eSubtype_C_region:
        case CSeqFeatData::eSubtype_D_segment:
        case CSeqFeatData::eSubtype_J_segment:
        case CSeqFeatData::eSubtype_V_segment:
            return xWriteFeatureTranscript(context, mf);
        case CSeqFeatData::eSubtype_gene: 
            return xWriteFeatureGene(context, mf);
        case CSeqFeatData::eSubtype_cdregion:
            return xWriteFeatureCds(context, mf);
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteFeatureGene(
    CGffFeatureContext& context,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (m_uFlags & fNoGeneFeatures) {
        return true;
    }

    CRef<CGtfRecord> pRecord( 
        new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
    if (!xAssignFeature(*pRecord, context, mf)) {
        return false;
    }

    return xWriteRecord(pRecord);
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteFeatureTranscript(
    CGffFeatureContext& context,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGtfRecord> pTranscript(new CGtfRecord(context));
    if (!xAssignFeature(*pTranscript, context, mf)) {
        return false;
    }
    pTranscript->SetType("transcript");
    pTranscript->SetAttribute(
        "gbkey", CSeqFeatData::SubtypeValueToName(
            mf.GetFeatSubtype()));
    xWriteRecord(pTranscript);
    return xWriteFeatureExons(context, mf);
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteFeatureExons(
    CGffFeatureContext& context,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGtfRecord> pMrna( new CGtfRecord( context ) );
    if (!xAssignFeature(*pMrna, context, mf)) {
        return false;
    }
    pMrna->CorrectType( "exon" );

    const CSeq_loc& loc = mf.GetLocation();
    unsigned int uExonNumber = 1;

    CRef< CSeq_loc > pLocMrna( new CSeq_loc( CSeq_loc::e_Mix ) );
    pLocMrna->Add( loc );
    pLocMrna->ChangeToPackedInt();

    if ( pLocMrna->IsPacked_int() && pLocMrna->GetPacked_int().CanGet() ) {
        list< CRef< CSeq_interval > >& sublocs = pLocMrna->SetPacked_int().Set();
        list< CRef< CSeq_interval > >::iterator it;
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            CSeq_interval& subint = **it;
            CRef<CGtfRecord> pExon( 
                new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
            pExon->MakeChildRecord( *pMrna, subint, uExonNumber++ );
            pExon->DropAttributes("gbkey");
            xWriteRecord( pExon );
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xGenerateMissingTranscript(
    CGffFeatureContext& context,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (!xGeneratingMissingTranscripts()) {
        return true;
    }
    if (sHasAccaptableTranscriptParent(mf)) {
        return true;
    }

    CRef<CSeq_feat> pRna(new CSeq_feat);
    pRna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    pRna->SetLocation().Assign(mf.GetLocation());
    pRna->SetLocation().SetPartialStart(false, eExtreme_Positional);
    pRna->SetLocation().SetPartialStop(false, eExtreme_Positional);
    pRna->ResetPartial();

    CScope& scope = mf.GetScope();
    CSeq_annot_Handle sah = mf.GetAnnot();
    CSeq_annot_EditHandle saeh = sah.GetEditHandle();
    saeh.AddFeat(*pRna);
    CMappedFeat tf = scope.GetObjectHandle(*pRna);
    context.FeatTree().AddFeature(tf);

    return xWriteFeatureTranscript(context, tf);
}
    
//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteFeatureCds(
    CGffFeatureContext& context,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    xGenerateMissingTranscript(context, mf);

    CRef<CGtfRecord> pParent( 
        new CGtfRecord( context, (m_uFlags & fNoExonNumbers) ) );
    if (!xAssignFeature(*pParent, context, mf)) {
        return false;
    }

    CRef< CSeq_loc > pLocStartCodon;
    CRef< CSeq_loc > pLocCode;
    CRef< CSeq_loc > pLocStopCodon;
    if (!xSplitCdsLocation( mf, pLocStartCodon, pLocCode, pLocStopCodon)) {
        return false;
    }

	feature::CFeatTree& featTree = context.FeatTree();
	CMappedFeat mRNA = feature::GetBestMrnaForCds(mf, &featTree);

    if (pLocCode) {
        pParent->CorrectType("CDS");
        if (!xWriteFeatureCdsFragments(*pParent, *pLocCode, mRNA)) {
            return false;
        }
    }
    if (pLocStartCodon) {
        pParent->CorrectType("start_codon");
        if (!xWriteFeatureCdsFragments(*pParent, *pLocStartCodon, mRNA)) {
            return false;
        }
    }
    if (pLocStopCodon) {
        pParent->CorrectType("stop_codon");
        if (!xWriteFeatureCdsFragments(*pParent, *pLocStopCodon, mRNA)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteFeatureCdsFragments(
    CGtfRecord& record,
	const CSeq_loc& cdsLoc,
    const CMappedFeat& mRna)
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_interval> > EXONS;
	typedef EXONS::const_iterator EXONIT;

    const EXONS& cdsExons = cdsLoc.GetPacked_int().Get();
	int count = 0;

	if (!mRna) {
		for (EXONIT cdsIt = cdsExons.begin(); cdsIt != cdsExons.end(); ++cdsIt) {
			count++;
			const CSeq_interval& cdsExon = **cdsIt;
			CRef<CGtfRecord> pRecord(new CGtfRecord(record));
			pRecord->MakeChildRecord( record, cdsExon);
			pRecord->SetCdsPhase(cdsExons, cdsLoc.GetStrand());
			xWriteRecord(pRecord.GetPointer());
		}
		return true;
	}

	CRef<CSeq_loc> pRnaLoc(new CSeq_loc(CSeq_loc::e_Mix));
	pRnaLoc->Add(mRna.GetLocation());
	pRnaLoc->ChangeToPackedInt();
	const EXONS& rnaExons = pRnaLoc->GetPacked_int().Get();

    for ( EXONIT cdsIt = cdsExons.begin(); cdsIt != cdsExons.end(); ++cdsIt ) {
		count++;
        const CSeq_interval& cdsExon = **cdsIt;
        unsigned int uExonNumber = 0;
		for (EXONIT rnaIt = rnaExons.begin(); rnaIt != rnaExons.end(); ++rnaIt) {
			const CSeq_interval& rnaExon = **rnaIt;
			uExonNumber++;
            if ( rnaExon.GetFrom() <= cdsExon.GetFrom()  &&  rnaExon.GetTo() >= cdsExon.GetTo() ) {
                 break;
            }
        }
        CRef<CGtfRecord> pRecord(new CGtfRecord(record));
        pRecord->MakeChildRecord( record, cdsExon, uExonNumber );
        pRecord->SetCdsPhase( cdsExons, cdsLoc.GetStrand() );
        xWriteRecord( pRecord.GetPointer() );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xSplitCdsLocation(
    const CMappedFeat& cds,
    CRef< CSeq_loc >& pLocStartCodon,
    CRef< CSeq_loc >& pLocCode,
    CRef< CSeq_loc >& pLocStopCodon ) const
//  ----------------------------------------------------------------------------
{
    //  Note: pLocCode will contain the location of the start codon but not the
    //  stop codon.

    const CSeq_loc& cdsLocation = cds.GetLocation();
    if ( cdsLocation.GetTotalRange().GetLength() < 6 ) {
        return false;
    }
    CSeq_id cdsLocId( cdsLocation.GetId()->AsFastaString() );

    pLocStartCodon.Reset( new CSeq_loc( CSeq_loc::e_Mix ) ); 
    pLocCode.Reset( new CSeq_loc( CSeq_loc::e_Mix ) ); 
    pLocStopCodon.Reset( new CSeq_loc( CSeq_loc::e_Mix ) ); 

    pLocCode->Add(cdsLocation);
    CRef< CSeq_loc > pLocCode2( new CSeq_loc( CSeq_loc::e_Mix ) );
    pLocCode2->Add(cdsLocation);

    CSeq_loc interval;
    interval.SetInt().SetId( cdsLocId );
    interval.SetStrand( cdsLocation.GetStrand() );

    for ( size_t u = 0; u < 3; ++u ) {

        TSeqPos uLowest = pLocCode2->GetTotalRange().GetFrom();
        interval.SetInt().SetFrom( uLowest );
        interval.SetInt().SetTo( uLowest );
        pLocStartCodon = pLocStartCodon->Add( 
            interval, CSeq_loc::fSortAndMerge_All, 0 );
        pLocCode2 = pLocCode2->Subtract( 
            interval, CSeq_loc::fMerge_Contained, 0, 0 );    

        TSeqPos uHighest = pLocCode->GetTotalRange().GetTo();
        interval.SetInt().SetFrom( uHighest );
        interval.SetInt().SetTo( uHighest );
        pLocStopCodon = pLocStopCodon->Add( 
            interval, CSeq_loc::fSortAndMerge_All, 0 );
        pLocCode = pLocCode->Subtract( 
            interval, CSeq_loc::fMerge_Contained, 0, 0 );    
    }

    if ( cdsLocation.GetStrand() == eNa_strand_minus ) {
        pLocCode = pLocCode2;
        pLocCode2 = pLocStartCodon;
        pLocStartCodon = pLocStopCodon;
        pLocStopCodon = pLocCode2;
    }

    pLocStartCodon->ChangeToPackedInt();
    pLocCode->ChangeToPackedInt();
    pLocStopCodon->ChangeToPackedInt();

    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureType(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    record.SetType("region");

    if (mf.IsSetQual()) {
        const auto& quals = mf.GetQual();
        auto it = quals.begin();
        for( ; it != quals.end(); ++it) {
            if (!(*it)->CanGetQual() || !(*it)->CanGetVal()) {
                continue;
            }
            if ((*it)->GetQual() == "standard_name") {
                record.SetType((*it)->GetVal());
                return true;
            }
        }
    }
    switch ( mf.GetFeatSubtype() ) {
    default:
        break;
    case CSeq_feat::TData::eSubtype_cdregion:
        record.SetType("CDS");
        break;
    case CSeq_feat::TData::eSubtype_exon:
        record.SetType("exon");
        break;
    case CSeq_feat::TData::eSubtype_misc_RNA:
        record.SetType("transcript");
        break;
    case CSeq_feat::TData::eSubtype_gene:
        record.SetType("gene");
        break;
    case CSeq_feat::TData::eSubtype_mRNA:
        record.SetType("mRNA");
        break;
    case CSeq_feat::TData::eSubtype_scRNA:
        record.SetType("scRNA");
        break;
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureMethod(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    record.SetMethod(".");

    if (mf.IsSetQual()) {
        const auto& quals = mf.GetQual();
        auto it = quals.begin();
        for (; it != quals.end(); ++it) {
            if (!(*it)->CanGetQual() || !(*it)->CanGetVal()) {
                continue;
            }
            if ((*it)->GetQual() == "gff_source") {
                record.SetMethod((*it)->GetVal());
                return true;
            }
        }
    }

    if (mf.IsSetExt()) {
        CConstRef<CUser_object> model_evidence = sGetUserObjectByType( 
            mf.GetExt(), "ModelEvidence");
        if (model_evidence) {
            string strMethod;
            if (model_evidence->HasField("Method") ) {
                record.SetMethod(
                    model_evidence->GetField("Method").GetData().GetStr());
                return true;
            }
        }
    }

    if (mf.IsSetExts()) {
        CConstRef<CUser_object> model_evidence = sGetUserObjectByType( 
            mf.GetExts(), "ModelEvidence");
        if (model_evidence) {
            string strMethod;
            if (model_evidence->HasField("Method")) {
                record.SetMethod(
                    model_evidence->GetField("Method").GetData().GetStr());
                return true;
            }
        }
    }

    CScope& scope = mf.GetScope();
    CSeq_id_Handle idh = sequence::GetIdHandle(mf.GetLocation(), &mf.GetScope());
    string typeFromId;
    CWriteUtil::GetIdType(scope.GetBioseqHandle(idh), typeFromId);
    if (!typeFromId.empty()) {
        record.SetMethod(typeFromId);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributesFormatSpecific(
    CGffFeatureRecord& rec,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    CGtfRecord& record = dynamic_cast<CGtfRecord&>(rec);
    return (
        xAssignFeatureAttributeGeneId(record, fc, mf)  &&
        xAssignFeatureAttributeTranscriptId(record, fc, mf)  &&
        xAssignFeatureAttributeTranscriptBiotype(record, fc, mf));
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributesQualifiers(
    CGffFeatureRecord& rec,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    const vector<string> specialCases = {
        "ID",
        "Parent",
        "gff_type",
        "transcript_id",
        "gene_id", 
    };

    CGtfRecord& record = dynamic_cast<CGtfRecord&>(rec);
    auto quals = mf.GetQual();
    for (auto qual: quals) {
        if (!qual->IsSetQual()  ||  !qual->IsSetVal()) {
            continue;
        }
        auto specialCase = std::find(
            specialCases.begin(), specialCases.end(), qual->GetQual());
        if (specialCase != specialCases.end()) {
            continue;
        }
        record.AddAttribute(qual->GetQual(), qual->GetVal());
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributeDbxref(
    CGffFeatureRecord& rec,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    if (!mf.IsSetDbxref()) {
        return true;
    }

    CGtfRecord& record = dynamic_cast<CGtfRecord&>(rec);
    for (const auto& dbxref: mf.GetDbxref()) {
        string gffDbxref;
        if (dbxref->IsSetDb()) {
            gffDbxref += dbxref->GetDb();
            gffDbxref += ":";
        }
        if (dbxref->IsSetTag()) {
            if (dbxref->GetTag().IsId()) {
                gffDbxref += NStr::UIntToString(dbxref->GetTag().GetId());
            }
            else if (dbxref->GetTag().IsStr()) {
                gffDbxref += dbxref->GetTag().GetStr();
            }
        }
        record.AddAttribute("db_xref", gffDbxref);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributeNote(
    CGffFeatureRecord& rec,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    if (!mf.IsSetComment()) {
        return true;
    }
    CGtfRecord& record = dynamic_cast<CGtfRecord&>(rec);
    record.SetAttribute("note", mf.GetComment());
    return true;
}

//  ----------------------------------------------------------------------------
string CGtfWriter::xGenericGeneId(
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    static unsigned int uId = 1;
    string strGeneId = string( "unassigned_gene_" ) + 
        NStr::UIntToString(uId);
    if (mf.GetData().GetSubtype() == CSeq_feat::TData::eSubtype_gene) {
        uId++;
    }
    return strGeneId;
}

//  ----------------------------------------------------------------------------
string CGtfWriter::xGenericTranscriptId(
    const CMappedFeat& mf)
    //  ----------------------------------------------------------------------------
{
    return CGtfIdGenerator::NextId("unassigned_transcript");
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributeTranscriptBiotype(
    CGtfRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    static list<CSeqFeatData::ESubtype> nonRnaTranscripts = {
        CSeqFeatData::eSubtype_mRNA,
        CSeqFeatData::eSubtype_otherRNA,
        CSeqFeatData::eSubtype_C_region,
        CSeqFeatData::eSubtype_D_segment,
        CSeqFeatData::eSubtype_J_segment,
        CSeqFeatData::eSubtype_V_segment
    };
    auto featSubtype = mf.GetFeatSubtype();
    if (!mf.GetData().IsRna()) {
        auto it = std::find(
            nonRnaTranscripts.begin(), nonRnaTranscripts.end(), featSubtype);
        if (it == nonRnaTranscripts.end()) {
            return true;
        }
    }
    const auto& feature = mf.GetOriginalFeature();
    string so_type;
    if (!CSoMap::FeatureToSoType(feature, so_type)) {
        return true;
    }
    
    record.SetAttribute("transcript_biotype", so_type);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributeTranscriptId(
    CGtfRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    CMappedFeat mrnaFeat;
    auto featSubtype = mf.GetFeatSubtype();
    switch(featSubtype) {
        default:
            if (mf.GetFeatType() == CSeqFeatData::e_Rna) {
                mrnaFeat = mf;
                break;
            }   
        case CSeq_feat::TData::eSubtype_mRNA:
        case CSeq_feat::TData::eSubtype_C_region:
        case CSeq_feat::TData::eSubtype_D_segment:
        case CSeq_feat::TData::eSubtype_J_segment:
        case CSeq_feat::TData::eSubtype_V_segment:
            mrnaFeat = mf;
            break;
        case CSeq_feat::TData::eSubtype_cdregion:
            if (sHasAccaptableTranscriptParent(mf)) {
                mrnaFeat = feature::GetParentFeature(mf);
            }
            break;
        case CSeq_feat::TData::eSubtype_gene:
            return true;
    }
    if (!mrnaFeat) {
        record.SetTranscriptId(xGenericTranscriptId(mf));
        return true;
    }

    using RNA_ID = string;
    using RNA_MAP = map<CMappedFeat, RNA_ID>;
    using RNA_IDS = list<RNA_ID>;

    static RNA_MAP rnaMap;
    const auto rnaIt = rnaMap.find(mrnaFeat);
    if (rnaMap.end() != rnaIt) {
        record.SetTranscriptId(rnaIt->second);
        return true;
    }

    static RNA_IDS usedRnaIds;
    RNA_ID rnaId = mf.GetNamedQual("transcript_id");
    if (rnaId.empty()  &&  mf.IsSetProduct()) {
        if (!CGenbankIdResolve::Get().GetBestId(
                mf.GetProductId(), mf.GetScope(), rnaId)) {
            rnaId.clear();
        }
    }
    if (rnaId.empty()) {
        rnaId = mf.GetNamedQual("orig_transcript_id");
    }

    if (rnaId.empty()) {
        rnaId = xGenericTranscriptId(mf);
        //we know the ID is going to be unique if we get it this way
        // not point in further checking
        usedRnaIds.push_back(rnaId);
        rnaMap[mf] = rnaId;
        record.SetTranscriptId(rnaId);
        return true;
    }
    //uniquify the ID we came up with
    auto cit = find(usedRnaIds.begin(), usedRnaIds.end(), rnaId);
    if (usedRnaIds.end() == cit) {
        usedRnaIds.push_back(rnaId);
        rnaMap[mf] = rnaId;
        record.SetTranscriptId(rnaId);
        return true;
    }     
    unsigned int suffix = 1;
    rnaId += "_";
    while (true) {
        auto qualifiedId = rnaId + NStr::UIntToString(suffix);   
        cit = find(usedRnaIds.begin(), usedRnaIds.end(), qualifiedId);
        if (usedRnaIds.end() == cit) {
            usedRnaIds.push_back(qualifiedId);
            rnaMap[mf] = qualifiedId;
            record.SetTranscriptId(qualifiedId);
            return true;
        }
        ++suffix;
    }   
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeatureAttributeGeneId(
    CGtfRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    CMappedFeat geneFeat = mf;
    if (mf.GetFeatSubtype() != CSeq_feat::TData::eSubtype_gene) {
        geneFeat = feature::GetBestGeneForFeat(mf, &fc.FeatTree());
    }
    if (!geneFeat) {
        const auto& geneIdQual = mf.GetNamedQual("gene_id");
        record.SetGeneId(geneIdQual); // empty most times but still best effort
        return true;
    }

    using GENE_ID = string;
    using GENE_MAP = map<CMappedFeat, GENE_ID>;
    using GENE_IDS = list<GENE_ID>;

    static GENE_IDS usedGeneIds;
    static GENE_MAP geneMap;

    auto  geneIt = geneMap.find(geneFeat);
    if (geneMap.end() != geneIt) {
        record.SetGeneId(geneIt->second);
        return true;
    }

    GENE_ID geneId;
    const auto& geneRef = mf.GetData().GetGene();

    geneId = mf.GetNamedQual("gene_id");
    if (geneId.empty()  &&  geneRef.IsSetLocus_tag()) {
        geneId = geneRef.GetLocus_tag();
    }
    if (geneId.empty() &&  geneRef.IsSetLocus()) {
        geneId = geneRef.GetLocus();
    }
    if (geneId.empty() &&  geneRef.IsSetSyn() ) {
        geneId = geneRef.GetSyn().front();
    }
    if (geneId.empty()) {
        geneId = xGenericGeneId(mf); 
        //we know the ID is going to be unique if we get it this way
        // not point in further checking
        usedGeneIds.push_back(geneId);
        geneMap[mf] = geneId;
        record.SetGeneId(geneId);
        return true;
    }
    auto cit = find(usedGeneIds.begin(), usedGeneIds.end(), geneId);
    if (usedGeneIds.end() == cit) {
        usedGeneIds.push_back(geneId);
        geneMap[mf] = geneId;
        record.SetGeneId(geneId);
        return true;
    }
    unsigned int suffix = 1;
    geneId += "_";
    while (true) {
        GENE_ID qualifiedGeneId = geneId + NStr::UIntToString(suffix);
        cit = find(usedGeneIds.begin(), usedGeneIds.end(), qualifiedGeneId);
        if (usedGeneIds.end() == cit) {
            usedGeneIds.push_back(qualifiedGeneId);
            geneMap[mf] = qualifiedGeneId;
            record.SetGeneId(qualifiedGeneId);
            return true;
        }
        ++suffix;
    }
    return true;
}

END_NCBI_SCOPE
