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
 *   GFF3 file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <util/line_reader.hpp>


#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/so_map.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Imp_feat.hpp>

#include <objtools/readers/gff3_reader.hpp>
#include <objtools/readers/gff3_location_merger.hpp>

#include "reader_message_handler.hpp"

#include <algorithm>
#include <util/regexp/ctre/ctre.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

unsigned int CGff3Reader::msGenericIdCounter = 0;

//  ----------------------------------------------------------------------------
string CGff3Reader::xMakeRecordId(
    const CGff2Record& record)
//  ----------------------------------------------------------------------------
{
    string id, parentId;
    record.GetAttribute("ID", id);
    record.GetAttribute("Parent", parentId);

    auto recordType = record.NormalizedType();
    if (recordType == "cds") {
        string cdsId = parentId;
        if (cdsId.empty()) {
            cdsId = (id.empty() ? xNextGenericId() : id);
        }
        else {
            cdsId += ":cds";
        }
        return cdsId;
    }
    if (id.empty()) {
        return xNextGenericId();
    }
    return id;
}

//  ----------------------------------------------------------------------------
string CGff3Reader::xNextGenericId()
//  ----------------------------------------------------------------------------
{
    return string("generic") + NStr::IntToString(msGenericIdCounter++);
}

//  ----------------------------------------------------------------------------
string CGff3ReadRecord::GetOneAttribute(const string& attr)
//  ----------------------------------------------------------------------------
{
    string val;
    GetAttribute(attr, val);
    return val;
}

//  ----------------------------------------------------------------------------
bool CGff3ReadRecord::AssignFromGff(
    const string& strRawInput )
//  ----------------------------------------------------------------------------
{
    if (!CGff2Record::AssignFromGff(strRawInput)) {
        return false;
    }
    string id, parent;
    GetAttribute("ID", id);
    GetAttribute("Parent", parent);

    if (! parent.empty() && parent == id) {
        throw CReaderMessage(eDiag_Error, 0,
                "Bad data line: "
                "ID and Parent have the same value \"" + parent + "\"");
    }

    if (m_strType == "protein_coding_gene" ||
        m_strType == "ncRNA_gene") {
        SetType("gene");
        return true;
    }

    if (m_strType == "pseudogene") {
        SetType("gene");
        m_Attributes["pseudo"] = "true";
        return true;
    }
    if (m_strType == "pseudogenic_transcript") {
        SetType("transcript");
        m_Attributes["pseudo"] = "true";
        return true;
    }
    if (m_strType == "pseudogenic_tRNA") {
        SetType("tRNA");
        m_Attributes["pseudo"] = "true";
        return true;
    }
    if (m_strType == "pseudogenic_rRNA") {
        SetType("rRNA");
        m_Attributes["pseudo"] = "true";
        return true;
    }
    if (m_strType == "pseudogenic_exon") {
        SetType("exon");
        return true;
    }
    if (m_strType == "pseudogenic_CDS") {
        SetType("CDS");
        m_Attributes["pseudo"] = "true";
        return true;
    }
    if (m_strType == "transcript") {
        SetType("misc_RNA");
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
string CGff3ReadRecord::x_NormalizedAttributeKey(
    const string& strRawKey )
//  ---------------------------------------------------------------------------
{
    string strKey = CGff2Record::xNormalizedAttributeKey( strRawKey );
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
    TReaderFlags uFlags,
    const string& name,
    const string& title,
    SeqIdResolver resolver,
    CReaderListener* pRL):
//  ----------------------------------------------------------------------------
    CGff2Reader( uFlags, name, title, resolver, pRL )
{
    mpLocations.reset(new CGff3LocationMerger(uFlags, resolver, 0, pRL));
    CGff2Record::ResetId();
}

//  ----------------------------------------------------------------------------
CGff3Reader::CGff3Reader(
    unsigned int uFlags,
    CReaderListener* pRL):
//  ----------------------------------------------------------------------------
    CGff3Reader(uFlags, "", "", CReadUtil::AsSeqId, pRL)
{
}

//  ----------------------------------------------------------------------------
CGff3Reader::~CGff3Reader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
CRef<CSeq_annot>
CGff3Reader::ReadSeqAnnot(
    ILineReader& lr,
    ILineErrorListener* pEC )
//  ----------------------------------------------------------------------------
{
    mCurrentFeatureCount = 0;
    mParsingAlignment = false;
    mAlignmentData.Reset();
    mpLocations->Reset();
    auto pAnnot = CReaderBase::ReadSeqAnnot(lr, pEC);
    if (pAnnot  &&  pAnnot->GetData().Which() == CSeq_annot::TData::e_not_set) {
        return CRef<CSeq_annot>();
    }
    return pAnnot;
}

//  ----------------------------------------------------------------------------
void
CGff3Reader::xProcessData(
    const TReaderData& readerData,
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    for (const auto& lineData: readerData) {
        const auto& line = lineData.mData;
        if (xParseStructuredComment(line)  &&
                !NStr::StartsWith(line, "##sequence-region") ) {
            continue;
        }
        if (xParseBrowserLine(line, annot)) {
            continue;
        }
        if (xParseFeature(line, annot, nullptr)) {
            continue;
        }
    }
}

//  ----------------------------------------------------------------------------
void CGff3Reader::xProcessAlignmentData(
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    for (const string& id : mAlignmentData.mIds) {
        CRef<CSeq_align> pAlign = Ref(new CSeq_align());
        if (x_MergeAlignments(mAlignmentData.mAlignments.at(id), pAlign)) {
            // if available, add current browser information
            if ( m_CurrentBrowserInfo ) {
                annot.SetDesc().Set().push_back( m_CurrentBrowserInfo );
            }

            annot.SetNameDesc("alignments");

            if ( !m_AnnotTitle.empty() ) {
                annot.SetTitleDesc(m_AnnotTitle);
            }
            // Add alignment
            annot.SetData().SetAlign().push_back(pAlign);
        }
    }
}

//  ----------------------------------------------------------------------------
bool
CGff3Reader::xParseFeature(
    const CTempString& line,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    if (CGff2Reader::IsAlignmentData(line)) {
        return xParseAlignment(line);
    }

    //parse record:
    shared_ptr<CGff3ReadRecord> pRecord(x_CreateRecord());
    try {
        if (!pRecord->AssignFromGff(line)) {
            return false;
        }

        string id = pRecord->GetOneAttribute("ID");
        string parent = pRecord->GetOneAttribute("Parent");

        if (! parent.empty()) {
            if (mIDsAlreadySeen.count(parent) == 0) {
cerr << "Parent " << parent << " not yet seen for ID " << id << endl;
                /*
                throw CReaderMessage(eDiag_Error, 0,
                            "Bad data line: "
                    "Parent \"" + parent + " not yet found as ID\"");
                */
            }
        }
        if (! id.empty()) {
            mIDsAlreadySeen.insert(id);
        }
    }
    catch(CObjReaderLineException& err) {
        ProcessError(err, pEC);
        return false;
    }

    //make sure we are interested:
    if (xIsIgnoredFeatureType(pRecord->Type())) {
        return true;
    }
    if (xIsIgnoredFeatureId(pRecord->Id())) {
        return true;
    }

    //no support for multiparented features in genbank mode:
    if (pRecord->IsMultiParent()) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Fatal,
            0,
            "This GFF3 reader does not support multiparented features"));
        ProcessError(*pErr, pEC);
    }

    //append feature to annot:
    if (!xUpdateAnnotFeature(*pRecord, annot, pEC)) {
        return false;
    }

    ++mCurrentFeatureCount;
    mParsingAlignment = false;
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff3Reader::xParseAlignment(
    const string& strLine)
//  ----------------------------------------------------------------------------
{
    if (IsInGenbankMode()) {
        return true;
    }
    auto& ids = mAlignmentData.mIds;
    auto& alignments = mAlignmentData.mAlignments;

    unique_ptr<CGff2Record> pRecord(x_CreateRecord());

    if ( !pRecord->AssignFromGff(strLine) ) {
        return false;
    }

    string id;
    if ( !pRecord->GetAttribute("ID", id) ) {
        id = pRecord->Id();
    }

    if (alignments.find(id) == alignments.end()) {
       ids.push_back(id);
    }

    CRef<CSeq_align> alignment;
    if (!x_CreateAlignment(*pRecord, alignment)) {
        return false;
    }

    alignments[id].push_back(alignment);

    ++mCurrentFeatureCount;
    mParsingAlignment = true;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotFeature(
    const CGff2Record& gffRecord,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    //if (gffRecord.Type() == "CDS") {
    //    if (gffRecord.SeqStart() == 114392786) {
    //        cerr << "";
    //    }
    //}

    mpLocations->AddRecord(gffRecord);

    CRef< CSeq_feat > pFeature(new CSeq_feat);

    auto recType = gffRecord.NormalizedType();
    if (recType == "exon" || recType == "five_prime_utr" || recType == "three_prime_utr") {
        return xUpdateAnnotExon(gffRecord, pFeature, annot, pEC);
    }
    if (recType == "cds") {
        return xUpdateAnnotCds(gffRecord, pFeature, annot, pEC);
    }
    if (recType == "gene") {
         return xUpdateAnnotGene(gffRecord, pFeature, annot, pEC);
    }
    if (NStr::EndsWith(recType, "rna")) {
        return xUpdateAnnotRna(gffRecord, pFeature, annot, pEC);
    }
    // allow exon before VDJC gene segment to not crash as data error
    if (NStr::EndsWith(recType, "_gene_segment")) {
        return xUpdateAnnotRna(gffRecord, pFeature, annot, pEC);
    }
    if (recType == "region") {
        return xUpdateAnnotRegion(gffRecord, pFeature, annot, pEC);
    }
    if (!xUpdateAnnotGeneric(gffRecord, pFeature, annot, pEC)) {
        return false;
    }
    return true;
}


//  ----------------------------------------------------------------------------
void CGff3Reader::xVerifyExonLocation(
    const string& mrnaId,
    const CGff2Record& exon)
//  ----------------------------------------------------------------------------
{
    map<string,CRef<CSeq_interval> >::const_iterator cit = mMrnaLocs.find(mrnaId);
    if (cit == mMrnaLocs.end()) {
        string message = "Bad data line: ";
        message += exon.Type();
        message += " referring to non-existent parent feature.";
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            message);
        throw error;
    }
    const CSeq_interval& containingInt = cit->second.GetObject();
    const CRef<CSeq_loc> pContainedLoc = exon.GetSeqLoc(m_iFlags, mSeqIdResolve);
    const CSeq_interval& containedInt = pContainedLoc->GetInt();
    if (containedInt.GetFrom() < containingInt.GetFrom()  ||
            containedInt.GetTo() > containingInt.GetTo()) {
        string message = "Bad data line: ";
        message += exon.Type();
        message += " extends beyond parent feature.";
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            message);
        throw error;
    }
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotExon(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    string parentId;
    if (record.GetAttribute("Parent", parentId)) {
        CRef<CSeq_feat> pParent;
        if (!x_GetFeatureById(parentId, pParent)) {
            return true;
        }
        if (pParent->GetData().IsRna()) {
            xVerifyExonLocation(parentId, record);
        }
        if (pParent->GetData().IsGene()) {
            if (!xInitializeFeature(record, pFeature)) {
                return false;
            }
            return xAddFeatureToAnnot(pFeature, annot);
        }
        IdToFeatureMap::iterator fit = m_MapIdToFeature.find(parentId);
        if (fit != m_MapIdToFeature.end()) {
            CRef<CSeq_feat> pParent = fit->second;
            if (!record.UpdateFeature(m_iFlags, pParent)) {
                return false;
            }
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotCds(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    xVerifyCdsParents(record);

    string cdsId = xMakeRecordId(record);
    mpLocations->AddRecordForId(cdsId, record);

    string parentId;
    record.GetAttribute("Parent", parentId);

    if (!parentId.empty()) {
        mCdsParents.insert(parentId);
        mpLocations->AddRecordForId(parentId, record);
    }

    auto pExistingFeature = m_MapIdToFeature.find(cdsId);
    if (pExistingFeature != m_MapIdToFeature.end()) {
        return true;
    }

    m_MapIdToFeature[cdsId] = pFeature;
    xInitializeFeature(record, pFeature);
    xAddFeatureToAnnot(pFeature, annot);

    if (!parentId.empty()) {
        xFeatureSetQualifier("Parent", parentId, pFeature);
        xFeatureSetXrefParent(parentId, pFeature);
        if (m_iFlags & fGeneXrefs) {
            xFeatureSetXrefGrandParent(parentId, pFeature);
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff3Reader::xFeatureSetXrefGrandParent(
    const string& parent,
    CRef<CSeq_feat> pFeature)
//  ----------------------------------------------------------------------------
{
    IdToFeatureMap::iterator it = m_MapIdToFeature.find(parent);
    if (it == m_MapIdToFeature.end()) {
        return false;
    }
    CRef<CSeq_feat> pParent = it->second;
    const string &grandParentsStr = pParent->GetNamedQual("Parent");
    list<string> grandParents;
    NStr::Split(grandParentsStr, ",", grandParents, 0);
    for (list<string>::const_iterator gpcit = grandParents.begin();
            gpcit != grandParents.end(); ++gpcit) {
        IdToFeatureMap::iterator gpit = m_MapIdToFeature.find(*gpcit);
        if (gpit == m_MapIdToFeature.end()) {
            return false;
        }
        CRef<CSeq_feat> pGrandParent = gpit->second;

        //xref grandchild->grandparent
        CRef<CFeat_id> pGrandParentId(new CFeat_id);
        pGrandParentId->Assign(pGrandParent->GetId());
        CRef<CSeqFeatXref> pGrandParentXref(new CSeqFeatXref);
        pGrandParentXref->SetId(*pGrandParentId);
        pFeature->SetXref().push_back(pGrandParentXref);

        //xref grandparent->grandchild
        CRef<CFeat_id> pGrandChildId(new CFeat_id);
        pGrandChildId->Assign(pFeature->GetId());
        CRef<CSeqFeatXref> pGrandChildXref(new CSeqFeatXref);
        pGrandChildXref->SetId(*pGrandChildId);
        pGrandParent->SetXref().push_back(pGrandChildXref);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xFeatureSetXrefParent(
    const string& parent,
    CRef<CSeq_feat> pChild)
//  ----------------------------------------------------------------------------
{
    IdToFeatureMap::iterator it = m_MapIdToFeature.find(parent);
    if (it == m_MapIdToFeature.end()) {
        return false;
    }
    CRef<CSeq_feat> pParent = it->second;

    //xref child->parent
    CRef<CFeat_id> pParentId(new CFeat_id);
    pParentId->Assign(pParent->GetId());
    CRef<CSeqFeatXref> pParentXref(new CSeqFeatXref);
    pParentXref->SetId(*pParentId);
    pChild->SetXref().push_back(pParentXref);

    //xref parent->child
    CRef<CFeat_id> pChildId(new CFeat_id);
    pChildId->Assign(pChild->GetId());
    CRef<CSeqFeatXref> pChildXref(new CSeqFeatXref);
    pChildXref->SetId(*pChildId);
    pParent->SetXref().push_back(pChildXref);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xFindFeatureUnderConstruction(
    const CGff2Record& record,
    CRef<CSeq_feat>& underConstruction)
//  ----------------------------------------------------------------------------
{
    string id;
    if (!record.GetAttribute("ID", id)) {
        return false;
    }
    IdToFeatureMap::iterator it = m_MapIdToFeature.find(id);
    if (it == m_MapIdToFeature.end()) {
        return false;
    }

    CReaderMessage fatal(
        eDiag_Fatal,
        m_uLineNumber,
        "Bad data line:  Duplicate feature ID \"" + id + "\".");
    CSeq_feat tempFeat;
    if (CSoMap::SoTypeToFeature(record.Type(), tempFeat)) {
        if (it->second->GetData().GetSubtype() != tempFeat.GetData().GetSubtype()) {
            throw fatal;
        }
    }

    underConstruction = it->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotGeneric(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pUnderConstruction(new CSeq_feat);
    if (xFindFeatureUnderConstruction(record, pUnderConstruction)) {
        return record.UpdateFeature(m_iFlags, pUnderConstruction);
    }

    string featType = record.Type();
    if (featType == "stop_codon_read_through"  ||  featType == "selenocysteine") {
        string cdsParent;
        if (!record.GetAttribute("Parent", cdsParent)) {
            CReaderMessage error(
                eDiag_Error,
                m_uLineNumber,
                "Bad data line: Unassigned code break.");
            throw error;
        }
        IdToFeatureMap::iterator it = m_MapIdToFeature.find(cdsParent);
        if (it == m_MapIdToFeature.end()) {
            CReaderMessage error(
                eDiag_Error,
                m_uLineNumber,
                "Bad data line: Code break assigned to missing feature.");
            throw error;
        }

        CRef<CCode_break> pCodeBreak(new CCode_break);
        CSeq_interval& cbLoc = pCodeBreak->SetLoc().SetInt();
        CRef< CSeq_id > pId = mSeqIdResolve(record.Id(), m_iFlags, true);
        cbLoc.SetId(*pId);
        cbLoc.SetFrom(static_cast<TSeqPos>(record.SeqStart()));
        cbLoc.SetTo(static_cast<TSeqPos>(record.SeqStop()));
        if (record.IsSetStrand()) {
            cbLoc.SetStrand(record.Strand());
        }
        pCodeBreak->SetAa().SetNcbieaa(
            (featType == "selenocysteine") ? 'U' : 'X');
        CRef<CSeq_feat> pCds = it->second;
        CCdregion& cdRegion = pCds->SetData().SetCdregion();
        list< CRef< CCode_break > >& codeBreaks = cdRegion.SetCode_break();
        codeBreaks.push_back(pCodeBreak);
        return true;
    }
    if (!xInitializeFeature(record, pFeature)) {
        return false;
    }
    if (! xAddFeatureToAnnot(pFeature, annot)) {
        return false;
    }
    string strId;
    if ( record.GetAttribute("ID", strId)) {
        m_MapIdToFeature[strId] = pFeature;
    }
    if (pFeature->GetData().IsRna()  ||
            pFeature->GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_RNA) {
        CRef<CSeq_interval> rnaLoc(new CSeq_interval);
        rnaLoc->Assign(pFeature->GetLocation().GetInt());
        mMrnaLocs[strId] = rnaLoc;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotRna(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pUnderConstruction(new CSeq_feat);
    if (xFindFeatureUnderConstruction(record, pUnderConstruction)) {
        return record.UpdateFeature(m_iFlags, pUnderConstruction);
    }

    if (!xInitializeFeature(record, pFeature)) {
        return false;
    }
    string parentsStr;
    if ((m_iFlags & fGeneXrefs)  &&  record.GetAttribute("Parent", parentsStr)) {
        list<string> parents;
        NStr::Split(parentsStr, ",", parents, 0);
        for (list<string>::const_iterator cit = parents.begin();
                cit != parents.end();
                ++cit) {
            if (!xFeatureSetXrefParent(*cit, pFeature)) {
                CReaderMessage error(
                    eDiag_Error,
                    m_uLineNumber,
                    "Bad data line: mRNA record with bad parent assignment.");
                throw error;
            }
        }
    }

    string strId;
    if ( record.GetAttribute("ID", strId)) {
        m_MapIdToFeature[strId] = pFeature;
    }
    CRef<CSeq_interval> mrnaLoc(new CSeq_interval);
    CSeq_loc::E_Choice choice = pFeature->GetLocation().Which();
    if (choice != CSeq_loc::e_Int) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Internal error: Unexpected location type.");
        throw error;
    }
    mrnaLoc->Assign(pFeature->GetLocation().GetInt());
    mMrnaLocs[strId] = mrnaLoc;

    if (! xAddFeatureToAnnot(pFeature, annot)) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotGene(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pUnderConstruction(new CSeq_feat);
    if (xFindFeatureUnderConstruction(record, pUnderConstruction)) {
        return record.UpdateFeature(m_iFlags, pUnderConstruction);
    }

    if (!xInitializeFeature(record, pFeature)) {
        return false;
    }
    if (! xAddFeatureToAnnot(pFeature, annot)) {
        return false;
    }
    string strId;
    if ( record.GetAttribute("ID", strId)) {
        m_MapIdToFeature[strId] = pFeature;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xUpdateAnnotRegion(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature,
    CSeq_annot& annot,
    ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    if (!record.InitializeFeature(m_iFlags, pFeature)) {
        return false;
    }

    if (! xAddFeatureToAnnot(pFeature, annot)) {
        return false;
    }
    string strId;
    if ( record.GetAttribute("ID", strId)) {
        mIdToSeqIdMap[strId] = record.Id();
        m_MapIdToFeature[strId] = pFeature;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xAddFeatureToAnnot(
    CRef< CSeq_feat > pFeature,
    CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    annot.SetData().SetFtable().push_back( pFeature ) ;
    return true;
}

//  ----------------------------------------------------------------------------
void CGff3Reader::xVerifyCdsParents(
    const CGff2Record& record)
//  ----------------------------------------------------------------------------
{
    string id;
    string parentId;
    if (!record.GetAttribute("ID", id)) {
        return;
    }
    record.GetAttribute("Parent", parentId);
    map<string, string>::iterator it = mCdsParentMap.find(id);
    if (it == mCdsParentMap.end()) {
        mCdsParentMap[id] = parentId;
        return;
    }
    if (it->second == parentId) {
        return;
    }
    CReaderMessage error(
        eDiag_Error,
        m_uLineNumber,
        "Bad data line: CDS record with bad parent assignments.");
    throw error;
}


//  ----------------------------------------------------------------------------
bool CGff3Reader::xHasCdsChild(const string& id) const
//  ----------------------------------------------------------------------------
{
    return (mCdsParents.find(id) != mCdsParents.end());
}


//  ----------------------------------------------------------------------------
bool CGff3Reader::xReadInit()
//  ----------------------------------------------------------------------------
{
    if (!CGff2Reader::xReadInit()) {
        return false;
    }
    mCdsParentMap.clear();
    mIDsAlreadySeen.clear();
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Reader::xIsIgnoredFeatureType(
    const CTempString& featureType)
//  ----------------------------------------------------------------------------
{
    typedef CStaticArraySet<string, PNocase> STRINGARRAY;

    string ftype(CSoMap::ResolveSoAlias(featureType));

    static const char* const ignoredTypesAlways_[] = {
        "protein",
        "start_codon", // also part of a cds feature
        "stop_codon", // in GFF3, also part of a cds feature
    };
    DEFINE_STATIC_ARRAY_MAP(STRINGARRAY, ignoredTypesAlways, ignoredTypesAlways_);
    STRINGARRAY::const_iterator cit = ignoredTypesAlways.find(ftype);
    if (cit != ignoredTypesAlways.end()) {
        return true;
    }
    if (!IsInGenbankMode()) {
        return false;
    }

    /* -genbank mode:*/
    static const char* const specialTypesGenbank_[] = {
        "antisense_RNA",
        "autocatalytically_spliced_intron",
        "guide_RNA",
        "hammerhead_ribozyme",
        "lnc_RNA",
        "miRNA",
        "piRNA",
        "rasiRNA",
        "ribozyme",
        "RNase_MRP_RNA",
        "RNase_P_RNA",
        "scRNA",
        "selenocysteine",
        "siRNA",
        "snoRNA",
        "snRNA",
        "SRP_RNA",
        "stop_codon_read_through",
        "telomerase_RNA",
        "vault_RNA",
        "Y_RNA"
    };
    DEFINE_STATIC_ARRAY_MAP(STRINGARRAY, specialTypesGenbank, specialTypesGenbank_);

    static const char* const ignoredTypesGenbank_[] = {
        "apicoplast_chromosome",
        "assembly",
        "cDNA_match",
        "chloroplast_chromosome",
        "chromoplast_chromosome",
        "chromosome",
        "contig",
        "cyanelle_chromosome",
        "dna_chromosome",
        "EST_match",
        "expressed_sequence_match",
        "intron",
        "leucoplast_chromosome",
        "macronuclear_chromosome",
        "match",
        "match_part",
        "micronuclear_chromosome",
        "mitochondrial_chromosome",
        "nuclear_chromosome",
        "nucleomorphic_chromosome",
        "nucleotide_motif",
        "nucleotide_to_protein_match",
        "partial_genomic_sequence_assembly",
        "protein_match",
        "replicon",
        "rna_chromosome",
        "sequence_assembly",
        "supercontig",
        "translated_nucleotide_match",
        "ultracontig",
    };
    DEFINE_STATIC_ARRAY_MAP(STRINGARRAY, ignoredTypesGenbank, ignoredTypesGenbank_);

    cit = specialTypesGenbank.find(ftype);
    if (cit != specialTypesGenbank.end()) {
        return false;
    }

    cit = ignoredTypesGenbank.find(ftype);
    if (cit != ignoredTypesGenbank.end()) {
        return true;
    }

    return false;
}

//  ----------------------------------------------------------------------------
bool
CGff3Reader::xInitializeFeature(
    const CGff2Record& record,
    CRef<CSeq_feat> pFeature)
//  ----------------------------------------------------------------------------
{
    if (!record.InitializeFeature(m_iFlags, pFeature)) {
        return false;
    }
    const auto& attrs = record.Attributes();
    const auto it = attrs.find("ID");
    if (it != attrs.end()) {
        mIdToSeqIdMap[it->second] = record.Id();
    }
    return true;
}

//  ----------------------------------------------------------------------------

static bool s_TreatAsRna(const CSeqFeatData& featData)
{
    if (featData.IsRna()) {
        return true;
    }
    const auto subtype = featData.GetSubtype();
    return (subtype == CSeqFeatData::eSubtype_C_region ||
            subtype == CSeqFeatData::eSubtype_D_segment ||
            subtype == CSeqFeatData::eSubtype_J_segment ||
            subtype == CSeqFeatData::eSubtype_V_segment);
}


//  ----------------------------------------------------------------------------
void CGff3Reader::xPostProcessAnnot(
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    if (mAlignmentData) {
        xProcessAlignmentData(annot);
        return;
    }
    if (! mCurrentFeatureCount) {
        return;
    }

    // location fixup:
    for (auto itLocation : mpLocations->LocationMap()) {
        auto id        = itLocation.first;
        auto itFeature = m_MapIdToFeature.find(id);
        if (itFeature == m_MapIdToFeature.end()) {
            continue;
        }
        CRef<CSeq_loc>    pNewLoc(new CSeq_loc);
        CRef<CSeq_feat>   pFeature = itFeature->second;
        CCdregion::EFrame frame;
        const auto&       locs = itLocation.second;

        if (pFeature->GetData().IsImp() &&
            pFeature->GetData().GetImp().IsSetKey() &&
            pFeature->GetData().GetImp().GetKey() == "misc_RNA") {
            // RW-143: if this has a child (pseudogenic) CDS, convert to mRNA
            if (xHasCdsChild(id)) {
                pFeature->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
            }
        }

        if (s_TreatAsRna(pFeature->GetData())) {
            // check for exons
            list<CGff3LocationRecord> exonLocs;
            for (auto record : locs) {
                if (record.mType == "exon") {
                    exonLocs.push_back(record);
                }
            }
            // prioritize exons over UTR and CDS child features
            if (! exonLocs.empty()) {
                mpLocations->MergeLocation(pNewLoc, frame, exonLocs);
                pFeature->SetLocation(*pNewLoc);
                continue;
            }
        } else if (pFeature->GetData().IsGene()) {

            list<CGff3LocationRecord> geneLocs;
            for (auto record : locs) {
                if (record.mType == "gene") {
                    geneLocs.push_back(record);
                }
            }

            if (! geneLocs.empty()) { // should never be empty
                mpLocations->MergeLocation(pNewLoc, frame, geneLocs);
                pFeature->SetLocation(*pNewLoc);
                continue;
            }
        }


        mpLocations->MergeLocation(pNewLoc, frame, itLocation.second);
        pFeature->SetLocation(*pNewLoc);
        if (pFeature->GetData().IsCdregion()) {
            auto& cdrData = pFeature->SetData().SetCdregion();
            cdrData.SetFrame(
                frame == CCdregion::eFrame_not_set ? CCdregion::eFrame_one : frame);
        }
    }

    return CGff2Reader::xPostProcessAnnot(annot);
}

//  ----------------------------------------------------------------------------
void CGff3Reader::xProcessSequenceRegionPragma(
    const CTempString& pragma)
//  ----------------------------------------------------------------------------
{
    TSeqPos sequenceSize(0);
    vector<string> tokens;
    NStr::Split(pragma, " \t", tokens, NStr::fSplit_MergeDelimiters);
    if (tokens.size() < 2) {
        CReaderMessage warning(
            ncbi::eDiag_Warning,
            m_uLineNumber,
            "Bad sequence-region pragma - ignored.");
        throw warning;
    }
    if (tokens.size() >= 4) {
        try {
            sequenceSize = NStr::StringToNonNegativeInt(tokens[3]);
        }
        catch(exception&) {
            CReaderMessage warning(
                ncbi::eDiag_Warning,
                m_uLineNumber,
                "Bad sequence-region pragma - ignored.");
            throw warning;
        }
    }
    mpLocations->SetSequenceSize(tokens[1], sequenceSize);
    auto resolvedId = mSeqIdResolve(tokens[1], m_iFlags, true)->AsFastaString();
    mpLocations->SetSequenceSize(resolvedId, sequenceSize);

}

//  ----------------------------------------------------------------------------
TSeqPos CGff3Reader::SequenceSize() const
//  ----------------------------------------------------------------------------
{
    return mpLocations->SequenceSize();
}

//  ----------------------------------------------------------------------------
TSeqPos CGff3Reader::GetSequenceSize(
    const string& seqId) const
//  ----------------------------------------------------------------------------
{
    return mpLocations->GetSequenceSize(seqId);
}

//  ----------------------------------------------------------------------------
void CGff3Reader::xValidateAnnot(
    const CSeq_annot& )
//  ----------------------------------------------------------------------------
{
    mpLocations->Validate();
}


END_objects_SCOPE
END_NCBI_SCOPE
