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
* Author:  Aaron Ucko, Mati Shomrat, Jonathan Kans, NCBI
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

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>

#include <objects/seqset/gb_release_file.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Byte_graph.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/seq_macros.hpp>

#include <objects/submit/Seq_submit.hpp>

#include <sra/data_loaders/wgs/wgsloader.hpp>

#include <objtools/cleanup/cleanup.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>
#include <objmgr/util/sequence.hpp>
#include <dbapi/driver/drivers.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ==========================================================================
class CAsn2FastaApp:
    public CNcbiApplication, public CGBReleaseFile::ISeqEntryHandler
//  ==========================================================================
{
public:
    void Init(void);
    int  Run (void);

    bool HandleSeqEntry(CRef<CSeq_entry>& se);
    bool HandleSeqEntry(CSeq_entry_Handle& se);
    bool HandleSeqID( const string& seqID );

    CSeq_entry_Handle ObtainSeqEntryFromSeqEntry(CObjectIStream& is);
    CSeq_entry_Handle ObtainSeqEntryFromBioseq(CObjectIStream& is);
    CSeq_entry_Handle ObtainSeqEntryFromBioseqSet(CObjectIStream& is);
    CSeq_entry_Handle ObtainSeqEntryFromSeqSubmit(CObjectIStream& is);

    CFastaOstream* OpenFastaOstream(const string& argname, const string& strname, bool use_stdout);

    void PrintQualityScores(const CBioseq& bsp, CNcbiOstream* out_stream);

private:
    CObjectIStream* x_OpenIStream(const CArgs& args);

    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CRef<CScope>                m_Scope;

    auto_ptr<CFastaOstream>     m_Os;           // all sequence output stream

    auto_ptr<CFastaOstream>     m_On;           // nucleotide output stream
    auto_ptr<CFastaOstream>     m_Og;           // genomic output stream
    auto_ptr<CFastaOstream>     m_Or;           // RNA output stream
    auto_ptr<CFastaOstream>     m_Op;           // protein output stream
    auto_ptr<CFastaOstream>     m_Ou;           // unknown output stream
    CNcbiOstream*               m_Oq;           // quality score output stream

    string                      m_OgHead;
    string                      m_OgTail;
    int                         m_OgIndex;
    TSeqPos                     m_OgMax;
    TSeqPos                     m_OgCurrLen;

    bool                        m_OnlyNucs;
    bool                        m_OnlyProts;
    bool                        m_DeflineOnly;
    bool                        m_do_cleanup;
};

//  --------------------------------------------------------------------------
void CAsn2FastaApp::Init(void)
//  --------------------------------------------------------------------------
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Convert an ASN.1 Seq-entry into a FASTA report",
        false);

    // input
    {{
        // name
        arg_desc->AddOptionalKey("i", "InputFile",
            "Input file name", CArgDescriptions::eInputFile);

        // input file serial format (AsnText\AsnBinary\XML, default: AsnText)
        arg_desc->AddOptionalKey("serial", "SerialFormat", "Input file format",
            CArgDescriptions::eString);
        arg_desc->SetConstraint("serial", &(*new CArgAllow_Strings,
            "text", "binary", "XML"));
        // id
        arg_desc->AddOptionalKey("id", "ID",
            "Specific ID to display", CArgDescriptions::eString);

        // input type:
        arg_desc->AddDefaultKey( "type", "AsnType", "ASN.1 object type",
            CArgDescriptions::eString, "any" );
        arg_desc->SetConstraint( "type",
            &( *new CArgAllow_Strings, "any", "seq-entry", "bioseq", "bioseq-set", "seq-submit" ) );
    }}


    // batch processing
    {{
        arg_desc->AddFlag("batch", "Process NCBI release file");
        // compression
        arg_desc->AddFlag("c", "Compressed file");
        // propogate top descriptors
        arg_desc->AddFlag("p", "Propogate top descriptors");

        arg_desc->AddFlag("defline-only",
                          "Only output the defline");
    }}

    // output
    {{
        arg_desc->AddFlag("show-mods", "Show FASTA header mods (e.g. [strain=abc])");

        arg_desc->AddOptionalKey("gap-mode", "GapMode", "Gap mode",
            CArgDescriptions::eString);
        arg_desc->SetConstraint("gap-mode", &(*new CArgAllow_Strings,
            "one-dash", "dashes", "letters", "count"));

        arg_desc->AddOptionalKey("width", "CHARS", "Output FASTA with an alternate number of columns", CArgDescriptions::eInteger);
        arg_desc->SetConstraint("width", new CArgAllow_Integers(1, kMax_Int));

        // single output name
        arg_desc->AddOptionalKey("o", "SingleOutputFile",
            "Single output file name", CArgDescriptions::eOutputFile);

        // filtering options for use with old style single output file:
        arg_desc->AddFlag("nucs-only", "Only emit nucleotide sequences");
        arg_desc->SetDependency("nucs-only", CArgDescriptions::eRequires, "o");

        arg_desc->AddFlag("prots-only", "Only emit protein sequences");
        arg_desc->SetDependency("prots-only", CArgDescriptions::eRequires, "o");
        arg_desc->SetDependency("prots-only", CArgDescriptions::eExcludes,
                                "nucs-only");

        // file names
        arg_desc->AddOptionalKey("on", "NucleotideOutputFile",
            "Nucleotide output file name", CArgDescriptions::eOutputFile);
        arg_desc->SetDependency("on", CArgDescriptions::eExcludes, "o");

        arg_desc->AddOptionalKey("og", "GenomicOutputFile",
            "Genomic output file name", CArgDescriptions::eOutputFile);
        arg_desc->SetDependency("og", CArgDescriptions::eExcludes, "o");
        arg_desc->SetDependency("og", CArgDescriptions::eExcludes, "on");

        arg_desc->AddOptionalKey("og_head", "GenomicOutputFileHead",
            "Genomic output file name stem",
            CArgDescriptions::eString);
        arg_desc->SetDependency("og_head", CArgDescriptions::eExcludes, "o");
        arg_desc->SetDependency("og_head", CArgDescriptions::eExcludes, "on");
        arg_desc->SetDependency("og_head", CArgDescriptions::eExcludes, "og");

        arg_desc->AddOptionalKey("og_tail", "GenomicOutputFileTail",
            "Genomic output file name suffix",
            CArgDescriptions::eString);
        arg_desc->SetDependency("og_tail", CArgDescriptions::eExcludes, "o");
        arg_desc->SetDependency("og_tail", CArgDescriptions::eExcludes, "on");
        arg_desc->SetDependency("og_tail", CArgDescriptions::eExcludes, "og");

        arg_desc->AddDefaultKey("x", "GenomeFileMaxSize",
                                "Maximum size of each genomic fasta file in Mb",
                                CArgDescriptions::eInteger, "1000");

        arg_desc->AddOptionalKey("or", "RNAOutputFile",
            "RNA output file name", CArgDescriptions::eOutputFile);
        arg_desc->SetDependency("or", CArgDescriptions::eExcludes, "o");
        arg_desc->SetDependency("or", CArgDescriptions::eExcludes, "on");

        arg_desc->AddOptionalKey("op", "ProteinOutputFile",
            "Protein output file name", CArgDescriptions::eOutputFile);
        arg_desc->SetDependency("op", CArgDescriptions::eExcludes, "o");

        arg_desc->AddOptionalKey("ou", "UnknownOutputFile",
            "Unknown output file name", CArgDescriptions::eOutputFile);
        arg_desc->SetDependency("ou", CArgDescriptions::eExcludes, "o");

        arg_desc->AddOptionalKey("oq", "QualityScoreOutputFile",
            "Quality score output file name", CArgDescriptions::eOutputFile);
    }}

    // misc
    {{
         // cleanup
         arg_desc->AddFlag("cleanup",
                           "Do internal data cleanup prior to formatting");
     }}

    SetupArgDescriptions(arg_desc.release());
}

//  --------------------------------------------------------------------------
CFastaOstream* CAsn2FastaApp::OpenFastaOstream(const string& argname, const string& strname, bool use_stdout)
//  --------------------------------------------------------------------------
{
    const CArgs& args = GetArgs();

    if (! use_stdout) {
        if (strname.empty()) {
            if (argname.empty()) return NULL;
            if (! args[argname]) return NULL;
        }
    }

    CNcbiOstream* os;
    if (use_stdout) {
        os = &NcbiCout;
    } else if ( !strname.empty() ) {
        // NB: This stream will leak unless explicitly managed.
        // (An app-wide smart pointer should suffice.)
        os = new CNcbiOfstream(strname.c_str());
    } else {
        if (args[argname]) {
            os = &args[argname].AsOutputFile();
        } else {
            return NULL;
        }
    }
    auto_ptr<CFastaOstream> fasta_os(new CFastaOstream(*os));

    fasta_os->SetAllFlags(
        CFastaOstream::fInstantiateGaps |
        CFastaOstream::fAssembleParts |
        CFastaOstream::fNoDupCheck |
        CFastaOstream::fKeepGTSigns |
        CFastaOstream::fNoExpensiveOps);
    if( GetArgs()["gap-mode"] ) {
        fasta_os->SetFlag(
            CFastaOstream::fInstantiateGaps);
        string gapmode = GetArgs()["gap-mode"].AsString();
        if ( gapmode == "one-dash" ) {
            fasta_os->SetGapMode(CFastaOstream::eGM_one_dash);
        } else if ( gapmode == "dashes" ) {
            fasta_os->SetGapMode(CFastaOstream::eGM_dashes);
        } else if ( gapmode == "letters" ) {
            fasta_os->SetGapMode(CFastaOstream::eGM_letters);
        } else if ( gapmode == "count" ) {
            fasta_os->SetGapMode(CFastaOstream::eGM_count);
        }
    }

    if( args["show-mods"] ) {
        fasta_os->SetFlag(CFastaOstream::fShowModifiers);
    }
    if( args["width"] ) {
        fasta_os->SetWidth( GetArgs()["width"].AsInteger() );
    }

    m_do_cleanup = ( args["cleanup"]);

    return fasta_os.release();
}

//  --------------------------------------------------------------------------
int CAsn2FastaApp::Run(void)
//  --------------------------------------------------------------------------
{
    // initialize conn library
    CONNECT_Init(&GetConfig());
#ifdef HAVE_PUBSEQ_OS
    // we may require PubSeqOS readers at some point, so go ahead and make
    // sure they are properly registered
    GenBankReaders_Register_Pubseq();
    GenBankReaders_Register_Pubseq2();
    DBAPI_RegisterDriver_FTDS();
#endif

    const CArgs&   args = GetArgs();

    // create object manager
    m_Objmgr = CObjectManager::GetInstance();
    if ( !m_Objmgr ) {
        NCBI_THROW(CException, eUnknown,
                   "Could not create object manager");
    }
    CGBDataLoader::RegisterInObjectManager(*m_Objmgr);
    CWGSDataLoader::RegisterInObjectManager(*m_Objmgr,
                                            CObjectManager::eDefault,
                                            88);
    m_Scope.Reset(new CScope(*m_Objmgr));
    m_Scope->AddDefaults();

    // open the output streams
    m_Os.reset( OpenFastaOstream ("o", "", false) );
    m_On.reset( OpenFastaOstream ("on", "", false) );
    m_Og.reset( OpenFastaOstream ("og", "", false) );
    m_Or.reset( OpenFastaOstream ("or", "", false) );
    m_Op.reset( OpenFastaOstream ("op", "", false) );
    m_Ou.reset( OpenFastaOstream ("ou", "", false) );
    m_Oq = args["oq"] ? &(args["oq"].AsOutputFile()) : NULL;

    m_OnlyNucs = args["nucs-only"];
    m_OnlyProts = args["prots-only"];

    m_OgHead = "";
    m_OgTail = "";
    m_OgIndex = 0;
    m_OgMax = 0;
    m_OgCurrLen = 0;
    if (args["og_head"] && args["og_tail"] && args["x"]) {
        m_OgHead = args["og_head"].AsString();
        m_OgTail = args["og_tail"].AsString();
        m_OgMax = TSeqPos (args["x"].AsInteger() * 1000000);
    }

    if (! m_OgHead.empty() && ! m_OgTail.empty()) {
        m_OgIndex++;
        string ogx = m_OgHead + NStr::IntToString(m_OgIndex) + m_OgTail;
        m_Og.reset( OpenFastaOstream ("", ogx, false) );
    }

    if (m_Os.get() == NULL  &&  m_On.get() == NULL  &&  m_Og.get() == NULL  &&
        m_Or.get() == NULL  &&  m_Op.get() == NULL  &&  m_Ou.get() == NULL  &&
        m_Oq == NULL) {
        // No output (-o*) argument given - default to stdout
        m_Os.reset( OpenFastaOstream ("", "", true) );
    }

    auto_ptr<CObjectIStream> is;
    is.reset( x_OpenIStream( args ) );
    if (is.get() == NULL) {
        string msg = args["i"]? "Unable to open input file" + args["i"].AsString() :
                        "Unable to read data from stdin";
        NCBI_THROW(CException, eUnknown, msg);
    }

    m_DeflineOnly = args["defline-only"];

    if ( args["batch"] ) {
        CGBReleaseFile in(*is.release());
        in.RegisterHandler(this);
        in.Read();  // HandleSeqEntry will be called from this function
    }
    else {

        if ( args["id"] ) {

            //
            //  Implies gbload; otherwise this feature would be pretty
            //  useless...
            //
            m_Scope->AddDefaults();
            string seqID = args["id"].AsString();
            HandleSeqID( seqID );

        } else {
            string asn_type = args["type"].AsString();
            CSeq_entry_Handle seh;

            if ( asn_type == "seq-entry" ) {
                //
                //  Straight through processing: Read a seq_entry, then process
                //  a seq_entry:
                //
                seh = ObtainSeqEntryFromSeqEntry(*is);
                while (seh) {
                    try {
                        HandleSeqEntry(seh);
                    }
                    catch (...) {
                        cerr << "Resolution error: Sequence dropped." << '\n';
                    }
                    m_Scope->RemoveTopLevelSeqEntry(seh);
                    m_Scope->ResetHistory();
                    seh = ObtainSeqEntryFromSeqEntry(*is);
                }
                return 0;
            }
            else if ( asn_type == "bioseq" ) {
                //
                //  Read object as a bioseq, wrap it into a seq_entry, then
                //  process the wrapped bioseq as a seq_entry:
                //
                seh = ObtainSeqEntryFromBioseq(*is);
                if ( !seh ) {
                    NCBI_THROW(CException, eUnknown,
                               "Unable to construct Seq-entry object" );
                }
                HandleSeqEntry(seh);
            }
            else if ( asn_type == "bioseq-set" ) {
                //
                //  Read object as a bioseq_set, wrap it into a seq_entry, then
                //  process the wrapped bioseq_set as a seq_entry:
                //
                seh = ObtainSeqEntryFromBioseqSet(*is);
                if ( !seh ) {
                    NCBI_THROW(CException, eUnknown,
                               "Unable to construct Seq-entry object" );
                }
                HandleSeqEntry(seh);
            }
            else if ( asn_type == "seq-submit" ) {
                //
                //  Read object as a seq_submit, then process the first seq_entry:
                //
                seh = ObtainSeqEntryFromSeqSubmit(*is);
                if ( !seh ) {
                    NCBI_THROW(CException, eUnknown,
                               "Unable to construct Seq-entry object" );
                }
                HandleSeqEntry(seh);
            }
            else if ( asn_type == "any" ) {
                //
                //  Try the first three in turn:
                //

                set<TTypeInfo> known_types, matching_types;
                known_types.insert(CSeq_entry::GetTypeInfo());
                known_types.insert(CSeq_submit::GetTypeInfo());
                known_types.insert(CBioseq_set::GetTypeInfo());
                known_types.insert(CBioseq::GetTypeInfo());

                while (!is->EndOfData()) {
                    matching_types = is->GuessDataType(known_types);
                    if (matching_types.empty()) {
                        NCBI_THROW(CException, eUnknown,
                            "Unidentifiable object" );
                    } else if (matching_types.size() > 1) {
                        NCBI_THROW(CException, eUnknown,
                            "Ambiguous object" );
                    } else if (*matching_types.begin() == CSeq_entry::GetTypeInfo()) {
                        seh = ObtainSeqEntryFromSeqEntry(*is);
                    } else if (*matching_types.begin() == CSeq_submit::GetTypeInfo()) {
                        seh = ObtainSeqEntryFromSeqSubmit(*is);
                    } else if (*matching_types.begin() == CBioseq_set::GetTypeInfo()) {
                        seh = ObtainSeqEntryFromBioseqSet(*is);
                    } else if (*matching_types.begin() == CBioseq::GetTypeInfo()) {
                        seh = ObtainSeqEntryFromBioseq(*is);
                    }
                    if (!seh) {
                        NCBI_THROW(CException, eUnknown,
                            "Unable to construct Seq-entry object" );
                    }
                    HandleSeqEntry(seh);
                    m_Scope->RemoveTopLevelSeqEntry(seh);
                    m_Scope->ResetHistory();
                }
            }
        }
    }

    return 0;
}

//  --------------------------------------------------------------------------
CSeq_entry_Handle CAsn2FastaApp::ObtainSeqEntryFromSeqEntry(
    CObjectIStream& is)
//  --------------------------------------------------------------------------
{
    try {
        CRef<CSeq_entry> se(new CSeq_entry);
        is >> *se;
        if (se->Which() == CSeq_entry::e_not_set) {
            NCBI_THROW(CException, eUnknown,
                       "provided Seq-entry is empty");
        }
        return m_Scope->AddTopLevelSeqEntry(*se);
    }
    catch (CEofException&) { // ignore
    }
    catch (CException& e) {
        ERR_POST(Error << e);
    }
    return CSeq_entry_Handle();
}

//  --------------------------------------------------------------------------
CSeq_entry_Handle CAsn2FastaApp::ObtainSeqEntryFromBioseq(
    CObjectIStream& is)
//  --------------------------------------------------------------------------
{
    try {
        CRef<CBioseq> bs(new CBioseq);
        is >> *bs;
        CBioseq_Handle bsh = m_Scope->AddBioseq(*bs);
        return bsh.GetTopLevelEntry();
    }
    catch (CEofException&) { // ignore
    }
    catch (CException& e) {
        ERR_POST(Error << e);
    }
    return CSeq_entry_Handle();
}

//  --------------------------------------------------------------------------
CSeq_entry_Handle CAsn2FastaApp::ObtainSeqEntryFromBioseqSet(
    CObjectIStream& is)
//  --------------------------------------------------------------------------
{
    try {
        CRef<CSeq_entry> entry(new CSeq_entry);
        is >> entry->SetSet();
        return m_Scope->AddTopLevelSeqEntry(*entry);
    }
    catch (CEofException&) { // ignore
    }
    catch (CException& e) {
        ERR_POST(Error << e);
    }
    return CSeq_entry_Handle();
}

//  --------------------------------------------------------------------------
CSeq_entry_Handle CAsn2FastaApp::ObtainSeqEntryFromSeqSubmit(
    CObjectIStream& is)
//  --------------------------------------------------------------------------
{
    try {
        CRef<CSeq_entry> se(new CSeq_entry);
        CRef<CSeq_submit> sub(new CSeq_submit());
        is >> *sub;
        se.Reset( sub->SetData().SetEntrys().front() );
        return m_Scope->AddTopLevelSeqEntry(*se);
    }
    catch (CEofException&) { // ignore
    }
    catch (CException& e) {
        ERR_POST(Error << e);
    }
    return CSeq_entry_Handle();
}

//  --------------------------------------------------------------------------
bool CAsn2FastaApp::HandleSeqID( const string& seq_id )
//  --------------------------------------------------------------------------
{
    CSeq_id id(seq_id);
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle( id );
    if ( ! bsh ) {
        ERR_FATAL("Unable to obtain data on ID \"" << seq_id << "\".");
    }

    //
    //  ... and use that to generate the flat file:
    //
    CSeq_entry_Handle seh = bsh.GetTopLevelEntry();
    return HandleSeqEntry(seh);
}

//  --------------------------------------------------------------------------
bool CAsn2FastaApp::HandleSeqEntry(CRef<CSeq_entry>& se)
//  --------------------------------------------------------------------------
{
    CSeq_entry_Handle seh = m_Scope->AddTopLevelSeqEntry(*se);
    bool ret = HandleSeqEntry(seh);
    m_Scope->RemoveTopLevelSeqEntry(seh);
    m_Scope->ResetHistory();
    return ret;
}

//  --------------------------------------------------------------------------
void CAsn2FastaApp::PrintQualityScores(const CBioseq& bsp, CNcbiOstream* out_stream)
//  --------------------------------------------------------------------------
{
    TSeqPos curpos = 0;
    TSeqPos len = 0;
    int i = 0;
    int j = 0;
    bool first = true;

    if (bsp.IsSetLength()) {
        len = bsp.GetLength();
    }
    FOR_EACH_SEQANNOT_ON_BIOSEQ (sa_itr, bsp) {
        const CSeq_annot& annot = **sa_itr;
        if (! annot.IsGraph()) continue;
        FOR_EACH_SEQGRAPH_ON_SEQANNOT (gr_itr, annot) {
            const CSeq_graph& graph = **gr_itr;
            if (first) {
                first = false;
                *out_stream << ">";
                CSeq_id::WriteAsFasta(*out_stream, bsp);
                if (graph.IsSetTitle()) {
                    const string& g_title = graph.GetTitle();
                    *out_stream << " " << g_title;
                }
                *out_stream << '\n';
            }
            const CSeq_graph::TGraph& src_data = graph.GetGraph();
            switch (src_data.Which()) {
                case CSeq_graph::TGraph::e_Byte:
                    {
                    if (graph.IsSetLoc()) {
                        const CSeq_loc& loc = graph.GetLoc();
                        TSeqPos left = loc.GetStart(eExtreme_Positional);
                        while (curpos < left) {
                            if (j > 0) {
                                *out_stream << " ";
                            }
                            *out_stream << "-1";
                            if (j < 19) {
                                j++;
                            } else {
                                *out_stream << '\n';
                                j = 0;
                            }
                            curpos++;
                        }
                    }
                    const CByte_graph& byte_graph = src_data.GetByte();
                    if (byte_graph.IsSetValues()) {
                        const CByte_graph::TValues& bytes = byte_graph.GetValues();
                        for (i = 0; i < bytes.size(); i++) {
                            if (j > 0) {
                                *out_stream << " ";
                            }
                            char ch = bytes [i];
                            if ((int) ch < 10) {
                                *out_stream << " ";
                            }
                            *out_stream << (int) ch;
                            if (j < 19) {
                                j++;
                            } else {
                                *out_stream << '\n';
                                j = 0;
                            }
                            curpos++;
                        }
                    }
                    }
                    break;
                default:
                    break;
            }
        }
    }
    if (! first) {
        while (curpos < len) {
            if (j > 0) {
                *out_stream << " ";
            }
            *out_stream << "-1";
            if (j < 19) {
                j++;
            } else {
                *out_stream << '\n';
                j = 0;
            }
            curpos++;
        }
    }
    if (j > 0) {
        *out_stream << '\n';
    }
}

//  --------------------------------------------------------------------------
bool CAsn2FastaApp::HandleSeqEntry(CSeq_entry_Handle& seh)
//  --------------------------------------------------------------------------
{
    if ( m_do_cleanup ) {
        CSeq_entry_EditHandle tseh = seh.GetTopLevelEntry().GetEditHandle();
        CBioseq_set_EditHandle bseth;
        CBioseq_EditHandle bseqh;
        CRef<CSeq_entry> tmp_se(new CSeq_entry);
        if ( tseh.IsSet() ) {
            bseth = tseh.SetSet();
            CConstRef<CBioseq_set> bset = bseth.GetCompleteObject();
            bseth.Remove(bseth.eKeepSeq_entry);
            tmp_se->SetSet(const_cast<CBioseq_set&>(*bset));
        }
        else {
            bseqh = tseh.SetSeq();
            CConstRef<CBioseq> bseq = bseqh.GetCompleteObject();
            bseqh.Remove(bseqh.eKeepSeq_entry);
            tmp_se->SetSeq(const_cast<CBioseq&>(*bseq));
        }

        CCleanup cleanup;
        cleanup.BasicCleanup( *tmp_se );

        if ( tmp_se->IsSet() ) {
            tseh.SelectSet(bseth);
        }
        else {
            tseh.SelectSeq(bseqh);
        }
    }

    for (CBioseq_CI bioseq_it(seh);  bioseq_it;  ++bioseq_it) {
        CBioseq_Handle bsh = *bioseq_it;
        CConstRef<CBioseq> bsr = bsh.GetCompleteBioseq();

        CFastaOstream* fasta_os = NULL;

        bool is_genomic = false;
        bool is_RNA = false;

        CConstRef<CSeqdesc> closest_molinfo = bsr->GetClosestDescriptor(CSeqdesc::e_Molinfo);
        if (closest_molinfo) {
            const CMolInfo& molinf = closest_molinfo->GetMolinfo();
            CMolInfo::TBiomol biomol = molinf.GetBiomol();
            switch (biomol) {
                case NCBI_BIOMOL(genomic):
                case NCBI_BIOMOL(other_genetic):
                case NCBI_BIOMOL(genomic_mRNA):
                case NCBI_BIOMOL(cRNA):
                    is_genomic = true;
                    break;
                case NCBI_BIOMOL(pre_RNA):
                case NCBI_BIOMOL(mRNA):
                case NCBI_BIOMOL(rRNA):
                case NCBI_BIOMOL(tRNA):
                case NCBI_BIOMOL(snRNA):
                case NCBI_BIOMOL(scRNA):
                case NCBI_BIOMOL(snoRNA):
                case NCBI_BIOMOL(transcribed_RNA):
                case NCBI_BIOMOL(ncRNA):
                case NCBI_BIOMOL(tmRNA):
                    is_RNA = true;
                    break;
                default:
                    break;
            }
        }

        if ( m_Oq != NULL && bsh.IsNa() ) {
            PrintQualityScores (*bsr, m_Oq);
        }

        if ( m_Os.get() != NULL ) {
            if ( m_OnlyNucs && ! bsh.IsNa() ) continue;
            if ( m_OnlyProts && ! bsh.IsAa() ) continue;
            fasta_os = m_Os.get();
        } else if ( bsh.IsNa() ) {
            if ( m_On.get() != NULL ) {
                fasta_os = m_On.get();
            } else if ( is_genomic && m_Og.get() != NULL ) {
                fasta_os = m_Og.get();
                if (! m_OgHead.empty() && ! m_OgTail.empty()) {
                    TSeqPos len = bsr->GetLength();
                    if ( m_OgCurrLen > 0 && m_OgCurrLen + len > m_OgMax ) {
                        m_OgIndex++;
                        m_OgCurrLen = 0;
                        string ogx = m_OgHead + NStr::IntToString(m_OgIndex) + m_OgTail;
                        m_Og.reset( OpenFastaOstream ("", ogx, false) );
                        fasta_os = m_Og.get();
                    }
                    m_OgCurrLen += len;
                }
            } else if ( is_RNA && m_Or.get() != NULL ) {
                fasta_os = m_Or.get();
            } else {
                continue;
            }
        } else if ( bsh.IsAa() ) {
            if ( m_Op.get() != NULL ) {
                fasta_os = m_Op.get();
            }
        } else {
            if ( m_Ou.get() != NULL ) {
                fasta_os = m_Ou.get();
            } else if ( m_On.get() != NULL ) {
                fasta_os = m_On.get();
            } else {
                continue;
            }
        }

        if ( fasta_os == NULL) continue;

        if ( m_DeflineOnly ) {
            fasta_os->WriteTitle(bsh);
        } else {
            fasta_os->Write(bsh);
        }
    }
    return true;
}

//  --------------------------------------------------------------------------
CObjectIStream* CAsn2FastaApp::x_OpenIStream(const CArgs& args)
//  --------------------------------------------------------------------------
{

    // determine the file serialization format.
    // default for batch files is binary, otherwise text.
    ESerialDataFormat serial = args["batch"] ? eSerial_AsnBinary :eSerial_AsnText;
    if ( args["serial"] ) {
        const string& val = args["serial"].AsString();
        if ( val == "text" ) {
            serial = eSerial_AsnText;
        } else if ( val == "binary" ) {
            serial = eSerial_AsnBinary;
        } else if ( val == "XML" ) {
            serial = eSerial_Xml;
        }
    }

    // make sure of the underlying input stream. If -i was given on the command line
    // then the input comes from a file. Otherwise, it comes from stdin:
    CNcbiIstream* pInputStream = &NcbiCin;
    bool bDeleteOnClose = false;
    if ( args["i"] ) {
        pInputStream = new CNcbiIfstream( args["i"].AsString().c_str(), ios::binary  );
        bDeleteOnClose = true;
    }

    // if -c was specified then wrap the input stream into a gzip decompressor before
    // turning it into an object stream:
    CObjectIStream* pI = NULL;
    if ( args["c"] ) {
        CZipStreamDecompressor* pDecompressor = new CZipStreamDecompressor(
            512, 512, kZlibDefaultWbits, CZipCompression::fCheckFileHeader );
        CCompressionIStream* pUnzipStream = new CCompressionIStream(
            *pInputStream, pDecompressor, CCompressionIStream::fOwnProcessor );
        pI = CObjectIStream::Open( serial, *pUnzipStream, eTakeOwnership );
    }
    else {
        pI = CObjectIStream::Open(
            serial, *pInputStream, (bDeleteOnClose ? eTakeOwnership : eNoOwnership));
    }

    if ( 0 != pI ) {
        pI->UseMemoryPool();
    }
    return pI;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

//  ==========================================================================
int main(int argc, const char** argv)
//  ==========================================================================
{
    return CAsn2FastaApp().AppMain(argc, argv);
}
