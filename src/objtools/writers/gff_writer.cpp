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

#include <objects/general/User_object.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature_edit.hpp>

#include <objtools/writers/write_util.hpp>
#include <objtools/writers/gff_writer.hpp>
#include <objtools/writers/genbank_id_resolve.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGff2Writer::CGff2Writer(
    CScope& scope,
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CWriterBase( ostr, uFlags ),
    m_bHeaderWritten(false)
{
    m_pScope.Reset( &scope );
    //SetAnnotSelector();
};

//  ----------------------------------------------------------------------------
CGff2Writer::CGff2Writer(
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CWriterBase( ostr, uFlags ),
    m_bHeaderWritten(false)
{
    m_pScope.Reset( new CScope( *CObjectManager::GetInstance() ) );
    m_pScope->AddDefaults();
    //SetAnnotSelector();
};

//  ----------------------------------------------------------------------------
CGff2Writer::~CGff2Writer()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteAnnot(
    const CSeq_annot& annot,
    const string& strAssemblyName,
    const string& strAssemblyAccession )
//  ----------------------------------------------------------------------------
{
    if ( ! x_WriteAssemblyInfo( strAssemblyName, strAssemblyAccession ) ) {
        return false;
    }
    if ( ! x_WriteAnnot( annot ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteAnnot(
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    CSeq_annot_Handle sah = m_pScope->AddSeq_annot( annot );
    bool bWrite = x_WriteSeqAnnotHandle( sah );
    m_pScope->RemoveSeq_annot( sah );
    return bWrite;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteSeqEntryHandle(
    CSeq_entry_Handle seh,
    const string& strAssemblyName,
    const string& strAssemblyAccession )
//  ----------------------------------------------------------------------------
{
    if ( ! x_WriteAssemblyInfo( strAssemblyName, strAssemblyAccession ) ) {
        return false;
    }
    if ( ! x_WriteSeqEntryHandle( seh ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteSeqEntryHandle(
    CSeq_entry_Handle seh )
//  ----------------------------------------------------------------------------
{
    bool isNucProtSet = (seh.IsSet()  &&  seh.GetSet().IsSetClass()  &&
        seh.GetSet().GetClass() == CBioseq_set::eClass_nuc_prot);
    if (isNucProtSet) {
        for (CBioseq_CI bci(seh); bci; ++bci) {
            if (!x_WriteBioseqHandle(*bci)) {
                return false;
            }
        }
        return true;
    }

    bool isGenbankSet = (seh.IsSet()  &&  seh.GetSet().IsSetClass()  &&
        seh.GetSet().GetClass() == CBioseq_set::eClass_genbank);
    if (isGenbankSet) {
        for (CSeq_entry_CI seci(seh); seci; ++seci) {

            if (!x_WriteSeqEntryHandle(*seci)) {
                return false;
            }
        }
        return true;
    }

    if (seh.IsSeq()) {
        CBioseq_Handle bsh = seh.GetSeq();
        return x_WriteBioseqHandle(bsh);
    }

    SAnnotSelector sel;
    sel.SetMaxSize(1);
    for (CAnnot_CI aci(seh, sel); aci; ++aci) {
        CFeat_CI fit(*aci, SetAnnotSelector());
        CSeq_id_Handle lastId;
        for ( /*0*/; fit; ++fit ) {
            CSeq_id_Handle currentId =fit->GetLocationId();
            if (currentId != lastId) {
                x_WriteSequenceHeader(currentId);
                lastId = currentId;
            }
            if (!xWriteFeature(fit)) {
                return false;
            }
        }
    }

    for (CSeq_entry_CI eci(seh); eci; ++eci) {
        if (!x_WriteSeqEntryHandle(*eci)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteBioseqHandle(
    CBioseq_Handle bsh,
    const string& strAssemblyName,
    const string& strAssemblyAccession )
//  ----------------------------------------------------------------------------
{
    if ( ! x_WriteAssemblyInfo( strAssemblyName, strAssemblyAccession ) ) {
        return false;
    }
    if ( ! x_WriteBioseqHandle( bsh ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteBioseqHandle(
    CBioseq_Handle bsh )
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel = SetAnnotSelector();
    const auto& display_range = GetRange();
    CFeat_CI feat_iter(bsh, display_range, sel);
    CGffFeatureContext fc(feat_iter, bsh);

    while (feat_iter) {
        CMappedFeat mf = *feat_iter;
        xWriteFeature(feat_iter);
        ++feat_iter;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xWriteAllChildren(
    CGffFeatureContext& fc,
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    feature::CFeatTree& featTree = fc.FeatTree();
    vector<CMappedFeat> vChildren;
    featTree.GetChildrenTo(mf, vChildren);
    for (auto cit = vChildren.begin(); cit != vChildren.end(); ++cit) {
        CMappedFeat mChild = *cit;
        if (!xWriteFeature(fc, mChild)) {
            //error - but should have been handled elsewhere
            continue;
        }
        xWriteAllChildren(fc, mChild);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteSeqAnnotHandle(
    CSeq_annot_Handle sah,
    const string& strAssemblyName,
    const string& strAssemblyAccession )
//  ----------------------------------------------------------------------------
{
    if ( ! x_WriteAssemblyInfo( strAssemblyName, strAssemblyAccession ) ) {
        return false;
    }
    if ( ! x_WriteSeqAnnotHandle( sah ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteSeqAnnotHandle(
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
        return true;
    }

    SAnnotSelector sel = SetAnnotSelector();
    CFeat_CI feat_iter(sah, sel);
    CGffFeatureContext fc(feat_iter, CBioseq_Handle(), sah);

    for (/*0*/; feat_iter; ++feat_iter) {
        if (!xWriteFeature(fc,*feat_iter)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xWriteFeature(
    CGffFeatureContext& context,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xWriteFeature(
    CFeat_CI feat_it)
//  ----------------------------------------------------------------------------
{
    if (!feat_it) {
        return false;
    }
    CGffFeatureContext fc(feat_it, CBioseq_Handle(), feat_it->GetAnnot());
    return xWriteFeature(fc, *feat_it);
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteAlign(
    const CSeq_align& align,
    const string& strAssName,
    const string& strAssAcc )
//  ----------------------------------------------------------------------------
{
    if ( ! x_WriteAssemblyInfo( strAssName, strAssAcc ) ) {
        return false;
    }
    if ( ! x_WriteAlign( align ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteAlign(
    const CSeq_align& align)
//  ----------------------------------------------------------------------------
{
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteHeader()
//  ----------------------------------------------------------------------------
{
    if (!m_bHeaderWritten) {
        m_Os << "##gff-version 2" << '\n';
        m_bHeaderWritten = true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::WriteFooter()
//  ----------------------------------------------------------------------------
{
    m_Os << "###" << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::x_WriteAssemblyInfo(
    const string& strName,
    const string& strAccession )
//  ----------------------------------------------------------------------------
{
    if ( !strName.empty() ) {
        m_Os << "##assembly name=" << strName << '\n';
    }
    if ( !strAccession.empty() ) {
        m_Os << "##assembly accession=" << strAccession << '\n';
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeature(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    return xAssignFeatureBasic(record, fc, mf);
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureBasic(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    if (!xAssignFeatureType(record, fc, mf)) {
        return false;
    }
    if (!xAssignFeatureSeqId(record, fc, mf)) {
        return false;
    }
    if (!xAssignFeatureMethod(record, fc, mf)) {
        return false;
    }
    if (!xAssignFeatureEndpoints(record, fc, mf)) {
        return false;
    }
    if (!xAssignFeatureScore(record, fc, mf)) {
        return false;
    }
    if (!xAssignFeatureStrand(record, fc, mf)) {
        return false;
    }
    if (!xAssignFeaturePhase(record, fc, mf)) {
        return false;
    }
    if (!xAssignFeatureAttributes(record, fc, mf)) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureSeqId(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    string bestId;
    if (!CGenbankIdResolve::Get().GetBestId(mf, bestId)) {
        bestId = ".";
    }
    record.SetSeqId(bestId);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureScore(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    if ( !mf.IsSetQual() ) {
        return true;
    }
    const vector< CRef< CGb_qual > >& quals = mf.GetQual();
    vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
    for ( ; it != quals.end(); ++it ) {
        if ( !(*it)->CanGetQual() || !(*it)->CanGetVal() ) {
            continue;
        }
        if ( (*it)->GetQual() == "gff_score" ) {
            record.SetScore((*it)->GetVal());
            return true;
        }
    }
    return true;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeaturePhase(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureType(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    record.SetType(".");
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureStrand(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    const auto& location = mf.GetLocation();
    record.SetStrand(
        location.IsSetStrand() ? location.GetStrand() : eNa_strand_plus);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureMethod(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    record.SetMethod(".");
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureEndpoints(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    unsigned int start(0);
    unsigned int stop(0);
    auto strand = mf.GetLocation().GetStrand();

    if (CWriteUtil::IsTransspliced(mf)) {
       if (!CWriteUtil::GetTranssplicedEndpoints(mf.GetLocation(),
                start, stop)) {
            return false;
        }
        CGffBaseRecord& baseRecord = record;
        baseRecord.SetLocation(start, stop, strand);
        return true;
    }
    start = mf.GetLocation().GetStart(eExtreme_Positional);
    stop = mf.GetLocation().GetStop(eExtreme_Positional);
    record.SetEndpoints(start, stop, strand);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributes(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (!xAssignFeatureAttributesFormatIndependent(record, fc, mf)) {
        return false;
    }
    return xAssignFeatureAttributesFormatSpecific(record, fc, mf);
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributesFormatIndependent(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    record.SetGbKeyFrom(mf);
    return(
        xAssignFeatureAttributesQualifiers(record, fc, mf)  &&
        xAssignFeatureAttributeDbxref(record, fc, mf)  &&
        xAssignFeatureAttributeNote(record, fc, mf)  &&
        xAssignFeatureAttributeException(record, fc, mf)  &&
        xAssignFeatureAttributeExperiment(record, fc, mf)  &&
        xAssignFeatureAttributeProduct(record, fc, mf)  &&
        xAssignFeatureAttributesGene(record, fc, mf)  &&
        xAssignFeatureAttributeOldLocusTag(record, fc, mf)  &&
        xAssignFeatureAttributeGeneBiotype(record, fc, mf)  &&
        xAssignFeatureAttributeMapLoc(record, fc, mf)  &&
        //xAssignFeatureAttributeRibosomalSlippage(record, fc, mf)  &&
        xAssignFeatureAttributePseudoGene(record, fc, mf)  &&
        xAssignFeatureAttributeFunction(record, fc, mf)  &&
        xAssignFeatureAttributesGoMarkup(record, fc, mf)  &&
        xAssignFeatureAttributeProteinId(record, fc, mf)  &&
        xAssignFeatureAttributeTranslationTable(record, fc, mf)  &&
        xAssignFeatureAttributeCodeBreak(record, fc, mf)  &&
        xAssignFeatureAttributeModelEvidence(record, fc, mf)  &&
        xAssignFeatureAttributeRptFamily(record, fc, mf)  &&
        xAssignFeatureAttributeIsOrdered(record, fc, mf)  &&
        xAssignFeatureAttributeEcNumbers(record, fc, mf)  &&
        xAssignFeatureAttributeExonNumber(record, fc, mf)  &&
        xAssignFeatureAttributePartial(record, fc, mf)  &&
        xAssignFeatureAttributePseudo(record, fc, mf));
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributesFormatSpecific(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributePseudo(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
    //  ----------------------------------------------------------------------------
{
    if (mf.IsSetPseudo()  &&  mf.GetPseudo()) {
        record.SetAttribute("pseudo", "true");
        fc.AssignShouldInheritPseudo(true);
        return true;
    }
    if (fc.ShouldInheritPseudo()) {
        record.SetAttribute("pseudo", "true");
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributePartial(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (mf.IsMapped()  &&  mf.IsSetPartial()  &&  mf.GetPartial()) {
        record.SetAttribute("partial", "true");
        return true;
    }

    if (!mf.IsMapped()  &&  mf.GetSeq_feat()->IsSetPartial()  &&
            mf.GetSeq_feat()->GetPartial()) {
        record.SetAttribute("partial", "true");
        return true;
    }

    const CRange<TSeqPos>& display_range = GetRange();
    const CRange<TSeqPos>& feat_range = mf.GetLocation().GetTotalRange();
    if (display_range.IntersectionWith(feat_range).NotEmpty() &&
            (display_range.GetFrom() > feat_range.GetFrom() ||
                display_range.GetTo() < feat_range.GetTo())) {
        record.SetAttribute("partial", "true");
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributeTranslationTable(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetData()  ||  !mf.GetData().IsCdregion()) {

        return true;
    }
    const CSeqFeatData::TCdregion& cds = mf.GetData().GetCdregion();
    if (!cds.IsSetCode()) {
        return true;
    }
    int id = cds.GetCode().GetId();
    if (id != 1  &&  id != 255) {//former gff3 version
    //if (true) {//former gtf version
        record.SetAttribute("transl_table", NStr::IntToString(id));
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributeRibosomalSlippage(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    auto featSubtype = mf.GetFeatSubtype();
    if (featSubtype != CSeq_feat::TData::eSubtype_cdregion) {
        return true;
    }
    if (mf.IsSetExcept_text()) {
        if (mf.GetExcept_text() == "ribosomal slippage") {
            record.AddAttribute("ribosomal_slippage", "");
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributeProteinId(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    auto featSubtype = mf.GetFeatSubtype();
    if (featSubtype != CSeq_feat::TData::eSubtype_cdregion) {
        return true;
    }

    auto protein_id = mf.GetNamedQual("protein_id");
    if (!protein_id.empty()) {
        record.AddAttribute("protein_id", protein_id);
        return true;
    }
    if (mf.IsSetProduct()) {
        string product;
        if (CGenbankIdResolve::Get().GetBestId(
                mf.GetProductId(), mf.GetScope(), product)) {
            record.AddAttribute("protein_id", product);
            return true;
        }
        record.AddAttribute(
            "protein_id", mf.GetProduct().GetId()->GetSeqIdString(true));
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributeProduct(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf )
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
                    SAnnotSelector sel(CSeqFeatData::eSubtype_prot);
                    sel.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
                    CFeat_CI it(bsh, sel);
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
            if (CGenbankIdResolve::Get().GetBestId(
                    mf.GetProductId(), mf.GetScope(), product)) {
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

                const CRange<TSeqPos>& display_range = GetRange();
                const CTrna_ext& trna = display_range.IsWhole() ?
                    rna.GetExt().GetTRNA() :
                    *sequence::CFeatTrim::Apply(rna.GetExt().GetTRNA(), display_range);

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
bool CGff2Writer::xAssignFeatureAttributeCodeBreak(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetData()  ||  !mf.GetData().IsCdregion()) {
        return true;
    }
    const CSeqFeatData::TCdregion& cds = mf.GetData().GetCdregion();
    if (!cds.IsSetCode_break()) {
        return true;
    }

    const list<CRef<CCode_break> >& code_breaks = cds.GetCode_break();

    const CRange<TSeqPos>& display_range = GetRange();
    if (!display_range.IsWhole()) { // Trim the code breaks before writing
        for (CRef<CCode_break> code_break : code_breaks) {
            string cbString;
            CRef<CCode_break> trimmed_cb = sequence::CFeatTrim::Apply(*code_break, display_range);
            if (trimmed_cb.NotEmpty() &&
                CWriteUtil::GetCodeBreak(*trimmed_cb, cbString)) {
                record.AddAttribute("transl_except", cbString);
            }
        }
        return true;
    }

    list<CRef<CCode_break> >::const_iterator it = code_breaks.begin();
    for (; it != code_breaks.end(); ++it) {
        string cbString;
        if (CWriteUtil::GetCodeBreak(**it, cbString)) {
            record.AddAttribute("transl_except", cbString);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
const CGene_ref&
sGetClosestGeneRef(
    CGffFeatureContext& fc,
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    static const CGene_ref noRef;
    if (mf.GetData().IsGene()) {
        return mf.GetData().GetGene();
    }
    // do not use xref gene ref directly !!!
    CMappedFeat gene = fc.FindBestGeneParent(mf);
    if (gene  &&  gene.IsSetData()  &&  gene.GetData().IsGene()) {
        return gene.GetData().GetGene();
    }
    return noRef;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributesGene(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    const auto& geneRef = sGetClosestGeneRef(fc, mf);
    if (geneRef.IsSetLocus()) {
        record.SetAttribute("gene", geneRef.GetLocus());
    }
    if (geneRef.IsSetLocus_tag()) {
        record.SetAttribute("locus_tag", geneRef.GetLocus_tag());
    }
    if (mf.GetData().IsGene()) {
        if (geneRef.IsSetDesc()) {
            record.SetAttribute("description", geneRef.GetDesc());
        }
        if (geneRef.IsSetSyn()) {
            const auto& syns = geneRef.GetSyn();
            auto it = syns.begin();
            while (it != syns.end()) {
                record.AddAttribute("gene_synonym", *(it++));
            }
        }
    }
    return true;

}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributeOldLocusTag(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    if (!mf.GetData().IsGene()) {
        return true;
    }
    if (!mf.IsSetQual()) {
        return true;
    }
    string old_locus_tags;
    vector<CRef<CGb_qual> > quals = mf.GetQual();
    for (vector<CRef<CGb_qual> >::const_iterator it = quals.begin();
            it != quals.end(); ++it) {
        if ((**it).IsSetQual() && (**it).IsSetVal()) {
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
bool CGff2Writer::xAssignFeatureAttributeGeneBiotype(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    // if a biosource is present then only compute if also is genomic record
    // if a biosource is not present then always compute
    if (!mf.GetData().IsGene()) {
        return true;
    }
    if (fc.HasSequenceBioSource()  &&  !fc.IsSequenceGenomicRecord()) {
        return true;
    }

    string biotype;
    if (!feature::GetFeatureGeneBiotypeFaster(fc.FeatTree(), mf, biotype)) {
        return true;
    }
    record.SetAttribute("gene_biotype", biotype);
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributeMapLoc(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.GetData().IsGene()) {
        return true;
    }
    const CGene_ref& gene_ref = mf.GetData().GetGene();
    if (!gene_ref.IsSetMaploc()) {
        return true;
    }
    record.SetAttribute("map", gene_ref.GetMaploc());
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributeException(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf )
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
bool CGff2Writer::xAssignFeatureAttributeExperiment(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    vector<string> experiments;
    const auto& quals = mf.GetQual();
    for (const auto& qual: quals) {
        if (qual->GetQual() == "experiment") {
           experiments.push_back(qual->GetVal());
        }
    }
    if (!experiments.empty()) {
        record.SetAttributes("experiment", experiments);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributeModelEvidence(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    string modelEvidence;
    if (!CWriteUtil::GetStringForModelEvidence(mf, modelEvidence)) {
        return true;
    }
    if (!modelEvidence.empty()) {
        record.SetAttribute("model_evidence", modelEvidence);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributeRptFamily(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf)
//  -----------------------------------------------------------------------------
{
    CSeqFeatData::ESubtype s = mf.GetFeatSubtype();
    switch (s) {
        default:
            return true;
        case CSeqFeatData::eSubtype_oriT:
        case CSeqFeatData::eSubtype_repeat_region: {
            const CSeq_feat::TQual& quals = mf.GetQual();
            if (quals.empty()) {
                return true;
            }
            for (CSeq_feat::TQual::const_iterator cit = quals.begin();
                cit != quals.end(); ++cit) {
                if ((*cit)->GetQual() == "rpt_family") {
                    record.SetAttribute("rpt_family", (*cit)->GetVal());
                    return true;
                }
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributePseudoGene(
    CGffFeatureRecord& record,
    CGffFeatureContext& fc,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    string pseudoGene = mf.GetNamedQual("pseudogene");
    if (!pseudoGene.empty()) {
        record.SetAttribute("pseudogene", pseudoGene);
        return true;
    }
    if (!CSeqFeatData::IsLegalQualifier(
            mf.GetFeatSubtype(), CSeqFeatData::eQual_pseudogene)) {
        return true;
    }
    CMappedFeat gene = fc.FindBestGeneParent(mf);
    if (!gene) {
        return true;
    }
    pseudoGene = gene.GetNamedQual("pseudogene");
    if (!pseudoGene.empty()) {
        record.SetAttribute("pseudogene", pseudoGene);
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributeIsOrdered(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (CWriteUtil::IsLocationOrdered(mf.GetLocation())) {
        record.SetAttribute("is_ordered", "true");
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributeFunction(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    const string& function = mf.GetNamedQual("function");
    if (!function.empty()) {
        record.SetAttribute("function", function);
        return true;
    }
    if (CSeqFeatData::e_Prot != mf.GetFeatType()) {
        return true;
    }
    const CProt_ref& prot = mf.GetData().GetProt();
    if (prot.CanGetActivity()  &&  !prot.GetActivity().empty()) {
        record.SetAttribute("function", prot.GetActivity().front());
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributesGoMarkup(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    const string& go_component = mf.GetNamedQual("go_component");
    if (!go_component.empty()) {
        record.SetAttribute("go_component", go_component);
        return true;
    }
    if (!mf.IsSetExt()) {
        return true;
    }

    const auto& ext = mf.GetExt();
    if (!ext.IsSetType()  ||  !ext.GetType().IsStr()) {
        return true;
    }
    if (ext.GetType().GetStr() == "GeneOntology") {
        list<string> goIds;
        const auto& goFields = ext.GetData();
        for (const auto& goField: goFields) {
            if (!goField->IsSetLabel()  ||  !goField->GetLabel().IsStr()) {
                continue;
            }
            const auto& goLabel = goField->GetLabel().GetStr();
            if (goLabel == "Component"  &&  goField->IsSetData()
                    &&  goField->GetData().IsFields()) {
                const auto& fields = goField->GetData().GetFields();
                vector<string> goStrings;
                if (CWriteUtil::GetStringsForGoMarkup(fields, goStrings)) {
                    record.SetAttributes("go_component", goStrings);
                }
                CWriteUtil::GetListOfGoIds(fields, goIds);
                continue;
            }
            if (goLabel == "Process"  &&  goField->IsSetData()
                    &&  goField->GetData().IsFields()) {
                const auto& fields = goField->GetData().GetFields();
                vector<string> goStrings;
                if (CWriteUtil::GetStringsForGoMarkup(fields, goStrings)) {
                    record.SetAttributes("go_process", goStrings);
                }
                CWriteUtil::GetListOfGoIds(fields, goIds);
                continue;
            }
            if (goLabel == "Function"  &&  goField->IsSetData()
                    &&  goField->GetData().IsFields()) {
                const auto& fields = goField->GetData().GetFields();
                vector<string> goStrings;
                if (CWriteUtil::GetStringsForGoMarkup(fields, goStrings)) {
                    record.SetAttributes("go_function", goStrings);
                }
                CWriteUtil::GetListOfGoIds(fields, goIds);
                continue;
            }
        }
        if (!goIds.empty()) {
            record.SetAttributes("Ontology_term", vector<string>(goIds.begin(), goIds.end()));
        }
    } else if (ext.GetType().GetStr() == "CombinedFeatureUserObjects") {
        const CUser_object::TData & ext_data = ext.GetData();
        ITERATE (CSeq_feat::TExt::TData, it, ext_data) {
            const CUser_field & field = **it;
            if( ! field.IsSetLabel() || ! field.IsSetData()  ) {
                continue;
            }
            const CUser_field::TLabel & field_label = field.GetLabel();
            const CUser_field::TData & field_data = field.GetData();
            if (field_label.IsStr() && field_label.GetStr() == "GeneOntology") {
                if (field_data.IsFields()) {
                    list<string> goIds;
                    const auto& goFields = field.GetData().GetFields();
                    for (const auto& goField: goFields) {
                        if (!goField->IsSetLabel()  ||  !goField->GetLabel().IsStr()) {
                            continue;
                        }
                        const auto& goLabel = goField->GetLabel().GetStr();
                        if (goLabel == "Component"  &&  goField->IsSetData()
                                &&  goField->GetData().IsFields()) {
                            const auto& fields = goField->GetData().GetFields();
                            vector<string> goStrings;
                            if (CWriteUtil::GetStringsForGoMarkup(fields, goStrings, true)) {
                                record.SetAttributes("go_component", goStrings);
                            }
                            CWriteUtil::GetListOfGoIds(fields, goIds, true);
                            continue;
                        }
                        if (goLabel == "Process"  &&  goField->IsSetData()
                                &&  goField->GetData().IsFields()) {
                            const auto& fields = goField->GetData().GetFields();
                            vector<string> goStrings;
                            if (CWriteUtil::GetStringsForGoMarkup(fields, goStrings, true)) {
                                record.SetAttributes("go_process", goStrings);
                            }
                            CWriteUtil::GetListOfGoIds(fields, goIds, true);
                            continue;
                        }
                        if (goLabel == "Function"  &&  goField->IsSetData()
                                &&  goField->GetData().IsFields()) {
                            const auto& fields = goField->GetData().GetFields();
                            vector<string> goStrings;
                            if (CWriteUtil::GetStringsForGoMarkup(fields, goStrings, true)) {
                                record.SetAttributes("go_function", goStrings);
                            }
                            CWriteUtil::GetListOfGoIds(fields, goIds, true);
                            continue;
                        }
                    }
                    if (!goIds.empty()) {
                        record.SetAttributes("Ontology_term", vector<string>(goIds.begin(), goIds.end()));
                    }
                }
            }
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributeEcNumbers(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (CSeqFeatData::e_Prot != mf.GetFeatType()) {
        return true;
    }
    const CProt_ref& prot = mf.GetData().GetProt();
    if (prot.CanGetEc()) {
        const list<string> ec = prot.GetEc();
        if (!ec.empty()) {
            record.SetAttributes(
                "ec_number", vector<string>(ec.begin(), ec.end()));
        }
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xAssignFeatureAttributeExonNumber(
    CGffFeatureRecord& record,
    CGffFeatureContext&,
    const CMappedFeat& mf )
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
bool CGff2Writer::IsTranscriptType(
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
   auto itType = std::find(
        acceptableTranscriptTypes.begin(), acceptableTranscriptTypes.end(),
        mf.GetFeatSubtype());
    return (itType != acceptableTranscriptTypes.end());
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::HasAccaptableTranscriptParent(
    CGffFeatureContext& context,
    const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    CMappedFeat parent = context.FeatTree().GetParent(mf);
    if (!parent) {
        return false;
    }
    return IsTranscriptType(parent);
}

//  ----------------------------------------------------------------------------
CMappedFeat CGff2Writer::xGenerateMissingTranscript(
    CGffFeatureContext& context,
    const CMappedFeat& mf )
//  ----------------------------------------------------------------------------
{
    if (!xGeneratingMissingTranscripts()) {
        return CMappedFeat();
    }
    if (HasAccaptableTranscriptParent(context, mf)) {
        return CMappedFeat();
    }
    CRef<CSeq_feat> pMissingTranscript(new CSeq_feat);
    pMissingTranscript.Reset(new CSeq_feat);
    pMissingTranscript->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    pMissingTranscript->SetLocation().Assign(mf.GetLocation());
    pMissingTranscript->SetLocation().SetPartialStart(false, eExtreme_Positional);
    pMissingTranscript->SetLocation().SetPartialStop(false, eExtreme_Positional);
    pMissingTranscript->ResetPartial();

    CScope& scope = mf.GetScope();
    CSeq_annot_Handle sah = mf.GetAnnot();
    CSeq_annot_EditHandle saeh = sah.GetEditHandle();
    saeh.AddFeat(*pMissingTranscript);
    CMappedFeat tf = scope.GetObjectHandle(*pMissingTranscript);
    context.FeatTree().AddFeature(tf);
    return tf;
}

//  ----------------------------------------------------------------------------
bool CGff2Writer::xIntervalsNeedPartNumbers(
    const list<CRef<CSeq_interval>>& sublocs)
//  ----------------------------------------------------------------------------
{
    _ASSERT(sublocs.size());
    if (sublocs.size() == 1) { // most common
        return false;
    }

    const auto& front = *sublocs.front();
    auto frontStrand = CWriteUtil::GetEffectiveStrand(front);
    auto lastStart = front.GetFrom();
    for (auto itComp = sublocs.begin()++; itComp != sublocs.end(); itComp++) {
        const auto& comp = **itComp;
        if (frontStrand != CWriteUtil::GetEffectiveStrand(comp)) {
            return true;
        }
        auto compStart = comp.GetFrom();
        if (frontStrand == eNa_strand_plus  &&  lastStart > compStart) {
            return true;
        }
        if (frontStrand == eNa_strand_minus  &&  lastStart < compStart) {
            return true;
        }
        lastStart = compStart;
    }
    return false;
}


END_NCBI_SCOPE
