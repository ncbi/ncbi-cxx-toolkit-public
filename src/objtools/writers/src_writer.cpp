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
 * File Description:  Write source qualifiers
 *
 */

#include <ncbi_pch.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>

#include <objmgr/seqdesc_ci.hpp>

#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqtable/SeqTable_single_data.hpp>
#include <objects/seqtable/SeqTable_multi_data.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/src_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

const string CSrcWriter::DELIMITER = "\t";
const string CSrcWriter::keyId = "id";
const string CSrcWriter::keyTaxname = "org.taxname";
const string CSrcWriter::keyGenome = "genome";
const string CSrcWriter::keyDivision = "org.div";
const string CSrcWriter::keyOrigin = "origin";
const string CSrcWriter::keyPcrPrimersFwdNames = "pcr-primers.names.fwd";
const string CSrcWriter::keyPcrPrimersFwdSequences = "pcr-primers.seq.fwd";
const string CSrcWriter::keyPcrPrimersRevNames = "pcr-primers.names.rev";
const string CSrcWriter::keyPcrPrimersRevSequences = "pcr-primers.seq.rev";
const string CSrcWriter::keyOrgCommon = "org.common";
const string CSrcWriter::keyOrgnameLineage = "org.orgname.lineage";

static const string arrRecognizedFields[] = {
    "div",                  //syn for org.orgname.div
    "division",             //syn for org.orgname.div
    "genome",
    "lineage",              //syn for org.orgname.lineage
    "orgmod",               //syn for org.mod
    "origin",
    "pcr-primers",
    "subtype",
    "taxname",              //syn for org.taxname

    "org.common",
    "org.mod",
    "org.orgname.div",
    "org.orgname.lineage",
    "org.taxname"
};
const size_t countRecognizedFields = sizeof(arrRecognizedFields)/sizeof(string);

const CSrcWriter::FIELDS CSrcWriter::sRecognizedFields(
    arrRecognizedFields, arrRecognizedFields + countRecognizedFields);

static const string arrDefaultFields[] = {
    "id",
    "taxname",
    "division",
};
const size_t countDefaultFields = sizeof(arrDefaultFields)/sizeof(string);

const CSrcWriter::FIELDS CSrcWriter::sDefaultFields(
    arrDefaultFields, arrDefaultFields + countDefaultFields);

//  ----------------------------------------------------------------------------
bool CSrcWriter::WriteBioseqHandle( 
    CBioseq_Handle bsh,
    const FIELDS& desiredFields,
    CNcbiOstream& out)
//  ----------------------------------------------------------------------------
{
    if (!xGather(bsh, desiredFields)) {
        return false;
    }
    if (!xFormatTabDelimited(out)) {
        return false;
    }
    return true;
};

//  ----------------------------------------------------------------------------
bool CSrcWriter::WriteBioseqHandles( 
    const vector<CBioseq_Handle>& vecBsh,
    const FIELDS& desiredFields,
    CNcbiOstream& out)
//  ----------------------------------------------------------------------------
{
    typedef vector<CBioseq_Handle> HANDLES;
    for (HANDLES::const_iterator it = vecBsh.begin(); it != vecBsh.end(); ++it) {
        if (!xGather(*it, desiredFields)) {
            return false;
        }
    }
    if (!xFormatTabDelimited(out)) {
        return false;
    }
    return true;
};

//  ----------------------------------------------------------------------------
void CSrcWriter::xInit()
//  ----------------------------------------------------------------------------
{
    mSrcTable.Reset(new CSeq_table());
    mSrcTable->SetNum_rows(0);

    CRef<CSeqTable_column> pColumnId(new CSeqTable_column());
    pColumnId->SetHeader().SetField_name(CSrcWriter::keyId);
    pColumnId->SetHeader().SetTitle("Accession");
    pColumnId->SetDefault().SetString("id");
    mColnameToIndex[CSrcWriter::keyId] = mSrcTable->GetColumns().size();
    mSrcTable->SetColumns().push_back(pColumnId);
}

//  ----------------------------------------------------------------------------
CSrcWriter::HANDLER CSrcWriter::xGetHandler(
    const string& fieldName)
//  ----------------------------------------------------------------------------
{
    typedef map<string, CSrcWriter::HANDLER> HANDLERMAP;
    HANDLER handler = &CSrcWriter::xGatherGenome;

    static HANDLERMAP handlerMap;
    static bool mapIsInitialized = false;
    if (!mapIsInitialized) {
        handlerMap["div"] = &CSrcWriter::xGatherDivision;
        handlerMap["division"] = &CSrcWriter::xGatherDivision;
        handlerMap["genome"] = &CSrcWriter::xGatherGenome;
        handlerMap["lineage"] = &CSrcWriter::xGatherOrgnameLineage;
        handlerMap["orgmod"] = &CSrcWriter::xGatherOrgMod;
        handlerMap["origin"] = &CSrcWriter::xGatherOrigin;
        handlerMap["pcr-primers"] = &CSrcWriter::xGatherPcrPrimers;
        handlerMap["subtype"] = &CSrcWriter::xGatherSubtype;
        handlerMap["taxname"] = &CSrcWriter::xGatherTaxname;
        handlerMap["org.common"] = &CSrcWriter::xGatherOrgCommon;
        handlerMap["org.mod"] = &CSrcWriter::xGatherOrgMod;
        handlerMap["org.orgname.div"] = &CSrcWriter::xGatherDivision;
        handlerMap["org.orgname.lineage"] = &CSrcWriter::xGatherOrgnameLineage;
        handlerMap["org.taxname"] = &CSrcWriter::xGatherTaxname;
    }
    HANDLERMAP::iterator it = handlerMap.find(fieldName);
    if (it == handlerMap.end()) {
        return 0;
    }
    return it->second;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xHandleSourceField(
    const CBioSource& src, 
    const string& fieldName)
//  ----------------------------------------------------------------------------
{
    HANDLER pHandler = xGetHandler(fieldName);
    if (!pHandler) {
        return false;
    }
    return (this->*pHandler)(src);
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGather(
    CBioseq_Handle bsh,
    const FIELDS& desiredFields)
//  ----------------------------------------------------------------------------
{
    if (!xGatherId(bsh)) {
        return false;
    }
    for (CSeqdesc_CI sdit(bsh, CSeqdesc::e_Source); sdit; ++sdit) {
        const CBioSource& src = sdit->GetSource();
        for (FIELDS::const_iterator cit = desiredFields.begin();
                cit != desiredFields.end(); ++cit) {
            if (!xHandleSourceField(src, *cit)) {
                return false;
            }
        }
        mSrcTable->SetNum_rows(mSrcTable->GetNum_rows()+1);
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherId(
    CBioseq_Handle bsh)
//  ----------------------------------------------------------------------------
{
    string label;
    bsh.GetSeqId()->GetLabel(&label);
    unsigned int index = mColnameToIndex[CSrcWriter::keyId];
    CSeqTable_column& column = *mSrcTable->SetColumns().at(index);
    column.SetData().SetString().push_back(label);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherTaxname(
    const CBioSource& src)
//  ----------------------------------------------------------------------------
{
    if (!src.IsSetTaxname()) {
        return true;
    }
    string taxName = src.GetTaxname();
    unsigned int index = mColnameToIndex[CSrcWriter::keyTaxname];
    CSeqTable_column& column = *mSrcTable->SetColumns().at(index);
    column.SetData().SetString().push_back(taxName);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherOrgCommon(
    const CBioSource& src)
//  ----------------------------------------------------------------------------
{
    if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetCommon()) {
        return true;
    }
    string commonName = src.GetOrg().GetCommon();
    unsigned int index = mColnameToIndex[CSrcWriter::keyOrgCommon];
    CSeqTable_column& column = *mSrcTable->SetColumns().at(index);
    column.SetData().SetString().push_back(commonName);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherOrgnameLineage(
    const CBioSource& src)
//  ----------------------------------------------------------------------------
{
    if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetOrgname()  
            ||  !src.GetOrg().GetOrgname().IsSetLineage()) {
        return true;
    }
    string lineage = src.GetOrg().GetOrgname().GetLineage();
    unsigned int index = mColnameToIndex[CSrcWriter::keyOrgnameLineage];
    CSeqTable_column& column = *mSrcTable->SetColumns().at(index);
    column.SetData().SetString().push_back(lineage);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherDivision(
    const CBioSource& src)
//  ----------------------------------------------------------------------------
{
    if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetDivision()) {
        return true;
    }
    string division = src.GetOrg().GetDivision();
    xPrepareTableColumn(CSrcWriter::keyDivision, "division", "?");
    unsigned int index = mColnameToIndex[CSrcWriter::keyDivision];
    CSeqTable_column& column = *mSrcTable->SetColumns().at(index);
    column.SetData().SetString().push_back(division);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherGenome(
    const CBioSource& src)
//  ----------------------------------------------------------------------------
{
    if (!src.IsSetGenome()) {
        return true;
    }
    string genome = CBioSource::GetOrganelleByGenome(src.GetGenome());
    xPrepareTableColumn(CSrcWriter::keyGenome, "genome", "genome");
    unsigned int index = mColnameToIndex[CSrcWriter::keyGenome];
    CSeqTable_column& column = *mSrcTable->SetColumns().at(index);
    column.SetData().SetString().push_back(genome);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherOrigin(
    const CBioSource& src)
//  ----------------------------------------------------------------------------
{
    if (!src.IsSetOrigin()) {
        return true;
    }
    string genome = CBioSource::GetStringFromOrigin(src.GetOrigin());
    xPrepareTableColumn(CSrcWriter::keyOrigin, "origin", "origin");
    unsigned int index = mColnameToIndex[CSrcWriter::keyOrigin];
    CSeqTable_column& column = *mSrcTable->SetColumns().at(index);
    column.SetData().SetString().push_back(genome);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherSubtype(
    const CBioSource& src)
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSubSource> > SUBSOURCES;
    if (!src.IsSetSubtype()) {
        return true;
    }
    const SUBSOURCES& subsources = src.GetSubtype();
    for (SUBSOURCES::const_iterator cit = subsources.begin(); 
            cit != subsources.end(); ++cit) {
        const CSubSource& subsrc = **cit;
        string key = CSubSource::GetSubtypeName(subsrc.GetSubtype());
        if (NStr::EqualNocase(key, "note")) {
            key = "subsource.note";
        }
        string value = "";
        if (subsrc.IsSetName()) {
            value = subsrc.GetName();
        }
        if (value.empty()  &&  CSubSource::NeedsNoText(subsrc.GetSubtype())) {
            value = "true";
        }
        xPrepareTableColumn(key, key, "");
        xAppendColumnValue(key, value);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherOrgMod(
    const CBioSource& src)
//  ----------------------------------------------------------------------------
{
    if (!src.IsSetOrgMod()) {
        return true;
    }

    typedef list<CRef<COrgMod> > ORGMODS;
    const ORGMODS& orgmods = src.GetOrgname().GetMod();
    for (ORGMODS::const_iterator cit = orgmods.begin(); 
            cit != orgmods.end(); ++cit) {
        const COrgMod& orgmod = **cit;
        string key = COrgMod::GetSubtypeName(orgmod.GetSubtype());
        if (NStr::EqualNocase(key, "note")) {
            key = "orgmod.note";
        }
        string value = orgmod.GetSubname();
        xPrepareTableColumn(key, key, "");
        xAppendColumnValue(key, value);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherPcrPrimers(
    const CBioSource& src)
//  ----------------------------------------------------------------------------
{
    if (!src.IsSetPcr_primers()) {
        return true;
    }
    string fwdName, fwdSequence, revName, revSequence;
    const CPCRReactionSet& pcrset = src.GetPcr_primers();

    typedef list<CRef<CPCRReaction> > REACTIONS;
    const REACTIONS& reactions = pcrset.Get();
    for (REACTIONS::const_iterator cit = reactions.begin();
            cit != reactions.end(); ++cit) {
        const CPCRReaction& reaction = **cit;
        if (reaction.IsSetForward()) {
            if (fwdName.empty()) {
                fwdName += ";";
                fwdSequence += ";";
            }
            fwdName += CSrcWriter::xPrimerSetNames(reaction.GetForward());
            fwdSequence += CSrcWriter::xPrimerSetSequences(reaction.GetForward());
        }
        if (reaction.IsSetReverse()) {
            if (revName.empty()) {
                revName += ";";
                revSequence += ";";
            }
            revName += CSrcWriter::xPrimerSetNames(reaction.GetReverse());
            revSequence += CSrcWriter::xPrimerSetSequences(reaction.GetReverse());
       }
    }
    xPrepareTableColumn(
        CSrcWriter::keyPcrPrimersFwdNames, "PCR Primers, Fwd Names", "");
    xAppendColumnValue(CSrcWriter::keyPcrPrimersFwdNames, fwdName);
    xPrepareTableColumn(
        CSrcWriter::keyPcrPrimersFwdSequences, "PCR Primers, Fwd Seqs", "");
    xAppendColumnValue(CSrcWriter::keyPcrPrimersFwdSequences, fwdSequence);

    xPrepareTableColumn(
        CSrcWriter::keyPcrPrimersRevNames, "PCR Primers, Rev Names", "");
    xAppendColumnValue(CSrcWriter::keyPcrPrimersRevNames, revName);
    xPrepareTableColumn(
        CSrcWriter::keyPcrPrimersRevSequences, "PCR Primers, Rev Seqs", "");
    xAppendColumnValue(CSrcWriter::keyPcrPrimersRevSequences, revSequence);
    return true;
}

//  ----------------------------------------------------------------------------
string CSrcWriter::xPrimerSetNames(const CPCRPrimerSet& pset)
//  ----------------------------------------------------------------------------
{
    string names = "";
    typedef list<CRef<CPCRPrimer> > PRIMERS;
    const PRIMERS& primers = pset.Get();
    for (PRIMERS::const_iterator cit = primers.begin(); 
            cit != primers.end(); ++cit) {
        const CPCRPrimer& primer = **cit;
        names += ",";
        if (primer.IsSetName()) {
            names += primer.GetName();
        }
    }
    return names.substr(1);
}

//  ----------------------------------------------------------------------------
string CSrcWriter::xPrimerSetSequences(const CPCRPrimerSet& pset)
//  ----------------------------------------------------------------------------
{
    string sequences = "";
    typedef list<CRef<CPCRPrimer> > PRIMERS;
    const PRIMERS& primers = pset.Get();
    for (PRIMERS::const_iterator cit = primers.begin(); 
            cit != primers.end(); ++cit) {
        const CPCRPrimer& primer = **cit;
        sequences += ",";
        if (primer.IsSetSeq()) {
            sequences += primer.GetSeq();
        }
    }
    return sequences.substr(1);
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::xFormatTabDelimited(
    CNcbiOstream& out)
//  ----------------------------------------------------------------------------
{
    //out << MSerial_AsnText << *mSrcTable << endl;
    //out.flush();
    typedef vector<CRef<CSeqTable_column> > COLUMNS;
    const COLUMNS& columns = mSrcTable->GetColumns();
    for (COLUMNS::const_iterator cit = columns.begin(); 
            cit != columns.end(); ++cit) {
        const CSeqTable_column& column = **cit;
        string columnName = column.GetHeader().GetTitle();
        out << columnName << CSrcWriter::DELIMITER;
    }
    out << endl;
    unsigned int numRows = mSrcTable->GetNum_rows();
    for (unsigned int u=0; u < numRows; ++u) {
        for (COLUMNS::const_iterator cit = columns.begin(); 
                cit != columns.end(); ++cit) {
            const CSeqTable_column& column = **cit;
            const string* pValue = column.GetStringPtr(u);
            out << *pValue << CSrcWriter::DELIMITER;
        }
        out << endl;
    }
    return true;
}

//  ---------------------------------------------------------------------------
void CSrcWriter::xPrepareTableColumn(
    const string& colName,
    const string& colTitle,
    const string& colDefault)
//  ---------------------------------------------------------------------------
{
    COLUMNMAP::iterator it = mColnameToIndex.find(colName);
    if (it == mColnameToIndex.end()) {
        CRef<CSeqTable_column> pColumn(new CSeqTable_column());
        pColumn->SetHeader().SetField_name(colName);
        pColumn->SetHeader().SetTitle(colTitle);
        pColumn->SetDefault().SetString(colDefault);
        mColnameToIndex[colName] = mSrcTable->GetColumns().size();
        mSrcTable->SetColumns().push_back(pColumn);
    }
    unsigned int index = mColnameToIndex[colName];
    CSeqTable_column& column = *mSrcTable->SetColumns().at(index);
    column.SetData().SetString();
    while (column.GetData().GetString().size() < mSrcTable->GetNum_rows()) {
        column.SetData().SetString().push_back(colDefault);
    }
}

//  ----------------------------------------------------------------------------
void CSrcWriter::xAppendColumnValue(
    const string& colName,
    const string& colValue)
//  ----------------------------------------------------------------------------
{
    unsigned int index = mColnameToIndex[colName];
    CSeqTable_column& column = *mSrcTable->SetColumns().at(index);
    column.SetData().SetString().push_back(colValue);
}

//  ----------------------------------------------------------------------------
bool CSrcWriter::ValidateFields(
    const FIELDS fields)
//  ----------------------------------------------------------------------------
{
    for (FIELDS::const_iterator cit = fields.begin(); cit != fields.end(); ++cit) {
        string field = *cit;
        bool foundIt = false;
        for (FIELDS::const_iterator fit = sRecognizedFields.begin(); 
                fit != sRecognizedFields.end(); ++fit) {
            if (*fit == *cit) {
                foundIt = true;
                break;
            }
        }
        if (!foundIt) {
            return false;
        }
    }
    return true;
}

END_NCBI_SCOPE
