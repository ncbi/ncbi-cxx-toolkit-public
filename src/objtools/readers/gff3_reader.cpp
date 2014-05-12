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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   GFF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/stream_utils.hpp>

#include <util/static_map.hpp>
#include <util/line_reader.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/edit/cds_fix.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/gff3_sofa.hpp>
#include <objtools/readers/gff2_reader.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/readers/gff2_data.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>

//#include "gff3_data.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ****************************************************************************

//  ----------------------------------------------------------------------------
class CFeatureTableEditor
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_feat> > FEATS;

public:
    CFeatureTableEditor(
        CSeq_annot&,
        IMessageListener* =0);

    void InferParentMrnas();
    void InferParentGenes();
    void InferPartials();

protected:
    string xNextFeatId();
    CRef<CSeq_feat> xMakeGeneForMrna(
        const CSeq_feat&,
        CScope&);

    CSeq_annot& mAnnot;
    IMessageListener* mpMessageListener;
    unsigned int mNextFeatId;
};

//  -------------------------------------------------------------------------
CFeatureTableEditor::CFeatureTableEditor(
    CSeq_annot& annot,
    IMessageListener* pMessageListener):
//  -------------------------------------------------------------------------
    mAnnot(annot),
    mpMessageListener(pMessageListener),
    mNextFeatId(1)
{
}

//  -------------------------------------------------------------------------
void CFeatureTableEditor::InferParentMrnas()
//  -------------------------------------------------------------------------
{
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_annot_Handle sah = scope.AddSeq_annot(mAnnot);
    CSeq_annot_EditHandle saeh = scope.GetEditHandle(sah);

    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI it(sah, sel);
    for ( ; it; ++it) {
        const CSeq_feat& cds = it->GetOriginalFeature();
        CRef<CSeq_feat> pRna = edit::MakemRNAforCDS(cds, scope);
        if (!pRna) {
            continue;
        }
        //find proper name for rna
        string rnaId(xNextFeatId());
        pRna->SetId().SetLocal().SetStr(rnaId);
        //add rna xref to cds
        CSeq_feat_EditHandle feh(scope.GetObjectHandle(cds));
        feh.AddFeatXref(rnaId);
        //add new rna to feature table
        saeh.AddFeat(*pRna);
    }
}

//  ---------------------------------------------------------------------------
void CFeatureTableEditor::InferParentGenes()
//  ---------------------------------------------------------------------------
{
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_annot_Handle sah = scope.AddSeq_annot(mAnnot);
    CSeq_annot_EditHandle saeh = scope.GetEditHandle(sah);

    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    CFeat_CI it(sah, sel);
    for ( ; it; ++it) {
        const CSeq_feat& rna = it->GetOriginalFeature();
        CRef<CSeq_feat> pGene = xMakeGeneForMrna(rna, scope);
        const CSeq_loc& rnaLoc = rna.GetLocation();
        if (!pGene) {
            continue;
        }
        //find proper name for gene
        string geneId(xNextFeatId());
        pGene->SetId().SetLocal().SetStr(geneId);
        //add gene xref to rna
        CSeq_feat_EditHandle feh(scope.GetObjectHandle(rna));
        feh.AddFeatXref(geneId);
        //add new gene to feature table
        saeh.AddFeat(*pGene);
    }
}

//  ----------------------------------------------------------------------------
void CFeatureTableEditor::InferPartials()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
string CFeatureTableEditor::xNextFeatId()
//  ----------------------------------------------------------------------------
{
    const int WIDTH = 6;
    const string padding = string(WIDTH, '0');
    string suffix = NStr::NumericToString(mNextFeatId++);
    if (suffix.size() < WIDTH) {
        suffix = padding.substr(0, WIDTH-suffix.size()) + suffix;
    }
    string nextTag("auto");
    return nextTag + suffix;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_feat> CFeatureTableEditor::xMakeGeneForMrna(
    const CSeq_feat& rna,
    CScope& scope)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pGene;
    CSeq_feat_Handle sfh = scope.GetSeq_featHandle(rna);
    CSeq_annot_Handle sah = sfh.GetAnnot();
    if (!sah) {
        return pGene;
    }
    CConstRef<CSeq_feat> pExistingGene;
    size_t bestLength(0);
    CFeat_CI findGene(sah, CSeqFeatData::eSubtype_gene);
    for ( ; findGene; ++findGene) {
        Int8 compare = sequence::TestForOverlap64(
            findGene->GetLocation(), rna.GetLocation(),
            sequence::eOverlap_CheckIntervals);
        if (compare == -1) {
            continue;
        }
        size_t currentLength = sequence::GetLength(findGene->GetLocation(), &scope);
        if (!bestLength  ||  currentLength > bestLength) {
            pExistingGene.Reset(&(findGene->GetOriginalFeature()));
            bestLength = currentLength;
        }
    }
    if (pExistingGene) {
        const CSeq_interval& geneLoc = pGene->GetLocation().GetInt();
        return pGene;
    }
    pGene.Reset(new CSeq_feat);
    pGene->SetLocation().SetInt();
    pGene->SetLocation().SetId(*rna.GetLocation().GetId());
    pGene->SetLocation().SetInt().SetFrom(rna.GetLocation().GetStart(
        eExtreme_Positional));
    pGene->SetLocation().SetInt().SetTo(rna.GetLocation().GetStop(
        eExtreme_Positional));
    pGene->SetData().SetGene();
    return pGene;
}

//  ****************************************************************************
//  ----------------------------------------------------------------------------
string CGff3ReadRecord::x_NormalizedAttributeKey(
    const string& strRawKey )
//  ---------------------------------------------------------------------------
{
    string strKey = CGff2Record::x_NormalizedAttributeKey( strRawKey );
    if ( 0 == NStr::CompareNocase( strRawKey, "ID" ) ) {
        return "ID";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Name" ) ) {
        return "Name";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Alias" ) ) {
        return "Alias";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Parent" ) ) {
        return "Parent";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Target" ) ) {
        return "Target";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Gap" ) ) {
        return "Gap";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Derives_from" ) ) {
        return "Derives_from";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Note" ) ) {
        return "Note";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Dbxref" )  ||
        0 == NStr::CompareNocase( strKey, "Db_xref" ) ) {
        return "Dbxref";
    }
    if ( 0 == NStr::CompareNocase( strKey, "Ontology_term" ) ) {
        return "Ontology_term";
    }
    return strKey;
}

//  ----------------------------------------------------------------------------
CGff3Reader::CGff3Reader(
    unsigned int uFlags,
    const string& name,
    const string& title ):
//  ----------------------------------------------------------------------------
    CGff2Reader( uFlags, name, title )
{
}

//  ----------------------------------------------------------------------------
CGff3Reader::~CGff3Reader()
//  ----------------------------------------------------------------------------
{
}

//  ---------------------------------------------------------------------------
void
CGff3Reader::FixupSeqAnnot(
    CRef<CSeq_annot> pAnnot,
    IMessageListener* pMessageListener )
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_feat> > FEATS;

    if (!pAnnot->IsFtable()) {
        return;
    }
    
    cerr << "";
    CFeatureTableEditor tableEditor(*pAnnot);
    if (IsGenbankMode()) {
        tableEditor.InferParentMrnas();
        //tableEditor.InferParentGenes();
        //tableEditor.InferPartials();
    }
    cerr << "";
}
      
//  ----------------------------------------------------------------------------
bool CGff3Reader::x_UpdateFeatureCds(
    const CGff2Record& gff,
    CRef<CSeq_feat> pFeature)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pAdd = CRef<CSeq_feat>(new CSeq_feat);
    if (!x_FeatureSetLocation(gff, pAdd)) {
        return false;
    }
    pFeature->SetLocation().Add(pAdd->GetLocation());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::x_UpdateAnnotFeature(
    const CGff2Record& record,
    CRef< CSeq_annot > pAnnot,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_feat > pFeature(new CSeq_feat);

    string type = record.Type();
    if (type == "exon"  ||  type == "five_prime_UTR"  ||  type == "three_prime_UTR") {
        return xUpdateAnnotExon(record, pFeature, pAnnot, pEC);
    }
    if (type == "CDS"  ||  type == "cds") {
        return xUpdateAnnotCds(record, pFeature, pAnnot, pEC);
    }
    if (type == "gene") {
        return xUpdateAnnotGene(record, pFeature, pAnnot, pEC);
    }
    return xUpdateAnnotGeneric(record, pFeature, pAnnot, pEC);
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotExon(
    const CGff2Record& record,
    CRef<CSeq_feat>,
    CRef<CSeq_annot>,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    list<string> parents;
    if (record.GetAttribute("Parent", parents)) {
        for (list<string>::const_iterator it = parents.begin(); it != parents.end(); 
                ++it) {
            IdToFeatureMap::iterator fit = m_MapIdToFeature.find(*it);
            if (fit != m_MapIdToFeature.end()) {
                if (!record.UpdateFeature(m_iFlags, fit->second)) {
                    return false;
                }
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotCds(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CRef<CSeq_annot> pAnnot,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    if (!xVerifyCdsParents(record)) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Fatal,
            0,
            "Bad data line: CDS record with bad parent assignments.",
            ILineError::eProblem_FeatureBadStartAndOrStop) );
        pErr->SetLineNumber(m_uLineNumber);
        ProcessError(*pErr, pEC);
    }
    list<string> parents;
    if (record.GetAttribute("Parent", parents)) {
        for (list<string>::const_iterator it = parents.begin(); it != parents.end(); 
                ++it) {
            IdToFeatureMap::iterator fit = m_MapIdToFeature.find(*it);
            if (fit != m_MapIdToFeature.end()) {
                CRef<CSeq_feat> pF = fit->second;
                const CSeq_loc& loc = pF->GetLocation();
                if (!record.UpdateFeature(m_iFlags, fit->second)) {
                    return false;
                }
            }
        }
    }
    string id;
    if (record.GetAttribute("ID", id)) {
        IdToFeatureMap::iterator it = m_MapIdToFeature.find(id);
        if (it != m_MapIdToFeature.end()) {
            return record.UpdateFeature(m_iFlags, it->second);
        }
    }

    if (!record.InitializeFeature(m_iFlags, pFeature)) {
        return false;
    }
    if (!xFeatureSetLocusTag(record, pFeature)) {
        return false;
    }
    if (!x_FeatureSetXref(record, pFeature)) {
        return false;
    }
    if (! x_AddFeatureToAnnot( pFeature, pAnnot )) {
        return false;
    }
    if ( !id.empty() ) {
        m_MapIdToFeature[ id ] = pFeature;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xFeatureSetLocusTag(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    string locusTag;
    if (record.Type() == "gene") {
        //create a brand new locus_tag
        locusTag = xNextLocusTag();
    }
    else {
        //find a suitable locus_tag to inherit
        list<string> parents;
        if (!record.GetAttribute("Parent", parents)) {
            //give up on the locus_tag, but allow processing to continue
            return true;
        }
        map<string, string>::iterator cit = mLocusTagMap.find(parents.front());
        if (cit == mLocusTagMap.end()) {
            //give up on the locus_tag, but allow processing to continue
            return true;
        }
        locusTag = cit->second;
    }
    CRef<CGb_qual> pLocusTag(new CGb_qual);
    pLocusTag->SetQual("locus_tag");
    pLocusTag->SetVal(locusTag);
    pFeature->SetQual().push_back(pLocusTag);

    string id;
    if (!record.GetAttribute("ID", id)) {
        return true;
    }
    mLocusTagMap[id] = locusTag;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotGeneric(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CRef<CSeq_annot> pAnnot,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    string id;
    if (record.GetAttribute("ID", id)) {
        IdToFeatureMap::iterator it = m_MapIdToFeature.find(id);
        if (it != m_MapIdToFeature.end()) {
            return record.UpdateFeature(m_iFlags, it->second);
        }
    }

    if (!record.InitializeFeature(m_iFlags, pFeature)) {
        return false;
    }
    if (IsLocusTagMode() && !xFeatureSetLocusTag(record, pFeature)) {
        return false;
    }
    if (!x_FeatureSetXref(record, pFeature)) {
        return false;
    }
    if (! x_AddFeatureToAnnot(pFeature, pAnnot)) {
        return false;
    }
    string strId;
    if ( record.GetAttribute("ID", strId)) {
        m_MapIdToFeature[strId] = pFeature;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotGene(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CRef<CSeq_annot> pAnnot,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    return xUpdateAnnotGeneric(record, pFeature, pAnnot, pEC);
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::x_AddFeatureToAnnot(
    CRef< CSeq_feat > pFeature,
    CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    pAnnot->SetData().SetFtable().push_back( pFeature ) ;
    return true;
}

//  ============================================================================
bool CGff3Reader::xAnnotPostProcess(
    CRef<CSeq_annot> pAnnot)
//  ============================================================================
{
    typedef list< CRef< CSeq_feat > > FTABLE;
    typedef FTABLE::iterator FEATIT;
    typedef vector< CRef< CSeqFeatXref > > XREFS;
    typedef XREFS::iterator XREFIT;

    CSeq_annot& annot = *pAnnot;
    if (!annot.GetData().IsFtable()) {
        return true; //don't mess up alignments!!!
    }
    FTABLE& ftable = annot.SetData().SetFtable();
    for (FEATIT it = ftable.begin(); it != ftable.end(); ++it) {
        CSeq_feat& feat = **it;
        if (!feat.GetData().IsCdregion()  ||  feat.GetXref().size() <= 1) {
            continue;
        }

        XREFS& xrefs = feat.SetXref();
        while (xrefs.size() > 1) {
            //turn each CDS with multiple parent xrefs into a set of
            // CDSs each with a single parent xref

            //select xref to slice out
            XREFIT xit = xrefs.begin()+1;
 
           //create new feature
            CRef<CSeq_feat> pNew(new CSeq_feat);
            pNew->Assign(feat);
            pNew->SetXref().clear();
            pNew->SetXref().push_back(*xit);
            //newFeats.push_back(pNew);
            ftable.insert(it, pNew);

            //remove the spliced out xref
            xrefs.erase(xit);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xVerifyCdsParents(
    const CGff2Record& record)
//  ----------------------------------------------------------------------------
{
    string id;
    string parents;
    if (!record.GetAttribute("ID", id)) {
        return true;
    }
    record.GetAttribute("Parent", parents);
    map<string, string>::iterator it = mCdsParentMap.find(id);
    if (it == mCdsParentMap.end()) {
        mCdsParentMap[id] = parents;
        return true;
    }
    return (it->second == parents);
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xReadInit()
//  ----------------------------------------------------------------------------
{
    if (!CGff2Reader::xReadInit()) {
        return false;
    }
    mCdsParentMap.clear();
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
