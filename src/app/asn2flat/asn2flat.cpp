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
* Author:  Aaron Ucko, Mati Shomrat, Mike DiCuccio, Jonathan Kans, NCBI
*
* File Description:
*   flat-file generator application
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <common/ncbi_source_ver.h>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_signal.hpp>
#include <connect/ncbi_core_cxx.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seq/seq_macros.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objects/seqset/gb_release_file.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

#include <objmgr/util/objutil.hpp>

#include <misc/data_loaders_util/data_loaders_util.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#define USE_CDDLOADER

#if defined(HAVE_LIBGRPC) && defined(HAVE_NCBI_VDB)
#define USE_SNPLOADER
#endif

#ifdef USE_SNPLOADER
// ID-5865 : For SNP retrieval in PSG mode via SNP data loader
#  include <misc/grpc_integration/grpc_integration.hpp>
#  include <grpc++/grpc++.h>
#  include <objects/dbsnp/primary_track/dbsnp.grpc.pb.h>
#  include <objects/dbsnp/primary_track/dbsnp.pb.h>
#endif

#ifdef USE_CDDLOADER
    #include <sra/data_loaders/snp/snploader.hpp>
    #include <objtools/data_loaders/cdd/cdd_loader/cdd_loader.hpp>
#endif



BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CHTMLFormatterEx : public IHTMLFormatter
{
public:
    CHTMLFormatterEx(CRef<CScope> scope);
    ~CHTMLFormatterEx() override;

    void FormatProteinId(string&, const CSeq_id& seq_id, const string& prot_id) const override;
    void FormatTranscriptId(string &, const CSeq_id& seq_id, const string& nuc_id) const override;
    void FormatNucSearch(CNcbiOstream& os, const string& id) const override;
    void FormatNucId(string& str, const CSeq_id& seq_id, TIntId gi, const string& acc_id) const override;
    void FormatTaxid(string& str, const TTaxId taxid, const string& taxname) const override;
    void FormatLocation(string& str, const CSeq_loc& loc, TIntId gi, const string& visible_text) const override;
    void FormatModelEvidence(string& str, const SModelEvidance& me) const override;
    void FormatTranscript(string& str, const string& name) const override;
    void FormatGeneralId(CNcbiOstream& os, const string& id) const override;
    void FormatGapLink(CNcbiOstream& os, TSeqPos gap_size, const string& id, bool is_prot) const override;
    void FormatUniProtId(string& str, const string& prot_id) const override;

private:
    mutable CRef<CScope> m_scope;
};


CHTMLFormatterEx::CHTMLFormatterEx(CRef<CScope> scope) : m_scope(scope)
{
}

CHTMLFormatterEx::~CHTMLFormatterEx()
{
}

void CHTMLFormatterEx::FormatProteinId(string& str, const CSeq_id& seq_id, const string& prot_id) const
{
    string index = prot_id;
    CBioseq_Handle bsh = m_scope->GetBioseqHandle(seq_id);
    vector< CSeq_id_Handle > ids = bsh.GetId();
    ITERATE(vector< CSeq_id_Handle >, it, ids) {
        CSeq_id_Handle hid = *it;
        if (hid.IsGi()) {
            index = NStr::NumericToString(hid.GetGi());
            break;
        }
    }
    str = "<a href=\"";
    str += strLinkBaseProt;
    str += index;
    str += "\">";
    str += prot_id;
    str += "</a>";
}

void CHTMLFormatterEx::FormatTranscriptId(string& str, const CSeq_id& seq_id, const string& nuc_id) const
{
    string index = nuc_id;
    CBioseq_Handle bsh = m_scope->GetBioseqHandle(seq_id);
    vector< CSeq_id_Handle > ids = bsh.GetId();
    ITERATE(vector< CSeq_id_Handle >, it, ids) {
        CSeq_id_Handle hid = *it;
        if (hid.IsGi()) {
            index = NStr::NumericToString(hid.GetGi());
            break;
        }
    }
    str = "<a href=\"";
    str += strLinkBaseNuc;
    str += index;
    str += "\">";
    str += nuc_id;
    str += "</a>";
}

void CHTMLFormatterEx::FormatNucId(string& str, const CSeq_id& seq_id, TIntId gi, const string& acc_id) const
{
    str = "<a href=\"";
    str += strLinkBaseNuc + NStr::NumericToString(gi) + "\">" + acc_id + "</a>";
}

void CHTMLFormatterEx::FormatTaxid(string& str, const TTaxId taxid, const string& taxname) const
{
    if (!NStr::StartsWith(taxname, "Unknown", NStr::eNocase)) {
        if (taxid > ZERO_TAX_ID) {
            str += "<a href=\"";
            str += strLinkBaseTaxonomy;
            str += "id=";
            str += NStr::NumericToString(taxid);
            str += "\">";
        }
        else {
            string t_taxname = taxname;
            replace(t_taxname.begin(), t_taxname.end(), ' ', '+');
            str += "<a href=\"";
            str += strLinkBaseTaxonomy;
            str += "name=";
            str += taxname;
            str += "\">";
        }
        str += taxname;
        str += "</a>";
    }
    else {
        str = taxname;
    }

    TryToSanitizeHtml(str);
}

void CHTMLFormatterEx::FormatNucSearch(CNcbiOstream& os, const string& id) const
{
    os << "<a href=\"" << strLinkBaseNucSearch << id << "\">" << id << "</a>";
}

void CHTMLFormatterEx::FormatLocation(string& strLink, const CSeq_loc& loc, TIntId gi, const string& visible_text) const
{
    // check if this is a protein or nucleotide link
    bool is_prot = false;
    {{
        CBioseq_Handle bioseq_h;
        ITERATE(CSeq_loc, loc_ci, loc) {
            bioseq_h = m_scope->GetBioseqHandle(loc_ci.GetSeq_id());
            if (bioseq_h) {
                break;
            }
        }
        if (bioseq_h) {
            is_prot = (bioseq_h.GetBioseqMolType() == CSeq_inst::eMol_aa);
        }
    }}

    // assembly of the actual string:
    strLink.reserve(100); // euristical URL length

    strLink = "<a href=\"";

    // link base
    if (is_prot) {
        strLink += strLinkBaseProt;
    }
    else {
        strLink += strLinkBaseNuc;
    }
    strLink += NStr::NumericToString(gi);

    // location
    if (loc.IsInt() || loc.IsPnt()) {
        TSeqPos iFrom = loc.GetStart(eExtreme_Positional) + 1;
        TSeqPos iTo = loc.GetStop(eExtreme_Positional) + 1;
        strLink += "?from=";
        strLink += NStr::IntToString(iFrom);
        strLink += "&amp;to=";
        strLink += NStr::IntToString(iTo);
    }
    else if (visible_text != "Precursor") {
        // TODO: this fails on URLs that require "?itemID=" (e.g. almost any, such as U54469)
        strLink += "?itemid=TBD";
    }

    strLink += "\">";
    strLink += visible_text;
    strLink += "</a>";
}

void CHTMLFormatterEx::FormatModelEvidence(string& str, const SModelEvidance& me) const
{
    str += "<a href=\"";
    str += strLinkBaseNuc;
    if (me.gi > ZERO_GI) {
        str += NStr::NumericToString(me.gi);
    }
    else {
        str += me.name;
    }
    str += "?report=graph";
    if ((me.span.first >= 0) && (me.span.second >= me.span.first)) {
        const Int8 kPadAmount = 500;
        // The "+1" is because we display 1-based to user and in URL
        str += "&v=";
        str += NStr::NumericToString(max<Int8>(me.span.first + 1 - kPadAmount, 1));
        str += ":";
        str += NStr::NumericToString(me.span.second + 1 + kPadAmount); // okay if second number goes over end of sequence
    }
    str += "\">";
    str += me.name;
    str += "</a>";
}

void CHTMLFormatterEx::FormatTranscript(string& str, const string& name) const
{
    str += "<a href=\"";
    str += strLinkBaseNuc;
    str += name;
    str += "\">";
    str += name;
    str += "</a>";
}

void CHTMLFormatterEx::FormatGeneralId(CNcbiOstream& os, const string& id) const
{
    os << "<a href=\"" << strLinkBaseNuc << id << "\">" << id << "</a>";
}

void CHTMLFormatterEx::FormatGapLink(CNcbiOstream& os, TSeqPos gap_size,
                                     const string& id, bool is_prot) const
{
    const string link_base = (is_prot ? strLinkBaseProt : strLinkBaseNuc);
    const char *mol_type = (is_prot ? "aa" : "bp" );
    os << "          [gap " << gap_size << " " << mol_type << "]" <<
        "    <a href=\"" << link_base << id << "?expand-gaps=on\">Expand Ns</a>";
}

void CHTMLFormatterEx::FormatUniProtId(string& str, const string& prot_id) const
{
    str = "<a href=\"";
    str += strLinkBaseUniProt;
    str += prot_id;
    str += "\">";
    str += prot_id;
    str += "</a>";
}

class CAsn2FlatApp : public CNcbiApplication, public CGBReleaseFile::ISeqEntryHandler
{
public:
    CAsn2FlatApp();
    ~CAsn2FlatApp();

    void Init() override;
    int Run() override;

    bool HandleSeqEntry(CRef<CSeq_entry>& se) override;
    bool HandleSeqEntry(const CSeq_entry_Handle& seh);

protected:
    void HandleSeqSubmit(CObjectIStream& is );
    void HandleSeqSubmit(CSeq_submit& sub);
    void HandleSeqId(const string& id);

    CSeq_entry_Handle ObtainSeqEntryFromSeqEntry(CObjectIStream& is, bool report = false);
    CSeq_entry_Handle ObtainSeqEntryFromBioseq(CObjectIStream& is, bool report = false);
    CSeq_entry_Handle ObtainSeqEntryFromBioseqSet(CObjectIStream& is, bool report = false);

    CNcbiOstream* OpenFlatfileOstream(const string& name);

private:
    // types
    typedef CFlatFileConfig::CGenbankBlockCallback TGenbankBlockCallback;

    CObjectIStream* x_OpenIStream(const CArgs& args);

    CFlatFileGenerator* x_CreateFlatFileGenerator(const CArgs& args);
    TGenbankBlockCallback* x_GetGenbankCallback(const CArgs& args);
    TSeqPos x_GetFrom(const CArgs& args);
    TSeqPos x_GetTo  (const CArgs& args);
    ENa_strand x_GetStrand(const CArgs& args);
    void x_GetLocation(const CSeq_entry_Handle& entry,
        const CArgs& args, CSeq_loc& loc);
    CBioseq_Handle x_DeduceTarget(const CSeq_entry_Handle& entry);
    void x_CreateCancelBenchmarkCallback();
    int x_AddSNPAnnots(CBioseq_Handle& bsh);

    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CRef<CScope>                m_Scope;

    CNcbiOstream*               m_Os;           // all sequence output stream
    bool                        m_OnlyNucs;
    bool                        m_OnlyProts;

    CNcbiOstream*               m_On;           // nucleotide output stream
    CNcbiOstream*               m_Og;           // genomic output stream
    CNcbiOstream*               m_Or;           // RNA output stream
    CNcbiOstream*               m_Op;           // protein output stream
    CNcbiOstream*               m_Ou;           // unknown output stream

    CRef<CFlatFileGenerator>    m_FFGenerator;  // Flat-file generator
    unique_ptr<ICanceled>         m_pCanceledCallback;
    bool                        m_do_cleanup;
    bool                        m_Exception;
    bool                        m_FetchFail;
    bool                        m_PSGMode;
#ifdef USE_SNPLOADER
    CRef<CSNPDataLoader>        m_SNPDataLoader;
    unique_ptr<ncbi::grpcapi::dbsnp::primary_track::DbSnpPrimaryTrack::Stub> m_SNPTrackStub;
#endif
#ifdef USE_CDDLOADER
    CRef<CCDDDataLoader>        m_CDDDataLoader;
#endif
};


// constructor
CAsn2FlatApp::CAsn2FlatApp()
{
    const CVersionInfo vers (6, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY);
    SetVersion (vers);
}

// destructor
CAsn2FlatApp::~CAsn2FlatApp()
{
}

void CAsn2FlatApp::Init()
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Convert an ASN.1 Seq-entry into a flat report",
        false);

    // input
    {{
         arg_desc->SetCurrentGroup("Input/Output Options");
         // name
         arg_desc->AddOptionalKey("i", "InputFile",
                                  "Input file name",
                                  CArgDescriptions::eInputFile);

         // input file serial format (AsnText\AsnBinary\XML, default: AsnText)
         arg_desc->AddOptionalKey("serial", "SerialFormat", "Input file format",
                                  CArgDescriptions::eString);
         arg_desc->SetConstraint("serial", &(*new CArgAllow_Strings,
                                             "text", "binary", "XML"));
         arg_desc->AddFlag("sub", "Submission");
         // id
         arg_desc->AddOptionalKey("id", "ID",
                                  "Specific ID to display",
                                  CArgDescriptions::eString);
         arg_desc->AddOptionalKey("ids", "IDFile",
                                  "FIle of IDs to display, one per line",
                                  CArgDescriptions::eInputFile);

         // input type:
         arg_desc->AddDefaultKey( "type", "AsnType", "ASN.1 object type",
                                  CArgDescriptions::eString, "any" );
         arg_desc->SetConstraint( "type",
                                  &( *new CArgAllow_Strings, "any", "seq-entry", "bioseq", "bioseq-set", "seq-submit" ) );

        // single output name
        arg_desc->AddOptionalKey("o", "SingleOutputFile",
            "Single output file name", CArgDescriptions::eOutputFile);

        // file names
        arg_desc->AddOptionalKey("on", "NucleotideOutputFile",
            "Nucleotide output file name", CArgDescriptions::eOutputFile);
        arg_desc->SetDependency("on", CArgDescriptions::eExcludes, "o");

        arg_desc->AddOptionalKey("og", "GenomicOutputFile",
            "Genomic output file name", CArgDescriptions::eOutputFile);
        arg_desc->SetDependency("og", CArgDescriptions::eExcludes, "o");
        arg_desc->SetDependency("og", CArgDescriptions::eExcludes, "on");

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
     }}

    // batch processing
    {{
         arg_desc->SetCurrentGroup("Batch Processing Options");
         arg_desc->AddFlag("batch", "Process NCBI release file");
         // compression
         arg_desc->AddFlag("c", "Compressed file");
         // propogate top descriptors
         arg_desc->AddFlag("p", "Propagate top descriptors");
     }}

    // in flat_file_config.cpp
    CFlatFileConfig::AddArgumentDescriptions(*arg_desc);

     // debugging options
     {{
         arg_desc->SetCurrentGroup("Debugging Options - Subject to change or removal without warning");

         // benchmark cancel-checking
         arg_desc->AddFlag(
             "benchmark-cancel-checking",
             "Check statistics on how often the flatfile generator checks if "
             "it should be canceled.  This also sets up SIGUSR1 to trigger "
             "cancellation." );
     }}

     CDataLoadersUtil::AddArgumentDescriptions(*arg_desc);

     arg_desc->SetCurrentGroup(kEmptyStr);
     SetupArgDescriptions(arg_desc.release());
}


CNcbiOstream* CAsn2FlatApp::OpenFlatfileOstream(const string& name)
{
    const CArgs& args = GetArgs();

    if (! args[name])
        return nullptr;

    CNcbiOstream* flatfile_os = &(args[name].AsOutputFile());

    // so we don't fail silently if, e.g., the output disk gets full
    flatfile_os->exceptions( ios::failbit | ios::badbit );

    return flatfile_os;
}

static void s_INSDSetOpen(bool is_insdseq, CNcbiOstream* os) {

    if (is_insdseq) {
        *os << "<INSDSet>" << endl;
    }
}

static void s_INSDSetClose(bool is_insdseq, CNcbiOstream* os) {

    if (is_insdseq) {
        *os << "</INSDSet>" << endl;
    }
}


int CAsn2FlatApp::Run()
{
    CTime expires = GetFullVersion().GetBuildInfo().GetBuildTime();
    if (!expires.IsEmpty())
    {
        expires.AddYear();
        if (CTime(CTime::eCurrent) > expires)
        {
            NcbiCerr << "This copy of " << GetProgramDisplayName()
                     << " is more than 1 year old. Please download the current version if it is newer." << endl;
        }
    }

    m_Exception = false;
    m_FetchFail = false;

    // initialize conn library
    CONNECT_Init(&GetConfig());

    const CArgs& args = GetArgs();

    CSignal::TrapSignals(CSignal::eSignal_USR1);

    // create object manager
    m_Objmgr = CObjectManager::GetInstance();
    if ( !m_Objmgr ) {
        NCBI_THROW(CException, eUnknown, "Could not create object manager");
    }
    if (args["gbload"] || args["id"] || args["ids"]) {
        static const CDataLoadersUtil::TLoaders default_loaders =
            CDataLoadersUtil::fGenbank | CDataLoadersUtil::fVDB | CDataLoadersUtil::fSRA;
        CDataLoadersUtil::SetupObjectManager(args, *m_Objmgr, default_loaders);

        if (( args["enable-external"] && ! args["no-external"] ) || args["policy"].AsString() == "external" ) {
            CGBDataLoader* gb_loader = dynamic_cast<CGBDataLoader*>(CObjectManager::GetInstance()->FindDataLoader("GBLOADER"));
            if (gb_loader) {
                // needed to find remote features when reading local ASN.1 file
                gb_loader->CGBDataLoader::SetAlwaysLoadExternal(true);
            }
        }
    }

    m_Scope.Reset(new CScope(*m_Objmgr));
    m_Scope->AddDefaults();

    const CNcbiRegistry& cfg = CNcbiApplication::Instance()->GetConfig();
    m_PSGMode = cfg.GetBool("genbank", "loader_psg", false, 0, CNcbiRegistry::eReturn);
    if (m_PSGMode) {
#ifdef USE_SNPLOADER
        string host = cfg.GetString("SNPAccess", "host", "");
        string port = cfg.GetString("SNPAccess", "port", "");
        string hostport = host + ":" + port;

        auto channel =
            grpc::CreateChannel(hostport, grpc::InsecureChannelCredentials());
        m_SNPTrackStub =
            ncbi::grpcapi::dbsnp::primary_track::DbSnpPrimaryTrack::NewStub(channel);
        CRef<CSNPDataLoader> snp_data_loader(CSNPDataLoader::RegisterInObjectManager(*m_Objmgr).GetLoader());
        m_Scope->AddDataLoader(snp_data_loader->GetLoaderNameFromArgs());
#endif
#ifdef USE_CDDLOADER
        bool use_mongo_cdd =
            cfg.GetBool("genbank", "vdb_cdd", false, 0, CNcbiRegistry::eReturn) &&
            cfg.GetBool("genbank", "always_load_external", false, 0, CNcbiRegistry::eReturn);
        if (use_mongo_cdd) {
            CRef<CCDDDataLoader> cdd_data_loader(CCDDDataLoader::RegisterInObjectManager(*m_Objmgr).GetLoader());
            m_Scope->AddDataLoader(cdd_data_loader->GetLoaderNameFromArgs());
        }
#endif
    }

    // open the output streams
    m_Os = OpenFlatfileOstream ("o");
    m_On = OpenFlatfileOstream ("on");
    m_Og = OpenFlatfileOstream ("og");
    m_Or = OpenFlatfileOstream ("or");
    m_Op = OpenFlatfileOstream ("op");
    m_Ou = OpenFlatfileOstream ("ou");

    m_OnlyNucs = false;
    m_OnlyProts = false;
    if (m_Os) {
        const string& view = args["view"].AsString();
        m_OnlyNucs = (view == "nuc");
        m_OnlyProts = (view == "prot");
    }

    if (!m_Os && !m_On && !m_Og && !m_Or && !m_Op && !m_Ou) {
        // No output (-o*) argument given - default to stdout
        m_Os = &cout;
    }

    bool is_insdseq = false;
    if (args["format"].AsString() == "insdseq") {
        // only print <INSDSet> ... </INSDSet> wrappers if single output stream
        if (m_Os) {
            is_insdseq = true;
        }
    }

    // create the flat-file generator
    m_FFGenerator.Reset(x_CreateFlatFileGenerator(args));
    if ( args["no-external"] || args["policy"].AsString() == "internal" ) {
        m_FFGenerator->SetAnnotSelector().SetExcludeExternal(true);
    }
//    else if (!m_Scope->GetKeepExternalAnnotsForEdit()) {
//       m_Scope->SetKeepExternalAnnotsForEdit();
//    }
    if( args["resolve-all"] || args["policy"].AsString() == "external") {
        m_FFGenerator->SetAnnotSelector().SetResolveAll();
    }
    if( args["depth"] ) {
        m_FFGenerator->SetAnnotSelector().SetResolveDepth(args["depth"].AsInteger());
    }
    if( args["max_search_segments"] ) {
        m_FFGenerator->SetAnnotSelector().SetMaxSearchSegments(args["max_search_segments"].AsInteger());
    }
    if( args["max_search_time"] ) {
        m_FFGenerator->SetAnnotSelector().SetMaxSearchTime(float(args["max_search_time"].AsDouble()));
    }

    unique_ptr<CObjectIStream> is;
    is.reset( x_OpenIStream( args ) );
    if (!is) {
        string msg = args["i"]? "Unable to open input file" + args["i"].AsString() :
            "Unable to read data from stdin";
        NCBI_THROW(CException, eUnknown, msg);
    }

    if ( args[ "sub" ] ) {
        s_INSDSetOpen ( is_insdseq, m_Os );
        HandleSeqSubmit( *is );
        s_INSDSetClose ( is_insdseq, m_Os );
        if (m_Exception) return -1;
        return 0;
    }

    if ( args[ "batch" ] ) {
        s_INSDSetOpen ( is_insdseq, m_Os );
        bool propagate = args[ "p" ];
        CGBReleaseFile in( *is.release(), propagate );
        in.RegisterHandler( this );
        in.Read();  // HandleSeqEntry will be called from this function
        s_INSDSetClose ( is_insdseq, m_Os );
        if (m_Exception) return -1;
        return 0;
    }

    if ( args[ "ids" ] ) {
        s_INSDSetOpen ( is_insdseq, m_Os );
        CNcbiIstream& istr = args["ids"].AsInputFile();
        string id_str;
        while (NcbiGetlineEOL(istr, id_str)) {
            id_str = NStr::TruncateSpaces(id_str);
            if (id_str.empty() || id_str[0] == '#') {
                continue;
            }

            try {
                LOG_POST(Error << "id = " << id_str);
                HandleSeqId( id_str );
            }
            catch (CException& e) {
                ERR_POST(Error << e);
            }
        }
        s_INSDSetClose ( is_insdseq, m_Os );
        if (m_Exception) return -1;
        return 0;
    }

    if ( args[ "id" ] ) {
        s_INSDSetOpen ( is_insdseq, m_Os );
        HandleSeqId( args[ "id" ].AsString() );
        s_INSDSetClose ( is_insdseq, m_Os );
        if (m_Exception) return -1;
        return 0;
    }

    string asn_type = args["type"].AsString();

    s_INSDSetOpen ( is_insdseq, m_Os );
    if ( asn_type == "seq-entry" ) {
        //
        //  Straight through processing: Read a seq_entry, then process
        //  a seq_entry:
        //
        while ( !is->EndOfData() ) {
            CSeq_entry_Handle seh = ObtainSeqEntryFromSeqEntry(*is, true);
            if ( !seh ) {
                NCBI_THROW(CException, eUnknown,
                           "Unable to construct Seq-entry object" );
            }
            HandleSeqEntry(seh);
            m_Scope->RemoveTopLevelSeqEntry(seh);
        }
    }
    else if ( asn_type == "bioseq" ) {
        //
        //  Read object as a bioseq, wrap it into a seq_entry, then process
        //  the wrapped bioseq as a seq_entry:
        //
        while ( !is->EndOfData() ) {
            CSeq_entry_Handle seh = ObtainSeqEntryFromBioseq(*is, true);
            if ( !seh ) {
                NCBI_THROW(CException, eUnknown,
                           "Unable to construct Seq-entry object" );
            }
            HandleSeqEntry(seh);
            m_Scope->RemoveTopLevelSeqEntry(seh);
        }
    }
    else if ( asn_type == "bioseq-set" ) {
        //
        //  Read object as a bioseq_set, wrap it into a seq_entry, then
        //  process the wrapped bioseq_set as a seq_entry:
        //
        while ( !is->EndOfData() ) {
            CSeq_entry_Handle seh = ObtainSeqEntryFromBioseqSet(*is, true);
            if ( !seh ) {
                NCBI_THROW(CException, eUnknown,
                           "Unable to construct Seq-entry object" );
            }
            HandleSeqEntry(seh);
            m_Scope->RemoveTopLevelSeqEntry(seh);
        }
    }
    else if ( asn_type == "seq-submit" ) {
        while ( !is->EndOfData() ) {
            HandleSeqSubmit( *is );
        }
    }
    else if ( asn_type == "any" ) {
        //
        //  Try the first four in turn:
        //
        while ( !is->EndOfData() ) {
            string strNextTypeName = is->PeekNextTypeName();

            CSeq_entry_Handle seh = ObtainSeqEntryFromSeqEntry(*is);
            if ( !seh ) {
                is->Close();
                is.reset( x_OpenIStream( args ) );
                seh = ObtainSeqEntryFromBioseqSet(*is);
                if ( !seh ) {
                    is->Close();
                    is.reset( x_OpenIStream( args ) );
                    seh = ObtainSeqEntryFromBioseq(*is);
                    if ( !seh ) {
                        is->Close();
                        is.reset( x_OpenIStream( args ) );
                        CRef<CSeq_submit> sub(new CSeq_submit);
                        *is >> *sub;
                        if (sub->IsSetSub() && sub->IsSetData()) {
                            HandleSeqSubmit(*sub);
                            if (m_Exception) return -1;
                            return 0;
                        } else {
                            NCBI_THROW(
                                       CException, eUnknown,
                                       "Unable to construct Seq-entry object"
                                      );
                        }
                    }
                }
            }
            HandleSeqEntry(seh);
            m_Scope->RemoveTopLevelSeqEntry(seh);
        }
    }
    s_INSDSetClose ( is_insdseq, m_Os );

    if (m_Exception) return -1;
    return 0;
}


void CAsn2FlatApp::HandleSeqSubmit(CSeq_submit& sub)
{
    if (!sub.IsSetSub() || !sub.IsSetData() || !sub.GetData().IsEntrys()) {
        return;
    }
    CRef<CScope> scope(new CScope(*m_Objmgr));
    scope->AddDefaults();
    if (m_do_cleanup) {
        CCleanup cleanup;
        cleanup.BasicCleanup(sub);
    }
    const CArgs& args = GetArgs();
    if (args["from"] || args["to"] || args["strand"]) {
        CRef<CSeq_entry> e(sub.SetData().SetEntrys().front());
        CSeq_entry_Handle seh;
        try {
            seh = scope->GetSeq_entryHandle(*e);
        } catch (CException&) {}

        if (!seh) {  // add to scope if not already in it
            seh = scope->AddTopLevelSeqEntry(*e);
        }
        // "remember" the submission block
        m_FFGenerator->SetSubmit(sub.GetSub());
        CSeq_loc loc;
        x_GetLocation(seh, args, loc);
        try {
            m_FFGenerator->Generate(loc, seh.GetScope(), *m_Os);
        }
        catch (CException& e) {
            ERR_POST(Error << e);
            m_Exception = true;
        }
    } else {
        try {
            m_FFGenerator->Generate(sub, *scope, *m_Os);
        }
        catch (CException& e) {
            ERR_POST(Error << e);
            m_Exception = true;
        }
    }
}


//  ============================================================================
void CAsn2FlatApp::HandleSeqSubmit(CObjectIStream& is )
//  ============================================================================
{
    CRef<CSeq_submit> sub(new CSeq_submit);
    is >> *sub;
    HandleSeqSubmit(*sub);
}

//  ============================================================================
void CAsn2FlatApp::HandleSeqId(
    const string& strId )
//  ============================================================================
{
    CSeq_entry_Handle seh;

    // This C++-scope gets a raw CSeq_entry that has no attachment
    // to any CScope and puts it into entry.
    {
        CSeq_id id(strId);
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle( id );
        if ( ! bsh ) {
            NCBI_THROW(
                CException, eUnknown,
                "Unable to retrieve data for the given ID"
                );
        }
        seh = bsh.GetParentEntry();
        /*
        CConstRef<CSeq_entry> ser = seh.GetTopLevelEntry().GetCompleteSeq_entry();
        if (ser) {
            cout << MSerial_AsnText << *ser << endl;
        }
        */
    }

    //
    //  ... and use that to generate the flat file:
    //
    HandleSeqEntry( seh );
}

//  ============================================================================
bool CAsn2FlatApp::HandleSeqEntry(const CSeq_entry_Handle& seh )
//  ============================================================================
{
    const CArgs& args = GetArgs();

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

    if ( args["faster"] || args["policy"].AsString() == "ftp" || args["policy"].AsString() == "web" ) {

            try {
                if ( args["from"] || args["to"] || args["strand"] ) {
                    CSeq_loc loc;
                    x_GetLocation( seh, args, loc );
                    CNcbiOstream* flatfile_os = m_Os;
                    m_FFGenerator->Generate(loc, seh.GetScope(), *flatfile_os, true, m_Os);
                } else {
                    CNcbiOstream* flatfile_os = m_Os;
                    m_FFGenerator->Generate( seh, *flatfile_os, true, m_Os, m_On, m_Og, m_Or, m_Op, m_Ou);
                }
            }
            catch (CException& e) {
                  ERR_POST(Error << e);
                  m_Exception = true;
            }

        return true;
    }

    m_FFGenerator->SetFeatTree(new feature::CFeatTree(seh));

    for (CBioseq_CI bioseq_it(seh);  bioseq_it;  ++bioseq_it) {
        CBioseq_Handle bsh = *bioseq_it;
        CConstRef<CBioseq> bsr = bsh.GetCompleteBioseq();

        CNcbiOstream* flatfile_os = nullptr;

        bool is_genomic = false;
        bool is_RNA = false;

        CConstRef<CSeqdesc> closest_molinfo
            = bsr->GetClosestDescriptor(CSeqdesc::e_Molinfo);
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

        if ( m_Os ) {
            if ( m_OnlyNucs && ! bsh.IsNa() ) continue;
            if ( m_OnlyProts && ! bsh.IsAa() ) continue;
            flatfile_os = m_Os;
        } else if ( bsh.IsNa() ) {
            if ( m_On ) {
                flatfile_os = m_On;
            } else if ( (is_genomic || ! closest_molinfo) && m_Og ) {
                flatfile_os = m_Og;
            } else if ( is_RNA && m_Or ) {
                flatfile_os = m_Or;
            } else {
                continue;
            }
        } else if ( bsh.IsAa() ) {
            if ( m_Op ) {
                flatfile_os = m_Op;
            }
        } else {
            if ( m_Ou ) {
                flatfile_os = m_Ou;
            } else if ( m_On ) {
                flatfile_os = m_On;
            } else {
                continue;
            }
        }

        if ( !flatfile_os ) continue;

        if (m_PSGMode && ( args["enable-external"] || args["policy"].AsString() == "external" ))
            x_AddSNPAnnots(bsh);

        // generate flat file
        if ( args["from"] || args["to"] || args["strand"] ) {
            CSeq_loc loc;
            x_GetLocation( seh, args, loc );
            try {
                m_FFGenerator->Generate(loc, seh.GetScope(), *flatfile_os);
            }
            catch (CException& e) {
                ERR_POST(Error << e);
                m_Exception = true;
            }
            // emulate the C Toolkit: only produce flatfile for first sequence
            // when range is specified
            return true;
        }
        else {
            int count = args["count"].AsInteger();
            for ( int i = 0; i < count; ++i ) {
                try {
                    m_FFGenerator->Generate( bsh, *flatfile_os);
                }
                catch (CException& e) {
                    ERR_POST(Error << e);
                    m_Exception = true;
                }
            }

        }
    }
    return true;
}

CSeq_entry_Handle CAsn2FlatApp::ObtainSeqEntryFromSeqEntry(CObjectIStream& is, bool report)
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
    catch (CException& e) {
        if (report) {
            ERR_POST(Error << e);
        }
    }
    return CSeq_entry_Handle();
}

CSeq_entry_Handle CAsn2FlatApp::ObtainSeqEntryFromBioseq(CObjectIStream& is, bool report)
{
    try {
        CRef<CBioseq> bs(new CBioseq);
        is >> *bs;
        CBioseq_Handle bsh = m_Scope->AddBioseq(*bs);
        return bsh.GetTopLevelEntry();
    }
    catch (CException& e) {
        if (report) {
            ERR_POST(Error << e);
        }
    }
    return CSeq_entry_Handle();
}

CSeq_entry_Handle CAsn2FlatApp::ObtainSeqEntryFromBioseqSet(CObjectIStream& is, bool report)
{
    try {
        CRef<CSeq_entry> entry(new CSeq_entry);
        is >> entry->SetSet();
        return m_Scope->AddTopLevelSeqEntry(*entry);
    }
    catch (CException& e) {
        if (report) {
            ERR_POST(Error << e);
        }
    }
    return CSeq_entry_Handle();
}


bool CAsn2FlatApp::HandleSeqEntry(CRef<CSeq_entry>& se)
{
    if (!se) {
        return false;
    }

    // add entry to scope
    CSeq_entry_Handle entry = m_Scope->AddTopLevelSeqEntry(*se);
    if ( !entry ) {
        NCBI_THROW(CException, eUnknown, "Failed to insert entry to scope.");
    }

    bool ret = HandleSeqEntry(entry);
    // Needed because we can really accumulate a lot of junk otherwise,
    // and we end up with significant slowdown due to repeatedly doing
    // linear scans on a growing CScope.
    m_Scope->ResetDataAndHistory();
    return ret;
}

CObjectIStream* CAsn2FlatApp::x_OpenIStream(const CArgs& args)
{
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
    if ( args["i"] ) {
        pInputStream = new CNcbiIfstream( args["i"].AsString(), ios::binary  );
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


CFlatFileGenerator* CAsn2FlatApp::x_CreateFlatFileGenerator(const CArgs& args)
{
    CFlatFileConfig cfg;
    cfg.FromArguments(args);
    m_do_cleanup = ( ! args["nocleanup"]);
    cfg.BasicCleanup(false);

    if (args["html"])
    {
        CRef<IHTMLFormatter> html_fmt(new CHTMLFormatterEx(m_Scope));
        cfg.SetHTMLFormatter(html_fmt);
    }

    CRef<TGenbankBlockCallback> genbank_callback( x_GetGenbankCallback(args) );

    if( args["benchmark-cancel-checking"] ) {
        x_CreateCancelBenchmarkCallback();
    }

    //CFlatFileConfig cfg(
    //    format, mode, style, flags, view, gff_options, genbank_blocks,
    //    genbank_callback.GetPointerOrNull(), m_pCanceledCallback.get(),
    //    args["cleanup"] );
    CFlatFileGenerator* generator = new CFlatFileGenerator(cfg);

    // ID-5865 : SNP annotations must be explicitly added to the annot selector
    if (!m_PSGMode && cfg.ShowSNPFeatures()) {
        cfg.SetHideSNPFeatures(false);
        generator->SetAnnotSelector().IncludeNamedAnnotAccession("SNP");
    }

    return generator;
}

CAsn2FlatApp::TGenbankBlockCallback*
CAsn2FlatApp::x_GetGenbankCallback(const CArgs& args)
{
    class CSimpleCallback : public TGenbankBlockCallback {
    public:
        CSimpleCallback() { }
        ~CSimpleCallback() override
        {
            cerr << endl;
            cerr << "GENBANK CALLBACK DEMO STATISTICS" << endl;
            cerr << endl;
            cerr << "Appearances of each type: " << endl;
            cerr << endl;
            x_DumpTypeToCountMap(m_TypeAppearancesMap);
            cerr << endl;
            cerr << "Total characters output for each type: " << endl;
            cerr << endl;
            x_DumpTypeToCountMap(m_TypeCharsMap);
            cerr << endl;
            cerr << "Average characters output for each type: " << endl;
            cerr << endl;
            x_PrintAverageStats();
        }

        EBioseqSkip notify_bioseq( CBioseqContext & ctx ) override
        {
            cerr << "notify_bioseq called." << endl;
            return eBioseqSkip_No;
        }

        // this macro is the lesser evil compared to the messiness that
        // you would see here otherwise. (plus it's less error-prone)
#define SIMPLE_CALLBACK_NOTIFY(TItemClass) \
        EAction notify( string & block_text, \
                        const CBioseqContext& ctx, \
                        const TItemClass & item ) override { \
        NStr::ReplaceInPlace(block_text, "MODIFY TEST", "WAS MODIFIED TEST" ); \
        cerr << #TItemClass << " {" << block_text << '}' << endl; \
        ++m_TypeAppearancesMap[#TItemClass]; \
        ++m_TypeAppearancesMap["TOTAL"]; \
        m_TypeCharsMap[#TItemClass] += block_text.length(); \
        m_TypeCharsMap["TOTAL"] += block_text.length(); \
        EAction eAction = eAction_Default; \
        if( block_text.find("SKIP TEST")  != string::npos ) { \
            eAction = eAction_Skip; \
        } \
        if ( block_text.find("HALT TEST") != string::npos ) { \
            eAction = eAction_HaltFlatfileGeneration;         \
        } \
        return eAction; }

        SIMPLE_CALLBACK_NOTIFY(CStartSectionItem);
        SIMPLE_CALLBACK_NOTIFY(CHtmlAnchorItem);
        SIMPLE_CALLBACK_NOTIFY(CLocusItem);
        SIMPLE_CALLBACK_NOTIFY(CDeflineItem);
        SIMPLE_CALLBACK_NOTIFY(CAccessionItem);
        SIMPLE_CALLBACK_NOTIFY(CVersionItem);
        SIMPLE_CALLBACK_NOTIFY(CGenomeProjectItem);
        SIMPLE_CALLBACK_NOTIFY(CDBSourceItem);
        SIMPLE_CALLBACK_NOTIFY(CKeywordsItem);
        SIMPLE_CALLBACK_NOTIFY(CSegmentItem);
        SIMPLE_CALLBACK_NOTIFY(CSourceItem);
        SIMPLE_CALLBACK_NOTIFY(CReferenceItem);
        SIMPLE_CALLBACK_NOTIFY(CCommentItem);
        SIMPLE_CALLBACK_NOTIFY(CPrimaryItem);
        SIMPLE_CALLBACK_NOTIFY(CFeatHeaderItem);
        SIMPLE_CALLBACK_NOTIFY(CSourceFeatureItem);
        SIMPLE_CALLBACK_NOTIFY(CFeatureItem);
        SIMPLE_CALLBACK_NOTIFY(CGapItem);
        SIMPLE_CALLBACK_NOTIFY(CBaseCountItem);
        SIMPLE_CALLBACK_NOTIFY(COriginItem);
        SIMPLE_CALLBACK_NOTIFY(CSequenceItem);
        SIMPLE_CALLBACK_NOTIFY(CContigItem);
        SIMPLE_CALLBACK_NOTIFY(CWGSItem);
        SIMPLE_CALLBACK_NOTIFY(CTSAItem);
        SIMPLE_CALLBACK_NOTIFY(CEndSectionItem);
#undef SIMPLE_CALLBACK_NOTIFY

    private:
        typedef map<string, size_t> TTypeToCountMap;
        // for each type, how many instances of that type did we see?
        // We use the special string "TOTAL" for a total count.
        TTypeToCountMap m_TypeAppearancesMap;
        // Like m_TypeAppearancesMap but counts total number of *characters*
        // instead of number of items.  Again, there is
        // the special value "TOTAL"
        TTypeToCountMap m_TypeCharsMap;

        void x_DumpTypeToCountMap(const TTypeToCountMap & the_map ) {
            ITERATE( TTypeToCountMap, map_iter, the_map ) {
                cerr << setw(25) << left << (map_iter->first + ':')
                     << " " << map_iter->second << endl;
            }
        }

        void x_PrintAverageStats() {
            ITERATE( TTypeToCountMap, map_iter, m_TypeAppearancesMap ) {
                const string sType = map_iter->first;
                const size_t iAppearances = map_iter->second;
                const size_t iTotalCharacters = m_TypeCharsMap[sType];
                cerr << setw(25) << left << (sType + ':')
                     << " " << (iTotalCharacters / iAppearances) << endl;
            }
        }
    };

    if( args["demo-genbank-callback"] ) {
        return new CSimpleCallback;
    } else {
        return nullptr;
    }
}

TSeqPos CAsn2FlatApp::x_GetFrom(const CArgs& args)
{
    return args["from"] ?
        static_cast<TSeqPos>(args["from"].AsInteger() - 1) :
        CRange<TSeqPos>::GetWholeFrom();
}


TSeqPos CAsn2FlatApp::x_GetTo(const CArgs& args)
{
    return args["to"] ?
        static_cast<TSeqPos>(args["to"].AsInteger() - 1) :
        CRange<TSeqPos>::GetWholeTo();
}

ENa_strand CAsn2FlatApp::x_GetStrand(const CArgs& args)
{
    return static_cast<ENa_strand>(args["strand"].AsInteger());
}


void CAsn2FlatApp::x_GetLocation
(const CSeq_entry_Handle& entry,
 const CArgs& args,
 CSeq_loc& loc)
{
    _ASSERT(entry);

    CBioseq_Handle h = x_DeduceTarget(entry);
    if ( !h ) {
        NCBI_THROW(CFlatException, eInvalidParam,
            "Cannot deduce target bioseq.");
    }
    TSeqPos length = h.GetInst_Length();
    TSeqPos from   = x_GetFrom(args);
    TSeqPos to     = min(x_GetTo(args), length-1);
    ENa_strand strand = eNa_strand_unknown;
    if ( args["strand"] ) {
        strand = x_GetStrand(args);
    }

    if ( from == CRange<TSeqPos>::GetWholeFrom() && to == length ) {
        // whole
        loc.SetWhole().Assign(*h.GetSeqId());
    } else {
        // interval
        loc.SetInt().SetId().Assign(*h.GetSeqId());
        loc.SetInt().SetFrom(from);
        loc.SetInt().SetTo(to);
        if ( strand > 0 ) {
            loc.SetInt().SetStrand(strand);
        }
    }
}


// if the 'from' or 'to' flags are specified try to guess the bioseq.
CBioseq_Handle CAsn2FlatApp::x_DeduceTarget(const CSeq_entry_Handle& entry)
{
    if ( entry.IsSeq() ) {
        return entry.GetSeq();
    }

    _ASSERT(entry.IsSet());
    CBioseq_set_Handle bsst = entry.GetSet();
    if ( !bsst.IsSetClass() ) {
        NCBI_THROW(CFlatException, eInvalidParam,
            "Cannot deduce target bioseq.");
    }
    _ASSERT(bsst.IsSetClass());
    switch ( bsst.GetClass() ) {
    case CBioseq_set::eClass_nuc_prot:
        // return the nucleotide
        for ( CSeq_entry_CI it(entry); it; ++it ) {
            if ( it->IsSeq() ) {
                CBioseq_Handle h = it->GetSeq();
                if ( h && CSeq_inst::IsNa(h.GetInst_Mol()) ) {
                    return h;
                }
            }
        }
        break;
    case CBioseq_set::eClass_gen_prod_set:
        // return the genomic
        for ( CSeq_entry_CI it(bsst); it; ++it ) {
            if ( it->IsSeq() &&
                 it->GetSeq().GetInst_Mol() == CSeq_inst::eMol_dna ) {
                 return it->GetSeq();
            }
        }
        break;
    case CBioseq_set::eClass_segset:
        // return the segmented bioseq
        for ( CSeq_entry_CI it(bsst); it; ++it ) {
            if ( it->IsSeq() ) {
                return it->GetSeq();
            }
        }
        break;
    case CBioseq_set::eClass_genbank:
        {
            CBioseq_CI bi(bsst, CSeq_inst::eMol_na);
            if (bi) {
                return *bi;
            }
        }
        break;
    default:
        break;
    }
    NCBI_THROW(CFlatException, eInvalidParam,
            "Cannot deduce target bioseq.");
}

void
CAsn2FlatApp::x_CreateCancelBenchmarkCallback()
{
    // This ICanceled interface always says "keep going"
    // unless SIGUSR1 is received,
    // and it keeps statistics on how often it is checked
    class CCancelBenchmarking : public ICanceled {
    public:
        CCancelBenchmarking()
            : m_TimeOfLastCheck(CTime::eCurrent)
        {
        }


        ~CCancelBenchmarking()
        {
            // On destruction, we call "IsCanceled" one more time to make
            // sure there wasn't a point where we stopped
            // checking IsCanceled.
            IsCanceled();

            // print statistics
            cerr << "##### Cancel-checking benchmarks:" << endl;
            cerr << endl;

            // maybe cancelation was never checked:
            if( m_GapSizeMap.empty() ) {
                cerr << "NO DATA" << endl;
                return;
            }

            cerr << "(all times in milliseconds)" << endl;
            cerr << endl;
            // easy stats
            cerr << "Min: " << m_GapSizeMap.begin()->first << endl;
            cerr << "Max: " << m_GapSizeMap.rbegin()->first << endl;

            // find average
            Int8   iTotalMsecs = 0;
            size_t uTotalCalls = 0;
            ITERATE( TGapSizeMap, gap_size_it, m_GapSizeMap ) {
                iTotalMsecs += gap_size_it->first * gap_size_it->second;
                uTotalCalls += gap_size_it->second;
            }
            cerr << "Avg: " << (iTotalMsecs / uTotalCalls) << endl;

            // find median
            const size_t uIdxWithMedian = (uTotalCalls / 2);
            size_t uCurrentIdx = 0;
            ITERATE( TGapSizeMap, gap_size_it, m_GapSizeMap ) {
                uCurrentIdx += gap_size_it->second;
                if( uCurrentIdx >= uIdxWithMedian ) {
                    cerr << "Median: " << gap_size_it->first << endl;
                    break;
                }
            }

            // first few
            cerr << "Chronologically first few check times: " << endl;
            copy( m_FirstFewGaps.begin(),
                m_FirstFewGaps.end(),
                ostream_iterator<Int8>(cerr, "\n") );

            // last few
            cerr << "Chronologically last few check times: " << endl;
            copy( m_LastFewGaps.begin(),
                m_LastFewGaps.end(),
                ostream_iterator<Int8>(cerr, "\n") );

            // slowest few and fastest few
            cerr << "Frequency distribution of slowest and fastest lookup times: " << endl;
            cerr << "\t" << "Time" << "\t" << "Count" << endl;
            if( m_GapSizeMap.size() <= (2 * x_kGetNumToSaveAtStartAndEnd()) ) {
                // if small enough, show the whole table at once
                ITERATE( TGapSizeMap, gap_size_it, m_GapSizeMap ) {
                    cerr << "\t" << gap_size_it ->first << "\t" << gap_size_it->second << endl;
                }
            } else {
                // table is big so only show the first few and the last few
                TGapSizeMap::const_iterator gap_size_it = m_GapSizeMap.begin();
                ITERATE_SIMPLE(x_kGetNumToSaveAtStartAndEnd()) {
                    cerr << "\t" << gap_size_it ->first << "\t" << gap_size_it->second << endl;
                    ++gap_size_it;
                }

                cout << "\t...\t..." << endl;

                TGapSizeMap::reverse_iterator gap_size_rit = m_GapSizeMap.rbegin();
                ITERATE_SIMPLE(x_kGetNumToSaveAtStartAndEnd()) {
                    cerr << "\t" << gap_size_it ->first << "\t" << gap_size_it->second << endl;
                    ++gap_size_rit;
                }
            }

            // total checks
            cerr << "Num checks: " << uTotalCalls << endl;
        }


        bool IsCanceled() const override
        {
            // getting the current time should be the first
            // command in this function.
            CTime timeOfThisCheck(CTime::eCurrent);

            const double dDiffInNsecs =
                timeOfThisCheck.DiffNanoSecond(m_TimeOfLastCheck);
            const Int8 iDiffInMsecs = static_cast<Int8>(dDiffInNsecs / 1000000.0);

            ++m_GapSizeMap[iDiffInMsecs];

            if( m_FirstFewGaps.size() < x_kGetNumToSaveAtStartAndEnd() ) {
                m_FirstFewGaps.push_back(iDiffInMsecs);
            }

            m_LastFewGaps.push_back(iDiffInMsecs);
            if( m_LastFewGaps.size() > x_kGetNumToSaveAtStartAndEnd() ) {
                m_LastFewGaps.pop_front();
            }

            const bool bIsCanceled =
                CSignal::IsSignaled(CSignal::eSignal_USR1);
            if( bIsCanceled ) {
                cerr << "Canceled by SIGUSR1" << endl;
            }

            // resetting m_TimeOfLastCheck should be the last command
            // in this function
            m_TimeOfLastCheck.SetCurrent();
            return bIsCanceled;
        }

    private:
        // local classes do not allow static fields
        size_t x_kGetNumToSaveAtStartAndEnd() const { return 10; }

        mutable CTime m_TimeOfLastCheck;
        // The key is the gap between checks in milliseconds,
        // (which is more than enough resolution for a human-level action)
        // and the value is the number of times a gap of that size occurred.
        typedef map<Int8, size_t> TGapSizeMap;
        mutable TGapSizeMap m_GapSizeMap;

        mutable vector<Int8> m_FirstFewGaps;
        mutable list<Int8>   m_LastFewGaps;
    };

    m_pCanceledCallback.reset( new CCancelBenchmarking );
}

int CAsn2FlatApp::x_AddSNPAnnots(CBioseq_Handle& bsh)
{
    int rc = 0;

    // SNP annotations can be available only for nucleotide human RefSeq records
    if (bsh.GetInst_Mol() == CSeq_inst::eMol_aa ||
        sequence::GetTaxId(bsh) != TAX_ID_CONST(9606))
        return 0;

    // Also skip large scaffolds and chromosomes
    CConstRef<CSeq_id> accid =
        FindBestChoice(bsh.GetBioseqCore()->GetId(), CSeq_id::Score);

    bool skip = (accid->Which() != CSeq_id::e_Other);
    if (!skip) {
        string acc;
        accid->GetLabel(&acc, CSeq_id::eContent);
        string acc_prefix = acc.substr(0,2);
        if (acc_prefix == "NC" || acc_prefix == "AC" ||
            acc_prefix == "NT" || acc_prefix == "NW") {
            skip = true;
        }
    }
    if (skip)
        return 0;

    // If GenBank loader is connecting to PubSeqOS, it's sufficient to add the 'SNP'
    // named annot type to the scope.
    // Otherwise (in PSG mode), use a separate SNP data loader. For that to work,
    // it is necessary to find the actual NA accession corresponding to this record's
    // SNP annotation and add it to the SAnnotSelector used by the flatfile generator.
#ifdef USE_SNPLOADER
    TGi gi = FindGi(bsh.GetBioseqCore()->GetId());
    if (gi > ZERO_GI) {
        ncbi::grpcapi::dbsnp::primary_track::SeqIdRequestStringAccverUnion request;
        request.set_gi(GI_TO(::google::protobuf::uint64, gi));
        ncbi::grpcapi::dbsnp::primary_track::PrimaryTrackReply reply;
        CGRPCClientContext context;
        auto snp_status = m_SNPTrackStub->ForSeqId(&context, request, &reply);
        if (snp_status.ok()) {
            string na_acc = reply.na_track_acc_with_filter();
            if (!na_acc.empty())
                m_FFGenerator->SetAnnotSelector().IncludeNamedAnnotAccession(na_acc);
        }
    }
#endif

    return rc;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//
// Main

int main(int argc, const char** argv)
{
    CScope::SetDefaultKeepExternalAnnotsForEdit(true);
    return CAsn2FlatApp().AppMain(argc, argv);
}

