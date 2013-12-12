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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   fasta-file generator application
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <dbapi/driver/drivers.hpp>

#include <objects/seqtable/Seq_table.hpp>
#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqtable/SeqTable_single_data.hpp>
#include <objects/seqtable/SeqTable_multi_data.hpp>

#include <objects/entrez2/Entrez2_boolean_element.hpp>
#include <objects/entrez2/Entrez2_boolean_reply.hpp>
#include <objects/entrez2/Entrez2_boolean_exp.hpp>
#include <objects/entrez2/Entrez2_eval_boolean.hpp>
#include <objects/entrez2/Entrez2_id_list.hpp>
#include <objects/entrez2/entrez2_client.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/gtf_writer.hpp>
#include <objtools/writers/gff3_writer.hpp>
#include <objtools/writers/gff3flybase_writer.hpp>
#include <objtools/writers/wiggle_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ============================================================================
class CSrcWriter: public CObject
//  ============================================================================
{
public:
    typedef map<string, int> COLUMNMAP;
    typedef vector<string> FIELDS;
public:
    static const string DELIMITER;
    static const string keyId;
    static const string keyTaxname;
    static const string keyGenome;
    static const string keyOrigin;
    static const string keyDivision;
    static const string keyPcrPrimersFwdNames;
    static const string keyPcrPrimersFwdSequences;
    static const string keyPcrPrimersRevNames;
    static const string keyPcrPrimersRevSequences;
    static const string keyOrgCommon;

    CSrcWriter(
        unsigned int flags=0 ) :
        mFlags( flags ) {
        xInit();
    };

    virtual ~CSrcWriter()
    {};

    virtual bool WriteBioseqHandle( 
        CBioseq_Handle,
        const FIELDS&,
        CNcbiOstream&);

    virtual bool WriteBioseqHandles( 
        const vector<CBioseq_Handle>&,
        const FIELDS&,
        CNcbiOstream&);

    static bool ValidateFields(
        const FIELDS fields);

protected:
    void xInit();

    virtual bool xGather(CBioseq_Handle, const FIELDS&);
    virtual bool xGatherId(CBioseq_Handle);
    virtual bool xGatherTaxname(const CBioSource&);
    virtual bool xGatherDivision(const CBioSource&);
    virtual bool xGatherGenome(const CBioSource&);
    virtual bool xGatherOrigin(const CBioSource&);
    virtual bool xGatherSubtype(const CBioSource&);
    virtual bool xGatherOrgMod(const CBioSource&);
    virtual bool xGatherOrgCommon(const CBioSource&);
    virtual bool xGatherPcrPrimers(const CBioSource&);

    virtual bool xFormatTabDelimited(CNcbiOstream&);

    static string xPrimerSetNames(const CPCRPrimerSet&);
    static string xPrimerSetSequences(const CPCRPrimerSet&);

    void xPrepareTableColumn(const string&, const string&, const string& ="");
    void xAppendColumnValue(const string&, const string&);

public:
    static const FIELDS RecognizedFields;
    static const FIELDS DefaultFields;
protected:
    CRef<CSeq_table> mSrcTable;
    COLUMNMAP mColnameToIndex;
    unsigned int mFlags;
};

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

static const string arrRecognizedFields[] = {
    "div",                  //syn for org.orgname.div
    "division",             //syn for org.orgname.div
    "genome",
    "orgmod",               //syn for org.mod
    "origin",
    "pcr-primers",
    "subtype",
    "taxname",              //syn for org.taxname

    "org.common",
    "org.mod",
    "org.orgname.div",
    "org.taxname"
};
const size_t countRecognizedFields = sizeof(arrRecognizedFields)/sizeof(string);

const CSrcWriter::FIELDS CSrcWriter::RecognizedFields(
    arrRecognizedFields, arrRecognizedFields + countRecognizedFields);

static const string arrDefaultFields[] = {
    "id",
    "taxname",
    "division",
};
const size_t countDefaultFields = sizeof(arrDefaultFields)/sizeof(string);

const CSrcWriter::FIELDS CSrcWriter::DefaultFields(
    arrDefaultFields, arrDefaultFields + countDefaultFields);

//  ============================================================================
class CSrcChkApp : public CNcbiApplication
//  ============================================================================
{
public:
    void Init();
    int Run();

private:
    CNcbiOstream* xInitOutputStream(
        const CArgs&);
    bool xTryProcessId(
        const CArgs&);
    bool xTryProcessIdFile(
        const CArgs&);
    bool xGetDesiredFields(
        const CArgs&,
        vector<string>&);
    CSrcWriter* xInitWriter(
        const CArgs&);

private:
    CRef<CObjectManager> m_pObjMngr;
    CRef<CScope> m_pScope;
    CRef<CSrcWriter> m_pWriter;
};

//  ----------------------------------------------------------------------------
void CSrcChkApp::Init()
//  ----------------------------------------------------------------------------
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Extract Genbank source qualifiers",
        false);
    
    // input
    {{
        arg_desc->AddOptionalKey( "i", "InputFile", 
            "Input file name", CArgDescriptions::eInputFile );
        arg_desc->AddOptionalKey( "id", "InputID",
            "Input ID (accession or GI number)", CArgDescriptions::eString );
        arg_desc->AddOptionalKey( "ids", "IDsFile", 
            "IDs file name", CArgDescriptions::eInputFile );
    }}

    // parameters
    {{
        arg_desc->AddOptionalKey( "f", "FieldsList",
            "List of fields", CArgDescriptions::eString );
        arg_desc->AddOptionalKey( "F", "FieldsFile",
            "File of fields", CArgDescriptions::eInputFile );
    }}
    // output
    {{ 
        arg_desc->AddOptionalKey( "o", "OutputFile", 
            "Output file name", CArgDescriptions::eOutputFile );
    }}

    SetupArgDescriptions(arg_desc.release());
}

//  ----------------------------------------------------------------------------
int CSrcChkApp::Run()
//  ----------------------------------------------------------------------------
{
    const CArgs& args = GetArgs();

	CONNECT_Init(&GetConfig());
    m_pObjMngr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*m_pObjMngr);
    m_pScope.Reset(new CScope(*m_pObjMngr));
    m_pScope->AddDefaults();

    m_pWriter.Reset(xInitWriter(args));

    if (xTryProcessId(args)) {
        return 0;
    }
    if (xTryProcessIdFile(args)) {
        return 0;
    }
    return 1;
}

//  -----------------------------------------------------------------------------
bool CSrcChkApp::xTryProcessIdFile(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    if(!args["ids"]) {
        return false;
    }
    CNcbiOstream* pOs = xInitOutputStream(args);

    const streamsize maxLineSize(100);
    char line[maxLineSize];

    CNcbiIstream& ifstr = args["ids"].AsInputFile();
    vector<CBioseq_Handle> vecBsh;
    while (!ifstr.eof()) {
        ifstr.getline(line, maxLineSize);
        if (line[0] == 0  ||  line[0] == '#') {
            continue;
        }
        string id(line);
        NStr::TruncateSpacesInPlace(id);
        CSeq_id_Handle seqh = CSeq_id_Handle::GetHandle(id);
        CBioseq_Handle bsh = m_pScope->GetBioseqHandle(seqh);
        if (!bsh) {
            return false;
        }
        vecBsh.push_back(bsh);
    }
    CSrcWriter::FIELDS desiredFields;
    if (!xGetDesiredFields(args, desiredFields)) {
        return false;
    }
    if (!m_pWriter->WriteBioseqHandles(vecBsh, desiredFields, *pOs)) {
        return false;
    }
    return true;
}    

//  -----------------------------------------------------------------------------
bool CSrcChkApp::xTryProcessId(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    if(!args["id"]) {
        return false;
    }
    CSeq_id_Handle seqh = CSeq_id_Handle::GetHandle(args["id"].AsString());
    CBioseq_Handle bsh = m_pScope->GetBioseqHandle(seqh);
    if (!bsh) {
        return false;
    }
    CNcbiOstream* pOs = xInitOutputStream(args);
    CSrcWriter::FIELDS desiredFields;
    if (!xGetDesiredFields(args, desiredFields)) {
        return false;
    }
    return m_pWriter->WriteBioseqHandle(bsh, desiredFields, *pOs);
}    

//  -----------------------------------------------------------------------------
bool CSrcChkApp::xGetDesiredFields(
    const CArgs& args,
    CSrcWriter::FIELDS& fields)
//  -----------------------------------------------------------------------------
{
    if (args["f"]) {
        string fieldString = args["f"].AsString();
        NStr::Tokenize(fieldString, ",", fields);
        return CSrcWriter::ValidateFields(fields);
    }
    if (args["F"]) {
        const streamsize maxLineSize(100);
        char line[maxLineSize];
        CNcbiIstream& ifstr = args["ids"].AsInputFile();
        while (!ifstr.eof()) {
            ifstr.getline(line, maxLineSize);
            if (line[0] == 0  ||  line[0] == '#') {
                continue;
            }
            string field(line);
            NStr::TruncateSpacesInPlace(field);
            if (field.empty()) {
                continue;
            }
            fields.push_back(field);
        }
        return CSrcWriter::ValidateFields(fields);
    }
    fields.assign(
        CSrcWriter::DefaultFields.begin(), CSrcWriter::DefaultFields.end());
    return true;
}

//  -----------------------------------------------------------------------------
CNcbiOstream* CSrcChkApp::xInitOutputStream(
    const CArgs& args)
//  -----------------------------------------------------------------------------
{
    if (!args["o"]) {
        return &cout;
    }
    try {
        return &args["o"].AsOutputFile();
    }
    catch(...) {
        NCBI_THROW(CObjWriterException, eArgErr, 
            "xInitOutputStream: Unable to create output file \"" +
            args["o"].AsString() +
            "\"");
    }
}

//  ----------------------------------------------------------------------------
CSrcWriter* CSrcChkApp::xInitWriter(
    const CArgs& args)
//  ----------------------------------------------------------------------------
{
    return new CSrcWriter(0);
}

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

    CRef<CSeqTable_column> pColumnTaxname(new CSeqTable_column());
    pColumnTaxname->SetHeader().SetField_name(CSrcWriter::keyTaxname);
    pColumnTaxname->SetHeader().SetTitle("Organism Name");
    pColumnTaxname->SetDefault().SetString("taxname");
    mColnameToIndex[CSrcWriter::keyTaxname] = mSrcTable->GetColumns().size();
    mSrcTable->SetColumns().push_back(pColumnTaxname);
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
            string key = *cit;
            if ((key == "taxname")  &&  !xGatherTaxname(src)) {
                return false;
            }
            if ((key == "div")  &&  !xGatherDivision(src)) {
                return false;
            }
            if ((key == "division")  &&  !xGatherDivision(src)) {
                return false;
            }
            if ((key == "genome")  &&  !xGatherGenome(src)) {
                return false;
            }
            if ((key == "orgmod")  &&  !xGatherOrgMod(src)) {
                return false;
            }
            if ((key == "origin")  &&  !xGatherOrigin(src)) {
                return false;
            }
            if ((key == "pcr-primers")  &&  !xGatherPcrPrimers(src)) {
                return false;
            }
            if ((key == "subtype")  &&  !xGatherSubtype(src)) {
                return false;
            }
            if ((key == "taxname")  &&  !xGatherTaxname(src)) {
                return false;
            }
            if ((key == "org.div")  &&  !xGatherDivision(src)) {
                return false;
            }
            if ((key == "org.mod")  &&  !xGatherOrgMod(src)) {
                return false;
            }
            if ((key == "org.taxname")  &&  !xGatherTaxname(src)) {
                return false;
            }
            if ((key == "org.common")  &&  !xGatherOrgCommon(src)) {
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
        for (FIELDS::const_iterator fit = RecognizedFields.begin(); 
                fit != RecognizedFields.end(); ++fit) {
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
USING_NCBI_SCOPE;

//  ===========================================================================
int main(int argc, const char** argv)
//  ===========================================================================
{
	return CSrcChkApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
