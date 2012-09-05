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
* Author:  Aaron Ucko, Mati Shomrat, NCBI
*
* File Description:
*   fasta-file generator application
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbireg.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/entrez2/entrez2_client.hpp>
#include <objects/entrez2/Entrez2_boolean_element.hpp>
#include <objects/entrez2/Entrez2_boolean_reply.hpp>
#include <objects/entrez2/Entrez2_eval_boolean.hpp>
#include <objects/entrez2/Entrez2_boolean_exp.hpp>
#include <objects/entrez2/Entrez2_id_list.hpp>
#include <objects/entrez2/Entrez2_docsum_data.hpp>

#include <objects/id1/id1_client.hpp>
#include <objects/id1/ID1Seq_hist.hpp>
#include <objects/id1/ID1server_maxcomplex.hpp>
#include <objects/seq/Seq_hist_rec.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seqres/Byte_graph.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/format/flat_file_generator.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
enum EOutFormat {
//  ----------------------------------------------------------------------------
    OF_ASN_TEXT = 1,
    OF_ASN_BINARY = 2,
    OF_GENBANK = 3,
    OF_GENPEPT = 4,
    OF_FASTA = 5,
    OF_QUALITY_SCORES = 6,
    OF_ENTREZ_DOCSUMS = 7,
    OF_FASTA_REVERSE_COMPLEMENT = 8
};

//  ----------------------------------------------------------------------------
enum ELookupType {
//  ----------------------------------------------------------------------------
    LT_SEQ_ENTRY = 0,
    LT_SEQ_STATE = 1,
    LT_SEQ_IDS = 2,
    LT_SEQ_HISTORY = 3,
    LT_SEQ_REVISIONS = 4
};    
 
//  ----------------------------------------------------------------------------
enum EMaxPlex {
//  ----------------------------------------------------------------------------
    MP_SEQENTRY = 0,
    MP_BIOSEQ = 1,
    MP_BIOSEQSET = 2,
    MP_NUCPROT = 3,
    MP_PUBSET = 4
};

//  ----------------------------------------------------------------------------
struct CTextColumn
//  ----------------------------------------------------------------------------
{
    size_t m_width;
    vector<string> m_entries;

    CTextColumn(): m_width(0) {};
    
    CTextColumn& AddStr(const string& str) {
        m_entries.push_back(str);
        if (str.size() < m_width) m_width = str.size();
        return *this;
    }

    string GetStr(size_t index) const {
        const string& str = m_entries[index];
        return str + string(m_width - str.size(), ' ');
    }

    size_t Width() const { return m_width; };
    size_t Height() const { return m_entries.size(); };
};

//  ----------------------------------------------------------------------------
class CSeqFetchApp : public CNcbiApplication
//  ----------------------------------------------------------------------------
{
public:
    void Init();
    int Run();

private:
    bool xWriteSequence(
        const string&);
    bool CSeqFetchApp::WriteHistoryTable(
        const CID1server_back&);

    bool xLookupFeatPlex(
        const string&,
        CRef<CID1server_back>);
    bool xLookupState(
        const string&,
        CRef<CID1server_back>);
    bool xLookupIds(
        const string&,
        CRef<CID1server_back>);
    bool xLookupHistory(
        const string&,
        CRef<CID1server_back>);
    bool xLookupRevisions(
        const string&,
        CRef<CID1server_back>);

    int xProcessFastaId();
    int xProcessGi();
    int xProcessGiFile();
    int xProcessEntrezQuery();
    int xProcessFlatId();

    bool xPoliceArguments();

    bool xNextId(
        string&);
    bool xFlatIdToGi(
        const string&,
        int&);

    CNcbiOstream& xOutStream();
    EOutFormat xOutFormat();
    string xEntrezQuery();
    string xDatabase();
    EEntry_complexities xMaxPlex();
    ELookupType xLookupType();

    CRef<CObjectManager> m_pObjMgr;
    CRef<CScope> m_pScope;
    list<string> m_Ids;
    list<string>::const_iterator m_Idit;
};

//  ----------------------------------------------------------------------------
void CSeqFetchApp::Init()
//  ----------------------------------------------------------------------------
{
    auto_ptr<CArgDescriptions> pArgDesc(new CArgDescriptions);
    pArgDesc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Fetch sequence by ID",
        false);

    {{  //output
        pArgDesc->AddDefaultKey( 
            "o", 
            "OutputFile", 
            "Output file name", 
            CArgDescriptions::eOutputFile, 
            "-", 
            CArgDescriptions::fBinary | CArgDescriptions::fOptionalSeparator);
    }}

    {{  //type
        pArgDesc->AddDefaultKey(
            "t",
            "Format",
            "Output Format",
            CArgDescriptions::eInteger,
            "1",
            CArgDescriptions::fOptionalSeparator);
        pArgDesc->SetConstraint(
            "t",
            new CArgAllow_Integers(1, 8));
    }}

    {{  //database
        pArgDesc->AddOptionalKey(
            "d",
            "Database",
            "Database to use",
            CArgDescriptions::eString,
            CArgDescriptions::fOptionalSeparator);
        pArgDesc->SetConstraint(
            "d",
            &(*new CArgAllow_Strings, "n", "p"));
    }}

    {{  //entity
        pArgDesc->AddOptionalKey(
            "e",
            "Entity",
            "Entity (retrieval) number to dump",
            CArgDescriptions::eInteger,
            CArgDescriptions::fOptionalSeparator);
        pArgDesc->SetConstraint(
            "e", 
            new CArgAllow_Integers(0, kMax_Int));
    }}

    {{  //lookup type
        pArgDesc->AddDefaultKey(
            "i",
            "Lookup",
            "Lookup type",
            CArgDescriptions::eInteger,
            0,
            CArgDescriptions::fOptionalSeparator);
        pArgDesc->SetConstraint(
            "i",
            new CArgAllow_Integers(0, 4));
    }}

    {{  //GI
        pArgDesc->AddOptionalKey(
            "g",
            "GI",
            "GI to fetch",
            CArgDescriptions::eInteger,
            CArgDescriptions::fOptionalSeparator);
        pArgDesc->SetConstraint(
            "g", 
            new CArgAllow_Integers(0, kMax_Int));
    }}

    {{  //ids from feedfile
        pArgDesc->AddOptionalKey(
            "G",
            "FeedFile",
            "File containing IDs to fetch",
            CArgDescriptions::eInputFile,
            CArgDescriptions::fPreOpen);
    }}

    {{  //compexity
        pArgDesc->AddDefaultKey(
            "c", 
            "MaxComplexity",
            "Maximum complexity",
            CArgDescriptions::eInteger,
            0,
            CArgDescriptions::fOptionalSeparator |
                CArgDescriptions::fOptionalSeparatorAllowConflict);
        pArgDesc->SetConstraint(
            "c", 
            new CArgAllow_Integers(0, 4));
    }}

    {{  //flattened
        pArgDesc->AddOptionalKey(
            "f",
            "Flattened",
            "Flattened SeqID, format may be "
            "'type([name][,[accession][,[release][,version]]])' "
            "[e.g., '5(HUMHBB)'], or "
            "type=accession[.version], "
            "or type:number",
            CArgDescriptions::eString,
            CArgDescriptions::fOptionalSeparator);
    }}

    {{  //fasta ID
        pArgDesc->AddOptionalKey(
            "s",
            "Seqid",
            "FASTA style ID to fetch",
            CArgDescriptions::eString,
            CArgDescriptions::fOptionalSeparator);
    }}

    {{  //log file
        pArgDesc->AddOptionalKey(
            "l", 
            "Log",
            "Log file",
            CArgDescriptions::eOutputFile,
            0);
    }}

    {{  //by entrez query
        pArgDesc->AddOptionalKey(
            "q", 
            "QueryString",
            "Generate GIs by Entrez query from command line",
            CArgDescriptions::eString);
    }}

    {{  //by entrez query
        pArgDesc->AddOptionalKey(
            "Q", 
            "QueryFile",
            "Generate GIs by Entrez query from file",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);
    }}

    {{  //gi only list
        pArgDesc->AddFlag(
            "n",
            "GiList");
    }}

    {{  //extra features
        pArgDesc->AddOptionalKey(
            "F",
            "ExtraFeats",
            "Add features, delimited by ',': "
            "Allowed Values are "
            "SNP, SNP_graph, CDD, MGC, HPRD, STS, tRNA, microRNA",
        CArgDescriptions::eString);
    }}
    SetupArgDescriptions(pArgDesc.release());
}

//  ----------------------------------------------------------------------------
int CSeqFetchApp::Run()
//  ----------------------------------------------------------------------------
{
    const CArgs& args = GetArgs();
    if (args["l"]) {
        SetDiagStream(&args["l"].AsOutputFile());
    }

    CONNECT_Init(&GetConfig());
    m_pObjMgr = CObjectManager::GetInstance();
    m_pScope.Reset(new CScope(*m_pObjMgr));
    CGBDataLoader::RegisterInObjectManager(*m_pObjMgr);
    m_pScope->AddDefaults();

    if (!xPoliceArguments()) {
        return 1;
    }

    if (args["s"]) {
        return xProcessFastaId();
    }
    if (args["g"]) {
        return xProcessGi();
    }
    if (args["G"]) {
        return xProcessGiFile();
    }
    if (args["f"]) {
        return xProcessFlatId();
    }
    if (args["q"]) {
        return xProcessEntrezQuery();
    }
    if (args["Q"]) {
        return xProcessEntrezQuery();
    }
     
    return 0;
}

//  ----------------------------------------------------------------------------
int  CSeqFetchApp::xProcessFastaId()
//  ----------------------------------------------------------------------------
{
    string idstr = GetArgs()["s"].AsString();
    if (!xWriteSequence(idstr)) {
        return 1;
    }
    return 0;
}

//  ----------------------------------------------------------------------------
int CSeqFetchApp::xProcessGi()
//  ----------------------------------------------------------------------------
{
    string idstr = GetArgs()["g"].AsString();
    if (!xWriteSequence(idstr)) {
        return 1;
    }
    return 0;
}

//  ----------------------------------------------------------------------------
int CSeqFetchApp::xProcessGiFile()
//  ----------------------------------------------------------------------------
{
    CNcbiIstream& feedfile = GetArgs()["G"].AsInputFile();
    while (feedfile  &&  !feedfile.eof()) {
        string idstr;
        feedfile >> idstr;
        if (idstr.empty()) {
            continue;
        }
        size_t sep = idstr.find_first_of(":=(");
        if (sep == string::npos) {
            if (!xWriteSequence(idstr)) {
                return 1;
            }
            continue;
        }
        else {
            int gi(0);
            if (!xFlatIdToGi(idstr, gi)) {
                return 1;
            }
            if (!xWriteSequence(NStr::IntToString(gi))) {
                return 1;
            }
            continue;
        }
    }
    return 0;
}

//  ----------------------------------------------------------------------------
int CSeqFetchApp::xProcessEntrezQuery()
//  ----------------------------------------------------------------------------
{
    CEntrez2Client e2Client;
    e2Client.SetDefaultRequest().SetTool("seqfetch");

    CRef<CEntrez2_boolean_element> pE2Element(
        new CEntrez2_boolean_element);
    pE2Element->SetStr(xEntrezQuery());
    
    CEntrez2_eval_boolean e2Eval;
    e2Eval.SetReturn_UIDs(true);
    CEntrez2_boolean_exp& e2Query = e2Eval.SetQuery();
    e2Query.SetExp().push_back(pE2Element);
    e2Query.SetDb() = CEntrez2_db_id(xDatabase());
    CRef<CEntrez2_boolean_reply> pE2Reply = e2Client.AskEval_boolean(e2Eval);
    
    if (!pE2Reply->GetCount()) {
        cerr << "Query error: Entrez query return no results" << endl;
        return 1;
    }
    for (CEntrez2_id_list::TConstUidIterator cit = pE2Reply->GetUids()
                .GetConstUidIterator();
            !cit.AtEnd();
            ++cit) {
        if (!xWriteSequence(NStr::IntToString(*cit))) {
            return 1;
        }
    }
    return 0;
}

//  ----------------------------------------------------------------------------
int CSeqFetchApp::xProcessFlatId()
//  ----------------------------------------------------------------------------
{
    CID1Client id1Client;
    string flatid(GetArgs()["f"].AsString());
    int gi(0);
    if (!xFlatIdToGi(flatid, gi)) {
        return 1;
    }
    xWriteSequence(NStr::IntToString(gi));
    return 0;
}

//  ----------------------------------------------------------------------------
bool CSeqFetchApp::xLookupFeatPlex(
    const string& idstr, 
    CRef<CID1server_back> pLookup)
//  ----------------------------------------------------------------------------
{
    CID1Client id1Client;
    EEntry_complexities maxplex = xMaxPlex();
    if (maxplex == eEntry_complexities_entry) {
        return false; 
    }
    CRef<CID1server_maxcomplex> pMaxPlex(new CID1server_maxcomplex);
    pMaxPlex->SetMaxplex(maxplex);
    pMaxPlex->SetGi(id1Client.AskGetgi(CSeq_id(idstr)));
    id1Client.AskGetsefromgi(*pMaxPlex, pLookup);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSeqFetchApp::xLookupState(
    const string& idstr, 
    CRef<CID1server_back> pLookup)
//  ----------------------------------------------------------------------------
{
    CID1Client id1Client;
    int gi = id1Client.AskGetgi(CSeq_id(idstr));
    int state = id1Client.AskGetgistate(gi, pLookup);
    if (xOutFormat() == OF_FASTA) {
        xOutStream() << "gi=" << gi << ", states: ";
        switch(state & 0xFF) {
        default:
            xOutStream() << "UNKNOWN";
            break;
        case 0x0:
            xOutStream() << "NONEXISTENT";
            break;
        case 0x10:
            xOutStream() << "DELETED";
            break;
        case 0x20:
            xOutStream() << "REPLACED";
            break;
        case 0x40:
            xOutStream() << "LIVE";
            break;
        }
        if (state & 0x100) {
            xOutStream() << "|SUPPRESSED";
        }
        if (state & 0x200) {
            xOutStream() << "|WITHDRAWN";
        }
        if (state & 0x400) {
            xOutStream() << "|CONFIDENTIAL";
        }
        xOutStream() << endl;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSeqFetchApp::xLookupIds(
    const string& idstr, 
    CRef<CID1server_back> pLookup)
//  ----------------------------------------------------------------------------
{
    CID1Client id1Client;
    int gi = id1Client.AskGetgi(CSeq_id(idstr));
    id1Client.AskGetseqidsfromgi(gi, pLookup);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSeqFetchApp::xLookupHistory(
    const string& idstr, 
    CRef<CID1server_back> pLookup)
//  ----------------------------------------------------------------------------
{
    CID1Client id1Client;
    int gi = id1Client.AskGetgi(CSeq_id(idstr));
    id1Client.AskGetgihist(gi, pLookup);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSeqFetchApp::xLookupRevisions(
    const string& idstr, 
    CRef<CID1server_back> pLookup)
//  ----------------------------------------------------------------------------
{
    CID1Client id1Client;
    int gi = id1Client.AskGetgi(CSeq_id(idstr));
    id1Client.AskGetgirev(gi, pLookup);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSeqFetchApp::xPoliceArguments()
//  ----------------------------------------------------------------------------
{
    const CArgs& args = GetArgs();

    int idspecs = 0;
    if (args["s"]) ++idspecs;
    if (args["g"]) ++idspecs;
    if (args["G"]) ++idspecs;
    if (args["f"]) ++idspecs;
    if (args["q"]) ++idspecs;
    if (args["Q"]) ++idspecs;
    if (1 != idspecs) {
        cerr << "Command line error: Need exactly one out of [s|g|G|f|q|Q]" << endl;
        return false;
    }
    if (args["q"]  &&  !args["d"]) {
        cerr << "Command line error: Option \"q\" needs option \"d\" specified" << endl;
        return false;
    }
    if (args["Q"]  &&  !args["d"]) {
        cerr << "Command line error: Option \"Q\" needs option \"d\" specified" << endl;
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSeqFetchApp::xWriteSequence(
    const string& idstr)
//  ----------------------------------------------------------------------------
{
    const CArgs& args = GetArgs();
    CConstRef<CSerialObject> pReply;

    //factor by lookup type
    if (LT_SEQ_ENTRY == xLookupType()) {
        CRef<CID1server_back> pLookup(new CID1server_back);
        xLookupFeatPlex(idstr, pLookup);
        pReply = pLookup;
    }

    if (LT_SEQ_STATE == xLookupType()) {
        CRef<CID1server_back> pLookup(new CID1server_back);
        xLookupState(idstr, pLookup);
        if (xOutFormat() != OF_FASTA) {
            pReply = pLookup;
        }
    }

    if (LT_SEQ_IDS == xLookupType()) {
        CRef<CID1server_back> pLookup(new CID1server_back);
        xLookupIds(idstr, pLookup);
        if (xOutFormat() != OF_FASTA) {
            pReply = pLookup;
        }
    }

    if (LT_SEQ_HISTORY == xLookupType()) {
        CRef<CID1server_back> pLookup(new CID1server_back);
        xLookupHistory(idstr, pLookup);
        if (xOutFormat() == OF_FASTA) {
            WriteHistoryTable(*pLookup);
        }
        else {
            pReply = pLookup;
        }
    }

    if (LT_SEQ_REVISIONS == xLookupType()) {
        CRef<CID1server_back> pLookup(new CID1server_back);
        xLookupRevisions(idstr, pLookup);
        if (xOutFormat() == OF_FASTA) {
            WriteHistoryTable(*pLookup);
        }
        else {
            pReply = pLookup;
        }
    }
    
    //based on what's in the reply object, setup a bioseq handle
    CBioseq_Handle bsh;
    if (pReply) {
        const CSeq_entry* pSe = dynamic_cast<const CSeq_entry*>(
            pReply.GetPointer());
        EOutFormat format = xOutFormat();
        if(pSe  &&  (format == OF_FASTA || format == OF_GENBANK || 
                format == OF_GENPEPT || format == OF_QUALITY_SCORES)) {
            //need handle for formatting
            m_pScope->ResetDataAndHistory();
            CSeq_entry_Handle seh = m_pScope->AddTopLevelSeqEntry(*pSe);
            CSeq_id id(idstr);
            bsh = m_pScope->GetBioseqHandleFromTSE(id, seh);
            if (!bsh) {
                cerr << "Query error: Bioseq not found: " << id.AsFastaString() 
                    << endl;
                return false;
            }
        }
    }
    else {//use object manager
        m_pScope->ResetDataAndHistory();
        CSeq_id id(idstr);
        CBioseq_Handle bsh = m_pScope->GetBioseqHandle(id);
        if (!bsh) {
            cerr << "Query error: Bioseq not found: " << id.AsFastaString() 
                << endl;
        }
        pReply = bsh.GetTopLevelEntry().GetCompleteSeq_entry();
    }


    switch(xOutFormat()) {
    default: {
            break;
        }

    case OF_FASTA_REVERSE_COMPLEMENT: {
            /*TODO*/
            break;
        }

    case OF_ENTREZ_DOCSUMS: {
            CEntrez2Client e2Client;
            e2Client.SetDefaultRequest().SetTool("seqfetch");

            CEntrez2_id_list idlist;
            idlist.SetDb() = CEntrez2_db_id(xDatabase());
            idlist.SetNum(1);
            idlist.SetUids().resize(idlist.sm_UidSize);

            CEntrez2_id_list::TUidIterator it = idlist.GetUidIterator();
            *it = NStr::StringToInt(idstr);
            CRef<CEntrez2_docsum_list> pDocsums = e2Client.AskGet_docsum(idlist);
            if (!pDocsums->GetCount()) {
                cerr << "Query error: Entrez query return no results" << endl;
                return false;
            }

            string caption, title;
            for (CTypeConstIterator<CEntrez2_docsum_data> it = ConstBegin(*pDocsums);
                    it;  ++it) {
                if (it->GetField_name() == "Caption") {
                        caption = it->GetField_value();
                } 
                else if (it->GetField_name() == "Title") {
                    title = it->GetField_value();
                }
            }
            xOutStream() << '>';
            if ( !caption.empty() ) {
                xOutStream() << caption;
            }
            xOutStream() << ' ';
            if ( !title.empty() ) {
                xOutStream() << title;
            }
            break;
        }

    case OF_QUALITY_SCORES: {
            string best_id = FindBestChoice(
                bsh.GetBioseqCore()->GetId(), CSeq_id::Score)
                    ->GetSeqIdString(true);
            for (CGraph_CI it(bsh); it; ++it) {
                string title = it->GetTitle();
                if (title.find("uality") == string::npos) {
                    continue;
                }

                const CByte_graph& bg = it->GetGraph().GetByte();
                xOutStream() << ">" << best_id << " " << title
                    << " (Length: " << it->GetNumval()
                    << ", Min: " << bg.GetMin()
                    << ", Max: " << bg.GetMax() << ")" << endl;

                for (unsigned int u=0; u < bg.GetValues().size(); ++u) {
                    int value = static_cast<int>(bg.GetValues()[u]);
                    xOutStream() << setw(3) << value;
                    if (19 == u % 20) {
                        xOutStream() << endl;
                    }
                }
            }
            break;
        }

    case OF_FASTA: {
            ELookupType lt = xLookupType();
            if (LT_SEQ_ENTRY == lt) {
                CFastaOstream out(xOutStream());
                out.SetFlag(CFastaOstream::fAssembleParts);
                out.SetFlag(CFastaOstream::fInstantiateGaps);
                out.Write(bsh);
                xOutStream() << endl;
                break;
            }
            if (LT_SEQ_IDS == lt) {
                list<CRef<CSeq_id > > ids = bsh.GetBioseqCore()->GetId();
                for (list<CRef<CSeq_id > >::const_iterator cit = ids.begin();
                        cit != ids.end(); cit++) {
                    if (cit != ids.begin()) xOutStream() << '|';
                    (*cit)->WriteAsFasta(xOutStream());
                }
                break;
            }
    }

    case OF_GENPEPT: {
            CSeq_id id(idstr);
            CSeq_entry_Handle seh = bsh.GetTopLevelEntry();

            CFlatFileConfig fc;
            fc.SetMode(CFlatFileConfig::eMode_Entrez);
            fc.SetFormat(CFlatFileConfig::eFormat_GenBank);
            fc.SetHideSNPFeatures().SetShowContigFeatures()
                .SetShowContigSources();
            fc.SetViewProt();

            CFlatFileGenerator fg(fc);
            fg.SetAnnotSelector().ExcludeNamedAnnots("SNP");
            fg.Generate(seh, xOutStream());
            break;
        }

    case OF_GENBANK: {
            CSeq_id id(idstr);
            CSeq_entry_Handle seh = bsh.GetTopLevelEntry();

            CFlatFileConfig fc;
            fc.SetMode(CFlatFileConfig::eMode_Entrez);
            fc.SetFormat(CFlatFileConfig::eFormat_GenBank);
            fc.SetHideSNPFeatures().SetShowContigFeatures()
                .SetShowContigSources();

            CFlatFileGenerator fg(fc);
            fg.SetAnnotSelector().ExcludeNamedAnnots("SNP");
            fg.Generate(seh, xOutStream());
            break;
        }

    case OF_ASN_BINARY: {
            CConstRef<CSeq_entry> pSeqEntry = 
                bsh.GetTopLevelEntry().GetCompleteSeq_entry();
            xOutStream() << MSerial_AsnBinary << *pSeqEntry;
            break;
        }

    case OF_ASN_TEXT: {
            CConstRef<CSeq_entry> pSeqEntry = 
                bsh.GetTopLevelEntry().GetCompleteSeq_entry();
            xOutStream() << MSerial_AsnText << *pSeqEntry << endl;
            break;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSeqFetchApp::WriteHistoryTable(
    const CID1server_back& id1Reply)
//  ----------------------------------------------------------------------------
{
    CTextColumn gis, dates, dbs, retrievals;
    gis.AddStr("GI").AddStr("--");
    dates.AddStr("Loaded").AddStr("------");
    dbs.AddStr("DB").AddStr("--");
    retrievals.AddStr("Retrieval No.").AddStr("-------------");
    for (CTypeConstIterator<CSeq_hist_rec> it = ConstBegin(id1Reply);
         it;  ++it) {
        int    gi = 0;
        string db, retrieval;

        if ( it->GetDate().IsStr() ) {
            dates.AddStr(it->GetDate().GetStr());
        } 
        else {
            CNcbiOstrstream oss;
            const CDate_std& date = it->GetDate().GetStd();
            oss << setfill('0') << setw(2) << date.GetMonth() << '/'
                << setw(2) << date.GetDay() << '/' << date.GetYear();
            dates.AddStr(CNcbiOstrstreamToString(oss));
        }

        for (CSeq_hist_rec::TIds::const_iterator cit=it->GetIds().begin();
                cit != it->GetIds().end(); ++cit) {
            if ((*cit)->IsGi()) {
                gi = (*cit)->GetGi();
                continue;
            } 
            if ((*cit)->IsGeneral()) {
                db = (*cit)->GetGeneral().GetDb();
                const CObject_id& tag = (*cit)->GetGeneral().GetTag();
                if (tag.IsStr()) {
                    retrieval = tag.GetStr();
                } 
                else {
                    retrieval = NStr::IntToString(tag.GetId());
                }
            }
        }
        gis.AddStr(NStr::IntToString(gi));
        dbs.AddStr(db);
        retrievals.AddStr(retrieval);
    }

    for (unsigned int n = 0;  n < gis.Height();  n++) {
        xOutStream() << gis.GetStr(n) << "  " << dates.GetStr(n) << "  "
            << dbs.GetStr(n) << "  " << retrievals.GetStr(n) << endl;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSeqFetchApp::xNextId(
    string& idstr)
//  ----------------------------------------------------------------------------
{
    if ( m_Idit != m_Ids.end()) {
        idstr = *m_Idit;
        m_Idit++;
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
CNcbiOstream& CSeqFetchApp::xOutStream()
//  ----------------------------------------------------------------------------
{
    static CNcbiOstream& ostr = GetArgs()["o"].AsOutputFile();
    return ostr;
}

//  ----------------------------------------------------------------------------
EOutFormat CSeqFetchApp::xOutFormat()
//  ----------------------------------------------------------------------------
{
    return EOutFormat(GetArgs()["t"].AsInteger());
}

//  ----------------------------------------------------------------------------
ELookupType CSeqFetchApp::xLookupType()
//  ----------------------------------------------------------------------------
{
    return ELookupType(GetArgs()["i"].AsInteger());
}

//  ----------------------------------------------------------------------------
EEntry_complexities CSeqFetchApp::xMaxPlex()
//  ----------------------------------------------------------------------------
{
    unsigned int mpReturn(eEntry_complexities_entry);
    const CArgs& args = GetArgs();
    list<string> extraFeats;
    if (args["F"]) {
        NStr::Split(args["F"].AsString(), ",", extraFeats);
    }
    EMaxPlex maxPlex = EMaxPlex(args["c"].AsInteger());

    if (extraFeats.empty()  &&  MP_SEQENTRY == maxPlex) {
        return EEntry_complexities(mpReturn);
    }

    switch(maxPlex) {
    default:
        break;
    case MP_BIOSEQ:
        mpReturn = eEntry_complexities_bioseq;
        break;
    case MP_BIOSEQSET:
        mpReturn = eEntry_complexities_bioseq_set;
        break;
    case MP_NUCPROT:
        mpReturn = eEntry_complexities_nuc_prot;
        break;
    case MP_PUBSET:
        mpReturn = eEntry_complexities_pub_set;
        break;
    }

    unsigned int featMask(0);
    for (list<string>::const_iterator cit = extraFeats.begin(); 
            cit != extraFeats.end(); ++cit) {
        string feat = *cit;
        NStr::ToLower(feat);
        if (feat == "snp") {
            featMask |= (1<<4);
        }
        else if (feat == "snp_graph") {
            featMask |= (1<<6);
        }
        else if (feat == "cdd") {
            featMask |= (1<<7);
        }
        else if (feat == "mgc") {
            featMask |= (1<<8);
        }
        else if (feat == "hprd") {
            featMask |= (1<<9);
        }
        else if (feat == "sts") {
            featMask |= (1<<10);
        }
        else if (feat == "trna") {
            featMask |= (1<<11);
        }
        else if (feat == "exon") {
            featMask |= (1<<13);
        }   
    }
    return EEntry_complexities(mpReturn | featMask);
}

//  ----------------------------------------------------------------------------
bool CSeqFetchApp::xFlatIdToGi(
    const string& flatid,
    int& gi)
//  ----------------------------------------------------------------------------
{
    CID1Client id1Client;

    CSeq_id::E_Choice idtype = static_cast<CSeq_id::E_Choice>(
        atoi(flatid.c_str()));
    size_t sep = flatid.find_first_of(":=(");
    string data = flatid.substr(sep+1);

    switch(flatid[sep]) {
        default:
            return false;
        case ':':
        case '=': {
            CSeq_id id(idtype, data, "");
            gi = id1Client.AskGetgi(id);
            break;
        }
        case '(': {
            data.erase(data.end() - 1);
            vector<string> parts;
            NStr::Tokenize(data, ",", parts);
            parts.resize(4, "");
            CSeq_id id(idtype, parts[1], parts[0], NStr::StringToInt(parts[3]),
                parts[2]);
            gi = id1Client.AskGetgi(id);
            break;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
string CSeqFetchApp::xEntrezQuery()
//  ----------------------------------------------------------------------------
{
    const CArgs& args = GetArgs();

    if (args["q"]) {
        return args["q"].AsString();
    }
    if (args["Q"]) {
        CNcbiIstream& istr(args["Q"].AsInputFile());
        CNcbiOstrstream ostr;
        ostr << istr.rdbuf();
        string query(ostr.str(), ostr.pcount());
        for (size_t i=0; i < query.size(); ++i) {
            if (iscntrl(query[i])) {
                query[i] = ' ';
            }
        }
        return query;
    }
    return "";
}

//  ---------------------------------------------------------------------------
string CSeqFetchApp::xDatabase()
//  ---------------------------------------------------------------------------
{
    string arg = GetArgs()["d"].AsString();
    if (arg == "n") {
        return "Nucleotide";
    }
    if (arg == "p") {
        return "Protein";
    }
    return arg;
}

END_NCBI_SCOPE
USING_NCBI_SCOPE;

//  ===========================================================================
int main(int argc, const char** argv)
//  ===========================================================================
{
	return CSeqFetchApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
