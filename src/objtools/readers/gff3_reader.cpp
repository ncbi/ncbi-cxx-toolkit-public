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

unsigned int CGff3Reader::msGenericIdCounter = 0;

//  ----------------------------------------------------------------------------
string CGff3Reader::xNextGenericId()
//  ----------------------------------------------------------------------------
{
    return string("generic") + NStr::IntToString(msGenericIdCounter++);
}

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
    NStr::ToLower(type);
    if (type == "exon"  ||  type == "five_prime_utr"  ||  type == "three_prime_utr") {
        return xUpdateAnnotExon(record, pFeature, pAnnot, pEC);
    }
    if (type == "cds") {
        return xUpdateAnnotCds(record, pFeature, pAnnot, pEC);
    }
    if (type == "gene") {
        return xUpdateAnnotGene(record, pFeature, pAnnot, pEC);
    }
    if (type == "mrna") {
        return xUpdateAnnotMrna(record, pFeature, pAnnot, pEC);
    }
    return xUpdateAnnotGeneric(record, pFeature, pAnnot, pEC);
}

//  ----------------------------------------------------------------------------
void CGff3Reader::xVerifyExonLocation(
    const string& mrnaId,
    const CGff2Record& exon,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    map<string,CRef<CSeq_interval> >::const_iterator cit = mMrnaLocs.find(mrnaId);
    if (cit == mMrnaLocs.end()) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Fatal,
            0,
            "Bad data line: Exon record referring to non-existing mRNA parent.",
            ILineError::eProblem_FeatureBadStartAndOrStop));
        ProcessError(*pErr, pEC);
    }
    const CSeq_interval& containingInt = cit->second.GetObject();
    const CRef<CSeq_loc> pContainedLoc = exon.GetSeqLoc(m_iFlags);
    const CSeq_interval& containedInt = pContainedLoc->GetInt();
    bool failed = (containedInt.GetFrom() < containingInt.GetFrom())  ||
        (containedInt.GetTo() > containingInt.GetTo());
    if (failed) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Fatal,
            0,
            "Bad data line: Exon record that lies outside its parent location.",
            ILineError::eProblem_FeatureBadStartAndOrStop));
        ProcessError(*pErr, pEC);
    }
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
            const string& parentId = *it; 
            xVerifyExonLocation(parentId, record, pEC);
            IdToFeatureMap::iterator fit = m_MapIdToFeature.find(parentId);
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
        ProcessError(*pErr, pEC);
    }

    //if the feature has a parent that's still under construction then
    // add feature location to parent location:
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

    string id;
    record.GetAttribute("ID", id);
    string strParents;
    if (record.GetAttribute("Parent", strParents)) {
        list<string> parents;
        NStr::Split(strParents, ",", parents);
        for (list<string>::const_iterator cit = parents.begin();
                cit != parents.end();
                ++cit) {
            //the following needs to happen for each of the parent mRNAs:
            // try to find a CDS feature that belongs to the same parent and that should
            //  be appended to
            // if successful then update the pre-existing CDS feature
            // if not then create a brand new CDS feature

            //find pre-existing cds to append to ---
            // if this record had an ID attribute then look for a cds of the same ID:parent
            // combination:
            string cdsId;
            if (!id.empty()) {
                cdsId = id + ":" + *cit;
                IdToFeatureMap::iterator it = m_MapIdToFeature.find(cdsId);
                if (it != m_MapIdToFeature.end()) {
                    record.UpdateFeature(m_iFlags, it->second);
                    continue;
                }
            }
            //find pre-existing cds to append to ---
            // if this record did not have an ID attribute then look for a cds with feature
            // ID of pattern genericXX:parent:
            else {
                cdsId = xNextGenericId() + ":" + *cit;
                IdToFeatureMap::iterator it;
                for (it = m_MapIdToFeature.begin(); it != m_MapIdToFeature.end(); ++it) {
                    string key = it->first;
                    string prefix, parent;
                    NStr::SplitInTwo(key, ":", prefix, parent);
                    if (!NStr::StartsWith(prefix, "generic")) {
                        continue;
                    }
                    if (parent != *cit) {
                        continue;
                    }
                    break;
                }
                if (it != m_MapIdToFeature.end()) {
                    record.UpdateFeature(m_iFlags, it->second);
                    continue;
                }
            }

            //still here?
            // create brand new CDS feature:
            if (!record.InitializeFeature(m_iFlags, pFeature)) {
                return false;
            }
            if (!xFeatureSetXrefParent(*cit, pFeature)) {
                AutoPtr<CObjReaderLineException> pErr(
                    CObjReaderLineException::Create(
                    eDiag_Warning,
                    0,
                    "Bad data line: CDS record with bad parent assignment.",
                    ILineError::eProblem_MissingContext) );
                ProcessError(*pErr, pEC);
            }
            if (m_iFlags & fGeneXrefs) {
                if (!xFeatureSetXrefGrandParent(*cit, pFeature)) {
                    AutoPtr<CObjReaderLineException> pErr(
                        CObjReaderLineException::Create(
                        eDiag_Warning,
                        0,
                        "Bad data line: CDS record with bad parent assignment.",
                        ILineError::eProblem_MissingContext) );
                    ProcessError(*pErr, pEC);
                }
            }
            if (! x_AddFeatureToAnnot(pFeature, pAnnot)) {
                return false;
            }
            if (! cdsId.empty()) {
                m_MapIdToFeature[cdsId] = pFeature;
            }
            pFeature.Reset(new CSeq_feat);
        }
        return true;
    }
    return false;
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
    NStr::Split(grandParentsStr, ",", grandParents);
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
bool CGff3Reader::xUpdateAnnotMrna(
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
    CRef<CSeq_interval> mrnaLoc(new CSeq_interval);
    CSeq_loc::E_Choice choice = pFeature->GetLocation().Which();
    if (choice != CSeq_loc::e_Int) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Fatal,
            0,
            "Internal error: Unexpected location type.",
            ILineError::eProblem_BadFeatureInterval));
    }
    mrnaLoc->Assign(pFeature->GetLocation().GetInt());
    mMrnaLocs[id] = mrnaLoc;

    string parentsStr;
    if ((m_iFlags & fGeneXrefs)  &&  record.GetAttribute("Parent", parentsStr)) {
        list<string> parents;
        NStr::Split(parentsStr, ",", parents);
        for (list<string>::const_iterator cit = parents.begin();
                cit != parents.end();
                ++cit) {
            if (!xFeatureSetXrefParent(*cit, pFeature)) {
                AutoPtr<CObjReaderLineException> pErr(
                    CObjReaderLineException::Create(
                    eDiag_Warning,
                    0,
                    "Bad data line: mRNA record with bad parent assignment.",
                    ILineError::eProblem_MissingContext) );
                ProcessError(*pErr, pEC);
            }
        }
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
