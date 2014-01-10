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
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/sofa_type.hpp>
#include <objects/seq/sofa_map.hpp>

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
bool sIsTransspliced(const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    return (mf.IsSetExcept_text()  &&  mf.GetExcept_text() == "trans-splicing");
}

//  ----------------------------------------------------------------------------
bool sGetTranssplicedEndpoints(
    const CSeq_loc& loc, 
    unsigned int& inPoint,
    unsigned int& outPoint)
//  start determined by the minimum start of any sub interval
//  stop determined by the maximum stop of any sub interval
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_interval> >::const_iterator CIT;

    if (!loc.IsPacked_int()) {
        return false;
    }
    const CPacked_seqint& packedInt = loc.GetPacked_int();
    inPoint = packedInt.GetStart(eExtreme_Biological);
    outPoint = packedInt.GetStop(eExtreme_Biological);
    const list<CRef<CSeq_interval> >& intvs = packedInt.Get();
    for (CIT cit = intvs.begin(); cit != intvs.end(); cit++) {
        const CSeq_interval& intv = **cit;
        if (intv.GetFrom() < inPoint) {
            inPoint = intv.GetFrom();
        }
        if (intv.GetTo() > outPoint) {
            outPoint = intv.GetTo();
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool sGetTranssplicedOutPoint(const CSeq_loc& loc, unsigned int& outPoint)
//  stop determined by the maximum stop of any sub interval
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_interval> >::const_iterator CIT;

    if (!loc.IsPacked_int()) {
        return false;
    }
    const CPacked_seqint& packedInt = loc.GetPacked_int();
    outPoint = packedInt.GetStop(eExtreme_Biological);
    const list<CRef<CSeq_interval> >& intvs = packedInt.Get();
    for (CIT cit = intvs.begin(); cit != intvs.end(); cit++) {
        const CSeq_interval& intv = **cit;
        if (intv.GetTo() > outPoint) {
            outPoint = intv.GetTo();
        }
    }
    return true;
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
    if ( ! x_WriteAssemblyInfo( strAssName, strAssAcc ) ) {
        return false;
    }
    if ( ! xWriteAlign( align ) ) {
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
            if ( ! xWriteAlign( *it ) ) {
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
        if ( ! xWriteFeature( fc, *feat_iter ) ) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteAlign( 
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
            return xWriteAlignDenseg( align );
        case CSeq_align::TSegs::e_Spliced:
            return xWriteAlignSpliced( align );
        case CSeq_align::TSegs::e_Disc:
            return xWriteAlignDisc( align );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteAlignDisc( 
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
        if (!xWriteAlign(*pA)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteAlignSpliced( 
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
        xWriteAlignment(record);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteAlignDenseg( 
    const CSeq_align& align)
//  ----------------------------------------------------------------------------
{
    CRef<CDense_seg> dsFilled = align.GetSegs().GetDenseg().FillUnaligned();
    CAlnMap alnMap(*dsFilled);

    size_t numRows = alnMap.GetNumRows();
    for (size_t sourceRow = 1; sourceRow < numRows; ++sourceRow) {

        CGffDenseSegRecord record(m_uFlags, m_uRecordId);
        if (!record.Initialize(*m_pScope, align, alnMap, sourceRow)) {
            return false;
        }
        xWriteAlignment(record);
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
bool CGff3Writer::xWriteSequenceHeader(
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
bool CGff3Writer::xWriteSequenceHeader(
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
    if (!xWriteSequenceHeader(bsh) ) {
        return false;
    }

    SAnnotSelector sel = GetAnnotSelector();
    CFeat_CI feat_iter(bsh, sel);
    CGffFeatureContext fc(feat_iter, bsh);

    if (!xWriteSource(fc)) {
        return false;
    }
    for ( ; feat_iter; ++feat_iter) {
        if (!xWriteFeature(fc, *feat_iter)) {
            return false;
        }
    }
    for (CAlign_CI align_it(bsh, sel);  align_it;  ++ align_it) {
        xWriteAlign(*align_it);
        m_uRecordId++;
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteSource(
    CGffFeatureContext& fc )
//  ----------------------------------------------------------------------------
{
    CBioseq_Handle bsh = fc.BioseqHandle();
    CSeqdesc_CI sdi(bsh.GetParentEntry(), CSeqdesc::e_Source, 0);
    if (!sdi) {
        return true; 
    }
    //fix me!
    //CRef<CGff3SourceRecord> pSource( 
    //    new CGff3SourceRecord( 
    //        fc, 
    //        string("id") + NStr::UIntToString(m_uPendingGenericId++)));
    //pSource->AssignData(*sdi);
    //x_WriteRecord(pSource);
    CRef<CGffFeatureRecord> pSource(new CGffFeatureRecord(xNextGenericId()));
    if (!xAssignSource(*pSource, fc, bsh)) {
        return false;
    }
    return xWriteFeatureRecord(*pSource);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeature(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    //CSeqFeatData::ESubtype s = mf.GetFeatSubtype();
    try {
        switch( mf.GetFeatSubtype() ) {
            default:
                if (mf.GetFeatType() == CSeqFeatData::e_Rna) {
                    return xWriteFeatureRna( fc, mf );
                }
                return xWriteFeatureGeneric( fc, mf );
            case CSeqFeatData::eSubtype_gene: 
                return xWriteFeatureGene( fc, mf );
            case CSeqFeatData::eSubtype_cdregion:
                return xWriteFeatureCds( fc, mf );
            case CSeqFeatData::eSubtype_tRNA:
                return xWriteFeatureTrna( fc, mf );

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
bool CGff3Writer::xWriteFeatureTrna(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGffFeatureRecord> pParent( new CGffFeatureRecord(xNextTrnaId()) );
    if (!this->xAssignFeature(*pParent, fc, mf)) {
        return false;
    }
    xAssignFeatureAttributeParentGene(*pParent, fc, mf);
    TSeqPos seqlength = 0;
    if(fc.BioseqHandle() && fc.BioseqHandle().CanGetInst())
        seqlength = fc.BioseqHandle().GetInst().GetLength();
    if (!xWriteFeatureRecords( *pParent, mf.GetLocation(), seqlength ) ) {
        return false;
    }

    CRef<CGffFeatureRecord> pRna(new CGffFeatureRecord(xNextTrnaId()));
    if (!xAssignFeature(*pRna, fc, mf)) {
        return false;
    }
    const CSeq_loc& PackedInt = pRna->Location();
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
            CRef<CGffFeatureRecord> pChild(new CGffFeatureRecord(*pRna));
            pChild->SetRecordId(xNextGenericId());
            pChild->SetFeatureType("exon");
            pChild->SetLocation(subint);
            if ( ! xWriteFeatureRecord(*pChild ) ) {
                return false;
            }
        }
    }
    return true;    
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureType(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    static CSafeStatic<CSofaMap> SOFAMAP;

    if (!mf.IsSetData()) {
        record.SetFeatureType(SOFAMAP->DefaultName());
    }
    record.SetFeatureType(
        SOFAMAP->MappedName(mf.GetFeatType(), mf.GetFeatSubtype()));
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureSeqId(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string bestId;
    if (!CWriteUtil::GetBestId(mf, bestId)) {
        bestId = ".";
    }
    record.SetSeqId(bestId);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureSource(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string source = ".";

    if ( mf.IsSetExt() ) {
        CConstRef<CUser_object> model_evidence = sGetUserObjectByType( 
            mf.GetExt(), "ModelEvidence" );
        if ( model_evidence ) {
            string strMethod;
            if ( model_evidence->HasField( "Method" ) ) {
                source = model_evidence->GetField( 
                    "Method" ).GetData().GetStr();
                record.SetSource(source);
                return true;
            }
        }
    }

    if (mf.IsSetExts()) {
        CConstRef<CUser_object> model_evidence = sGetUserObjectByType( 
            mf.GetExts(), "ModelEvidence" );
        if ( model_evidence ) {
            string strMethod;
            if ( model_evidence->HasField( "Method" ) ) {
                source = model_evidence->GetField( 
                    "Method" ).GetData().GetStr();
                record.SetSource(source);
                return true;
            }
        }
    }
        
    CSeq_id_Handle idh = mf.GetLocationId();
    if (!CWriteUtil::GetIdType(*idh.GetSeqId(), source)) {
        return false;
    }
    if (source == "Local") {
        source = ".";
    }
    record.SetSource(source);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureEndpoints(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    unsigned int seqStart(0);
    unsigned int seqStop(0);

    //if ( record.m_pLoc ) {
        if (sIsTransspliced(mf)) {
            
            if (!sGetTranssplicedEndpoints(
                record.Location(), seqStart, seqStop)) {
                return false;
            }
            record.SetLocation(seqStart, seqStop);
            //return true;
        }
        else {
            seqStart = record.Location().GetStart(eExtreme_Positional);
            if (record.Location().IsPartialStart(eExtreme_Biological)) {
                string min = NStr::IntToString(seqStart + 1);
                record.SetAttribute("start_range", string(".,") + min);
            }
            seqStop = record.Location().GetStop(eExtreme_Positional);
            if (record.Location().IsPartialStop(eExtreme_Biological)) {
                string max = NStr::IntToString(seqStop + 1);
                record.SetAttribute("end_range", max + string(",."));
            }
            record.SetLocation(seqStart, seqStop);
            //return true;
        }
    //}
    CBioseq_Handle bsh = fc.BioseqHandle();
    if (!CWriteUtil::IsSequenceCircular(bsh)) {
        return true;
    }

    unsigned int bstart = record.Location().GetStart( eExtreme_Biological );
    unsigned int bstop = record.Location().GetStop( eExtreme_Biological );

    ENa_strand strand = record.Location().GetStrand();
    if (strand == eNa_strand_minus) {
        if (seqStart < bstop) {
            seqStart += bsh.GetInst().GetLength();
        }
        if (seqStop < bstop) {
            seqStop += bsh.GetInst().GetLength();
        }
        record.SetLocation(seqStart, seqStop);
        return true;
    }
    //everything else considered eNa_strand_plus
    if (seqStart < bstart) {
        seqStart += bsh.GetInst().GetLength();
    }
    if (seqStop < bstart) {
        seqStop += bsh.GetInst().GetLength();
    }
    record.SetLocation(seqStart, seqStop);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureScore(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureStrand(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    record.SetStrand(mf.GetLocation().GetStrand());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeaturePhase(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (mf.GetFeatSubtype() == CSeq_feat::TData::eSubtype_cdregion) {
        record.SetPhase(0);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributes(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!xAssignFeatureAttributeGbKey(record, fc, mf)) {
        return false;
    }

    //attributes common to all feature types:
    if (!xAssignFeatureAttributeProduct(record, fc, mf) ||
            !xAssignFeatureAttributeParent(record, fc, mf)  ||
            !xAssignFeatureAttributeException(record, fc, mf)  ||
            !xAssignFeatureAttributeExonNumber(record, fc, mf)  ||
            !xAssignFeatureAttributePseudo(record, fc, mf)  ||
            !xAssignFeatureAttributeDbXref(record, fc, mf)  ||
            !xAssignFeatureAttributeGene(record, fc, mf)  ||
            !xAssignFeatureAttributeNote(record, fc, mf)  ||
            !xAssignFeatureAttributeIsOrdered(record, fc, mf)  ||
            !xAssignFeatureAttributeTranscriptId(record, fc, mf)) {
        return false;
    }

    //attributes specific to certain feature types:
    switch(mf.GetData().GetSubtype()) {
        default:
            break;

        case CSeqFeatData::eSubtype_gene:
            if (!xAssignFeatureAttributeLocusTag(record, fc, mf)  ||
                    !xAssignFeatureAttributeGeneSynonym(record, fc, mf)  ||
                    !xAssignFeatureAttributeOldLocusTag(record, fc, mf)  ||
                    !xAssignFeatureAttributePartial(record, fc, mf)  ||
                    !xAssignFeatureAttributeGeneDesc(record, fc, mf)  ||
                    !xAssignFeatureAttributeMapLoc(record, fc, mf)) {
                return false;
            }
            break;

        case CSeqFeatData::eSubtype_mRNA:
            if (!xAssignFeatureAttributePartial(record, fc, mf)) {
                return false;
            }
            break;

        case CSeqFeatData::eSubtype_cdregion:
            if (!xAssignFeatureAttributePartial(record, fc, mf)  ||
                    !xAssignFeatureAttributeProteinId(record, fc, mf)  ||
                    !xAssignFeatureAttributeTranslationTable(record, fc, mf)  ||
                    !xAssignFeatureAttributeCodeBreak(record, fc, mf)) {
                return false;
            }
            break;

        case CSeqFeatData::eSubtype_ncRNA:
            if (!xAssignFeatureAttributeNcrnaClass(record, fc, mf)) {
                return false;
            }
            break;
    }

    //  deriviate attributes --- depend on other attributes. Hence need to be
    //  done last: 
    if (!xAssignFeatureAttributeName(record, fc, mf)) {
        return false;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeIsOrdered(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (CWriteUtil::IsLocationOrdered(mf.GetLocation())) {
        record.SetAttribute("is_ordered", "true");
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeExonNumber(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetQual()) {
        return true;
    }
    const CSeq_feat::TQual& quals = mf.GetQual();
    for ( CSeq_feat::TQual::const_iterator cit = quals.begin(); 
        cit != quals.end(); 
        ++cit ) {
        const CGb_qual& qual = **cit;
        if (qual.IsSetQual()  &&  qual.GetQual() == "number") {
            record.SetAttribute("exon_number", qual.GetVal());
            return true;
        }
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributePseudo(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetPseudo()) {
        return true;
    }
    if (mf.GetPseudo() == true) {
        record.SetAttribute("pseudo", "true");
        return true; 
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeDbXref(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData::E_Choice choice = mf.GetData().Which();

    if (mf.IsSetDbxref()) {
        const CSeq_feat::TDbxref& dbxrefs = mf.GetDbxref();
        for (size_t i=0; i < dbxrefs.size(); ++i) {
            string tag;
            if (CWriteUtil::GetDbTag(*dbxrefs[i], tag)) {
                record.AddAttribute("Dbxref", tag);
            }
        }
    }

    switch (choice) {
        default: {
            CMappedFeat parent;
            try {
                parent = fc.FeatTree().GetParent( mf );
            }
            catch(...) {
            }
            if (parent  &&  parent.IsSetDbxref()) {
                const CSeq_feat::TDbxref& more_dbxrefs = parent.GetDbxref();
                for (size_t i=0; i < more_dbxrefs.size(); ++i) {
                    string tag;
                    if (CWriteUtil::GetDbTag(*more_dbxrefs[i], tag)) {
                        record.AddAttribute("Dbxref", tag);
                    }
                }
            }
            return true;
        }

        case CSeq_feat::TData::e_Rna:
        case CSeq_feat::TData::e_Cdregion: {
            if (mf.IsSetProduct()) {
                CSeq_id_Handle idh = sequence::GetId( 
                    mf.GetProductId(), mf.GetScope(), sequence::eGetId_ForceAcc);
                if (idh) {
                    string str;
                    idh.GetSeqId()->GetLabel(&str, CSeq_id::eContent);
                    if (NPOS != str.find('_')) { //nucleotide
                        str = string("Genbank:") + str;
                    }
                    else { //protein
                        str = string("NCBI_GP:") + str;
                    }
                    record.AddAttribute("Dbxref", str);
                }
                else {
                    idh = sequence::GetId(
                        mf.GetProductId(), mf.GetScope(), sequence::eGetId_ForceGi);
                    if (idh) {
                        string str;
                        idh.GetSeqId()->GetLabel(&str, CSeq_id::eContent);
                        str = string("NCBI_gi:") + str;
                        record.AddAttribute("Dbxref", str);
                    }
                }
            }
            CMappedFeat gene_feat = fc.FeatTree().GetParent(mf, CSeqFeatData::e_Gene);
            if (gene_feat  &&  mf.IsSetXref()) {
                const CSeq_feat::TXref& xref = mf.GetXref();
                for (CSeq_feat::TXref::const_iterator cit = xref.begin(); 
                        cit != xref.end(); ++cit) {
                    if ((*cit)->IsSetData()  &&  (*cit)->GetData().IsGene()) {
                        const CSeqFeatData::TGene& gene = (*cit)->GetData().GetGene();
                        if (gene.IsSuppressed()) {
                            gene_feat = CMappedFeat();
                            return true;
                        }
                        if (!gene_feat.IsSetDbxref()) {
                            return true;
                        }
                        const CSeq_feat::TDbxref& dbxrefs = gene_feat.GetDbxref();
                        for ( size_t i=0; i < dbxrefs.size(); ++i ) {
                            string tag;
                            if (CWriteUtil::GetDbTag(*dbxrefs[i], tag)) {
                                record.AddAttribute("Dbxref", tag);
                            }
                        }
                        return true;
                    }
                }
            }
        }
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeGene(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string strGene;
    if (mf.GetData().Which() == CSeq_feat::TData::e_Gene) {
        const CGene_ref& gene_ref = mf.GetData().GetGene();
        CWriteUtil::GetGeneRefGene(gene_ref, strGene);
        record.SetAttribute("gene", strGene);
        return true;
    }

    if (mf.IsSetXref()) {
        const vector<CRef<CSeqFeatXref> > xrefs = mf.GetXref();
        for (vector<CRef<CSeqFeatXref> >::const_iterator it = xrefs.begin();
                it != xrefs.end();
                ++it) {
            const CSeqFeatXref& xref = **it;
            if (xref.CanGetData() && xref.GetData().IsGene()) {
                CWriteUtil::GetGeneRefGene(xref.GetData().GetGene(), strGene);
                record.SetAttribute("gene", strGene);
                return true;
            }
        }
    }

    CMappedFeat gene = fc.FindBestGeneParent(mf);
    if (gene.IsSetData()  &&  gene.GetData().IsGene()) {
        CWriteUtil::GetGeneRefGene(gene.GetData().GetGene(), strGene);
        record.SetAttribute("gene", strGene);
        return true; 
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeNote(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetComment()  ||  mf.GetComment().empty()) {
        return true;
    }
    record.SetAttribute("Note", mf.GetComment());
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeException(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (mf.IsSetExcept_text()) {
        record.SetAttribute("exception", mf.GetExcept_text());
        return true;
    }
    if (mf.IsSetExcept()) {
        // what should I do?
        return true;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeTranscriptId(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (mf.GetFeatType() != CSeqFeatData::e_Rna) {
        return true;
    }
    const CSeq_feat::TQual& quals = mf.GetQual();
    for (CSeq_feat::TQual::const_iterator cit = quals.begin(); 
      cit != quals.end(); ++cit) {
        if ((*cit)->GetQual() == "transcript_id") {
            record.SetAttribute("transcript_id", (*cit)->GetVal());
            return true;
        }
    }

    if (mf.IsSetProduct()) {
        string transcript_id;
        if (CWriteUtil::GetBestId(mf.GetProductId(), mf.GetScope(), transcript_id)) {
            record.SetAttribute("transcript_id", transcript_id);
            return true;
        }
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeGbKey(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    record.SetAttribute("gbkey", mf.GetData().GetKey());
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeGeneDesc(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mf.GetData().GetGene();
    if (!gene_ref.IsSetDesc()) {
        return true;
    }
    record.SetAttribute("description", gene_ref.GetDesc());
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeGeneSynonym(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mf.GetData().GetGene();
    if (!gene_ref.IsSetSyn()) {
        return true;
    }

    const list<string>& syns = gene_ref.GetSyn();
    list<string>::const_iterator it = syns.begin();
    if (!gene_ref.IsSetLocus() && !gene_ref.IsSetLocus_tag()) {
        ++it;
    }    
    if (it == syns.end()) {
        return true;
    }
    while (it != syns.end()) {
        record.AddAttribute("gene_synonym", *(it++));
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeOldLocusTag(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetQual()) {
        return true;
    }
    string old_locus_tags;
    vector<CRef<CGb_qual> > quals = mf.GetQual();
    for (vector<CRef<CGb_qual> >::const_iterator it = quals.begin(); 
            it != quals.end(); ++it) {
        if ((**it).IsSetQual()  &&  (**it).IsSetVal()) {
            string qual = (**it).GetQual();
            if (qual != "old_locus_tag") {
                continue;
            }
            if (!old_locus_tags.empty()) {
                old_locus_tags += ",";
            }
            old_locus_tags += (**it).GetVal();
        }
    }
    if (!old_locus_tags.empty()) {
        record.SetAttribute("old_locus_tag", old_locus_tags);
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeMapLoc(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mf.GetData().GetGene();
    if (!gene_ref.IsSetMaploc()) {
        return true;
    }
    record.SetAttribute("map", gene_ref.GetMaploc());
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeName(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    vector<string> value;   
    switch (mf.GetFeatSubtype()) {
        default:
            break;

        case CSeqFeatData::eSubtype_gene:
            if (record.GetAttributes("gene", value)) {
                record.SetAttribute("Name", value.front());
                return true;
            }
            if (record.GetAttributes("locus_tag", value)) {
                record.SetAttribute("Name", value.front());
                return true;
            }
            return true;

        case CSeqFeatData::eSubtype_cdregion:
            if (record.GetAttributes("protein_id", value)) {
                record.SetAttribute("Name", value.front());
                return true;
            }
            return true;
    }

    if (record.GetAttributes("transcript_id", value)) {
        record.SetAttribute("Name", value.front());
        return true;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeLocusTag(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mf.GetData().GetGene();
    if (!gene_ref.IsSetLocus_tag()) {
        return true;
    }
    record.SetAttribute("locus_tag", gene_ref.GetLocus_tag());
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributePartial(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData::ESubtype subtype = mf.GetData().GetSubtype();
    if (mf.IsSetPartial()) {
        if (mf.GetPartial() == true) {
            record.SetAttribute("partial", "true");
            return true;
        }
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeProteinId(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetProduct()) {
        return true;
    }
    string protein_id;
    if (CWriteUtil::GetBestId(mf.GetProductId(), mf.GetScope(), protein_id)) {
        record.SetAttribute("protein_id", protein_id);
        return true;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeTranslationTable(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetData()  ||  
            mf.GetFeatSubtype() != CSeqFeatData::eSubtype_cdregion) {
        return true;
    }
    const CSeqFeatData::TCdregion& cds = mf.GetData().GetCdregion();
    if (!cds.IsSetCode()) {
        return true;
    }
    int id = cds.GetCode().GetId();
    if (id != 1  &&  id != 255) {
        record.SetAttribute("transl_table", NStr::IntToString(id));
        return true;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeCodeBreak(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetData()  ||  
            mf.GetFeatSubtype() != CSeqFeatData::eSubtype_cdregion) {
        return true;
    }
    const CSeqFeatData::TCdregion& cds = mf.GetData().GetCdregion();
    if (!cds.IsSetCode_break()) {
        return true;
    }
    const list<CRef<CCode_break> >& code_breaks = cds.GetCode_break();
    list<CRef<CCode_break> >::const_iterator it = code_breaks.begin();
    for (; it != code_breaks.end(); ++it) {
        string cbString;
        if (CWriteUtil::GetCodeBreak(**it, cbString)) {
            record.SetAttribute("transl_except", cbString);
        }
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeProduct(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
    if (subtype == CSeqFeatData::eSubtype_cdregion) {

        // Possibility 1:
        // Product name comes from a prot-ref which stored in the seqfeat's 
        // xrefs:
        const CProt_ref* pProtRef = mf.GetProtXref();
        if ( pProtRef && pProtRef->IsSetName() ) {
            const list<string>& names = pProtRef->GetName();
            record.SetAttribute("product", names.front());
            return true;
        }

        // Possibility 2:
        // Product name is from the prot-ref refered to by the seqfeat's 
        // data.product:
        if (mf.IsSetProduct()) {
            const CSeq_id* pId = mf.GetProduct().GetId();
            if (pId) {
                CBioseq_Handle bsh = mf.GetScope().GetBioseqHandle(*pId); 
                if (bsh) {
                    CFeat_CI it(bsh, SAnnotSelector(CSeqFeatData::eSubtype_prot));
                    if (it  &&  it->IsSetData() 
                            &&  it->GetData().GetProt().IsSetName()
                            &&  !it->GetData().GetProt().GetName().empty()) {
                        record.SetAttribute("product",
                            it->GetData().GetProt().GetName().front());
                        return true;
                    }
                }
            }
            
            string product;
            if (CWriteUtil::GetBestId(mf.GetProductId(), mf.GetScope(), product)) {
                record.SetAttribute("product", product);
                return true;
            }
        }
    }

    CSeqFeatData::E_Choice type = mf.GetFeatType();
    if (type == CSeqFeatData::e_Rna) {
        const CRNA_ref& rna = mf.GetData().GetRna();

        if (subtype == CSeqFeatData::eSubtype_tRNA) {
            if (rna.IsSetExt()  &&  rna.GetExt().IsTRNA()) {
                const CTrna_ext& trna = rna.GetExt().GetTRNA();
                string anticodon;
                if (CWriteUtil::GetTrnaAntiCodon(trna, anticodon)) {
                    record.SetAttribute("anticodon", anticodon);
                }
                string codons;
                if (CWriteUtil::GetTrnaCodons(trna, codons)) {
                    record.SetAttribute("codons", codons);
                }
                string aa;
                if (CWriteUtil::GetTrnaProductName(trna, aa)) {
                    record.SetAttribute("product", aa);
                    return true;
                }
            }
        }

        if (rna.IsSetExt()  &&  rna.GetExt().IsName()) {
            record.SetAttribute("product", rna.GetExt().GetName());
            return true;
        }

        if (rna.IsSetExt()  &&  rna.GetExt().IsGen()  &&  
                rna.GetExt().GetGen().IsSetProduct() ) {
            record.SetAttribute("product", rna.GetExt().GetGen().GetProduct());
            return true;
        }
    }

    // finally, look for gb_qual
    if (mf.IsSetQual()) {
        const CSeq_feat::TQual& quals = mf.GetQual();
        for ( CSeq_feat::TQual::const_iterator cit = quals.begin(); 
                cit != quals.end(); ++cit) {
            if ((*cit)->IsSetQual()  &&  (*cit)->IsSetVal()  &&  
                    (*cit)->GetQual() == "product") {
                record.SetAttribute("product", (*cit)->GetVal());
                return true;
            }
        }
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeNcrnaClass(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetData()  ||  
            (mf.GetData().GetSubtype() != CSeqFeatData::eSubtype_ncRNA)) {
        return true;
    }
    const CSeqFeatData::TRna& rna = mf.GetData().GetRna();
    if (!rna.IsSetExt()) {
        return true;
    }
    const CRNA_ref::TExt& ext = rna.GetExt();
    if (!ext.IsGen()  ||  !ext.GetGen().IsSetClass()) {
        return true;
    }
    record.SetAttribute("ncrna_class", ext.GetGen().GetClass());
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureAttributeParent(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    switch (mf.GetFeatSubtype()) {

    default:
        return true;

    case CSeqFeatData::eSubtype_cdregion:
    case CSeqFeatData::eSubtype_exon:
        return (xAssignFeatureAttributeParentMrna(record, fc,mf)  ||
            xAssignFeatureAttributeParentGene(record, fc,mf));

    case CSeqFeatData::eSubtype_mRNA:
    case CSeqFeatData::eSubtype_C_region:
    case CSeqFeatData::eSubtype_D_segment:
    case CSeqFeatData::eSubtype_J_segment:
    case CSeqFeatData::eSubtype_V_segment:
        return xAssignFeatureAttributeParentGene(record, fc,mf);

    case CSeqFeatData::eSubtype_gene:
        //genes have no parents
        return true;
    }
    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeatureBasic(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    return (xAssignFeatureType(record, fc, mf)  &&
        xAssignFeatureSeqId(record, fc, mf)  &&
        xAssignFeatureSource(record, fc, mf)  &&
        xAssignFeatureEndpoints(record, fc, mf)  &&
        xAssignFeatureScore(record, fc, mf)  &&
        xAssignFeatureStrand(record, fc, mf)  &&
        xAssignFeaturePhase(record, fc, mf)  &&
        xAssignFeatureAttributes(record, fc, mf));
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignFeature(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> pLoc(new CSeq_loc(CSeq_loc::e_Mix));
    pLoc->Add(mf.GetLocation());
    CWriteUtil::ChangeToPackedInt(*pLoc);
    record.InitLocation(*pLoc);

    CBioseq_Handle bsh = fc.BioseqHandle();
    if (!CWriteUtil::IsSequenceCircular(bsh)) {
        return xAssignFeatureBasic(record, fc, mf);
    }

    //  intervals wrapping around the origin extend beyond the sequence length
    //  instead of breaking and restarting at the origin.
    //
    unsigned int len = bsh.GetInst().GetLength();
    list< CRef< CSeq_interval > >& sublocs = pLoc->SetPacked_int().Set();
    if (sublocs.size() < 2) {
        return xAssignFeatureBasic(record, fc, mf);
    }

    list< CRef<CSeq_interval> >::iterator it;
    list< CRef<CSeq_interval> >::iterator it_ceil=sublocs.end(); 
    list< CRef<CSeq_interval> >::iterator it_floor=sublocs.end();
    for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
        //fix intervals broken in two for crossing the origin to extend
        //  into virtual space instead
        CSeq_interval& subint = **it;
        if (subint.IsSetFrom()  &&  subint.GetFrom() == 0) {
            it_floor = it;
        }
        if (subint.IsSetTo()  &&  subint.GetTo() == len-1) {
            it_ceil = it;
        }
        if (it_floor != sublocs.end()  &&  it_ceil != sublocs.end()) {
            break;
        } 
    }
    if ( it_ceil != sublocs.end()  &&  it_floor != sublocs.end() ) {
        (*it_ceil)->SetTo( (*it_ceil)->GetTo() + (*it_floor)->GetTo() + 1 );
        sublocs.erase(it_floor);
    }

    return xAssignFeatureBasic(record, fc, mf);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSource(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    return (xAssignSourceType(record, fc, bsh)  &&
        xAssignSourceSeqId(record, fc, bsh)  &&
        xAssignSourceSource(record, fc, bsh)  &&
        xAssignSourceEndpoints(record, fc, bsh)  &&
        xAssignSourceAttributes(record, fc, bsh));
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceType(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    record.SetFeatureType("region");
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceSeqId(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    CConstRef<CSeq_id> pId = bsh.GetNonLocalIdOrNull();
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*pId);
    string bestId;
    if (!pId  ||  !CWriteUtil::GetBestId(idh, bsh.GetScope(), bestId)) {
        bestId = ".";
    }
    record.SetSeqId(bestId);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceSource(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    string source(".");
    CWriteUtil::GetIdType(bsh, source);
    record.SetSource(source);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceEndpoints(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    unsigned int seqStart = 0;//always for source
    unsigned int seqStop = bsh.GetBioseqLength() - 1;
    ENa_strand seqStrand = eNa_strand_plus;
    if (bsh.CanGetInst_Strand()) {
        //now that's nuts- how should we act on GetInst_Strand() ???
    }
    record.SetLocation(seqStart, seqStop, seqStrand);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributes(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    return (xAssignSourceAttributeGbKey(record, fc, bsh)  &&
        xAssignSourceAttributeMolType(record, fc, bsh)  &&
        xAssignSourceAttributeIsCircular(record, fc, bsh)  &&
        xAssignSourceAttributesBioSource(record, fc, bsh)); 
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeGbKey(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    record.SetAttribute("gbkey", "Src");
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeMolType(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    string molType;
    if (!CWriteUtil::GetBiomol(bsh, molType)) {
        return true;
    }
    record.SetAttribute("mol_type", molType);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeIsCircular(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    if (!CWriteUtil::IsSequenceCircular(bsh)) {
        return true;
    }
    record.SetAttribute("Is_circular", "true");
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributesBioSource(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    CSeqdesc_CI sdi( bsh.GetParentEntry(), CSeqdesc::e_Source, 0 );
    if (!sdi) {
        return true;
    }
    const CBioSource& bioSrc = sdi->GetSource();
    return (xAssignSourceAttributeGenome(record, fc, bioSrc)  &&
        xAssignSourceAttributeName(record, fc, bioSrc)  &&
        xAssignSourceAttributeDbxref(record, fc, bioSrc)  &&
        xAssignSourceAttributesOrgMod(record, fc, bioSrc)  &&
        xAssignSourceAttributesSubSource(record, fc, bioSrc));
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeGenome(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CBioSource& bioSrc)
//  ----------------------------------------------------------------------------
{
    string genome;
    if (!CWriteUtil::GetGenomeString(bioSrc, genome)) {
        return true;
    }
    record.SetAttribute("genome", genome);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeName(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CBioSource& bioSrc)
//  ----------------------------------------------------------------------------
{
    string name = bioSrc.GetRepliconName();
    if (name.empty()) {
        return true;
    }
    record.SetAttribute("Name", name);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributeDbxref(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CBioSource& bioSrc)
//  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CDbtag> > DBTAGS;

    if (!bioSrc.IsSetOrg()) {
        return true;
    }
    const COrg_ref& orgRef = bioSrc.GetOrg();
    if (!orgRef.IsSetDb()) {
        return true;
    }
    const DBTAGS& tags = orgRef.GetDb();
    for (DBTAGS::const_iterator cit = tags.begin(); cit != tags.end(); ++cit) {
        string tag;
        if (CWriteUtil::GetDbTag(**cit, tag)) {
            record.AddAttribute("Dbxref", tag);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributesOrgMod(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CBioSource& bioSrc)
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<COrgMod> > MODS;

    if (!bioSrc.IsSetOrg()) {
        return true;
    }
    const COrg_ref& orgRef = bioSrc.GetOrg();
    if (!orgRef.IsSetOrgname()) {
        return true;
    }
    const COrgName& orgName = orgRef.GetOrgname();
    if (!orgName.IsSetMod()) {
        return true;
    }
    const MODS& mods = orgName.GetMod();
    for (MODS::const_iterator cit = mods.begin(); cit != mods.end(); ++cit) {
        string key, value;
        if (CWriteUtil::GetOrgModSubType(**cit, key, value)) {
            record.SetAttribute(key, value);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xAssignSourceAttributesSubSource(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CBioSource& bioSrc)
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSubSource> > SUBS;

    if (!bioSrc.IsSetSubtype()) {
        return true;
    }
    const SUBS& subs = bioSrc.GetSubtype();
    for (SUBS::const_iterator cit = subs.begin(); cit != subs.end(); ++cit) {
        string key, value;
        if (CWriteUtil::GetSubSourceSubType(**cit, key, value)) {
            record.SetAttribute(key, value);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureGene(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGffFeatureRecord> pRecord( 
        new CGffFeatureRecord( 
            string("gene") + NStr::UIntToString(m_uPendingGeneId++)));

    if (!xAssignFeature(*pRecord, fc, mf)) {
        return false;
    }
    m_GeneMapNew[mf] = pRecord;
    return xWriteFeatureRecords(*pRecord, pRecord->Location(), 0);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureCds(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGffFeatureRecord> pCds(new CGffFeatureRecord());
    if (!xAssignFeature(*pCds, fc, mf)) {
        return false;
    }

    const CSeq_loc& PackedInt = pCds->Location();
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
        string cdsId = xNextCdsId();
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            const CSeq_interval& subint = **it;
            CRef<CGffFeatureRecord> pExon(
                new CGffFeatureRecord(*pCds));
            pExon->SetRecordId(cdsId);
            pExon->SetFeatureType("CDS");
            pExon->SetLocation(subint);
            pExon->SetPhase( bStrandAdjust ? (3-iPhase) : iPhase );
            if (!xWriteFeatureRecord(*pExon)) {
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
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureRna(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string rnaId = xNextTrnaId();
    CRef<CGffFeatureRecord> pRna(new CGffFeatureRecord(rnaId));
    if (!xAssignFeature(*pRna, fc, mf)) {
        return false;
    }

    if (!xWriteFeatureRecord(*pRna)) {
        return false;
    }
    if (mf.GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA) {
        m_MrnaMapNew[mf] = pRna;
    }    

    const CSeq_loc& PackedInt = pRna->Location();
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
            CRef<CGffFeatureRecord> pChild(
                new CGffFeatureRecord(*pRna));
            pChild->SetRecordId(xNextGenericId());
            pChild->SetFeatureType("exon");
            pChild->SetLocation(subint);
            pChild->DropAttributes("Name"); //explicitely not inherited
            pChild->SetAttribute("Parent", rnaId);
            if (!xWriteFeatureRecord(*pChild)) {
                return false;
            }
        }
        return true;
    }
    return true;    
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureGeneric(
    CGffFeatureContext& fc,
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CRef<CGffFeatureRecord> pParent(new CGffFeatureRecord(xNextGenericId()));
    if (!xAssignFeature(*pParent, fc, mf)) {
        return false;
    }

    TSeqPos seqlength = 0;
    if(fc.BioseqHandle() && fc.BioseqHandle().CanGetInst())
        seqlength = fc.BioseqHandle().GetInst().GetLength();
    return xWriteFeatureRecords( *pParent, mf.GetLocation(), seqlength );
}

//  ============================================================================
bool CGff3Writer::xWriteFeatureRecords(
    const CGffFeatureRecord& record,
    const CSeq_loc& location,
    unsigned int seqLength )
//  ============================================================================
{
    CRef<CSeq_loc> pPackedInt(new CSeq_loc(CSeq_loc::e_Mix));
    pPackedInt->Add(location);
    CWriteUtil::ChangeToPackedInt(*pPackedInt);

    if (!pPackedInt->IsPacked_int() || !pPackedInt->GetPacked_int().CanGet()) {
        return xWriteFeatureRecord(record);
    }
    const list<CRef<CSeq_interval> >& sublocs = pPackedInt->GetPacked_int().Get();
    if (sublocs.size() == 1) {
        return xWriteFeatureRecord(record);
    }

    list<CRef<CSeq_interval> >::const_iterator it;
    string totIntervals = string("/") + NStr::IntToString(sublocs.size());
    unsigned int curInterval = 1;
    for (it = sublocs.begin(); it != sublocs.end(); ++it) {
        const CSeq_interval& subint = **it;
        CRef<CGffFeatureRecord> pChild(new CGffFeatureRecord(record));
        pChild->SetLocation(subint);
        string part = NStr::IntToString(curInterval++) + totIntervals;
        pChild->SetAttribute("part", part);
        if (!xWriteFeatureRecord(*pChild)) {
            return false;
        }
    }
    return true;
}

//  ============================================================================
void CGff3Writer::xWriteAlignment( 
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
bool CGff3Writer::xAssignFeatureAttributeParentGene(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    CMappedFeat mf)
//  ============================================================================
{
    CMappedFeat gene = fc.FindBestGeneParent(mf);
    if (!gene) {
        return false;
    }
    TGeneMapNew::iterator it = m_GeneMapNew.find(gene);
    if (it == m_GeneMapNew.end()) {
        return false;
    }
    vector<string> parentId;
    if (!it->second->GetAttributes("ID", parentId)) {
        return false;
    }
    return record.SetAttributes("Parent", parentId);
}

//  ============================================================================
bool CGff3Writer::xAssignFeatureAttributeParentMrna(
    CGffFeatureRecord& record,
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
    TMrnaMapNew::iterator it = m_MrnaMapNew.find(mrna);
    if (it == m_MrnaMapNew.end()) {
        return false;
    }
    vector<string> parentId;
    if (!it->second->GetAttributes("ID", parentId)) {
        return false;
    }
    return record.SetAttributes("Parent", parentId);
}

//  ----------------------------------------------------------------------------
bool CGff3Writer::xWriteFeatureRecord( 
    const CGffFeatureRecord& record )
//  ----------------------------------------------------------------------------
{
    m_Os << record.StrSeqId() << '\t';
    m_Os << record.StrSource() << '\t';
    m_Os << record.StrType() << '\t';
    m_Os << record.StrSeqStart() << '\t';
    m_Os << record.StrSeqStop() << '\t';
    m_Os << record.StrScore() << '\t';
    m_Os << record.StrStrand() << '\t';
    m_Os << record.StrPhase() << '\t';
    m_Os << record.StrAttributes() << endl;
    return true;
}

//  ----------------------------------------------------------------------------
string CGff3Writer::xNextGenericId()
//  ----------------------------------------------------------------------------
{
    return (string("id") + NStr::UIntToString(m_uPendingGenericId++));
}

//  ----------------------------------------------------------------------------
string CGff3Writer::xNextCdsId()
//  ----------------------------------------------------------------------------
{
    return (string("cds") + NStr::UIntToString(m_uPendingCdsId++));
}

//  ----------------------------------------------------------------------------
string CGff3Writer::xNextTrnaId()
//  ----------------------------------------------------------------------------
{
    return (string("rna") + NStr::UIntToString(m_uPendingTrnaId++));
}

END_NCBI_SCOPE
