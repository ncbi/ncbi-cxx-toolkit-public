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
#include <common/ncbi_source_ver.h>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
//#include <objtools/data_loaders/genbank/gbloader.hpp>
//#include <objtools/data_loaders/genbank/readers.hpp>

#include <objects/seqset/gb_release_file.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Byte_graph.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/seq_macros.hpp>

#include <objects/submit/Seq_submit.hpp>

//#ifdef HAVE_NCBI_VDB
//#  include <sra/data_loaders/wgs/wgsloader.hpp>
//#endif

#include <objtools/cleanup/cleanup.hpp>

#include <objmgr/util/sequence.hpp>
#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>
#include <dbapi/driver/drivers.hpp>

#include <misc/data_loaders_util/data_loaders_util.hpp>
#include <objtools/writers/fasta_writer.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//#define CANCELER_CODE
#if defined(CANCELER_CODE)
//  ============================================================================
class TestCanceler: public ICanceled
//  ============================================================================
{
public:
    static const unsigned int CALLS_UNTIL_CANCELLED = 25;
    bool IsCanceled() const {
        if (0 == ++mNumCalls % 100) {
            cerr << "Iterations until cancelled: "
                 << (CALLS_UNTIL_CANCELLED - mNumCalls) << "\n";
        }
        return (mNumCalls > CALLS_UNTIL_CANCELLED);
    };
    static unsigned int mNumCalls;
};
unsigned int TestCanceler::mNumCalls = 0;
TestCanceler canceller;
#endif


//  ==========================================================================
class CAsn2FastaApp:
    public CNcbiApplication, public CGBReleaseFile::ISeqEntryHandler
//  ==========================================================================
{
public:
    CAsn2FastaApp();
    ~CAsn2FastaApp();

    void Init() override;
    int Run() override;

    bool HandleSeqEntry(CRef<CSeq_entry>& se) override;
    bool HandleSeqEntry(CSeq_entry_Handle& se);
    bool HandleSeqID( const string& seqID );

    CSeq_entry_Handle ObtainSeqEntryFromSeqEntry(CObjectIStream& is);
    CSeq_entry_Handle ObtainSeqEntryFromBioseq(CObjectIStream& is);
    CSeq_entry_Handle ObtainSeqEntryFromBioseqSet(CObjectIStream& is);
    CSeq_entry_Handle ObtainSeqEntryFromSeqSubmit(CObjectIStream& is);

    CFastaOstreamEx* OpenFastaOstream(const string& argname, const string& strname, bool use_stdout);

    void PrintQualityScores(const CBioseq& bioseq, CNcbiOstream& ostream);

private:
    CFastaOstreamEx* x_GetFastaOstream(CBioseq_Handle& handle);
    CObjectIStream* x_OpenIStream(const string& ifname);
    bool x_IsOtherFeat(const CSeq_feat_Handle& feat) const;
    void x_InitOStreams(const CArgs& args);
    void x_BatchProcess(CObjectIStream& istr);
    void x_InitFeatDisplay(const string& feats);
    void x_ProcessIStream(const string& asn_type, CObjectIStream& istr);
    int x_ProcessISubdirectory(const CDir&);


    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CRef<CScope>                m_Scope;

    unique_ptr<CFastaOstreamEx> m_Os;           // all sequence output stream

    unique_ptr<CFastaOstreamEx> m_On;           // nucleotide output stream
    unique_ptr<CFastaOstreamEx> m_Og;           // genomic output stream
    unique_ptr<CFastaOstreamEx> m_Or;           // RNA output stream
    unique_ptr<CFastaOstreamEx> m_Op;           // protein output stream
    unique_ptr<CFastaOstreamEx> m_Ou;           // unknown output stream
    CNcbiOstream*               m_Oq = nullptr; // quality score output stream
    unique_ptr<CQualScoreWriter>  m_pQualScoreWriter;

    string                      m_OgHead;
    string                      m_OgTail;
    int                         m_OgIndex;
    TSeqPos                     m_OgMax;
    TSeqPos                     m_OgCurrLen;

    bool                        m_OnlyNucs;
    bool                        m_OnlyProts;
    bool                        m_DeflineOnly;
    bool                        m_do_cleanup;
    bool                        m_AllFeats;
    bool                        m_CDS;
    bool                        m_TranslateCDS;
    bool                        m_Gene;
    bool                        m_RNA;
    bool                        m_OtherFeats;
    CArgValue::TStringArray     m_FeatureSelection;
    bool                        m_ResolveAll;
};

// constructor
CAsn2FastaApp::CAsn2FastaApp()
{
    SetVersion(CVersionInfo(1, 0, 3));
}

// destructor
CAsn2FastaApp::~CAsn2FastaApp()
{
}


static const CDataLoadersUtil::TLoaders kDefaultLoaders =
        CDataLoadersUtil::fGenbank |
        CDataLoadersUtil::fVDB |
        CDataLoadersUtil::fSRA |
        CDataLoadersUtil::fVDBOnByDefault;

//  --------------------------------------------------------------------------
void CAsn2FastaApp::Init()
//  --------------------------------------------------------------------------
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
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
        arg_desc->SetDependency("id", CArgDescriptions::eExcludes, "i");

        arg_desc->AddOptionalKey("ids", "IDFile",
            "FIle of IDs to display, one per line", CArgDescriptions::eInputFile);
        arg_desc->SetDependency("ids", CArgDescriptions::eExcludes, "i");
        arg_desc->SetDependency("ids", CArgDescriptions::eExcludes, "id");

        // input type:
        arg_desc->AddDefaultKey( "type", "AsnType", "ASN.1 object type",
            CArgDescriptions::eString, "any" );
        arg_desc->SetConstraint( "type",
            &( *new CArgAllow_Strings, "any", "seq-entry", "bioseq", "bioseq-set", "seq-submit" ) );

        // directory processing
        arg_desc->AddOptionalKey("indir", "InputDirectory",
            "Path to input files", CArgDescriptions::eDirectory);
        arg_desc->AddOptionalKey("x", "InputExtension",
            "Input files extension", CArgDescriptions::eString);
        arg_desc->SetDependency("indir", CArgDescriptions::eExcludes, "og_head");
        arg_desc->SetDependency("indir", CArgDescriptions::eExcludes, "og_tail");
    }}


    // batch processing
    {{
        arg_desc->AddFlag("batch", "Process NCBI release file");
        // compression
        arg_desc->AddFlag("c", "Compressed file");
        // propagate top descriptors
        arg_desc->AddFlag("p", "Propagate top descriptors");

        arg_desc->AddFlag("defline-only",
                          "Only output the defline");
        string feat_desc = "Comma-separated list of feature types.\n";
        feat_desc += " Allowed types are:\n";
        feat_desc += "   fasta_cds_na,\n";
        feat_desc += "   fasta_cds_aa,\n";
        feat_desc += "   gene_fasta,\n";
        feat_desc += "   rna_fasta,\n";
        feat_desc += "   other_fasta,\n";
        feat_desc += "   all";
        arg_desc->AddOptionalKey("feats", "String", feat_desc, CArgDescriptions::eString);
        arg_desc->SetDependency("feats", CArgDescriptions::eExcludes,
                                "prots-only");
    }}

    // output
    {{

        arg_desc->AddFlag("resolve-all", "Resolves all, e.g for contigs");


        arg_desc->AddFlag("show-mods", "Show FASTA header mods (e.g. [strain=abc])");

        arg_desc->AddFlag("auto-def", "Use AutoDef algorithm for nucleotides");

        arg_desc->AddFlag("no-auto-def", "Do not use AutoDef algorithm for nucleotides");

        arg_desc->AddOptionalKey("gap-mode", "GapMode", "\
 Gap mode:\n\
   letters:  letters will show gaps as runs of Ns (default mode)\n\
   count:    count shows gaps as '>?123'\n\
   dashes:   will show gaps as runs of dashes\n\
   one-dash: shows single gap literal as one das\n\
",
            CArgDescriptions::eString);
        arg_desc->SetConstraint("gap-mode", &(*new CArgAllow_Strings,
            "letters", "count", "dashes", "one-dash"));

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

        arg_desc->AddOptionalKey("og_maxsize", "GenomeFileMaxSize",
            "Maximum size of each genomic fasta file in Mb",
            CArgDescriptions::eInteger);

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
        arg_desc->AddFlag("enable-gi", "Enable GI output in defline");

        arg_desc->AddFlag("ignore-origid", "Ignore OriginalID descriptor when constructing defline");

    }}

    // misc
    {{
         // cleanup
         arg_desc->AddFlag("cleanup",
                           "Do internal data cleanup prior to formatting");
    }}

    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc, kDefaultLoaders);

    SetupArgDescriptions(arg_desc.release());
}


//  --------------------------------------------------------------------------
CFastaOstreamEx* CAsn2FastaApp::OpenFastaOstream(const string& argname, const string& strname, bool use_stdout)
//  --------------------------------------------------------------------------
{
    const CArgs& args = GetArgs();

    if (! use_stdout) {
        if (strname.empty()) {
            if (argname.empty()) return nullptr;
            if (! args[argname]) return nullptr;
        }
    }

    CNcbiOstream* os;
    if (use_stdout) {
        os = &NcbiCout;
    } else if ( !strname.empty() ) {
        // NB: This stream will leak unless explicitly managed.
        // (An app-wide smart pointer should suffice.)
        os = new CNcbiOfstream(strname);
    } else {
        if (args[argname]) {
            os = &args[argname].AsOutputFile();
        } else {
            return nullptr;
        }
    }

    unique_ptr<CFastaOstreamEx> fasta_os(new CFastaOstreamEx(*os));
    fasta_os->SetAllFlags(
        CFastaOstreamEx::fInstantiateGaps |
        CFastaOstreamEx::fAssembleParts |
        CFastaOstreamEx::fNoDupCheck |
        CFastaOstreamEx::fKeepGTSigns |
        CFastaOstreamEx::fNoExpensiveOps |
        CFastaOstreamEx::fHideGenBankPrefix );

    if (args["enable-gi"])
    {
        fasta_os->SetFlag(CFastaOstreamEx::fEnableGI);
    }

    if (args["ignore-origid"])
    {
        fasta_os->SetFlag(CFastaOstreamEx::fIgnoreOriginalID);
    }

    if( args["gap-mode"] ) {
        fasta_os->SetFlag(
            CFastaOstreamEx::fInstantiateGaps);
        string gapmode = args["gap-mode"].AsString();
        if ( gapmode == "one-dash" ) {
            fasta_os->SetGapMode(CFastaOstreamEx::eGM_one_dash);
        } else if ( gapmode == "dashes" ) {
            fasta_os->SetGapMode(CFastaOstreamEx::eGM_dashes);
        } else if ( gapmode == "letters" ) {
            fasta_os->SetGapMode(CFastaOstreamEx::eGM_letters);
        } else if ( gapmode == "count" ) {
            fasta_os->SetGapMode(CFastaOstreamEx::eGM_count);
        }
    }

    if( args["show-mods"] ) {
        fasta_os->SetFlag(CFastaOstreamEx::fShowModifiers);
    }
    /*
    if( args["auto-def"] ) {
        fasta_os->SetFlag(CFastaOstreamEx::fUseAutoDef);
    }
    */
    if( args["no-auto-def"] ) {
        fasta_os->SetFlag(CFastaOstreamEx::fDoNotUseAutoDef);
    }
    if( args["width"] ) {
        fasta_os->SetWidth( args["width"].AsInteger() );
    }

    m_do_cleanup = ( args["cleanup"]);

    return fasta_os.release();
}


//  --------------------------------------------------------------------------
int CAsn2FastaApp::Run()
//  --------------------------------------------------------------------------
{
    // initialize conn library
    CONNECT_Init(&GetConfig());
    // create object manager
    m_Objmgr = CObjectManager::GetInstance();
    if ( !m_Objmgr ) {
        NCBI_THROW(CException, eUnknown,
                   "Could not create object manager");
    }

    try {
        const CArgs& args = GetArgs();
        CDataLoadersUtil::SetupObjectManager(args, *m_Objmgr, kDefaultLoaders);

        m_Scope.Reset(new CScope(*m_Objmgr));
        m_Scope->AddDefaults();

        m_OnlyNucs = args["nucs-only"];
        m_OnlyProts = args["prots-only"];

        m_DeflineOnly = args["defline-only"];
        // Default is not to look at features
        m_AllFeats = m_CDS
                   = m_TranslateCDS
                   = m_Gene
                   = m_RNA
                   = m_OtherFeats
                   = false;

        if ( args["feats"] ) {
            m_OnlyNucs = true; // feature only supported on nucleotide sequences
            x_InitFeatDisplay(args["feats"].AsString());
        }

        m_ResolveAll = args["resolve-all"];

        if ( args["id"] ) {
            x_InitOStreams(args);
            //
            //  Implies gbload; otherwise this feature would be pretty
            //  useless...
            //
            m_Scope->AddDefaults();
            string seqID = args["id"].AsString();
            HandleSeqID( seqID );
            return 0;
        }
        else if ( args["ids"] ) {
            CNcbiIstream& istr = args["ids"].AsInputFile();
            string id_str;
            int ret_value = 0;
            x_InitOStreams(args);
            while (NcbiGetlineEOL(istr, id_str)) {
                id_str = NStr::TruncateSpaces(id_str);
                if (id_str.empty() || id_str[0] == '#') {
                    continue;
                }

                try {
                    HandleSeqID(id_str);
                }
                catch (CException& e) {
                    ERR_POST(Error << e);
                    ret_value = 1;
                }
            }
            return ret_value;
        }

        if (args["indir"]) {
            return x_ProcessISubdirectory(args["indir"].AsDirectory());
        }

        string ifname;
        if (args["i"]) {
            ifname = args["i"].AsString();
        }

        unique_ptr<CObjectIStream> is(x_OpenIStream(ifname));
        if (!is) {
            string msg = !ifname.empty() ?
                "Unable to open input file " + ifname :
                "Unable to read data from stdin";
            NCBI_THROW(CException, eUnknown, msg);
        }
        x_InitOStreams(args);

        if ( args["batch"] ) {
            x_BatchProcess(*is.release());
        }
        else {
            string asn_type = args["type"].AsString();
            x_ProcessIStream(asn_type, *is);
        }
        return 0;
    }
    catch (CException& e) {
        ERR_POST(Error << e);
    }
    catch (exception& e) {
        ERR_POST(Error << e);
    }
    return 1;
}


//  --------------------------------------------------------------------------
void CAsn2FastaApp::x_InitOStreams(const CArgs& args)
//  --------------------------------------------------------------------------
{
    // open the output streams
    m_Os.reset( OpenFastaOstream ("o", "", false) );
    m_On.reset( OpenFastaOstream ("on", "", false) );
    m_Og.reset( OpenFastaOstream ("og", "", false) );
    m_Or.reset( OpenFastaOstream ("or", "", false) );
    m_Op.reset( OpenFastaOstream ("op", "", false) );
    m_Ou.reset( OpenFastaOstream ("ou", "", false) );
    m_Oq = args["oq"] ? &(args["oq"].AsOutputFile()) : nullptr;

    m_OgHead.clear();
    m_OgTail.clear();
    m_OgIndex = 0;
    m_OgMax = 0;
    m_OgCurrLen = 0;
    if (args["og_head"] && args["og_tail"]) {
        if (args["x"]) {
            WARNING_POST("-x is deprecated. Use -og_maxsize instead.");
        }
        int og_maxsize = 1000;
        if (args["og_maxsize"]) {
            og_maxsize = args["og_maxsize"].AsInteger();
        } else {
            // old behavior
            if (args["x"]) {
                const string& s = args["x"].AsString();
                og_maxsize = NStr::StringToInt(s);
            }
        }
        m_OgHead = args["og_head"].AsString();
        m_OgTail = args["og_tail"].AsString();
        m_OgMax = TSeqPos(og_maxsize * 1000000);
    }

    if (! m_OgHead.empty() && ! m_OgTail.empty()) {
        m_OgIndex++;
        string ogx = m_OgHead + NStr::IntToString(m_OgIndex) + m_OgTail;
        m_Og.reset( OpenFastaOstream ("", ogx, false) );
    }

    if ( !m_Os && !m_On && !m_Og && !m_Or && !m_Op && !m_Ou && !m_Oq ) {
        // No output (-o*) argument given - default to stdout
        m_Os.reset( OpenFastaOstream ("", "", true) );
    }
}


//  --------------------------------------------------------------------------
void CAsn2FastaApp::x_InitFeatDisplay(const string& feats)
//  --------------------------------------------------------------------------
{
    list<string> feat_list;
    NStr::Split(feats,
                ",",
                feat_list,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    if (feat_list.empty()) {
        m_AllFeats = true;
    } else{
        for(const string& feat_name : feat_list) {
            if (feat_name == "fasta_cds_na") {
                m_CDS = true;
            }
            else
            if (feat_name == "fasta_cds_aa") {
                m_CDS = true;
                m_TranslateCDS = true;
            }
            else
            if (feat_name == "gene_fasta") {
                m_Gene = true;
            }
            else
            if (feat_name == "rna_fasta") {
                m_RNA = true;
            }
            else
            if (feat_name == "other_fasta") {
                m_OtherFeats = true;
            }
            else
            if (feat_name == "all") {
                m_AllFeats = true;
            }
            else {
                NCBI_THROW(CException, eUnknown,
                    "Unrecognized feature type: "+feat_name );
            }
        }
    }
    m_CDS   |= m_AllFeats;
    m_Gene  |= m_AllFeats;
    m_RNA   |= m_AllFeats;
    m_OtherFeats |= m_AllFeats;
}


//  --------------------------------------------------------------------------
void CAsn2FastaApp::x_BatchProcess(CObjectIStream& istr)
//  --------------------------------------------------------------------------
{
    CGBReleaseFile in(istr);
    in.RegisterHandler(this);
    in.Read();  // HandleSeqEntry will be called from this function
}


//  --------------------------------------------------------------------------
void CAsn2FastaApp::x_ProcessIStream(const string& asn_type, CObjectIStream& istr)
//  --------------------------------------------------------------------------
{
    CSeq_entry_Handle seh;

    if ( asn_type == "seq-entry" ) {
        //
        //  Straight through processing: Read a seq_entry, then process
        //  a seq_entry:
        //
        seh = ObtainSeqEntryFromSeqEntry(istr);
        while (seh) {
            HandleSeqEntry(seh);
            m_Scope->RemoveTopLevelSeqEntry(seh);
            m_Scope->ResetHistory();
            seh = ObtainSeqEntryFromSeqEntry(istr);
        }
        return;
    }

    if ( asn_type == "any" ) {
        set<TTypeInfo> known_types, matching_types;
        known_types.insert(CSeq_entry::GetTypeInfo());
        known_types.insert(CSeq_submit::GetTypeInfo());
        known_types.insert(CBioseq_set::GetTypeInfo());
        known_types.insert(CBioseq::GetTypeInfo());

        while (!istr.EndOfData()) {
            matching_types = istr.GuessDataType(known_types);
            if (matching_types.empty()) {
                NCBI_THROW(CException, eUnknown,
                    "Unidentifiable object" );
            } else if (matching_types.size() > 1) {
                NCBI_THROW(CException, eUnknown,
                    "Ambiguous object" );
            } else if (*matching_types.begin() == CSeq_entry::GetTypeInfo()) {
                seh = ObtainSeqEntryFromSeqEntry(istr);
            } else if (*matching_types.begin() == CSeq_submit::GetTypeInfo()) {
                seh = ObtainSeqEntryFromSeqSubmit(istr);
            } else if (*matching_types.begin() == CBioseq_set::GetTypeInfo()) {
                seh = ObtainSeqEntryFromBioseqSet(istr);
            } else if (*matching_types.begin() == CBioseq::GetTypeInfo()) {
                seh = ObtainSeqEntryFromBioseq(istr);
            }
            if (!seh) {
                NCBI_THROW(CException, eUnknown,
                    "Unable to construct Seq-entry object" );
            }
            HandleSeqEntry(seh);
            m_Scope->RemoveTopLevelSeqEntry(seh);
            m_Scope->ResetHistory();
        }
        return;
    }

    if ( asn_type == "bioseq" ) {
        seh = ObtainSeqEntryFromBioseq(istr);
    }
    else if ( asn_type == "bioseq-set" ) {
        seh = ObtainSeqEntryFromBioseqSet(istr);
    }
    else if ( asn_type == "seq-submit" ) {
        seh = ObtainSeqEntryFromSeqSubmit(istr);
    }

    if ( !seh ) {
        NCBI_THROW(CException, eUnknown,
        "Unable to construct Seq-entry object" );
    }
    HandleSeqEntry(seh);
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
void CAsn2FastaApp::PrintQualityScores(const CBioseq& bioseq,
        CNcbiOstream& ostream)
//  --------------------------------------------------------------------------
{

    if (!m_pQualScoreWriter) {
        m_pQualScoreWriter.reset(new CQualScoreWriter(ostream, GetArgs()["enable-gi"]));
    }

    m_pQualScoreWriter->Write(bioseq);
}


CFastaOstreamEx* CAsn2FastaApp::x_GetFastaOstream(CBioseq_Handle& bsh)
{
    CConstRef<CBioseq> bsr = bsh.GetCompleteBioseq();

    CFastaOstreamEx* fasta_os = nullptr;

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
        case NCBI_BIOMOL(other):
            {
                CBioseq_Handle::TMol mol = bsh.GetSequenceType();
                switch (mol) {
                    case CSeq_inst::eMol_dna:
                        is_genomic = true;
                        break;
                    case CSeq_inst::eMol_rna:
                        is_RNA = true;
                        break;
                    case CSeq_inst::eMol_na:
                        is_genomic = true;
                        break;
                    default:
                        break;
                }
            }
            break;
        default:
            break;
        }
    }

    if ( m_Oq && bsh.IsNa() ) {
        PrintQualityScores (*bsr, *m_Oq);
    }

    if ( m_Os ) {
        if ( m_OnlyNucs && ! bsh.IsNa() ) return nullptr;
        if ( m_OnlyProts && ! bsh.IsAa() ) return nullptr;
        fasta_os = m_Os.get();
    } else if ( bsh.IsNa() ) {
        if ( m_On ) {
            fasta_os = m_On.get();
        } else if ( (is_genomic || ! closest_molinfo) && m_Og ) {
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
        } else if ( is_RNA && m_Or ) {
            fasta_os = m_Or.get();
        } else {
            return nullptr;
        }
    } else if ( bsh.IsAa() ) {
        if ( m_Op ) {
            fasta_os = m_Op.get();
        }
    } else {
        if ( m_Ou ) {
            fasta_os = m_Ou.get();
        } else if ( m_On ) {
            fasta_os = m_On.get();
        } else {
            return nullptr;
        }
    }
    return fasta_os;
}


//  --------------------------------------------------------------------------
bool CAsn2FastaApp::x_IsOtherFeat(const CSeq_feat_Handle& feat_handle) const
//  --------------------------------------------------------------------------
{
    return !(feat_handle.GetData().IsCdregion() ||
            feat_handle.GetData().IsGene() ||
            feat_handle.GetData().IsRna());
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

    bool any_feats = m_AllFeats
                   | m_CDS
                   | m_TranslateCDS
                   | m_Gene
                   | m_RNA
                   | m_OtherFeats;


    if(!any_feats) {
        for (CBioseq_CI bioseq_it(seh);  bioseq_it;  ++bioseq_it) {
            CBioseq_Handle bsh = *bioseq_it;
            if (!bsh)
                continue;
            CFastaOstreamEx* fasta_os = x_GetFastaOstream(bsh);
            #if defined(CANCELER_CODE)
                fasta_os->SetCanceler(&canceller);
            #endif

            if ( !fasta_os ) continue;

            if ( m_DeflineOnly ) {
                fasta_os->WriteTitle(bsh);
            } else {
                fasta_os->Write(bsh);
            }
            #if defined(CANCELER_CODE)
                fasta_os->SetCanceler(nullptr);
            #endif
        }
        return true;
    }

    // Iterate over features
    SAnnotSelector sel;
    if (m_ResolveAll) {
        sel.SetResolveAll();
        sel.SetAdaptiveDepth(true);
    }
    sel.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
    sel.ExcludeNamedAnnots("SNP");
    sel.SetExcludeExternal();

    CScope& scope = seh.GetScope();

    for (CBioseq_CI bioseq_it(seh); bioseq_it; ++bioseq_it) {
        CBioseq_Handle bsh = *bioseq_it;
        if (!bsh)
            continue;

        CFastaOstreamEx* fasta_os = x_GetFastaOstream(bsh);

        CFeat_CI feat_it(bsh, sel);
        for ( ; feat_it; ++feat_it) {
            if (!feat_it->IsSetData() ||
                (feat_it->GetData().IsCdregion() && !m_CDS) ||
                (feat_it->GetData().IsGene() && !m_Gene) ||
                (feat_it->GetData().IsRna() && !m_RNA) ||
                (x_IsOtherFeat(*feat_it) && !m_OtherFeats)) {
                continue;
            }
            const CSeq_feat& feat = feat_it->GetMappedFeature();
            if ( m_DeflineOnly ) {
                fasta_os->WriteFeatureTitle(feat, scope, m_TranslateCDS);
            } else {
                fasta_os->WriteFeature(feat, scope, m_TranslateCDS);
            }
        }
    }

    return true;
}

//  --------------------------------------------------------------------------
CObjectIStream* CAsn2FastaApp::x_OpenIStream(const string& ifname)
//  --------------------------------------------------------------------------
{
    const CArgs& args = GetArgs();
    // determine the file serialization format.
    // default for batch files is binary, otherwise text.
    ESerialDataFormat serial = args["batch"] ? eSerial_AsnBinary : eSerial_AsnText;
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
    if ( !ifname.empty() ) {
        pInputStream = new CNcbiIfstream( ifname, ios::binary );
        bDeleteOnClose = true;
    }

    // if -c was specified then wrap the input stream into a gzip decompressor before
    // turning it into an object stream:
    CObjectIStream* pI = nullptr;
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

    if ( pI ) {
        pI->UseMemoryPool();
    }
    return pI;
}

//  --------------------------------------------------------------------------
int CAsn2FastaApp::x_ProcessISubdirectory(const CDir& dir)
//  --------------------------------------------------------------------------
{
    if (!dir.Exists() || !dir.CheckAccess(CDirEntry::fRead)) {
        ERR_POST("Unable to read directory: " + dir.GetName());
        return 2;
    }

    const CArgs& args = GetArgs();

    string ext = ".sqn";
    if (args["x"]) {
        ext = args["x"].AsString();
    }
    const string mask = "*" + ext;

    CDir::TEntries entries = dir.GetEntries(mask, CDir::fIgnoreRecursive);
    if (entries.empty()) {
        WARNING_POST("No files matching " + mask + " found in " + dir.GetName());
    }

    for (CDir::TEntry de : entries) {
        const string ifname = de->GetPath();
        string ofname = ifname;
        if (NStr::EndsWith(ofname, ext)) {
            // strip extension
            ofname.resize(ofname.size() - ext.size());
        }
        ofname += ".faa";

        unique_ptr<CObjectIStream> is(x_OpenIStream(ifname));
        if (!is) {
            const string msg = "Unable to open input file" + ifname;
            NCBI_THROW(CException, eUnknown, msg);
        }

        m_Os.reset(OpenFastaOstream("", ofname, false));

        if (args["batch"]) {
            x_BatchProcess(*is.release());
        } else {
            string asn_type = args["type"].AsString();
            x_ProcessIStream(asn_type, *is);
        }
    }

    return 0;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

//  ==========================================================================
int main(int argc, const char** argv)
//  ==========================================================================
{
    return CAsn2FastaApp().AppMain(argc, argv);
}
