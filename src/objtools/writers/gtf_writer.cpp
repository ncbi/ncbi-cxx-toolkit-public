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

    //const auto& feat = mf.GetMappedFeature();
    //if (subtype == CSeqFeatData::eSubtype_tRNA) {
    //    auto from = feat.GetLocation().GetStart(ESeqLocExtremes::eExtreme_Biological);
    //    if (from == 48738) {
    //        cerr << "";
    //    }
    //}

    switch(subtype) {
        default:
            if (mf.GetFeatType() == CSeqFeatData::e_Rna) {
                return xWriteRecordsTranscript(context, mf);
            }
            // GTF is not interested --- ignore
            return true;
        case CSeqFeatData::eSubtype_C_region:
        case CSeqFeatData::eSubtype_D_segment:
        case CSeqFeatData::eSubtype_J_segment:
        case CSeqFeatData::eSubtype_V_segment:
            return xWriteRecordsTranscript(context, mf);
        case CSeqFeatData::eSubtype_gene: 
            return xWriteRecordsGene(context, mf);
        case CSeqFeatData::eSubtype_cdregion:
            return xWriteRecordsCds(context, mf);
    }
    return false;
}


//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteRecordsGene(
    CGffFeatureContext& context,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (m_uFlags & fNoGeneFeatures) {
        return true;
    }

    list<CRef<CGtfRecord>> records;
    if (!xAssignFeaturesGene(records, context, mf)) {
        return false;
    }

    for (const auto record: records) {
        if (!xWriteRecord(record)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteRecordsCds(
    CGffFeatureContext& context,
    const CMappedFeat& mf,
    const string& transcriptIdPreAssigned)
//  ----------------------------------------------------------------------------
{
    string transcriptId(transcriptIdPreAssigned);
    //if (transcriptId.empty()) {
    //    transcriptId = CGtfIdGenerator::NextId("unassigned_transcript");
    //}

    CMappedFeat tf = xGenerateMissingTranscript(context, mf);
    if (tf) {
        if (!xWriteRecordsTranscript(context, tf, transcriptId)) {
            return false;
        }
    }

    list<CRef<CGtfRecord>> records;
    if (!xAssignFeaturesCds(records, context, mf, transcriptId)) {
        return false;
    }

    for (const auto record: records) {
        if (!xWriteRecord(record)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteRecordsTranscript(
    CGffFeatureContext& context,
    const CMappedFeat& mf,
    const string& transcriptIdPreAssigned )
//  ----------------------------------------------------------------------------
{
    string transcriptId(transcriptIdPreAssigned);
    //if (transcriptId.empty()) {
    //    transcriptId = CGtfIdGenerator::NextId("unassigned_transcript");
    //}

    list<CRef<CGtfRecord>> records;
    if (!xAssignFeaturesTranscript(records, context, mf, transcriptId)) {
        return false;
    }

    for (const auto record: records) {
        if (!xWriteRecord(record)) {
            return false;
        }
    }
    return xWriteFeatureExons(context, mf, transcriptId);
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeaturesGene(
    list<CRef<CGtfRecord>>& recordList,
    CGffFeatureContext& context,
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    const auto& mfLoc = mf.GetLocation();
    
    CSeq_loc mfLocAsPackedInt;
    mfLocAsPackedInt.Assign(mfLoc);
    mfLocAsPackedInt.ChangeToPackedInt();

    const auto& sublocs = mfLocAsPackedInt.GetPacked_int().Get();
    bool needsPartNumbers = (sublocs.size() > 1);
    unsigned int partNum = 1;
    for ( auto it = sublocs.begin(); it != sublocs.end(); it++ ) {
        const CSeq_interval& intv = **it;
        CRef<CGtfRecord> pRecord( 
            new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
        if (!xAssignFeature(*pRecord, context, mf)) {
            return false;
        }
        pRecord->SetEndpoints(intv.GetFrom(), intv.GetTo(), 
            (intv.IsSetStrand() ? intv.GetStrand() : eNa_strand_plus));
        if (needsPartNumbers) {
            pRecord->SetAttribute("part", NStr::NumericToString(partNum++));
        }
        recordList.push_back(pRecord);
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeaturesTranscript(
    list<CRef<CGtfRecord>>& recordList,
    CGffFeatureContext& context,
    const CMappedFeat& mf,
    const string& transcriptId)
//  ----------------------------------------------------------------------------
{
    const auto& mfLoc = mf.GetLocation();
    
    CSeq_loc mfLocAsPackedInt;
    mfLocAsPackedInt.Assign(mfLoc);
    mfLocAsPackedInt.ChangeToPackedInt();

    const auto& sublocs = mfLocAsPackedInt.GetPacked_int().Get();
    CRef<CGtfRecord> pRecord(
        new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
    if (!transcriptId.empty()) {
        pRecord->SetTranscriptId(transcriptId);
    }
    if (!xAssignFeature(*pRecord, context, mf)) {
        return false;
    }
    TSeqPos lastFrom = sublocs.front()->GetFrom();
    TSeqPos lastTo = sublocs.front()->GetTo();
    auto it = sublocs.begin();
    for ( it++; it != sublocs.end(); it++ ) {
        const CSeq_interval& intv = **it;

        if (intv.IsSetStrand()  &&  intv.GetStrand() == eNa_strand_minus) {
            if (intv.GetTo() <= lastFrom) {
                lastFrom = intv.GetFrom();
            }
            else {
                pRecord->SetEndpoints(lastFrom, lastTo, mfLoc.GetStrand());
                recordList.push_back(pRecord);
                pRecord.Reset(new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
                if (!transcriptId.empty()) {
                    pRecord->SetTranscriptId(transcriptId);
                }
                if (!xAssignFeature(*pRecord, context, mf)) {
                    return false;
                }
                lastFrom = intv.GetFrom();
                lastTo = intv.GetTo();
            }
        }
        else {
            if (intv.GetFrom() >= lastTo) {
                lastTo = intv.GetTo();
            }
            else { //wrapping back to 0
                pRecord->SetEndpoints(lastFrom, lastTo, mfLoc.GetStrand());
                recordList.push_back(pRecord);
                pRecord.Reset(new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
                if (!transcriptId.empty()) {
                    pRecord->SetTranscriptId(transcriptId);
                }
                if (!xAssignFeature(*pRecord, context, mf)) {
                    return false;
                }
                lastFrom = 0;
                lastTo = intv.GetTo();
            }
        }
    }
    pRecord->SetEndpoints(lastFrom, lastTo, mfLoc.GetStrand());
    recordList.push_back(pRecord);

    bool needPartNumbers = (recordList.size() > 1);
    unsigned int partNum = 1;
    for (auto& record: recordList) {
        if (needPartNumbers) {
            record->SetAttribute("part", NStr::NumericToString(partNum++));
        }
        record->SetType("transcript");
        record->SetAttribute(
            "gbkey", CSeqFeatData::SubtypeValueToName(mf.GetFeatSubtype()));
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool CdsNeedsExonNumbers(
    const CMappedFeat& cdsMf)
//  ---------------------------------------------------------------------------
{
    CSeq_loc cdsLocation;
    cdsLocation.Assign(cdsMf.GetLocation());
    cdsLocation.ChangeToPackedInt();
    if (cdsLocation.GetPacked_int().Get().size() > 1) {
        return true;
    }

    CMappedFeat rna = feature::GetBestMrnaForCds(cdsMf);
    if (!rna) {
        return false;
    }
    CSeq_loc rnaLocation;
    rnaLocation.Assign(rna.GetLocation());
    rnaLocation.ChangeToPackedInt();
    return (rnaLocation.GetPacked_int().Get().size() > 1);
}

//  ---------------------------------------------------------------------------
unsigned int FindExonNumberForCdsPart(
    const CMappedFeat& mf,
    const CGtfRecord& record)
//  ---------------------------------------------------------------------------
{
    CMappedFeat rna = feature::GetBestMrnaForCds(mf);
    if (!rna) {
        return 0;
    }
    CSeq_loc rnaLocation;
    rnaLocation.Assign(rna.GetLocation());
    rnaLocation.ChangeToPackedInt();
    unsigned int exonNumber = 1;
    auto recordFrom = record.SeqStart();
    auto recordTo = record.SeqStop();
    for (const auto pExonLocation: rnaLocation.GetPacked_int().Get()) {
        auto exonFrom = pExonLocation->GetFrom();
        auto exonTo = pExonLocation->GetTo();
        if (recordFrom >= exonFrom  &&  recordTo <= exonTo) {
            return exonNumber;
        }
        ++exonNumber;
    }
    return 0; 
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::xAssignFeaturesCds(
    list<CRef<CGtfRecord>>& recordList,
    CGffFeatureContext& context,
    const CMappedFeat& mf,
    const string& transcriptId)
//  ----------------------------------------------------------------------------
{
    const auto& mfLoc = mf.GetLocation();
    auto mfStrand = mfLoc.IsSetStrand() ? mfLoc.GetStrand() : eNa_strand_plus;

    CSeq_loc mfLocAsPackedInt;
    mfLocAsPackedInt.Assign(mfLoc);
    mfLocAsPackedInt.ChangeToPackedInt();
    const auto& sublocs = mfLocAsPackedInt.GetPacked_int().Get();

    bool needsPartNumbers = (sublocs.size() > 1);
    unsigned int partNum = 1;
    for ( auto it = sublocs.begin(); it != sublocs.end(); it++ ) {
        const CSeq_interval& intv = **it;
        CRef<CGtfRecord> pRecord( 
            new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
        if (!xAssignFeature(*pRecord, context, mf)) {
            return false;
        }
        pRecord->SetEndpoints(intv.GetFrom(), intv.GetTo(), mfStrand);
        if (needsPartNumbers) {
            pRecord->SetAttribute("part", NStr::NumericToString(partNum++));
        }
        if (!transcriptId.empty()) {
            pRecord->SetTranscriptId(transcriptId);
        }
        pRecord->SetCdsPhase(sublocs, mfStrand);
        recordList.push_back(pRecord);
    }
    // correct endpoint of last record:
    auto pLastRecord = recordList.back();
    if (mfLoc.GetStrand() == eNa_strand_minus) {
        pLastRecord->SetEndpoints(
            pLastRecord->SeqStart() + 3, pLastRecord->SeqStop(), mfStrand);
    }
    else {
        pLastRecord->SetEndpoints(
            pLastRecord->SeqStart(), pLastRecord->SeqStop() - 3, mfStrand);
    }

    // generate start codon:
    const CSeq_interval& first = *sublocs.front();
    if (!first.IsPartialStart(eExtreme_Biological)) {
        CRef<CGtfRecord> pRecord( 
            new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
        if (!xAssignFeature(*pRecord, context, mf)) {
            return false;
        }
        pRecord->SetType("start_codon");
        if (mfStrand == eNa_strand_minus) {
            pRecord->SetEndpoints(first.GetTo() - 2, first.GetTo(),  mfStrand);
        }
        else {
            pRecord->SetEndpoints(first.GetFrom(), first.GetFrom() + 2, mfStrand);
        }
        if (!transcriptId.empty()) {
            pRecord->SetTranscriptId(transcriptId);
        }
        pRecord->SetCdsPhase(sublocs, mfStrand);
        recordList.push_back(pRecord);
    }

    // generate stop codon:
    const CSeq_interval& last = *sublocs.back();
    if (!last.IsPartialStop(eExtreme_Biological)) {
        CRef<CGtfRecord> pRecord( 
            new CGtfRecord(context, (m_uFlags & fNoExonNumbers)));
        if (!xAssignFeature(*pRecord, context, mf)) {
            return false;
        }
        pRecord->SetType("stop_codon");
        if (mfStrand == eNa_strand_minus) {
            pRecord->SetEndpoints(last.GetFrom(), last.GetFrom() + 2, mfStrand); 
        }
        else {
            pRecord->SetEndpoints(last.GetTo() - 2, last.GetTo(), mfStrand);
        }
        if (!transcriptId.empty()) {
            pRecord->SetTranscriptId(transcriptId);
        }
        pRecord->SetCdsPhase(sublocs, mfStrand);
        recordList.push_back(pRecord);
    }

    // assign exon numbers:
    bool needsExonNumbers = CdsNeedsExonNumbers(mf);
    if (needsExonNumbers) {
        unsigned int assignedExon = 1;
        for (auto& pRecord: recordList) {
            unsigned int impliedExon = FindExonNumberForCdsPart(mf, *pRecord);
            if (impliedExon != 0) {
                pRecord->SetExonNumber(impliedExon);
                continue;
            }
            // if we can't compute the exon number then go with the most
            //  reasonable assignment
            if (pRecord->StrType() == "start_codon") {
                pRecord->SetExonNumber(1);
                continue;
            }
            if (pRecord->StrType() == "stop_codon") {
                pRecord->SetExonNumber(sublocs.size());
                continue;
            }
            pRecord->SetExonNumber(assignedExon++);
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGtfWriter::xWriteFeatureExons(
    CGffFeatureContext& context,
    const CMappedFeat& mf,
    const string& transcriptId)
//  ----------------------------------------------------------------------------
{
    CRef<CGtfRecord> pMrna( new CGtfRecord( context ) );
    if (!transcriptId.empty()) {
        pMrna->SetTranscriptId(transcriptId);
    }
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
bool CGtfWriter::xWriteFeatureCds(
    CGffFeatureContext& context,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    CMappedFeat tf = xGenerateMissingTranscript(context, mf);
    string transcriptId;
    if (tf) {
        transcriptId = CGtfIdGenerator::NextId("unassigned_transcript");
        if (!xWriteRecordsTranscript(context, tf, transcriptId)) {
            return false;
        }
    }

    CRef<CGtfRecord> pParent( 
        new CGtfRecord( context, (m_uFlags & fNoExonNumbers) ) );
    if (!transcriptId.empty()) {
        pParent->SetTranscriptId(transcriptId);
    }
    if (!xAssignFeature(*pParent, context, mf)) {
        return false;
    }

    CRef< CSeq_loc > pLocStartCodon;
    CRef< CSeq_loc > pLocCode;
    CRef< CSeq_loc > pLocStopCodon;
    if (!xSplitCdsLocation( mf, pLocStartCodon, pLocCode, pLocStopCodon)) {
        return false;
    }

    CMappedFeat mRna = tf;
    if (!mRna) {
	    feature::CFeatTree& featTree = context.FeatTree();
	    mRna = feature::GetBestMrnaForCds(mf, &featTree);
    }

    if (pLocCode) {
        pParent->CorrectType("CDS");
        if (!xWriteFeatureCdsFragments(*pParent, *pLocCode, mRna)) {
            return false;
        }
    }
    if (pLocStartCodon) {
        pParent->CorrectType("start_codon");
        if (!xWriteFeatureCdsFragments(*pParent, *pLocStartCodon, mRna)) {
            return false;
        }
    }
    if (pLocStopCodon) {
        pParent->CorrectType("stop_codon");
        if (!xWriteFeatureCdsFragments(*pParent, *pLocStopCodon, mRna)) {
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
			pRecord->MakeChildRecord( record, cdsExon, count);
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
        pRecord->MakeChildRecord( record, cdsExon, count );
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

        auto lowest = pLocCode2->GetStart(eExtreme_Positional);
        interval.SetInt().SetFrom(lowest);
        interval.SetInt().SetTo(lowest);
        pLocStartCodon = pLocStartCodon->Add( 
            interval, CSeq_loc::fSortAndMerge_All, 0 );
        pLocCode2 = pLocCode2->Subtract( 
            interval, CSeq_loc::fMerge_Contained, 0, 0 );    

        auto highest = pLocCode->GetStop(eExtreme_Positional);
        interval.SetInt().SetFrom(highest);
        interval.SetInt().SetTo(highest);
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
    using FEAT_ID = string;
    using FEAT_MAP = map<CMappedFeat, FEAT_ID>;
    using FEAT_IDS = list<FEAT_ID>;

    static FEAT_MAP featMap;
    static FEAT_IDS usedFeatIds;

    if (!record.TranscriptId().empty()) {
        return true; //special case hence already assigned
    }

    const auto mfIt = featMap.find(mf);
    if (featMap.end() != mfIt) {
        record.SetTranscriptId(mfIt->second);
        return true;
    }

    CMappedFeat mrnaFeat;
    auto featSubtype = mf.GetFeatSubtype();
    switch(featSubtype) {
        default:
            mrnaFeat = mf;
            break;
        case CSeq_feat::TData::eSubtype_mRNA:
        case CSeq_feat::TData::eSubtype_C_region:
        case CSeq_feat::TData::eSubtype_D_segment:
        case CSeq_feat::TData::eSubtype_J_segment:
        case CSeq_feat::TData::eSubtype_V_segment:
            mrnaFeat = mf;
            break;
        case CSeq_feat::TData::eSubtype_cdregion:
            if (HasAccaptableTranscriptParent(fc, mf)) {
                mrnaFeat = feature::GetParentFeature(mf);
            }
            else {
                mrnaFeat = mf; 
                //there must be one somewhere, and that's the closest we can
                // get to it.
            }
            break;
        case CSeq_feat::TData::eSubtype_gene:
            return true;
    }

    if (!mrnaFeat) {
        record.SetTranscriptId(xGenericTranscriptId(mf));
        return true;
    }

    const auto featIt = featMap.find(mrnaFeat);
    if (featMap.end() != featIt) {
        record.SetTranscriptId(featIt->second);
        return true;
    }

    FEAT_ID featId = mf.GetNamedQual("transcript_id");
    if (featId.empty()  &&  mf.GetData().IsRna()  &&  mf.IsSetProduct()) {
        if (!CGenbankIdResolve::Get().GetBestId(
                mf.GetProductId(), mf.GetScope(), featId)) {
            featId.clear();
        }
    }
    if (featId.empty()) {
        featId = mf.GetNamedQual("orig_transcript_id");
    }

    if (featId.empty()) {
        featId = xGenericTranscriptId(mf);
        //we know the ID is going to be unique if we get it this way
        // not point in further checking
        usedFeatIds.push_back(featId);
        featMap[mf] = featId;
        record.SetTranscriptId(featId);
        return true;
    }
    //uniquify the ID we came up with
    auto cit = find(usedFeatIds.begin(), usedFeatIds.end(), featId);
    if (usedFeatIds.end() == cit) {
        usedFeatIds.push_back(featId);
        featMap[mf] = featId;
        record.SetTranscriptId(featId);
        return true;
    }     
    unsigned int suffix = 1;
    featId += "_";
    while (true) {
        auto qualifiedId = featId + NStr::UIntToString(suffix);   
        cit = find(usedFeatIds.begin(), usedFeatIds.end(), qualifiedId);
        if (usedFeatIds.end() == cit) {
            usedFeatIds.push_back(qualifiedId);
            featMap[mf] = qualifiedId;
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
    if (!record.GeneId().empty()) {
        return true;
    }
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
