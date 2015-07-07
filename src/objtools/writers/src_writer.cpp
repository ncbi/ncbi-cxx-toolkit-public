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

#include <objects/general/general_macros.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>

#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/create_defline.hpp>

#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqtable/SeqTable_single_data.hpp>
#include <objects/seqtable/SeqTable_multi_data.hpp>
#include <objects/seq/seq_macros.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/writers/src_writer.hpp>
#include <objmgr/util/sequence.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CSrcWriter::HANDLERMAP CSrcWriter::sHandlerMap;
CSrcWriter::NAMEMAP CSrcWriter::sFieldnameToColname;
//  ----------------------------------------------------------------------------
//  Default Fields:
//  ----------------------------------------------------------------------------
static const string arrDefaultSrcCheckFields[] = {
    "id",
    "gi",
    "organism",
    "taxid",
    "specimen-voucher",
    "culture-collection",
    "bio-material",
    "strain",
    "sub-strain",
    "isolate",
    "sub-species",
    "variety",
    "forma",
    "cultivar",
    "ecotype",
    "serotype",
    "serovar",
    "type-material",
    "old-name"
};

static const string arrDefaultSeqEntryFields[] = {
    "id",
    "gi",
    "organism",
    "taxid",
    "localid",
    "specimen-voucher",
    "culture-collection",
    "bio-material",
    "strain",
    "sub-strain",
    "isolate",
    "sub-species",
    "variety",
    "forma",
    "cultivar",
    "ecotype",
    "serotype",
    "serovar",
    "type-material",
    "old-name"
};


const size_t countDefaultSrcCheckFields = sizeof(arrDefaultSrcCheckFields)/sizeof(string);

const CSrcWriter::FIELDS CSrcWriter::sDefaultSrcCheckFields(
        arrDefaultSrcCheckFields, arrDefaultSrcCheckFields + countDefaultSrcCheckFields);

const CSrcWriter::FIELDS CSrcWriter::sAllSrcCheckFields(
        xGetOrderedFieldNames(CSrcWriter::sDefaultSrcCheckFields));

const size_t countDefaultSeqEntryFields = sizeof(arrDefaultSeqEntryFields)/sizeof(string);

const CSrcWriter::FIELDS CSrcWriter::sDefaultSeqEntryFields(
        arrDefaultSeqEntryFields, arrDefaultSeqEntryFields + countDefaultSeqEntryFields);

const CSrcWriter::FIELDS CSrcWriter::sAllSeqEntryFields(
        xGetOrderedFieldNames(CSrcWriter::sDefaultSeqEntryFields));

//  ----------------------------------------------------------------------------
bool CSrcWriter::WriteBioseqHandle( 
        CBioseq_Handle bsh,
        const FIELDS& desiredFields,
        CNcbiOstream& out)
//  ----------------------------------------------------------------------------
{
    FIELDS colNames = xProcessFieldNames(desiredFields);

    if (!xGather(bsh, colNames)) {
        return false;
    }
    if (!xFormatTabDelimited(colNames, out)) {
        return false;
    }
    return true;
};


//  ----------------------------------------------------------------------------
bool CSrcWriter::WriteBioseqHandles( 
        const vector<CBioseq_Handle>& vecBsh,
        const FIELDS& desiredFields,
        CNcbiOstream& out,
        ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    typedef vector<CBioseq_Handle> HANDLES;
    FIELDS colNames = xProcessFieldNames(desiredFields);

    for (HANDLES::const_iterator it = vecBsh.begin(); it != vecBsh.end(); ++it) {
        if (!xGather(*it, colNames)) {
            return false;
        }
    }

    if (!xFormatTabDelimited(colNames,out)) {
        return false;
    }
    return true;
};


//  ----------------------------------------------------------------------------
bool CSrcWriter::WriteSeqEntry(
        const CSeq_entry& seqEntry,
        CScope& scope,
        CNcbiOstream& out,
        const bool nucsOnly) 
//  ----------------------------------------------------------------------------
{
    CSeq_entry_Handle handle = scope.AddTopLevelSeqEntry(seqEntry);
    vector<CBioseq_Handle> vecBsh;
    for (CBioseq_CI bci(handle); bci; ++bci) {
        if(!nucsOnly || bci->IsNa()) {
            vecBsh.push_back(*bci);
        }
    }
    WriteBioseqHandles(vecBsh, sAllSeqEntryFields, out, 0);

    return true;
}


//  ----------------------------------------------------------------------------
CSrcWriter::FIELDS CSrcWriter::xProcessFieldNames(
        const FIELDS& desiredFields)
//  ----------------------------------------------------------------------------
{
    FIELDS colNames;
    if (desiredFields[0] != "id") {
        colNames.push_back("id");
    }
    for (FIELDS::const_iterator cit = desiredFields.begin();
            cit != desiredFields.end();  ++cit)  {  
        NAMEMAP::const_iterator mapIterator = sFieldnameToColname.find(xCompressFieldName(*cit));
        if (mapIterator != sFieldnameToColname.end()) {
            colNames.push_back(mapIterator->second);
        } else {
            colNames.push_back(*cit);
        }
    }
    return colNames;
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
    size_t index = mColnameToIndex[colName];
    CSeqTable_column& column = *mSrcTable->SetColumns().at(index);
    column.SetData().SetString();
    while (column.GetData().GetString().size() < mSrcTable->GetNum_rows()) {
        column.SetData().SetString().push_back(colDefault);
    }
}


//  ----------------------------------------------------------------------------
void CSrcWriter::xInit()
    //  ----------------------------------------------------------------------------
{
    if (sHandlerMap.empty()) {
        sHandlerMap["db"] = &CSrcWriter::xGatherDb;
        sHandlerMap["taxid"] = &CSrcWriter::xGatherTaxonId;
        sHandlerMap["division"] = &CSrcWriter::xGatherDivision;
        sHandlerMap["genome"] = &CSrcWriter::xGatherGenome;
        sHandlerMap["lineage"] = &CSrcWriter::xGatherOrgnameLineage;
        sHandlerMap["common"] = &CSrcWriter::xGatherOrgCommon;
        sHandlerMap["origin"] = &CSrcWriter::xGatherOrigin;
        sHandlerMap["pcr-primers"] = &CSrcWriter::xGatherPcrPrimers;
        sHandlerMap["organism"] = &CSrcWriter::xGatherTaxname;

        sHandlerMap["note"] = &CSrcWriter::xGatherOrgModFeat;
        sHandlerMap["subsource-note"] = &CSrcWriter::xGatherSubtypeFeat;

        NAMELIST nameList = xGetOrgModSubtypeNames();
        for (NAMELIST::const_iterator cit=nameList.begin();
                cit != nameList.end(); ++cit) {
            if (*cit != "other" && *cit != "common") {
                sHandlerMap[*cit] = &CSrcWriter::xGatherOrgModFeat;
            }
        }

        nameList = xGetSubSourceSubtypeNames();
        for (NAMELIST::const_iterator cit=nameList.begin();
                cit != nameList.end(); ++cit) {
            if (*cit != "other") {
                sHandlerMap[*cit] = &CSrcWriter::xGatherSubtypeFeat;
            }
        }
    }


    if (sFieldnameToColname.empty()) {
        sFieldnameToColname["id"] = "id";
        sFieldnameToColname["gi"] = "gi";
        sFieldnameToColname["localid"] = "localid";
        sFieldnameToColname["definition"] = "definition";
        sFieldnameToColname["def"] = "definition";
        sFieldnameToColname["defline"] = "definition";

        sFieldnameToColname["db"] = "db";
        sFieldnameToColname["org.db"] = "db";

        sFieldnameToColname["taxid"] = "taxid";
        sFieldnameToColname["div"] = "division";
        sFieldnameToColname["division"] = "division";
        sFieldnameToColname["genome"] = "genome";
        sFieldnameToColname["lineage"] = "lineage";
        sFieldnameToColname["common"] = "common";
        sFieldnameToColname["org.common"] = "common";

        sFieldnameToColname["origin"] = "origin";
        sFieldnameToColname["pcrprimers"] = "pcr-primers";
        sFieldnameToColname["organism"] = "organism";
        sFieldnameToColname["taxname"] = "organism";
        sFieldnameToColname["org.taxname"] = "organism";
        sFieldnameToColname["org"] = "organism";

        // OrgMod
        sFieldnameToColname["note"] = "note";
        sFieldnameToColname["orgmod.note"] = "note";

        // Subsource
        sFieldnameToColname["insertionseq"] = "insertion-seq";
        sFieldnameToColname["plasmid"] = "plasmid";
        sFieldnameToColname["transposon"] = "transposon";

        sFieldnameToColname["subsourcenote"] = "subsource-note";
        sFieldnameToColname["subsrcnote"] = "subsource-note";

        NAMELIST nameList = xGetOrgModSubtypeNames();
        for (NAMELIST::const_iterator cit=nameList.begin();  
                cit != nameList.end();  ++cit) {

            if (*cit != "other") {
                sFieldnameToColname[xCompressFieldName(*cit)] = *cit;
            }
        }

        nameList = xGetSubSourceSubtypeNames();
        for (NAMELIST::const_iterator cit=nameList.begin();
                cit != nameList.end();  ++cit) {
            if(*cit != "other") {
                sFieldnameToColname[xCompressFieldName(*cit)] = *cit;
            }
        }
    } 


    mSrcTable.Reset(new CSeq_table());
    mSrcTable->SetNum_rows(0);
}


//  ----------------------------------------------------------------------------
CSrcWriter::FIELDS CSrcWriter::xGetOrderedFieldNames(const FIELDS& defaultFields)
    //  ----------------------------------------------------------------------------
{
    FIELDS orderedFields;
    set<string> defaultSet;

    for (FIELDS::const_iterator cit=defaultFields.begin();
            cit != defaultFields.end();
            ++cit) {
        string colName = *cit;
        orderedFields.push_back(colName);
        defaultSet.insert(colName);
    }

    FIELDS lexicalFields;
    lexicalFields.push_back("organism");
    lexicalFields.push_back("genome");
    lexicalFields.push_back("pcr-primers");
    lexicalFields.push_back("db");
    lexicalFields.push_back("common");
    lexicalFields.push_back("lineage");
    lexicalFields.push_back("origin");
    lexicalFields.push_back("note");
    lexicalFields.push_back("subsource-note");
    lexicalFields.push_back("division");
    lexicalFields.push_back("definition");

    NAMELIST nameList = xGetOrgModSubtypeNames();
    for(NAMELIST::const_iterator cit=nameList.begin();
            cit != nameList.end();  ++cit) {
        if (*cit != "other" && *cit != "common") {
            lexicalFields.push_back(*cit);
        }
    }

    nameList = xGetSubSourceSubtypeNames();
    for(NAMELIST::const_iterator cit=nameList.begin();
            cit != nameList.end();  ++cit) {
        if(*cit != "other") {
            lexicalFields.push_back(*cit);
        }
    }

    sort(lexicalFields.begin(), lexicalFields.end());

    for (FIELDS::const_iterator cit = lexicalFields.begin();
            cit != lexicalFields.end();  ++cit)
    {
        if (defaultSet.find(*cit) == defaultSet.end()) {
            orderedFields.push_back(*cit);
        }
    }
    return orderedFields;
}


//  ----------------------------------------------------------------------------
CSrcWriter::NAMELIST CSrcWriter::xGetOrgModSubtypeNames() 
    //  ----------------------------------------------------------------------------
{
    NAMELIST subtypeNames;

    typedef const CEnumeratedTypeValues::TValues TVALUES;
    TVALUES nameValPairs = COrgMod::ENUM_METHOD_NAME(ESubtype)()->GetValues();

    for (TVALUES::const_iterator cit = nameValPairs.begin();
            cit != nameValPairs.end();  ++cit) {
        subtypeNames.push_back(cit->first);    
    }
    return subtypeNames;
}


//  ----------------------------------------------------------------------------
CSrcWriter::NAMELIST CSrcWriter::xGetSubSourceSubtypeNames() 
    //  ----------------------------------------------------------------------------
{
    NAMELIST subtypeNames;

    typedef const CEnumeratedTypeValues::TValues TVALUES;
    TVALUES nameValPairs = CSubSource::ENUM_METHOD_NAME(ESubtype)()->GetValues();

    for (TVALUES::const_iterator cit = nameValPairs.begin(); 
            cit != nameValPairs.end();  ++cit) {
        subtypeNames.push_back(cit->first);    
    }
    return subtypeNames;
}


//  ----------------------------------------------------------------------------
string CSrcWriter::xCompressFieldName(
        const string& fieldName) 
//  ----------------------------------------------------------------------------
{
    string name = NStr::TruncateSpaces(fieldName);
    NStr::ToLower(name);
    NStr::ReplaceInPlace(name,"\"","");
    NStr::ReplaceInPlace(name,"-","");
    NStr::ReplaceInPlace(name, "_", "");
    NStr::ReplaceInPlace(name, " ", "");

    return name;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xIsSubsourceTypeSuppressed(
        CSubSource::TSubtype subtype)
//  ----------------------------------------------------------------------------
{
    if (CSubSource::IsDiscouraged(subtype)) {
        return true;
    }
    return false;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xIsOrgmodTypeSuppressed(
        COrgMod::TSubtype subtype)
//  ----------------------------------------------------------------------------
{
    if (COrgMod::eSubtype_old_name == subtype) {
        return false;
    }
    if (COrgMod::IsDiscouraged(subtype)) {
        return true;
    }
    return false;
}


//  ----------------------------------------------------------------------------
CSrcWriter::HANDLER CSrcWriter::xGetHandler(
        const string& fieldName)
//  ----------------------------------------------------------------------------
{
    return sHandlerMap[fieldName];
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xHandleSourceField(
        const CBioSource& src, 
        const string& fieldName,
        ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    HANDLER pHandler = xGetHandler(fieldName);

    if (!pHandler) {
        CSrcError* pE = CSrcError::Create(
                ncbi::eDiag_Error,
                "Unable to find handler for field \"" + fieldName + "\".");
        pEC->PutError(*pE);
        delete pE;
        return false;
    }

    return (this->*pHandler)(src, fieldName, pEC);
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGather(
        CBioseq_Handle bsh,
        const FIELDS& desiredFields,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    // for each of biosources we may create individual record
    // with the same ID
    bool wantGi = ( find(desiredFields.begin(), desiredFields.end(), "gi") != desiredFields.end() ); 
    bool wantLocalId = ( find(desiredFields.begin(), desiredFields.end(), "localid") != desiredFields.end() );
    bool wantDef = ( find(desiredFields.begin(), desiredFields.end(), "definition") != desiredFields.end() );



    for (CSeqdesc_CI sdit(bsh, CSeqdesc::e_Source); sdit; ++sdit) {

        if (!xGatherId(bsh)) {
            return false;
        }

        if (wantGi) {
            if (!xGatherGi(bsh)) {
                return false;
            }
        }

        if (wantLocalId) {
            if (!xGatherLocalId(bsh)) {
                return false;
            }
        }

        if (wantDef) {
            if (!xGatherDefline(bsh)) {
                return false;
            }
        }

        const CBioSource& src = sdit->GetSource();
        for (FIELDS::const_iterator cit = desiredFields.begin();
                cit != desiredFields.end(); ++cit) {
            if (*cit == "id" || *cit == "gi" || *cit == "definition" || *cit == "localid") {
                continue;
            }
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
        CBioseq_Handle bsh,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    static const string colName = "id";
    CConstRef<CSeq_id> sid = bsh.GetSeqId();
    string label = sequence::GetAccessionForId(*sid, bsh.GetScope());
    if (!label.empty()) {
        const string defaultValue = "";
        xPrepareTableColumn(colName, "accession", defaultValue);
        xAppendColumnValue(colName, label);
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherGi(
        CBioseq_Handle bsh,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    static const string colName = "gi";

    ITERATE( CBioseq_Handle::TId, it, bsh.GetId() ) {
        if( it->IsGi() ){
            string label;
            it->GetSeqId()->GetLabel(&label, CSeq_id::eContent);
            const string displayName = "gi";
            const string defaultValue = "";
            xPrepareTableColumn(colName, displayName, defaultValue);
            xAppendColumnValue(colName, label);
            return true;
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
string CSrcWriter::xGetOriginalId(const CBioseq_Handle& bsh) const
//  ----------------------------------------------------------------------------
{
    const CBioseq_Handle::TDescr& descr= bsh.GetDescr();

    FOR_EACH_SEQDESC_ON_SEQDESCR (it, descr) {
        const CSeqdesc& desc = **it;
        if ( !desc.IsUser() || !desc.GetUser().IsSetType() ) continue;
        const CUser_object& usr = desc.GetUser();
        const CObject_id& oi = usr.GetType();
        if ( !oi.IsStr() ) continue;
        const string& type = oi.GetStr();
        if ( !NStr::EqualNocase(type, "OrginalID") && !NStr::EqualNocase(type, "OriginalID") ) continue;
        FOR_EACH_USERFIELD_ON_USEROBJECT (uitr, usr) {
            const CUser_field& fld = **uitr;
            if ( FIELD_IS_SET_AND_IS(fld, Label, Str) ) {
                const string &label_str = GET_FIELD(fld.GetLabel(), Str);
                if ( !NStr::EqualNocase(label_str, "LocalId") ) continue;
                if ( fld.IsSetData() && fld.GetData().IsStr() ) {
                    return fld.GetData().GetStr();
                }
            }
        }
    }

    return "";
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherLocalId(
        CBioseq_Handle bsh,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    static const string colName = "localid";
    static const string displayName = colName;
    static const string defaultValue = "";

    // First check to see if OriginalID is available
    //string local_id = s_FastaGetOriginalID(*(bsh.GetCompleteBioseq()));
    string local_id = xGetOriginalId(bsh);

    if ( NStr::IsBlank(local_id) ) {
        CConstRef<CSeq_id> seq_id = bsh.GetLocalIdOrNull();
        if ( !seq_id ) {
            return true;
        }
        seq_id->GetLabel(&local_id, CSeq_id::eContent);
    }
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, local_id);
    return true;
}


bool CSrcWriter::xGatherDefline(
        CBioseq_Handle bsh,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    static const string colName = "definition";
    static const string displayName = colName;
    static const string defaultValue = "";

    string label = sequence::CDeflineGenerator().GenerateDefline(bsh);
    if (label.empty()) {
        return true;
    }
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, label);
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherTaxname(
        const CBioSource& src,
        const string& colName,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    const string displayName = "organism";
    const string defaultValue = "";

    if (!src.IsSetTaxname()) {
        return true;
    }
    string value = src.GetTaxname();
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, value);
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherOrgCommon(
        const CBioSource& src,
        const string& colName,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    const string displayName = "common";
    const string defaultValue = "";

    if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetCommon()) {
        return true;
    }
    string value = src.GetOrg().GetCommon();
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, value);
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherOrgnameLineage(
        const CBioSource& src,
        const string& colName,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    const string displayName = "lineage";
    const string defaultValue = "";

    if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetOrgname()  
            ||  !src.GetOrg().GetOrgname().IsSetLineage()) {
        return true;
    }
    string value = src.GetOrg().GetOrgname().GetLineage();
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, value);
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherDivision(
        const CBioSource& src,
        const string& colName,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    const string displayName = "division";
    const string defaultValue = "";

    if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetDivision()) {
        return true;
    }
    string value = src.GetOrg().GetDivision();
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, value);
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherGenome(
        const CBioSource& src,
        const string& colName,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    const string displayName = "genome";
    const string defaultValue = "";

    if (!src.IsSetGenome()) {
        return true;
    }
    string value = CBioSource::GetOrganelleByGenome(src.GetGenome());
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, value);
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherOrigin(
        const CBioSource& src,
        const string& colName,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    const string displayName = "origin";
    const string defaultValue = "";

    if (!src.IsSetOrigin()) {
        return true;
    }
    string value = CBioSource::GetStringFromOrigin(src.GetOrigin());
    xPrepareTableColumn(colName, displayName, defaultValue);
    xAppendColumnValue(colName, value);
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherSubtypeFeat(
        const CBioSource& src,
        const string& colName,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{


    if ( !src.IsSetSubtype() ) {
        return true;
    }

    CSubSource::TSubtype subtype = CSubSource::GetSubtypeValue(colName,CSubSource::eVocabulary_raw);

    if ( xIsSubsourceTypeSuppressed(subtype) ) {
        return true;
    }

    typedef list<CRef<CSubSource> > SUBSOURCES;
    const SUBSOURCES& subsources = src.GetSubtype();

    typedef map<string, int> INDEXCOUNTER;
    INDEXCOUNTER indexCounter;

    string key = colName;
    for (SUBSOURCES::const_iterator cit = subsources.begin();
            cit != subsources.end(); ++cit) {

        const CSubSource& subsrc = **cit;
        if (subsrc.GetSubtype() != subtype) {
            continue;
        }

        INDEXCOUNTER::iterator it = indexCounter.find(key);
        if (it != indexCounter.end()) {
            int index = it->second;
            indexCounter[key] = index+1;
            key += "#" + NStr::IntToString(index);
        }
        else {
            indexCounter[key] = 1;
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
bool CSrcWriter::xGatherOrgModFeat(
        const CBioSource& src,
        const string& colName,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    if ( !src.IsSetOrgMod() ) {
        return true;
    }

    COrgMod::TSubtype subtype = COrgMod::GetSubtypeValue(colName,COrgMod::eVocabulary_raw);

    if ( xIsOrgmodTypeSuppressed(subtype) ) {
        return true;
    }

    typedef list<CRef<COrgMod> > ORGMODS;
    const ORGMODS& orgmods = src.GetOrgname().GetMod();

    typedef map<string, int> INDEXCOUNTER;
    INDEXCOUNTER indexCounter;

    string key = colName;
    for (ORGMODS::const_iterator cit = orgmods.begin();
            cit != orgmods.end(); ++cit) {
        const COrgMod& orgmod = **cit;

        if (orgmod.GetSubtype() != subtype) {
            continue;
        }

        INDEXCOUNTER::iterator it = indexCounter.find(key);
        if (it != indexCounter.end()) {
            int index = it->second;
            indexCounter[key] = index+1;
            key += "#" + NStr::IntToString(index);
        }
        else {
            indexCounter[key] = 1;
        }

        string value = orgmod.GetSubname();
        xPrepareTableColumn(key, key, "");
        xAppendColumnValue(key, value);
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherDb(
        const CBioSource& src,
        const string& colName,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{

    static const string displayName = "db";
    static const string defaultValue = "";

    if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetDb()) {
        return true;
    }

    typedef vector< CRef< CDbtag > > DBTAGS;
    const DBTAGS& tags = src.GetOrg().GetDb();
    for (DBTAGS::const_iterator cit = tags.begin(); cit != tags.end(); ++cit) {
        const CDbtag& tag = **cit;
        if (!tag.IsSetDb()  ||  tag.GetDb().empty()  ||  !tag.IsSetTag()) {
            continue;
        }
        const CObject_id& objid = tag.GetTag();
        string dbtagStr;
        switch (objid.Which()) {
            default:
                break;
            case CObject_id::e_Str:
                if (objid.GetStr().empty()) {
                    continue;
                }
                dbtagStr = objid.GetStr();
                break;
            case CObject_id::e_Id:
                dbtagStr = NStr::IntToString(objid.GetId());
                break;
        }
        string curColName = colName;
        string curDisplayName = displayName;
        curColName += tag.GetDb();
        curDisplayName += tag.GetDb();
        xPrepareTableColumn(curColName, curDisplayName, "");
        xAppendColumnValue(curColName, dbtagStr);
    } 
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherTaxonId(
        const CBioSource& src,
        const string& colName,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    static const string displayName = "taxid";
    static const string defaultValue = "";

    if (!src.IsSetOrg()  ||  !src.GetOrg().IsSetDb()) {
        return true;
    }

    typedef vector< CRef< CDbtag > > DBTAGS;
    const DBTAGS& tags = src.GetOrg().GetDb();
    string taxonIdStr;
    for (DBTAGS::const_iterator cit = tags.begin(); cit != tags.end(); ++cit) {
        const CDbtag& tag = **cit;
        if (!tag.IsSetDb()  ||  tag.GetDb() != "taxon") {
            continue;
        }
        const CObject_id& objid = tag.GetTag();
        switch (objid.Which()) {
            default:
                return false;
            case CObject_id::e_Str:
                if (objid.GetStr().empty()) {
                    continue;
                }
                taxonIdStr = objid.GetStr();
                break;
            case CObject_id::e_Id:
                taxonIdStr = NStr::IntToString(objid.GetId());
                break;
        }
        break;
    }
    string curDisplayName = displayName;
    xPrepareTableColumn(colName, displayName, "");
    xAppendColumnValue(colName, taxonIdStr);
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xGatherPcrPrimers(
        const CBioSource& src,
        const string& colName,
        ILineErrorListener*)
//  ----------------------------------------------------------------------------
{
    const string pcrPrimersFwdNames = "pcr-primers.names.fwd";
    const string pcrPrimersFwdSequences = "pcr-primers.sequences.fwd";
    const string pcrPrimersRevNames = "pcr-primers.names.reverse";
    const string pcrPrimersRevSequences = "pcr-primers.sequences.reverse";

    unsigned int columnSetCounter = 0;

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
    string keyPcrPrimersFwdNames = pcrPrimersFwdNames;
    string keyPcrPrimersFwdSequences = pcrPrimersFwdSequences;
    string keyPcrPrimersRevNames = pcrPrimersRevNames;
    string keyPcrPrimersRevSequences = pcrPrimersRevSequences;
    if (columnSetCounter > 0) {
        keyPcrPrimersFwdNames += "#" + NStr::IntToString(columnSetCounter);
        keyPcrPrimersFwdSequences += "#" + NStr::IntToString(columnSetCounter);
        keyPcrPrimersRevNames += "#" + NStr::IntToString(columnSetCounter);
        keyPcrPrimersRevSequences += "#" + NStr::IntToString(columnSetCounter);
    }
    xPrepareTableColumn(
            keyPcrPrimersFwdNames, keyPcrPrimersFwdNames, "");
    xAppendColumnValue(keyPcrPrimersFwdNames, fwdName);
    xPrepareTableColumn(
            keyPcrPrimersFwdSequences, keyPcrPrimersFwdSequences, "");
    xAppendColumnValue(keyPcrPrimersFwdSequences, fwdSequence);

    xPrepareTableColumn(
            keyPcrPrimersRevNames, keyPcrPrimersRevNames, "");
    xAppendColumnValue(keyPcrPrimersRevNames, revName);
    xPrepareTableColumn(
            keyPcrPrimersRevSequences, keyPcrPrimersRevSequences, "");
    xAppendColumnValue(keyPcrPrimersRevSequences, revSequence);
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
        const FIELDS& colStubs,
        CNcbiOstream& out)
//  ----------------------------------------------------------------------------
{
    // Print columns in the order given in colStubs
    map<string,NAMELIST >  ColstubToColnames;
    typedef map<string,NAMELIST > COLSTUBNAMESMAP;


    for (COLUMNMAP::const_iterator cit=mColnameToIndex.begin();
            cit != mColnameToIndex.end();  ++cit) {
        string colName = cit->first;
        string colStub = xGetColStub(colName);
        if (ColstubToColnames.find(colStub) == ColstubToColnames.end()) {
            ColstubToColnames[colStub] = NAMELIST(1,colName);
        } else {
            ColstubToColnames[colStub].push_back(colName);
        }
    }


    NAMELIST colNames;
    for (FIELDS::const_iterator cit = colStubs.begin();
            cit != colStubs.end();  ++cit) {
        COLSTUBNAMESMAP::iterator mapIter = ColstubToColnames.find(*cit);
        if (mapIter != ColstubToColnames.end()) {
            colNames.insert(colNames.end(), mapIter->second.begin(),
                    mapIter->second.end());
        }
    }

    // Write the output table
    for (NAMELIST::const_iterator cit = colNames.begin();
            cit != colNames.end();  ++cit) {
        const CSeqTable_column& column = mSrcTable->GetColumn(*cit);
        string displayName = column.GetHeader().GetTitle();
        out << displayName << CSrcWriter::mDelimiter;
    }
    out << '\n';


    unsigned int numRows = mSrcTable->GetNum_rows();
    for (unsigned int u=0; u < numRows; ++u) {     
        for (NAMELIST::const_iterator cit = colNames.begin();
                cit != colNames.end();  ++cit) {
            const CSeqTable_column& column = mSrcTable->GetColumn(*cit);
            const string* pValue = column.GetStringPtr(u);
            bool needsQuotes = xValueNeedsQuoting(*pValue);
            if (needsQuotes) {
                out << "\"";
            }
            out << xDequotedValue(*pValue) << CSrcWriter::mDelimiter;
            if (needsQuotes) {
                out << "\"";
            }
        }
        out << '\n';
    }
    return true;  
}


//  ----------------------------------------------------------------------------
string CSrcWriter::xGetColStub(
        const string& colName)
//  ----------------------------------------------------------------------------
{
    // pcr-primers special case
    if  (NStr::Find(colName,"pcr-primers") != NPOS) {
        return "pcr-primers";
    }

    // case where column name takes the form colStub#Number
    size_t position;
    if ((position = NStr::Find(colName,"#")) != NPOS) {
        return colName.substr(0,position);
    }
    return colName;
}


//  ----------------------------------------------------------------------------
void CSrcWriter::xAppendColumnValue(
        const string& colName,
        const string& colValue)
//  ----------------------------------------------------------------------------
{
    size_t index = mColnameToIndex[colName];
    CSeqTable_column& column = *mSrcTable->SetColumns().at(index);
    column.SetData().SetString().push_back(colValue);
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::ValidateFields(
        const FIELDS& fields,
        ILineErrorListener* pEC)
//  ----------------------------------------------------------------------------
{
    for (FIELDS::const_iterator cit = fields.begin(); cit != fields.end(); ++cit) { 
        string field = *cit;
        NAMEMAP::const_iterator mapIter = sFieldnameToColname.find(xCompressFieldName(field));
        if (mapIter == sFieldnameToColname.end()) {
            CSrcError* pE = CSrcError::Create(
                    ncbi::eDiag_Error,
                    "Field name \"" + field + "\" not recognized.");
            pEC->PutError(*pE);
            delete pE;
            return false;
        }
    }
    return true;
}


//  ----------------------------------------------------------------------------
bool CSrcWriter::xValueNeedsQuoting(
        const string& value)
//  ----------------------------------------------------------------------------
{
    return (value.find(mDelimiter) != string::npos);
}


//  ----------------------------------------------------------------------------
string CSrcWriter::xDequotedValue(
        const string& value)
//  For lack of better idea, replace all occurences of "\"" with "\'\'"
//  -----------------------------------------------------------------------------
{
    return NStr::Replace(value, "\"", "\'\'");
}


//  -----------------------------------------------------------------------------
CSrcError::CSrcError(
        EDiagSev severity,
        const string& message):
    //  -----------------------------------------------------------------------------
    CLineError(ILineError::eProblem_Unset, severity, "", 0,
            "", "", "", message, CLineError::TVecOfLines())
{
}


//  -----------------------------------------------------------------------------
CSrcError* CSrcError::Create(
        EDiagSev severity,
        const string& message)
//  -----------------------------------------------------------------------------
{
    return new CSrcError(severity, message);
}

END_NCBI_SCOPE
