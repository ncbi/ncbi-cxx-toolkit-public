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
 * Author:  Denis Vakatov, Aleksey Grichenko, Aaron Ucko
 *
 * File Description:
 *   New IDFETCH network client (get data from "ID1")
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbireg.hpp>

#include <connect/ncbi_core_cxx.hpp>

#include <serial/enumvalues.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/iterator.hpp>

#include <objects/entrez2/Entrez2_boolean_element.hpp>
#include <objects/entrez2/Entrez2_boolean_exp.hpp>
#include <objects/entrez2/Entrez2_boolean_reply.hpp>
#include <objects/entrez2/Entrez2_db_id.hpp>
#include <objects/entrez2/Entrez2_docsum.hpp>
#include <objects/entrez2/Entrez2_docsum_data.hpp>
#include <objects/entrez2/Entrez2_docsum_list.hpp>
#include <objects/entrez2/Entrez2_eval_boolean.hpp>
#include <objects/entrez2/Entrez2_id_list.hpp>
#include <objects/entrez2/entrez2_client.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/id1/ID1Seq_hist.hpp>
#include <objects/id1/ID1server_maxcomplex.hpp>
#include <objects/id1/id1_client.hpp>

#include <objmgr/graph_ci.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqres/Byte_graph.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#if 1
#include <objtools/flat/flat_ncbi_formatter.hpp>
#else
#include <objmgr/util/genbank.hpp>
#endif
#include <objmgr/util/sequence.hpp>

#include <memory>
#include <algorithm>


BEGIN_NCBI_SCOPE
USING_SCOPE(NCBI_NS_NCBI::objects); // MSVC requires qualification (!)


/////////////////////////////////
//  CId1FetchApp::
//

class CId1FetchApp : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run (void);
    virtual void Exit(void);

private:
    bool LookUpGI(int gi);
    int  LookUpFastaSeqID(const string& s);
    int  LookUpFlatSeqID(const string& s);

    void WriteFastaIDs     (const list< CRef< CSeq_id > >& ids);

    void WriteHistoryTable (const CID1server_back& id1_reply);
    void WriteQualityScores(CBioseq_Handle& handle);

    CNcbiOstream*        m_OutputFile;
    CID1Client           m_ID1Client;
    CEntrez2Client       m_E2Client;
    CRef<CObjectManager> m_ObjMgr;
    CRef<CScope>         m_Scope;
};


void CId1FetchApp::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI
    arg_desc->AddOptionalKey
        ("gi", "SeqEntryID",
         "GI id of the Seq-Entry to fetch",
         CArgDescriptions::eInteger);
    arg_desc->SetConstraint
        ("gi", new CArgAllow_Integers(0, 99999999));

    // Output format
    arg_desc->AddDefaultKey
        ("fmt", "OutputFormat",
         "Format to dump the resulting data in",
         CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint
        ("fmt", &(*new CArgAllow_Strings,
                  "asn", "asnb", "xml", "genbank", "genpept", "fasta",
                  "quality", "docsum"));

    // Output datafile
    arg_desc->AddDefaultKey
        ("out", "ResultFile",
         "File to dump the resulting data to",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fBinary);

    // Log file
    arg_desc->AddOptionalKey
        ("log", "LogFile",
         "File to post errors and messages to",
         CArgDescriptions::eOutputFile,
         0);

    // Database to use
    arg_desc->AddOptionalKey
        ("db", "Database", // was "-d" in `idfetch'
         "Database to use",
         CArgDescriptions::eString);
    
    // Entity number
    arg_desc->AddOptionalKey
        ("ent", "EntityNumber", // was "-e" in `idfetch'
         "(Sub)entity number (retrieval number) to dump",
         CArgDescriptions::eInteger);
    arg_desc->SetConstraint
        ("ent", new CArgAllow_Integers(0, 99999999));

    // Type of lookup
    arg_desc->AddDefaultKey
        ("lt", "LookupType", // combination of "-i" (!) and "-n" in `idfetch'
         "Type of lookup",
         CArgDescriptions::eString, "entry");
    arg_desc->SetConstraint
        ("lt", &(*new CArgAllow_Strings,
                 "entry", "state", "ids", "history", "revisions", "none"));
    
    // File with list of stuff to dump
    arg_desc->AddOptionalKey
        ("in", "RequestFile", // was "-G" (!) in `idfetch'
         "File with list of GIs, (versioned) accessions, FASTA SeqIDs to dump",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);
         
    // Maximum complexity
    arg_desc->AddDefaultKey
        ("maxplex", "MaxComplexity", // was "-c" in `idfetch'
         "Maximum complexity to return",
         CArgDescriptions::eString, "entry");
    arg_desc->SetConstraint
        ("maxplex", &(*new CArgAllow_Strings,
                      "entry", "bioseq", "bioseq-set", "nuc-prot",
                      "pub-set"));
    
    // Flattened SeqID
    arg_desc->AddOptionalKey
        ("flat", "FlatID", // was "-f" in `idfetch'
         "Flattened SeqID; format can be\n"
         "\t'type([name][,[accession][,[release][,version]]])'"
         " [e.g., '5(HUMHBB)'],\n"
         "\ttype=accession[.version], or type:number",
         CArgDescriptions::eString);
    
    // FASTA-style SeqID
    arg_desc->AddOptionalKey
        ("fasta", "FastaID", // was "-s" in `idfetch'
         "FASTA-style SeqID, in the form \"type|data\"; choices are\n"
         "\tlcl|int lcl|str bbs|int bbm|int gim|int gb|acc|loc emb|acc|loc\n"
         "\tpir|acc|name sp|acc|name pat|country|patent|seq ref|acc|name|rel\n"
         "\tgnl|db|id gi|int dbj|acc|loc prf|acc|name pdb|entry|chain\n"
         "\ttpg|acc|name tpe|acc|name tpd|acc|name",
         CArgDescriptions::eString);

    // Generate GI list by Entrez query
    arg_desc->AddOptionalKey
        ("query", "EntrezQueryString", // was "-q" in `idfetch'
         "Generate GI list by Entrez query given on command line",
         CArgDescriptions::eString);
    arg_desc->AddOptionalKey
        ("qf", "EntrezQueryFile", // was "-Q" in `idfetch'
         "Generate GI list by Entrez query in given file",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    // Program description
    string prog_description =
        "Fetch SeqEntry from ID server by its GI ID, possibly obtained from\n"
        "its SeqID or an Entrez query";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);


    arg_desc->AddDefaultKey
        ("repeat", "repeat",
         "Repeat fetch number of times",
         CArgDescriptions::eInteger, "1");

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


// Workaround for "replace_if"
inline bool s_IsControl(char c)
{
    return iscntrl(c) ? true : false;
}


int CId1FetchApp::Run(void)
{
    // Process command line args
    const CArgs& args = GetArgs();

    // Setup and tune logging facilities
    if ( args["log"] ) {
        SetDiagStream( &args["log"].AsOutputFile() );
    }
#ifdef _DEBUG
    // SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_All);
#endif

    // Make sure the combination of arguments is valid
    {{
        int id_count = 0;
        const string& fmt = args["fmt"].AsString();

        if (args["gi"])     id_count++;
        if (args["in"])     id_count++;
        if (args["flat"])   id_count++;
        if (args["fasta"])  id_count++;
        if (args["query"])  id_count++;
        if (args["qf"])     id_count++;

        if (id_count != 1) {
            NCBI_THROW(CArgException,eNoArg,
                "You must supply exactly one argument"
                " indicating what to look up.");
        }
        if ((args["query"]  ||  args["qf"]  ||  fmt == "docsum")
            &&  !args["db"]) {
            ERR_POST("No Entrez database supplied.  Try -db Nucleotide or "
                     "-db Protein.");
            return -1;
        }
        if ((fmt == "genbank"  ||  fmt == "genpept"  ||  fmt == "quality")
            &&  args["lt"].AsString() != "entry") {
            ERR_POST("The output format '" << fmt
                     << "' is only available for Seq-Entries.");
            return -1;
        }
    }}

    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    m_E2Client.SetDefaultRequest().SetTool("id1_fetch");

    // Open output file
    m_OutputFile = &args["out"].AsOutputFile();

    // Set up object manager
    m_ObjMgr = new CObjectManager;
    m_Scope = new CScope(*m_ObjMgr);
    m_ObjMgr->RegisterDataLoader( *new CGBDataLoader("GENBANK"));
    m_Scope->AddDataLoader("GENBANK");

    int repeat = args["repeat"].AsInteger();
    for ( int pass = 0; pass < repeat; ++pass ) {
    if (args["gi"]) {
        if ( !LookUpGI(args["gi"].AsInteger()) )
            return -1;
    }

    if (args["fasta"]) {
        int gi = LookUpFastaSeqID(args["fasta"].AsString());
        if (gi <= 0  ||  !LookUpGI(gi)) {
            return -1;
        }
    }

    if (args["flat"]) {
        int gi = LookUpFlatSeqID(args["flat"].AsString());
        if (gi <= 0  ||  !LookUpGI(gi)) {
            return -1;
        }
    }

    if (args["in"]) {
        CNcbiIstream& is = args["in"].AsInputFile();
        while (is  &&  !is.eof()) {
            string id;
            int    gi;

            is >> id;
            if (id.empty()) {
                break;
            }
            if (id.find('|') != NPOS) {
                gi = LookUpFastaSeqID(id);
            } else if (id.find_first_of(":=(") != NPOS) {
                gi = LookUpFlatSeqID(id);
            } else {
                gi = NStr::StringToInt(id);
            }

            if (gi <= 0  ||  !LookUpGI(gi)) {
                return -1;
            }
        }
    }

    if (args["query"]  ||  args["qf"]) {
        // Form query
        CRef<CEntrez2_boolean_element> e2_element
            (new CEntrez2_boolean_element);

        if (args["query"]) {
            e2_element->SetStr(args["query"].AsString());
        } else {
            CNcbiIstream& is = args["qf"].AsInputFile();
            CNcbiOstrstream oss;
            oss << is.rdbuf();
            string& str = e2_element->SetStr();
            str.assign(oss.str(), oss.pcount());
            oss.freeze(false);
            replace_if(str.begin(), str.end(), s_IsControl, ' ');
        }

        // Make the actual query
        CRef<CEntrez2_boolean_reply> reply;
        {{
            CEntrez2_eval_boolean eb;
            eb.SetReturn_UIDs(true);
            CEntrez2_boolean_exp& query = eb.SetQuery();
            query.SetExp().push_back(e2_element);
            query.SetDb() = CEntrez2_db_id(args["db"].AsString());
            reply = m_E2Client.AskEval_boolean(eb);
        }}
        if ( !reply->GetCount() ) {
            ERR_POST("Entrez query returned no results.");
            return -1;
        }

        // Query succeeded; proceed to next stage of lookup
        for (CEntrez2_id_list::TConstUidIterator it
                 = reply->GetUids().GetConstUidIterator();
             !it.AtEnd();  ++it) {
            if ( !LookUpGI(*it) ) {
                return -1;
            }
        }
    }
    }
    return 0;
}


bool CId1FetchApp::LookUpGI(int gi)
{    
    const CArgs&             args       = GetArgs();
    const string&            fmt        = args["fmt"].AsString();
    const string&            lt         = args["lt"].AsString();
    CConstRef<CSerialObject> reply_object;
    bool                     use_objmgr = false;

    if (lt == "none") {
        *m_OutputFile << gi << NcbiEndl;
        return true;  // Done
    } else if (fmt == "docsum") {
        // Handling this here costs some efficiency when the GI came
        // from an Entrez query in the first place, but wins on generality.
        CEntrez2_id_list uids;
        uids.SetDb() = CEntrez2_db_id(args["db"].AsString());
        uids.SetNum(1);
        uids.SetUids().resize(uids.sm_UidSize);
        {{
            CEntrez2_id_list::TUidIterator it = uids.GetUidIterator();
            *it = gi;
        }}
        CRef<CEntrez2_docsum_list> docs = m_E2Client.AskGet_docsum(uids);
        if ( !docs->GetCount() ) {
            ERR_POST("Entrez query returned no results.");
            return false;
        }

        string caption, title;
        for (CTypeConstIterator<CEntrez2_docsum_data> it = ConstBegin(*docs);
             it;  ++it) {
            // Should this be case-insensitive?
            if (it->GetField_name() == "Caption") {
                caption = it->GetField_value();
            } else if (it->GetField_name() == "Title") {
                title = it->GetField_value();
            }
        }
        *m_OutputFile << '>';
        if ( !caption.empty() ) {
            *m_OutputFile << caption;
        }
        *m_OutputFile << ' ';
        if ( !title.empty() ) {
            *m_OutputFile << title;
        }
    } else if (lt == "entry") {
        use_objmgr = true;
    } else if (lt == "state") {
        CRef<CID1server_back> id1_reply(new CID1server_back);
        int state = m_ID1Client.AskGetgistate(gi, id1_reply);
        if (fmt == "fasta") {
            *m_OutputFile << "gi = " << gi << ", states: ";
            switch (state & 0xff) {
            case  0: *m_OutputFile << "NONEXISTENT"; break; // was "NOT EXIST"
            case 10: *m_OutputFile << "DELETED";     break;
            case 20: *m_OutputFile << "REPLACED";    break;
            case 40: *m_OutputFile << "LIVE";        break;
            default: *m_OutputFile << "UNKNOWN";     break;
            }
            if (state & 0x100) {
                *m_OutputFile << "|SUPPRESSED";
            }
            if (state & 0x200) {
                *m_OutputFile << "|WITHDRAWN";
            }
            if (state & 0x400) {
                *m_OutputFile << "|CONFIDENTIAL";
            }
        } else {
            reply_object = id1_reply;
        }
    } else if (lt == "ids") {
#if 1
        CRef<CID1server_back> id1_reply(new CID1server_back);
        CID1server_back::TIds ids
            = m_ID1Client.AskGetseqidsfromgi(gi, id1_reply);
        if (fmt == "fasta") {
            WriteFastaIDs(ids);
        } else {
            reply_object = id1_reply;
        }
#else
        use_objmgr = true;
#endif
    } else if (lt == "history"  ||  lt == "revisions") {
        CRef<CID1server_back> id1_reply(new CID1server_back);
        // ignore result -- it's simpler to use id1_reply
        if (lt == "history") {
            m_ID1Client.AskGetgihist(gi, id1_reply);
        } else {
            m_ID1Client.AskGetgirev(gi, id1_reply);
        }
        if (fmt == "fasta") {
            WriteHistoryTable(*id1_reply);
        } else {
            reply_object = id1_reply;
        }
    }

    CBioseq_Handle handle;
    if (use_objmgr) {
        // What about db, ent, and maxplex?
        CSeq_id id;
        id.SetGi(gi);
        handle = m_Scope->GetBioseqHandle(id);
        if ( !handle ) {
            ERR_POST(Fatal << "Bioseq not found: " << id.DumpAsFasta());
        }
        reply_object.Reset(&handle.GetTopLevelSeqEntry());
    }

    // Dump server response in the specified format
    ESerialDataFormat format = eSerial_None;
    if        (fmt == "asn") {
        format = eSerial_AsnText;
    } else if (fmt == "asnb") {
        format = eSerial_AsnBinary;
    } else if (fmt == "xml") {
        format = eSerial_Xml;
    } else if (fmt == "fasta"  &&  lt == "ids") {
        if (use_objmgr) {
            WriteFastaIDs(handle.GetBioseq().GetId());
        }
    } else if (fmt == "fasta"  &&  lt == "entry") {
        CFastaOstream out(*m_OutputFile);
        out.SetFlag(CFastaOstream::eAssembleParts);
        out.Write(handle);
    } else if (fmt == "quality") {
        WriteQualityScores(handle);
    } else if (fmt == "genbank"  ||  fmt == "genpept") {
        bool gp = fmt == "genpept";
        const CSeq_entry& entry = handle.GetTopLevelSeqEntry();
#if 1
        CFlatNCBIFormatter formatter(*new CFlatTextOStream(*m_OutputFile),
                                     *m_Scope, IFlatFormatter::eMode_Entrez);
        formatter.Format(entry, formatter,
                         gp ? IFlatFormatter::fSkipNucleotides
                         : IFlatFormatter::fSkipProteins);
#else
        CGenbankWriter(*m_OutputFile, *m_Scope,
                       gp ? CGenbankWriter::eFormat_Genpept
                       : CGenbankWriter::eFormat_Genbank)
            .Write(entry);
#endif
    }

    if (reply_object.NotEmpty()  &&  format != eSerial_None) {
        auto_ptr<CObjectOStream> asn_output
           (CObjectOStream::Open(format, *m_OutputFile));
        // *asn_output << *reply_object;
        asn_output->Write(reply_object, reply_object->GetThisTypeInfo());
    }

    if (fmt != "asnb") {
        *m_OutputFile << NcbiEndl;
    }

    return true;  // Done
}


// Cleanup
void CId1FetchApp::Exit(void)
{
    SOCK_ShutdownAPI();
    SetDiagStream(0);
}


int CId1FetchApp::LookUpFastaSeqID(const string& s)
{
    CSeq_id id(s);
    return m_ID1Client.AskGetgi(s);
}


int CId1FetchApp::LookUpFlatSeqID(const string& s)
{
    CSeq_id::E_Choice type = static_cast<CSeq_id::E_Choice>(atoi(s.c_str()));
    SIZE_TYPE pos = s.find_first_of(":=(");
    if (pos == NPOS) {
        THROW0_TRACE(runtime_error("Malformatted flat ID " + s));
    }
    string data = s.substr(pos + 1);

    switch (s[pos]) {
    case ':':
    case '=':
    {
        CSeq_id id(type, data, kEmptyStr);
        return m_ID1Client.AskGetgi(id);
    }
    case '(':
    {
        data.erase(data.end() - 1);
        // remove last character, which should be ')'
        vector<string> pieces;
        NStr::Tokenize(data, ",", pieces);
        pieces.resize(4, kEmptyStr);
        // name acc rel ver -> acc name ver rel
        CSeq_id id(type, pieces[1], pieces[0], pieces[3], pieces[2]);
        return m_ID1Client.AskGetgi(id);
    }
    default: // can't happen, but shut the compiler up
        return -1;
    }
}


void CId1FetchApp::WriteFastaIDs(const list< CRef< CSeq_id > >& ids)
{
    ITERATE (list< CRef< CSeq_id > >, it, ids) {
        if (it != ids.begin()) {
            *m_OutputFile << '|';
        }
        (*it)->WriteAsFasta(*m_OutputFile);
    }
}


// for formatting text
class CTextColumn
{
public:
    CTextColumn() : m_Width(0) { }
    CTextColumn& Add(string s) {
        m_Strings.push_back(s);
        if (s.size() > m_Width)
            m_Width = s.size();
        return *this;
    }
    string Get(unsigned int index) const {
        const string& s = m_Strings[index];
        return s + string(m_Width - s.size(), ' ');
    }
    SIZE_TYPE Width()  const { return m_Width; }
    size_t    Height() const { return m_Strings.size(); }
 
private:
    SIZE_TYPE      m_Width;
    vector<string> m_Strings;
};


void CId1FetchApp::WriteHistoryTable(const CID1server_back& id1_reply)
{
    CTextColumn gis, dates, dbs, numbers;
    gis.Add("GI").Add("--");
    dates.Add("Loaded").Add("------");
    dbs.Add("DB").Add("--");
    numbers.Add("Retrieval No.").Add("-------------");
    for (CTypeConstIterator<CSeq_hist_rec> it = ConstBegin(id1_reply);
         it;  ++it) {
        int    gi = 0;
        string dbname, number;

        if ( it->GetDate().IsStr() ) {
            dates.Add(it->GetDate().GetStr());
        } else {
            CNcbiOstrstream oss;
            const CDate_std& date = it->GetDate().GetStd();
            oss << setfill('0') << setw(2) << date.GetMonth() << '/'
                << setw(2) << date.GetDay() << '/' << date.GetYear();
            dates.Add(CNcbiOstrstreamToString(oss));
        }

        ITERATE (CSeq_hist_rec::TIds, it2, it->GetIds()) {
            if ( (*it2)->IsGi() ) {
                gi = (*it2)->GetGi();
            } else if ( (*it2)->IsGeneral() ) {
                dbname = (*it2)->GetGeneral().GetDb();
                const CObject_id& tag = (*it2)->GetGeneral().GetTag();
                if ( tag.IsStr() ) {
                    number = tag.GetStr();
                } else {
                    number = NStr::IntToString(tag.GetId());
                }
            }
        }

        gis.Add(NStr::IntToString(gi));
        dbs.Add(dbname);
        numbers.Add(number);
    }

    for (unsigned int n = 0;  n < gis.Height();  n++) {
        *m_OutputFile << gis.Get(n) << "  " << dates.Get(n) << "  "
                      << dbs.Get(n) << "  " << numbers.Get(n) << NcbiEndl;
    }
}



void CId1FetchApp::WriteQualityScores(CBioseq_Handle& handle)
{   
    /* Test case:
     * /net/ncbi/ncbi/ftp/genbank/quality_scores/gbvrt.qscore.gz
     *  GI 13508865: gotseqentry.set.annot.data.graph
     *  >AL590146.2 Phrap Quality (Length:121086, Min: 31, Max: 99)
     *   99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99
     *   ...
     */
    string id = FindBestChoice(handle.GetBioseqCore()->GetId(), CSeq_id::Score)
        ->GetSeqIdString(true);
    
    for (CGraph_CI it(handle, 0, 0);  it;  ++it) {
        string title = it->GetTitle();
        if (title.find("uality") == NPOS) {
            continue;
        }

        const CByte_graph& data = it->GetGraph().GetByte();
        *m_OutputFile << '>' << id << ' ' << title
                      << " (Length: " << it->GetNumval()
                      << ", Min: " << data.GetMin()
                      << ", Max: " << data.GetMax() << ')' << NcbiEndl;
        for (SIZE_TYPE n = 0;  n < data.GetValues().size();  ++n) {
            *m_OutputFile << setw(3) << static_cast<int>(data.GetValues()[n]);
            if (n % 20 == 19) {
                *m_OutputFile << NcbiEndl;
            }
        }
    }
}

END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
    return CId1FetchApp().AppMain(argc, argv);
}

/*
* ===========================================================================
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.48  2004/04/26 16:53:17  ucko
* Don't try to pass temporary CSeq_id objects, even by const reference,
* as CSeq_id has no public copy constructor.
*
* Revision 1.47  2004/01/05 17:59:32  vasilche
* Moved genbank loader and its readers sources to new location in objtools.
* Genbank is now in library libncbi_xloader_genbank.
* Id1 reader is now in library libncbi_xreader_id1.
* OBJMGR_LIBS macro updated correspondingly.
*
* Old headers temporarily will contain redirection to new location
* for compatibility:
* objmgr/gbloader.hpp > objtools/data_loaders/genbank/gbloader.hpp
* objmgr/reader_id1.hpp > objtools/data_loaders/genbank/readers/id1/reader_id1.hpp
*
* Revision 1.46  2003/10/21 13:48:48  grichenk
* Redesigned type aliases in serialization library.
* Fixed the code (removed CRef-s, added explicit
* initializers etc.)
*
* Revision 1.45  2003/06/02 16:06:16  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.44  2003/05/13 17:14:17  ucko
* Direct flat output to *m_OutputFile rather than hard-coding cout.
*
* Revision 1.43  2003/04/24 16:17:10  vasilche
* Added '-repeat' option.
* Updated includes.
*
* Revision 1.42  2003/04/09 16:00:15  ucko
* Give all RPC clients unique basenames.
*
* Revision 1.41  2003/04/03 17:45:25  ucko
* Switch to new (differently buggy ;-)) flat-file generator.
* Drop s_SplitString, as NStr::Tokenize does the same thing.
*
* Revision 1.40  2003/03/10 18:48:48  kuznets
* iterate->ITERATE
*
* Revision 1.39  2002/11/13 20:14:51  ucko
* Rework to take advantage of new ID1 and Entrez2 client classes
* generated by datatool
*
* Revision 1.38  2002/08/27 21:41:53  ucko
* Use CFastaOstream rather than custom code.
* Fix spelling of NONEXISTENT.
*
* Revision 1.37  2002/08/14 20:28:02  ucko
* Fix behavior when given a list of IDs.
*
* Revision 1.36  2002/07/11 14:23:48  gouriano
* exceptions replaced by CNcbiException-type ones
*
* Revision 1.35  2002/06/28 17:25:53  grichenk
* +Error message if a GI was not found
*
* Revision 1.34  2002/06/12 16:51:55  lavr
* Take advantage of CONNECT_Init()
*
* Revision 1.33  2002/06/07 16:11:40  ucko
* GetTitle() is now in sequence::.
*
* Revision 1.32  2002/06/06 23:46:24  vakatov
* Use GetTitle() from <objects/util/sequence.hpp>
*
* Revision 1.31  2002/05/06 16:13:46  ucko
* Merge in Andrei Gourianov's changes to use the new OM (thanks!)
* Remove some dead code.
* Don't automatically turn on tracing, even when building with _DEBUG;
* it is always possible to set DIAG_TRACE in the environment instead.
* Move CVS log to end.
*
*
* *** These four entries are from src/app/id1_fetch1/id1_fetch1.cpp ***
* Revision 1.5  2002/05/06 03:31:52  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/05/03 21:28:21  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.3  2002/04/10 21:02:21  gouriano
* moved construction of iterators out of "for" loop initialization:
* Sun Workshop compiler does not call destructors of such objects
* in case we use break to exit the loop
*
* Revision 1.2  2002/04/10 16:07:30  gouriano
* corrected data output in different formats
*
* Revision 1.1  2002/04/04 16:31:36  gouriano
* id1_fetch1 - modified version of id1_fetch, which uses objmgr1
*
* Revision 1.29  2002/03/11 21:52:05  lavr
* Print complete debug and trace information when compiled with _DEBUG
*
* Revision 1.28  2002/01/16 22:14:00  ucko
* Removed CRef<> argument from choice variant setter, updated sources to
* use references instead of CRef<>s
*
* Revision 1.27  2001/12/07 21:15:16  ucko
* Remove duplicate includes.
*
* Revision 1.26  2001/12/07 21:11:01  grichenk
* Fixed includes to work with the updated datatool
*
* Revision 1.25  2001/12/07 21:03:47  ucko
* Add #includes required by new datatool version.
*
* Revision 1.24  2001/11/16 16:06:45  ucko
* Handle new Entrez docsum interface properly.
*
* Revision 1.23  2001/10/26 14:49:16  ucko
* Restructured to avoid CRefs as arguments.
*
* Revision 1.22  2001/10/23 20:05:12  ucko
* Request ASCII from CSeq_vector.
*
* Revision 1.21  2001/10/17 21:17:53  ucko
* Seq_vector now properly starts from zero rather than one; adjust code
* that uses it accordingly.
*
* Revision 1.20  2001/10/12 19:32:58  ucko
* move BREAK to a central location; move CBioseq::GetTitle to object manager
*
* Revision 1.19  2001/10/12 15:34:01  ucko
* Edit in-source version of CVS log to avoid end-of-comment marker.  (Oops.)
*
* Revision 1.18  2001/10/12 15:29:08  ucko
* Drop {src,include}/objects/util/asciiseqdata.* in favor of CSeq_vector.
* Rewrite GenBank output code to take fuller advantage of the object manager.
*
* Revision 1.17  2001/10/10 16:02:53  ucko
* Clean up includes.
*
* Revision 1.16  2001/10/04 19:11:56  ucko
* Centralize (rudimentary) code to get a sequence's title.
*
* Revision 1.15  2001/09/25 20:31:13  ucko
* Work around bug in Workshop's handling of declarations in for-loop
* initializers.
*
* Revision 1.14  2001/09/25 20:12:02  ucko
* More cleanups from Denis.
* Put utility code in the objects namespace.
* Moved utility code to {src,include}/objects/util (to become libxobjutil).
* Moved static members of CGenbankWriter to above their first use.
*
* Revision 1.13  2001/09/25 13:26:36  lavr
* CConn_ServiceStream() - arguments adjusted
*
* Revision 1.12  2001/09/24 03:22:09  vakatov
* Un-#include <cstdlib> and <cctype> which break IRIX/MIPSpro compilation and
* apparently are not needed after all
*
* Revision 1.11  2001/09/21 22:38:59  ucko
* Cope with new Entrez interface; fix MSVC build.
*
* Revision 1.10  2001/09/05 16:25:56  ucko
* Adapted to latest revision of object manager interface.
*
* Revision 1.9  2001/09/05 14:44:37  ucko
* Use NStr::IntToString instead of Stringify.
*
* Revision 1.8  2001/09/04 16:20:53  ucko
* Dramatically fleshed out id1_fetch
*
* Revision 1.7  2001/07/19 19:40:20  lavr
* Typo fixed
*
* Revision 1.6  2001/06/01 18:43:44  vakatov
* Comment out excessive debug/trace printout
*
* Revision 1.5  2001/05/16 17:55:37  grichenk
* Redesigned support for non-blocking stream read operations
*
* Revision 1.4  2001/05/11 20:41:16  grichenk
* Added support for non-blocking stream reading
*
* Revision 1.3  2001/05/11 14:06:45  grichenk
* The first working revision
*
* Revision 1.2  2001/04/13 14:09:34  grichenk
* Next debug version, still not working
*
* Revision 1.1  2001/04/10 22:39:04  vakatov
* Initial revision.
* Compiles and links, but apparently is not working yet.
*
* ===========================================================================
*/
