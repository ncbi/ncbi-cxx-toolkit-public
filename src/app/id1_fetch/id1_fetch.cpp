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

#include <ncbi_pch.hpp>
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
#include <objtools/format/flat_file_generator.hpp>

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
#include <objmgr/util/sequence.hpp>

#include <memory>
#include <algorithm>
#include <list>


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
    bool LookUpGI(TGi gi);
    TGi  LookUpFastaSeqID(const string& s);
    TGi  LookUpRawSeqID(const string& s);
    TGi  LookUpFlatSeqID(const string& s);

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
        ("gi", new CArgAllow_Integers(0, kMax_Int));

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
        ("ent", new CArgAllow_Integers(0, kMax_Int));

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
    arg_desc->AddOptionalKey
        ("maxplex", "MaxComplexity", // was "-c" in `idfetch'
         "Maximum complexity to return",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("maxplex", &(*new CArgAllow_Strings,
                      "entry", "bioseq", "bioseq-set", "nuc-prot",
                      "pub-set"));
    
    // External features
    arg_desc->AddOptionalKey
        ("extfeat", "ExtFeat", // was "-F" in `idfetch'
         "Add features, delimited by ',': "
         "SNP, SNP_graph, CDD, MGC, HPRD, STS, tRNA, Exon",
         CArgDescriptions::eString);
    
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

    // timeout
    arg_desc->AddOptionalKey
        ("timeout", "Timeout",
         "Network connection timeout in seconds",
         CArgDescriptions::eInteger);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


// Workaround for "replace_if"
inline bool s_IsControl(char c)
{
    return iscntrl((unsigned char) c) ? true : false;
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

    if ( args["timeout"] ) {
        int timeout = args["timeout"].AsInteger();
        STimeout tmo;
        tmo.sec = timeout;
        tmo.usec = 0;
        m_ID1Client.SetTimeout(&tmo);
    }

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
    m_ObjMgr = CObjectManager::GetInstance();
    m_Scope = new CScope(*m_ObjMgr);
    CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
    m_Scope->AddDefaults();

    int repeat = args["repeat"].AsInteger();
    for ( int pass = 0; pass < repeat; ++pass ) {
    if (args["gi"]) {
        if ( !LookUpGI(GI_FROM(int, args["gi"].AsInteger())) )
            return -1;
    }

    if (args["fasta"]) {
        TGi gi = LookUpFastaSeqID(args["fasta"].AsString());
        if (gi <= ZERO_GI  ||  !LookUpGI(gi)) {
            return -1;
        }
    }

    if (args["flat"]) {
        TGi gi = LookUpFlatSeqID(args["flat"].AsString());
        if (gi <= ZERO_GI  ||  !LookUpGI(gi)) {
            return -1;
        }
    }

    if (args["in"]) {
        CNcbiIstream& is = args["in"].AsInputFile();
        while (is  &&  !is.eof()) {
            string id;
            TGi    gi;

            is >> id;
            if (id.empty()) {
                break;
            }
            if (id.find('|') != NPOS) {
                gi = LookUpFastaSeqID(id);
            } else if (id.find_first_of(":=(") != NPOS) {
                gi = LookUpFlatSeqID(id);
            } else {
                gi = LookUpRawSeqID(id);
            }

            if (gi <= ZERO_GI  ||  !LookUpGI(gi)) {
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
            string str;
            NcbiStreamToString(&str, args["qf"].AsInputFile());
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
            if ( !LookUpGI(GI_FROM(CEntrez2_id_list::TConstUidIterator::value_type, *it)) ) {
                return -1;
            }
        }
    }
    }
    return 0;
}


bool CId1FetchApp::LookUpGI(TGi gi)
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
            *it = GI_TO(CEntrez2_id_list::TConstUidIterator::value_type, gi);
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
        if ( args["maxplex"] || args["extfeat"] ) {
            CRef<CID1server_back> id1_reply(new CID1server_back);
            CRef<CID1server_maxcomplex> maxcomplex(new CID1server_maxcomplex);
            int mp = eEntry_complexities_entry;
            if ( args["maxplex"] ) {
                string maxplex = args["maxplex"].AsString();
                if ( maxplex == "bioseq" ) {
                    mp = eEntry_complexities_bioseq;
                }
                else if ( maxplex == "bioseq-set" ) {
                    mp = eEntry_complexities_bioseq_set;
                }
                else if ( maxplex == "nuc-prot" ) {
                    mp = eEntry_complexities_nuc_prot;
                }
                else if ( maxplex == "pub-set" ) {
                    mp = eEntry_complexities_pub_set;
                }
            }
            if ( args["extfeat"] ) {
                int ff = 0;
                list<string> extfeat;
                NStr::Split(args["extfeat"].AsString(), ",", extfeat);
                ITERATE ( list<string>, it, extfeat ) {
                    if ( *it == "SNP" ) {
                        ff |= 1 << 0;
                    }
                    else if ( *it == "SNP_graph" ) {
                        ff |= 1 << 2;
                    }
                    else if ( *it == "CDD" ) {
                        ff |= 1 << 3;
                    }
                    else if ( *it == "MGC" ) {
                        ff |= 1 << 4;
                    }
                    else if ( *it == "HPRD" ) {
                        ff |= 1 << 5;
                    }
                    else if ( *it == "STS" ) {
                        ff |= 1 << 6;
                    }
                    else if ( *it == "tRNA" ) {
                        ff |= 1 << 7;
                    }
                    else if ( *it == "Exon" ) {
                        ff |= 1 << 9;
                    }
                    else {
                        ERR_POST("Unknown extfeat type: "<<*it);
                    }
                }
                mp |= ~ff << 4;
            }
            maxcomplex->SetMaxplex(mp);
            maxcomplex->SetGi(gi);
            reply_object = m_ID1Client.AskGetsefromgi(*maxcomplex, id1_reply);
        }
        else {
            use_objmgr = true;
        }
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
        m_Scope->ResetDataAndHistory();
        // What about db, ent, and maxplex?
        CSeq_id id;
        id.SetGi(gi);
        handle = m_Scope->GetBioseqHandle(id);
        if ( !handle ) {
            ERR_POST(Fatal << "Bioseq not found: " << id.DumpAsFasta());
        }
        reply_object = handle.GetTopLevelEntry().GetCompleteSeq_entry();
    }
    else if ( dynamic_cast<const CSeq_entry*>(reply_object.GetPointer()) &&
              ((fmt == "fasta"  &&  (lt == "ids" || lt == "entry")) ||
               fmt == "quality" ||
               fmt == "genbank"  ||
               fmt == "genpept") ) {
        // these formatting modes require CBioseq_Handle
        m_Scope->ResetDataAndHistory();
        const CSeq_entry& se = dynamic_cast<const CSeq_entry&>(*reply_object);
        CSeq_entry_Handle tse = m_Scope->AddTopLevelSeqEntry(se);
        CSeq_id id;
        id.SetGi(gi);
        handle = m_Scope->GetBioseqHandleFromTSE(id, tse);
        if ( !handle ) {
            ERR_POST(Fatal << "Bioseq not found: " << id.DumpAsFasta());
        }
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
            WriteFastaIDs(handle.GetBioseqCore()->GetId());
        }
    } else if (fmt == "fasta"  &&  lt == "entry") {
        CFastaOstream out(*m_OutputFile);
        out.SetFlag(CFastaOstream::fAssembleParts);
        out.SetFlag(CFastaOstream::fInstantiateGaps);
        out.Write(handle);
    } else if (fmt == "quality") {
        WriteQualityScores(handle);
    } else if (fmt == "genbank"  ||  fmt == "genpept") {
        bool gp = fmt == "genpept";
        CSeq_entry_Handle entry = handle.GetTopLevelEntry();

        CFlatFileConfig ff_config;
        ff_config.SetMode(CFlatFileConfig::eMode_Entrez);
        ff_config.SetFormatGenbank();
        ff_config
            .SetHideSNPFeatures()
            .SetShowContigFeatures()
            .SetShowContigSources();
        if (gp) {
            ff_config.SetViewProt();
        }
        CFlatFileGenerator ffg(ff_config);
        ffg.SetAnnotSelector().ExcludeNamedAnnots("SNP");

        ffg.Generate(entry, *m_OutputFile);
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


TGi CId1FetchApp::LookUpFastaSeqID(const string& s)
{
    CSeq_id id(s);
    return m_ID1Client.AskGetgi(id);
}


TGi CId1FetchApp::LookUpRawSeqID(const string& s)
{
    try {
        CSeq_id id(s, CSeq_id::fParse_AnyRaw | CSeq_id::fParse_NoFASTA);
        if (id.IsGi()) {
            return id.GetGi();
        } else {
            return m_ID1Client.AskGetgi(id);
        }
    } catch (CSeqIdException&) {
        return GI_FROM(TIntGI, -1);
    }
}


TGi CId1FetchApp::LookUpFlatSeqID(const string& s)
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
        CSeq_id id(type, pieces[1], pieces[0],
                   pieces[3].empty() ? 0 : NStr::StringToInt(pieces[3]),
                   pieces[2]);
        return m_ID1Client.AskGetgi(id);
    }
    default: // can't happen, but shut the compiler up
        return GI_FROM(TIntGi, -1);
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
        TGi    gi = ZERO_GI;
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

        gis.Add(NStr::NumericToString(gi));
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

    for (CGraph_CI it(handle);  it;  ++it) {
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
