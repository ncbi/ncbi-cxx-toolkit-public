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
#include <corelib/ncbistr.hpp>
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
#include <objects/seq/seq_loc_from_string.hpp>

#include <objects/misc/sequence_macros.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_html.hpp>
#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objects/seqset/gb_release_file.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

#include <objmgr/util/objutil.hpp>

#include <misc/data_loaders_util/data_loaders_util.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/edit/huge_file_process.hpp>
#include <objtools/edit/huge_asn_reader.hpp>
#include <future>
#include "fileset.hpp"
#include <objtools/writers/atomics.hpp>

#define USE_THREAD_POOL1

#ifdef USE_THREAD_POOL
#include "threadpool.hpp"
#endif


#define USE_CDDLOADER

#if defined(HAVE_LIBGRPC) && defined(HAVE_NCBI_VDB)
#    define USE_SNPLOADER
#endif

#ifdef USE_SNPLOADER
// ID-5865 : For SNP retrieval in PSG mode via SNP data loader
#    include <misc/grpc_integration/grpc_integration.hpp>
#    include <grpc++/grpc++.h>
#    include <objects/dbsnp/primary_track/dbsnp.grpc.pb.h>
#    include <objects/dbsnp/primary_track/dbsnp.pb.h>
#endif

#ifdef USE_CDDLOADER
#    include <sra/data_loaders/snp/snploader.hpp>
#    include <objtools/data_loaders/cdd/cdd_loader/cdd_loader.hpp>
#endif

// For command-line app, the URL paths need to be absolute
#define NCBI_URL_BASE "https://www.ncbi.nlm.nih.gov"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


// all sequence output stream
// nucleotide output stream
// genomic output stream
// RNA output stream
// protein output stream
// unknown output stream
enum class eFlatFileCodes { all, nuc, gen, rna, prot, unk, };

using CFFMultiSourceFileSet = TMultiSourceFileSet<eFlatFileCodes,
    eFlatFileCodes::all,
    eFlatFileCodes::nuc,
    eFlatFileCodes::gen,
    eFlatFileCodes::rna,
    eFlatFileCodes::prot,
    eFlatFileCodes::unk>;

class CWrapINSDSet;

class CAsn2FlatApp : public CNcbiApplication
{
public:
    CAsn2FlatApp();
    ~CAsn2FlatApp();

    void Init() override;
    int  Run() override;
    bool WrapINSDSet(bool unwrap);

    // Each thread should have its own context
    struct TFFContext {
        CCleanup                            m_cleanup;
        CRef<CScope>                        m_Scope;
        CRef<CFlatFileGenerator>            m_FFGenerator; // Flat-file generator
        CFFMultiSourceFileSet::fileset_type m_streams;     // multiple streams for each of eFlatFileCodes
    };

    using fileset_type = decltype(TFFContext::m_streams);

protected:
    bool SetFlatfileOstream(eFlatFileCodes _code, const string& name);

private:
    // types
    typedef CFlatFileConfig::CGenbankBlockCallback TGenbankBlockCallback;

    using TThreadStatePool = TResourcePool<TFFContext>;

    bool HandleSeqEntry(TFFContext& context, CRef<CSeq_entry> se) const;
    bool HandleSeqEntryHandle(TFFContext& context, CSeq_entry_Handle seh) const;
    bool HandleSeqSubmit(TFFContext& context, CObjectIStream& is) const;
    bool HandleSeqSubmit(TFFContext& context, CSeq_submit& sub) const;
    void HandleTextId(TFFContext& context, const string& id) const;
    bool HandleSeqId(TFFContext& context, const edit::CHugeAsnReader* reader, CConstRef<CSeq_id> seqid) const;

    CSeq_entry_Handle ObtainSeqEntryFromSeqEntry(TFFContext& context, CObjectIStream& is, bool report) const;
    CSeq_entry_Handle ObtainSeqEntryFromBioseq(TFFContext& context, CObjectIStream& is, bool report) const;
    CSeq_entry_Handle ObtainSeqEntryFromBioseqSet(TFFContext& context, CObjectIStream& is, bool report) const;

    [[nodiscard]] unique_ptr<CObjectIStream> x_OpenIStream(const CArgs& args) const;

    void             x_CreateFlatFileGenerator(TFFContext& context, const CArgs& args) const;
    TSeqPos          x_GetFrom(const CArgs& args) const;
    TSeqPos          x_GetTo(const CArgs& args) const;
    ENa_strand       x_GetStrand(const CArgs& args) const;
    void             x_GetLocation(const CSeq_entry_Handle& entry, const CArgs& args, CSeq_loc& loc) const;
    CBioseq_Handle   x_DeduceTarget(const CSeq_entry_Handle& entry) const;
    int              x_AddSNPAnnots(CBioseq_Handle& bsh, TFFContext& context) const;
    void             x_FFGenerate(CSeq_entry_Handle seh, TFFContext& context) const;
    int              x_GenerateBatchMode(unique_ptr<CObjectIStream> is);
    int              x_GenerateTraditionally(unique_ptr<CObjectIStream> is, TFFContext& context, const CArgs& args) const;
    int              x_GenerateHugeMode();
    template<typename _TMethod, typename... TArgs>
    bool             x_OneShot(_TMethod method, TArgs&& ... args);
    template<typename _TMethod, typename... TArgs>
    static void      x_OneShotMethod(_TMethod method, const CAsn2FlatApp* app, CAsn2FlatApp::TThreadStatePool::TUniqPointer thread_state, TArgs ... args);

    void             x_InitNewContext(TFFContext& context);
    void             x_ResetContext(TFFContext& context);

    //[[nodiscard]] TGenbankBlockCallback* x_GetGenbankCallback(const CArgs& args) const;
    //[[nodiscard]] ICanceled*             x_CreateCancelBenchmarkCallback() const;

    // data
    CRef<CObjectManager>      m_Objmgr; // Object Manager can be mutable
    TThreadStatePool          m_state_pool;
#ifdef USE_THREAD_POOL
    TThreadPool               m_thread_pool;
#endif

    // Everything else should be unchangeable within CFlatfileGenerator runs

    bool m_OnlyNucs;
    bool m_OnlyProts;
    bool                      m_use_mt{false};
    CFFMultiSourceFileSet     m_writers;
    unique_ptr<ICanceled>     m_pCanceledCallback;
    bool                      m_do_cleanup;
    bool                      m_do_html;
    mutable std::atomic<bool> m_Exception;
    bool                      m_FetchFail;
    bool                      m_PSGMode;
    bool                      m_HugeFileMode;
    string                    m_AccessionFilter;
    mutable std::atomic<bool> m_stopit{false};
    edit::CHugeFileProcess    m_huge_process;

#ifdef USE_SNPLOADER
    CRef<CSNPDataLoader>      m_SNPDataLoader;
    unique_ptr<ncbi::grpcapi::dbsnp::primary_track::DbSnpPrimaryTrack::Stub> m_SNPTrackStub;
#endif
#ifdef USE_CDDLOADER
    CRef<CCDDDataLoader>      m_CDDDataLoader;
#endif
};

// only print <INSDSet> ... </INSDSet> wrappers if single output stream
class CWrapINSDSet
{
public:
    CWrapINSDSet(CAsn2FlatApp* app)
    {
        if (app)
            if (app->WrapINSDSet(false))
                m_app = app;
    }
    ~CWrapINSDSet()
    {
        if (m_app)
            m_app->WrapINSDSet(true);
    }

private:
    CAsn2FlatApp* m_app = nullptr;
};

// constructor
CAsn2FlatApp::CAsn2FlatApp()
{
    const CVersionInfo vers(6, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY);
    SetVersion(vers);
}

// destructor
CAsn2FlatApp::~CAsn2FlatApp()
{
}

void CAsn2FlatApp::Init()
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext("", "Convert an ASN.1 Seq-entry into a flat report");

    // input
    {
        arg_desc->SetCurrentGroup("Input/Output Options");
        // name
        arg_desc->AddOptionalKey("i", "InputFile", "Input file name", CArgDescriptions::eInputFile);

        // input file serial format (AsnText\AsnBinary\XML, default: AsnText)
        arg_desc->AddOptionalKey("serial", "SerialFormat", "Input file format", CArgDescriptions::eString);
        arg_desc->SetConstraint("serial",
                                &(*new CArgAllow_Strings, "text", "binary", "XML"));
        arg_desc->AddFlag("sub", "Submission");
        // id
        arg_desc->AddOptionalKey("id", "ID", "Specific ID to display", CArgDescriptions::eString);
        arg_desc->AddOptionalKey("ids", "IDFile", "FIle of IDs to display, one per line", CArgDescriptions::eInputFile);
        // accn
        arg_desc->AddOptionalKey("accn", "AccnFilter", "Limit to specific accession", CArgDescriptions::eString);

        // input type:
        arg_desc->AddDefaultKey("type", "AsnType", "ASN.1 object type", CArgDescriptions::eString, "any");
        arg_desc->SetConstraint("type",
                                &(*new CArgAllow_Strings, "any", "seq-entry", "bioseq", "bioseq-set", "seq-submit"));

        // single output name
        arg_desc->AddOptionalKey("o", "SingleOutputFile", "Single output file name", CArgDescriptions::eOutputFile);

        // file names
        arg_desc->AddOptionalKey("on", "NucleotideOutputFile", "Nucleotide output file name", CArgDescriptions::eOutputFile);
        arg_desc->SetDependency("on", CArgDescriptions::eExcludes, "o");

        arg_desc->AddOptionalKey("og", "GenomicOutputFile", "Genomic output file name", CArgDescriptions::eOutputFile);
        arg_desc->SetDependency("og", CArgDescriptions::eExcludes, "o");
        arg_desc->SetDependency("og", CArgDescriptions::eExcludes, "on");

        arg_desc->AddOptionalKey("or", "RNAOutputFile", "RNA output file name", CArgDescriptions::eOutputFile);
        arg_desc->SetDependency("or", CArgDescriptions::eExcludes, "o");
        arg_desc->SetDependency("or", CArgDescriptions::eExcludes, "on");

        arg_desc->AddOptionalKey("op", "ProteinOutputFile", "Protein output file name", CArgDescriptions::eOutputFile);
        arg_desc->SetDependency("op", CArgDescriptions::eExcludes, "o");

        arg_desc->AddOptionalKey("ou", "UnknownOutputFile", "Unknown output file name", CArgDescriptions::eOutputFile);
        arg_desc->SetDependency("ou", CArgDescriptions::eExcludes, "o");
    }

    // batch processing
    {
        arg_desc->SetCurrentGroup("Batch Processing Options");
        arg_desc->AddFlag("batch", "Process NCBI release file");
        // compression
        arg_desc->AddFlag("c", "Compressed file");
        // propogate top descriptors
        arg_desc->AddFlag("p", "Propagate top descriptors");
    }

    // in flat_file_config.cpp
    CFlatFileConfig::AddArgumentDescriptions(*arg_desc);

    // debugging options
    {
        arg_desc->SetCurrentGroup("Debugging Options - Subject to change or removal without warning");

        arg_desc->AddFlag("huge", "Use Huge files mode");
        arg_desc->AddFlag("disable-huge", "Explicitly disable huge-files mode");
        arg_desc->SetDependency("disable-huge",
                                CArgDescriptions::eExcludes,
                                "huge");
        arg_desc->AddFlag("use_mt", "Use multiple threads when possible");

#if 0
        // benchmark cancel-checking
        arg_desc->AddFlag(
            "benchmark-cancel-checking",
            "Check statistics on how often the flatfile generator checks if "
            "it should be canceled.  This also sets up SIGUSR1 to trigger "
            "cancellation.");
#endif
    }

    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc);

    arg_desc->SetCurrentGroup(kEmptyStr);
    SetupArgDescriptions(arg_desc.release());
}


bool CAsn2FlatApp::SetFlatfileOstream(eFlatFileCodes _code, const string& name)
{
    const CArgs& args = GetArgs();
    if (args[name]) {
        auto filename = args[name].AsString();
        m_writers.Open(_code, filename);
        return true;
    } else
        return false;
}

int CAsn2FlatApp::Run()
{
    CTime expires = GetFullVersion().GetBuildInfo().GetBuildTime();
    if (! expires.IsEmpty()) {
        expires.AddYear();
        if (CTime(CTime::eCurrent) > expires) {
            NcbiCerr << "This copy of " << GetProgramDisplayName()
                     << " is more than 1 year old. Please download the current version if it is newer." << endl;
        }
    }

    m_Exception    = false;
    m_FetchFail    = false;
    m_HugeFileMode = false;
    m_AccessionFilter.clear();

    // initialize conn library
    CONNECT_Init(&GetConfig());

    const CArgs& args = GetArgs();

    CSignal::TrapSignals(CSignal::eSignal_USR1);

    // create object manager
    m_Objmgr = CObjectManager::GetInstance();
    if (! m_Objmgr) {
        NCBI_THROW(CException, eUnknown, "Could not create object manager");
    }
    if (args["gbload"] || args["id"] || args["ids"]) {
        static const CDataLoadersUtil::TLoaders default_loaders =
            CDataLoadersUtil::fGenbank | CDataLoadersUtil::fVDB | CDataLoadersUtil::fSRA;
        CDataLoadersUtil::SetupObjectManager(args, *m_Objmgr, default_loaders);

        if ((args["enable-external"] && ! args["no-external"]) || args["policy"].AsString() == "external") {
            CGBDataLoader* gb_loader = dynamic_cast<CGBDataLoader*>(CObjectManager::GetInstance()->FindDataLoader("GBLOADER"));
            if (gb_loader) {
                // needed to find remote features when reading local ASN.1 file
                gb_loader->CGBDataLoader::SetAlwaysLoadExternal(true);
            }
        }
    }

    //const CNcbiRegistry& cfg = CNcbiApplication::Instance()->GetConfig();
    const CNcbiRegistry& cfg = GetConfig();
    m_PSGMode = cfg.GetBool("genbank", "loader_psg", false, 0, CNcbiRegistry::eReturn);
    if (m_PSGMode) {
#ifdef USE_SNPLOADER
        string host     = cfg.GetString("SNPAccess", "host", "");
        string port     = cfg.GetString("SNPAccess", "port", "");
        string hostport = host + ":" + port;

        auto channel =
            grpc::CreateChannel(hostport, grpc::InsecureChannelCredentials());
        m_SNPTrackStub =
            ncbi::grpcapi::dbsnp::primary_track::DbSnpPrimaryTrack::NewStub(channel);
        m_SNPDataLoader.Reset(CSNPDataLoader::RegisterInObjectManager(*m_Objmgr).GetLoader());
#endif
#ifdef USE_CDDLOADER
        bool use_mongo_cdd =
            cfg.GetBool("genbank", "vdb_cdd", false, 0, CNcbiRegistry::eReturn) &&
            cfg.GetBool("genbank", "always_load_external", false, 0, CNcbiRegistry::eReturn);
        if (use_mongo_cdd) {
            m_CDDDataLoader.Reset(CCDDDataLoader::RegisterInObjectManager(*m_Objmgr).GetLoader());
        }
#endif
    }

    m_state_pool.SetReserved(20);
    m_state_pool.SetInitFunc(
        [this](TFFContext& ctx) {
            x_InitNewContext(ctx);
        },
        [this](TFFContext& ctx) {
            x_ResetContext(ctx);
        });

    m_HugeFileMode = ! args["disable-huge"] && (args["huge"] || cfg.GetBool("asn2flat", "UseHugeFiles", false));

    if (m_HugeFileMode && ! args["i"]) {
        NcbiCerr << "Use of -huge mode also requires use of the -i argument. Disabling -huge mode." << endl;
        m_HugeFileMode = false;
    }
    if (m_HugeFileMode && args["i"].AsString() == "/dev/stdin") {
        NcbiCerr << "Use of -huge mode is incompatible with -i /dev/stdin. Disabling -huge mode." << endl;
        m_HugeFileMode = false;
    }
    if (m_HugeFileMode && args["batch"]) {
        NcbiCerr << "Use of -huge cannot be combined with -batch. Disabling -huge mode." << endl;
        m_HugeFileMode = false;
    }
    if (m_HugeFileMode && args["c"]) {
        NcbiCerr << "Use of -huge cannot be combined with -c. Disabling -huge mode." << endl;
        m_HugeFileMode = false;
    }

    m_use_mt = args["use_mt"] && (args["batch"] || m_HugeFileMode);

    m_writers.SetUseMT(m_use_mt);
    m_writers.SetDepth(20);

    // open the output streams
    bool has_o_arg  = SetFlatfileOstream(eFlatFileCodes::all, "o");
    bool has_on_arg = SetFlatfileOstream(eFlatFileCodes::nuc, "on");
    bool has_og_arg = SetFlatfileOstream(eFlatFileCodes::gen, "og");
    bool has_or_arg = SetFlatfileOstream(eFlatFileCodes::rna, "or");
    bool has_op_arg = SetFlatfileOstream(eFlatFileCodes::prot, "op");
    bool has_ou_arg = SetFlatfileOstream(eFlatFileCodes::unk, "ou");

    bool has_o_args = has_o_arg || has_on_arg || has_og_arg || has_or_arg || has_op_arg || has_ou_arg;
    if (! has_o_args) {
        // No output (-o*) argument given - default to stdout
        m_writers.Open(eFlatFileCodes::all, std::cout);
    }

    m_do_cleanup = ! args["nocleanup"];
    m_do_html = args["html"];
    m_OnlyNucs   = false;
    m_OnlyProts  = false;
    if (args["o"]) {
        auto view   = args["view"].AsString();
        m_OnlyNucs  = (view == "nuc");
        m_OnlyProts = (view == "prot");
    }

    if (args["accn"]) {
        m_AccessionFilter = args["accn"].AsString();
    }

    CWrapINSDSet wrap(this);

    if (args["id"]) {
        auto        thread_state = m_state_pool.Allocate(); // this can block and wait if too many writers already allocated
        TFFContext& context      = *thread_state;
        HandleTextId(context, args["id"].AsString());
        if (m_Exception)
            return -1;
        return 0;
    }

    if (args["ids"]) {
        auto        thread_state = m_state_pool.Allocate(); // this can block and wait if too many writers already allocated
        TFFContext& context      = *thread_state;

        CNcbiIstream& istr = args["ids"].AsInputFile();
        string        id_str;
        while (NcbiGetlineEOL(istr, id_str)) {
            id_str = NStr::TruncateSpaces(id_str);
            if (id_str.empty() || id_str[0] == '#') {
                continue;
            }

            try {
                LOG_POST(Error << "id = " << id_str);
                HandleTextId(context, id_str);
            } catch (CException& e) {
                ERR_POST(Error << e);
            }
        }
        if (m_Exception)
            return -1;
        return 0;
    }

    if (args["i"]) {
        m_huge_process.OpenFile(args["i"].AsString(), args["c"] ? nullptr : &edit::CHugeFileProcess::g_supported_types);
    }

    if (m_HugeFileMode) {
        m_huge_process.OpenReader();
        return x_GenerateHugeMode();
    }

    // only uncompressed files go this faster shortcut
    if (args["i"] && args["batch"] && !args["c"] &&
        m_huge_process.GetFile().IsOpen() &&
        (m_huge_process.GetFile().m_serial_format == eSerial_AsnBinary ||
         m_huge_process.GetFile().m_serial_format == eSerial_AsnText)
    ) {
        auto content = m_huge_process.GetFile().RecognizeContent(0);
        if (content == CBioseq_set::GetTypeInfo()) {
            auto istr = m_huge_process.GetFile().MakeObjStream();
            return x_GenerateBatchMode(std::move(istr));
        }
    }

    // m_huge_process.GetFile() would be still used if it is already open
    unique_ptr<CObjectIStream> is = x_OpenIStream(args);
    if (! is) {
        string msg = args["i"] ? "Unable to open input file" + args["i"].AsString() : "Unable to read data from stdin";
        NCBI_THROW(CException, eUnknown, msg);
    }

    // traditional way of opening batch mode
    if (args["batch"]) {
        // batch mode can do multi-threaded, so it has it's own TFFContext(s)
        return x_GenerateBatchMode(std::move(is));
    }

    // temporaly, only one context exists
    auto        thread_state = m_state_pool.Allocate(); // this can block and wait if too many writers already allocated
    TFFContext& context      = *thread_state;

    if (args["sub"]) {
        HandleSeqSubmit(context, *is);
        if (m_Exception)
            return -1;
        return 0;
    }

    return x_GenerateTraditionally(std::move(is), context, args);
}

template<typename _TMethod, typename... TArgs>
void CAsn2FlatApp::x_OneShotMethod(_TMethod method, const CAsn2FlatApp* app, CAsn2FlatApp::TThreadStatePool::TUniqPointer thread_state, TArgs ... args)
{ // this could called from main or processing threads
    if (app->m_stopit)
        return;

    CAsn2FlatApp::TFFContext& context = *thread_state;

    auto success = method(app, context, std::forward<TArgs>(args)...);
    if (!success) app->m_stopit = true;
}

template<typename _TMethod, typename... TArgs>
bool CAsn2FlatApp::x_OneShot(_TMethod method, TArgs&& ... args)
{
    static_assert(std::is_member_function_pointer<_TMethod>::value,
        "CAsn2FlatApp::x_OneShot should be used with class member function pointers");

    if (m_stopit) // faster exit if other threads already failed
        return false;

    // each thread needs to have it's own context
    // writers queue control how many of them can run simultaneosly
    // these resources must allocated sequentially in order of incoming entry's
    auto thread_state = m_state_pool.Allocate(); // this can block and wait if too many writers already allocated

    // method is just a pointer to a member function, need to make some calleable out of it
    auto member = std::mem_fn(method);
    // just make an instantiated std::function
    auto calleable = x_OneShotMethod<decltype(member), std::decay_t<TArgs>...>;

    if (m_use_mt) {
#ifdef USE_THREAD_POOL
        m_thread_pool.OneShot(calleable, member, this, std::move(thread_state), std::forward<TArgs>(args)...);
#else
        std::thread(calleable, member, this, std::move(thread_state), std::forward<TArgs>(args)...).detach();
#endif
    } else {
        calleable(member, this, std::move(thread_state), std::forward<TArgs>(args)...);
        if (m_stopit)
            return false;
    }

    return true;
}

bool CAsn2FlatApp::HandleSeqId(TFFContext& context, const edit::CHugeAsnReader* reader, CConstRef<CSeq_id> seqid) const
{
    if (reader && seqid) {
        auto entry = reader->LoadSeqEntry(seqid);
        if (!entry) {
            return false;
        }
        if (auto pSubmitBlock = reader->GetSubmitBlock(); pSubmitBlock) {
            auto pSeqSubmit = Ref(new CSeq_submit());
            pSeqSubmit->SetSub().Assign(*pSubmitBlock);
            pSeqSubmit->SetData().SetEntrys().push_back(entry);
            return HandleSeqSubmit(context, *pSeqSubmit);
        }
        else {
            return HandleSeqEntry(context, entry);
        }
    }
    return false;
}

int CAsn2FlatApp::x_GenerateBatchMode(unique_ptr<CObjectIStream> is)
{
    bool propagate = GetArgs()[ "p" ];
    CGBReleaseFile in( *is.release(), propagate ); // CGBReleaseFile will delete the input stream

    // for multi-threading processing TFFContext instance is associated with thread
    // thread can run detached, entry and context will be associated with the thread
    in.RegisterHandler( [this] (CRef<CSeq_entry>& entry)->bool
    {
        return x_OneShot(&CAsn2FlatApp::HandleSeqEntry, std::move(entry));
    });

    in.Read(); // registered handlers will be called from this function

    m_writers.FlushAll(); // this will wait until all writers quit

    if (m_Exception) return -1;
    return 0;
}

int CAsn2FlatApp::x_GenerateHugeMode()
{
    CRef<CSeq_id>          seqid;
    if (! m_AccessionFilter.empty()) {
        CBioseq::TId ids;
        CSeq_id::ParseIDs(ids, m_AccessionFilter);
        if (! ids.empty()) {
            seqid = ids.front();
        }
    }

    bool all_success = m_huge_process.Read([this, seqid](edit::CHugeAsnReader* reader, const std::list<CConstRef<CSeq_id>>& idlist)
    {
        bool success = true;
        if (seqid) {
            success = x_OneShot(&CAsn2FlatApp::HandleSeqId, reader, seqid);
        }
        else {
            for (auto id: idlist) {
                if (!x_OneShot(&CAsn2FlatApp::HandleSeqId, reader, id)) {
                    success = false;
                    break;
                }
            }
        }
        m_writers.FlushAll(); // this will wait until all writers quit
        return success;
    });

    if (! all_success || m_Exception)
        return -1;
    return 0;
}

int CAsn2FlatApp::x_GenerateTraditionally(unique_ptr<CObjectIStream> is, TFFContext& context, const CArgs& args) const
{
    TTypeInfo asn_info = nullptr;
    string asn_type = args["type"].AsString();

    if (m_huge_process.GetConstFile().IsOpen()) {
        asn_info = m_huge_process.GetConstFile().m_content;
    }

    if (asn_info == nullptr)
    { // if we failed to recognize content type let's take user's instruction which is otherwise ignored
        if (asn_type == "seq-entry") {
            asn_info = CSeq_entry::GetTypeInfo();
        } else if (asn_type == "bioseq") {
            asn_info = CBioseq::GetTypeInfo();
        } else if (asn_type == "bioseq-set") {
            asn_info = CBioseq_set::GetTypeInfo();
        } else if (asn_type == "seq-submit") {
            asn_info = CSeq_submit::GetTypeInfo();
        }
    }

    if (asn_info == CSeq_entry::GetTypeInfo()) {
        //
        //  Straight through processing: Read a seq_entry, then process
        //  a seq_entry:
        //
        while (! is->EndOfData()) {
            CSeq_entry_Handle seh = ObtainSeqEntryFromSeqEntry(context, *is, true);
            if (! seh) {
                NCBI_THROW(CException, eUnknown, "Unable to construct Seq-entry object");
            }
            HandleSeqEntryHandle(context, seh);
            context.m_Scope->RemoveTopLevelSeqEntry(seh);
        }
    } else if (asn_info == CBioseq::GetTypeInfo()) {
        //
        //  Read object as a bioseq, wrap it into a seq_entry, then process
        //  the wrapped bioseq as a seq_entry:
        //
        while (! is->EndOfData()) {
            CSeq_entry_Handle seh = ObtainSeqEntryFromBioseq(context, *is, true);
            if (! seh) {
                NCBI_THROW(CException, eUnknown, "Unable to construct Seq-entry object");
            }
            HandleSeqEntryHandle(context, seh);
            context.m_Scope->RemoveTopLevelSeqEntry(seh);
        }
    } else if (asn_info == CBioseq_set::GetTypeInfo()) {
        //
        //  Read object as a bioseq_set, wrap it into a seq_entry, then
        //  process the wrapped bioseq_set as a seq_entry:
        //
        while (! is->EndOfData()) {
            CSeq_entry_Handle seh = ObtainSeqEntryFromBioseqSet(context, *is, true);
            if (! seh) {
                NCBI_THROW(CException, eUnknown, "Unable to construct Seq-entry object");
            }
            HandleSeqEntryHandle(context, seh);
            context.m_Scope->RemoveTopLevelSeqEntry(seh);
        }
    } else if (asn_info == CSeq_submit::GetTypeInfo()) {
        while (! is->EndOfData()) {
            HandleSeqSubmit(context, *is);
        }
    } else if (asn_type == "any") {
        //
        //  Try the first four in turn:
        //
        while (! is->EndOfData()) {
            string strNextTypeName = is->PeekNextTypeName();

            CSeq_entry_Handle seh = ObtainSeqEntryFromSeqEntry(context, *is, false);
            if (! seh) {
                is->Close();
                is = x_OpenIStream(args);
                seh = ObtainSeqEntryFromBioseqSet(context, *is, false);
                if (! seh) {
                    is->Close();
                    is = x_OpenIStream(args);
                    seh = ObtainSeqEntryFromBioseq(context, *is, false);
                    if (! seh) {
                        is->Close();
                        is = x_OpenIStream(args);
                        CRef<CSeq_submit> sub(new CSeq_submit);
                        *is >> *sub;
                        if (sub->IsSetSub() && sub->IsSetData()) {
                            HandleSeqSubmit(context, *sub);
                            if (m_Exception)
                                return -1;
                            return 0;
                        } else {
                            NCBI_THROW(
                                CException, eUnknown, "Unable to construct Seq-entry object");
                        }
                    }
                }
            }
            HandleSeqEntryHandle(context, seh);
            context.m_Scope->RemoveTopLevelSeqEntry(seh);
        }
    }

    if (m_Exception)
        return -1;
    return 0;
}


bool CAsn2FlatApp::HandleSeqSubmit(TFFContext& context, CSeq_submit& sub) const
{
    if (! sub.IsSetSub() || ! sub.IsSetData() || ! sub.GetData().IsEntrys() || sub.GetData().GetEntrys().empty()) {
        return false;
    }

    if (m_do_cleanup && ! m_do_html) {
        Uint4 options = CCleanup::eClean_ForFlatfile;
        context.m_cleanup.BasicCleanup(sub, options);
    }
    // NB: though the spec specifies a submission may contain multiple entries
    // this is not the case. A submission should only have a single Top-level
    // Seq-entry
    CConstRef<CSeq_entry> e(sub.GetData().GetEntrys().front());
    CSeq_entry_Handle     seh;
    try {
        seh = context.m_Scope->GetSeq_entryHandle(*e);
    } catch (CException&) {
    }

    if (! seh) { // add to scope if not already in it
        seh = context.m_Scope->AddTopLevelSeqEntry(*e);
    }
    // "remember" the submission block
    context.m_FFGenerator->SetSubmit(sub.GetSub());

    try {
        x_FFGenerate(seh, context);
    } catch (const CException& exc) {
        ERR_POST(Error << exc);
        m_Exception = true;
    }
    context.m_Scope->RemoveTopLevelSeqEntry(seh);
    return true;
}


//  ============================================================================
bool CAsn2FlatApp::HandleSeqSubmit(TFFContext& context, CObjectIStream& is) const
//  ============================================================================
{
    CRef<CSeq_submit> sub(new CSeq_submit);
    is >> *sub;
    return HandleSeqSubmit(context, *sub);
}

//  ============================================================================
void CAsn2FlatApp::HandleTextId(TFFContext& context, const string& strId) const
//  ============================================================================
{
    CSeq_entry_Handle seh;

    // This C++-scope gets a raw CSeq_entry that has no attachment
    // to any CScope and puts it into entry.
    {
        CSeq_id        id(strId);
        CBioseq_Handle bsh = context.m_Scope->GetBioseqHandle(id);
        if (! bsh) {
            NCBI_THROW(
                CException, eUnknown, "Unable to retrieve data for the given ID");
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
    HandleSeqEntryHandle(context, seh);
}

//  ============================================================================
bool CAsn2FlatApp::HandleSeqEntryHandle(TFFContext& context, CSeq_entry_Handle seh) const
//  ============================================================================
{
    //const CArgs& args = GetArgs();

    bool doConditionalCleanup = m_do_cleanup;
    if (m_do_cleanup) {
        if (m_do_html && CFlatFileGenerator::HasInference(seh)) {
            doConditionalCleanup = false;
        }
    }

    if (doConditionalCleanup) {
        CSeq_entry_EditHandle  tseh = seh.GetTopLevelEntry().GetEditHandle();
        CBioseq_set_EditHandle bseth;
        CBioseq_EditHandle     bseqh;
        CRef<CSeq_entry>       tmp_se(new CSeq_entry);
        if (tseh.IsSet()) {
            bseth                       = tseh.SetSet();
            CConstRef<CBioseq_set> bset = bseth.GetCompleteObject();
            bseth.Remove(bseth.eKeepSeq_entry);
            tmp_se->SetSet(const_cast<CBioseq_set&>(*bset));
        } else {
            bseqh                   = tseh.SetSeq();
            CConstRef<CBioseq> bseq = bseqh.GetCompleteObject();
            bseqh.Remove(bseqh.eKeepSeq_entry);
            tmp_se->SetSeq(const_cast<CBioseq&>(*bseq));
        }

        Uint4 options = CCleanup::eClean_ForFlatfile;
        context.m_cleanup.BasicCleanup(*tmp_se, options);

        if (tmp_se->IsSet()) {
            tseh.SelectSet(bseth);
        } else {
            tseh.SelectSeq(bseqh);
        }
    }

    try {
        x_FFGenerate(seh, context);
    } catch (CException& e) {
        ERR_POST(Error << e);
        m_Exception = true;
    }

    return true;
}

CSeq_entry_Handle CAsn2FlatApp::ObtainSeqEntryFromSeqEntry(TFFContext& context, CObjectIStream& is, bool report) const
{
    try {
        CRef<CSeq_entry> se(new CSeq_entry);
        is >> *se;
        if (se->Which() == CSeq_entry::e_not_set) {
            NCBI_THROW(CException, eUnknown, "provided Seq-entry is empty");
        }
        return context.m_Scope->AddTopLevelSeqEntry(*se);
    } catch (CException& e) {
        if (report) {
            ERR_POST(Error << e);
        }
    }
    return CSeq_entry_Handle();
}

CSeq_entry_Handle CAsn2FlatApp::ObtainSeqEntryFromBioseq(TFFContext& context, CObjectIStream& is, bool report) const
{
    try {
        CRef<CBioseq> bs(new CBioseq);
        is >> *bs;
        CBioseq_Handle bsh = context.m_Scope->AddBioseq(*bs);
        return bsh.GetTopLevelEntry();
    } catch (CException& e) {
        if (report) {
            ERR_POST(Error << e);
        }
    }
    return CSeq_entry_Handle();
}

CSeq_entry_Handle CAsn2FlatApp::ObtainSeqEntryFromBioseqSet(TFFContext& context, CObjectIStream& is, bool report) const
{
    try {
        CRef<CSeq_entry> entry(new CSeq_entry);
        is >> entry->SetSet();
        return context.m_Scope->AddTopLevelSeqEntry(*entry);
    } catch (CException& e) {
        if (report) {
            ERR_POST(Error << e);
        }
    }
    return CSeq_entry_Handle();
}

bool CAsn2FlatApp::HandleSeqEntry(TFFContext& context, CRef<CSeq_entry> se) const
{
    if (! se) {
        return false;
    }

    // add entry to scope
    CSeq_entry_Handle entry = context.m_Scope->AddTopLevelSeqEntry(*se);
    if (! entry) {
        NCBI_THROW(CException, eUnknown, "Failed to insert entry to scope.");
    }

    bool ret = HandleSeqEntryHandle(context, entry);
    // Needed because we can really accumulate a lot of junk otherwise,
    // and we end up with significant slowdown due to repeatedly doing
    // linear scans on a growing CScope.
    context.m_Scope->ResetDataAndHistory();
    return ret;
}

unique_ptr<CObjectIStream> CAsn2FlatApp::x_OpenIStream(const CArgs& args) const
{
    // determine the file serialization format.
    // default for batch files is binary, otherwise text.
    ESerialDataFormat serial = args["batch"] ? eSerial_AsnBinary : eSerial_AsnText;
    if (args["serial"]) {
        const string& val = args["serial"].AsString();
        if (val == "text") {
            serial = eSerial_AsnText;
        } else if (val == "binary") {
            serial = eSerial_AsnBinary;
        } else if (val == "XML") {
            serial = eSerial_Xml;
        }
    }

    // make sure of the underlying input stream. If -i was given on the command line
    // then the input comes from a file. Otherwise, it comes from stdin:
    CNcbiIstream* pInputStream   = nullptr;
    bool          bDeleteOnClose = false;

    if (m_huge_process.GetConstFile().IsOpen()) {
        if (!args["c"])
            return m_huge_process.GetConstFile().MakeObjStream();

        pInputStream = m_huge_process.GetConstFile().m_stream.get();
    } else {
        if (args["i"]) {
            pInputStream   = new CNcbiIfstream(args["i"].AsString(), ios::binary);
            bDeleteOnClose = true;
        } else
            pInputStream   = &std::cin;
    }

    // if -c was specified then wrap the input stream into a gzip decompressor before
    // turning it into an object stream:
    CObjectIStream* pI = nullptr;
    if (args["c"]) {
        CZipStreamDecompressor* pDecompressor =
            new CZipStreamDecompressor(CZipCompression::fCheckFileHeader);
        CCompressionIStream* pUnzipStream =
            new CCompressionIStream(*pInputStream, pDecompressor, CCompressionIStream::fOwnProcessor);
        pI = CObjectIStream::Open(serial, *pUnzipStream, eTakeOwnership);
    } else {
        pI = CObjectIStream::Open(
            serial, *pInputStream, (bDeleteOnClose ? eTakeOwnership : eNoOwnership));
    }

    if (pI) {
        pI->UseMemoryPool();
    }

    return unique_ptr<CObjectIStream>{pI};
}


void CAsn2FlatApp::x_CreateFlatFileGenerator(TFFContext& context, const CArgs& args) const
{
    CFlatFileConfig cfg;
    cfg.FromArguments(args);
    cfg.BasicCleanup(false);

    if (m_do_html) {
        CHTMLFormatterEx* html_fmt_ex = new CHTMLFormatterEx(context.m_Scope);
        html_fmt_ex->SetNcbiURLBase(NCBI_URL_BASE);
        CRef<IHTMLFormatter> html_fmt(html_fmt_ex);
        cfg.SetHTMLFormatter(html_fmt);
    }

#if 0
    // temporarly disabled because they never used
    CRef<TGenbankBlockCallback> genbank_callback( x_GetGenbankCallback(args) );

    if( args["benchmark-cancel-checking"] ) {
        m_pCanceledCallback.reset(x_CreateCancelBenchmarkCallback());
    }
#endif

    {
        bool nuc  = args["og"] || args["or"] || args["on"];
        bool prot = args["op"];
        if (nuc && prot) {
            cfg.SetViewAll();
        } else {
            if (nuc) {
                cfg.SetViewNuc();
            } else if (prot) {
                cfg.SetViewProt();
            }
        }
    }

    // CFlatFileConfig cfg(
    //     format, mode, style, flags, view, gff_options, genbank_blocks,
    //     genbank_callback.GetPointerOrNull(), m_pCanceledCallback.get(),
    //     args["cleanup"]);
    context.m_FFGenerator.Reset(new CFlatFileGenerator(cfg));

    // ID-5865 : SNP annotations must be explicitly added to the annot selector
    if (! m_PSGMode && cfg.ShowSNPFeatures()) {
        cfg.SetHideSNPFeatures(false);
        context.m_FFGenerator->SetAnnotSelector().IncludeNamedAnnotAccession("SNP");
    }

    if (args["no-external"] || args["policy"].AsString() == "internal") {
        context.m_FFGenerator->SetAnnotSelector().SetExcludeExternal(true);
    }
    //    else if (!m_Scope->GetKeepExternalAnnotsForEdit()) {
    //       m_Scope->SetKeepExternalAnnotsForEdit();
    //    }
    if (args["resolve-all"] || args["policy"].AsString() == "external") {
        context.m_FFGenerator->SetAnnotSelector().SetResolveAll();
    }
    if (args["depth"]) {
        context.m_FFGenerator->SetAnnotSelector().SetResolveDepth(args["depth"].AsInteger());
    }
    if (args["max_search_segments"]) {
        context.m_FFGenerator->SetAnnotSelector().SetMaxSearchSegments(args["max_search_segments"].AsInteger());
    }
    if (args["max_search_time"]) {
        context.m_FFGenerator->SetAnnotSelector().SetMaxSearchTime(float(args["max_search_time"].AsDouble()));
    }

}

TSeqPos CAsn2FlatApp::x_GetFrom(const CArgs& args) const
{
    return args["from"] ? static_cast<TSeqPos>(args["from"].AsInteger() - 1) : CRange<TSeqPos>::GetWholeFrom();
}


TSeqPos CAsn2FlatApp::x_GetTo(const CArgs& args) const
{
    return args["to"] ? static_cast<TSeqPos>(args["to"].AsInteger() - 1) : CRange<TSeqPos>::GetWholeTo();
}

ENa_strand CAsn2FlatApp::x_GetStrand(const CArgs& args) const
{
    return static_cast<ENa_strand>(args["strand"].AsInteger());
}


class CGetSeqLocFromStringHelper_ReadLocFromText : public CGetSeqLocFromStringHelper {
public:
    CGetSeqLocFromStringHelper_ReadLocFromText( CScope *scope )
        : m_scope(scope) { }

    virtual CRef<CSeq_loc> Seq_loc_Add(
        const CSeq_loc&    loc1,
        const CSeq_loc&    loc2,
        CSeq_loc::TOpFlags flags )
    {
        return sequence::Seq_loc_Add( loc1, loc2, flags, m_scope );
    }

private:
    CScope *m_scope;
};

void CAsn2FlatApp::x_GetLocation(const CSeq_entry_Handle& entry,
                                 const CArgs&             args,
                                 CSeq_loc&                loc) const
{
    _ASSERT(entry);

    CBioseq_Handle h = x_DeduceTarget(entry);
    if (! h) {
        NCBI_THROW(CFlatException, eInvalidParam, "Cannot deduce target bioseq.");
    }

    if (args["location"]) {
        vector<string> location;
        const string& locn = args["location"].AsString();
        CGetSeqLocFromStringHelper_ReadLocFromText helper(&entry.GetScope());
        CRef<CSeq_loc> lc = GetSeqLocFromString(locn, h.GetSeqId(), &helper);
        if (lc) {
            loc.Assign(*lc);
        }
        return;
    }

    TSeqPos    length = h.GetInst_Length();
    TSeqPos    from   = x_GetFrom(args);
    TSeqPos    to     = min(x_GetTo(args), length - 1);
    ENa_strand strand = eNa_strand_unknown;
    if (args["strand"]) {
        strand = x_GetStrand(args);
    }

    if (from == CRange<TSeqPos>::GetWholeFrom() && to == length - 1 && strand == eNa_strand_unknown) {
        // whole
        loc.SetWhole().Assign(*h.GetSeqId());
    } else {
        // interval
        loc.SetInt().SetId().Assign(*h.GetSeqId());
        loc.SetInt().SetFrom(from);
        loc.SetInt().SetTo(to);
        if (strand > 0) {
            loc.SetInt().SetStrand(strand);
        }
    }
}


// if the 'from' or 'to' flags are specified try to guess the bioseq.
CBioseq_Handle CAsn2FlatApp::x_DeduceTarget(const CSeq_entry_Handle& entry) const
{
    if (entry.IsSeq()) {
        return entry.GetSeq();
    }

    _ASSERT(entry.IsSet());
    CBioseq_set_Handle bsst = entry.GetSet();
    if (! bsst.IsSetClass()) {
        NCBI_THROW(CFlatException, eInvalidParam, "Cannot deduce target bioseq.");
    }
    _ASSERT(bsst.IsSetClass());
    switch (bsst.GetClass()) {
    case CBioseq_set::eClass_nuc_prot:
        // return the nucleotide
        for (CSeq_entry_CI it(entry); it; ++it) {
            if (it->IsSeq()) {
                CBioseq_Handle h = it->GetSeq();
                if (h && CSeq_inst::IsNa(h.GetInst_Mol())) {
                    return h;
                }
            }
        }
        break;
    case CBioseq_set::eClass_gen_prod_set:
        // return the genomic
        for (CSeq_entry_CI it(bsst); it; ++it) {
            if (it->IsSeq() &&
                it->GetSeq().GetInst_Mol() == CSeq_inst::eMol_dna) {
                return it->GetSeq();
            }
        }
        break;
    case CBioseq_set::eClass_segset:
        // return the segmented bioseq
        for (CSeq_entry_CI it(bsst); it; ++it) {
            if (it->IsSeq()) {
                return it->GetSeq();
            }
        }
        break;
    case CBioseq_set::eClass_genbank: {
        CBioseq_CI bi(bsst, CSeq_inst::eMol_na);
        if (bi) {
            return *bi;
        }
    } break;
    default:
        break;
    }
    NCBI_THROW(CFlatException, eInvalidParam, "Cannot deduce target bioseq.");
}

int CAsn2FlatApp::x_AddSNPAnnots(CBioseq_Handle& bsh, TFFContext& ff_context) const
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
    if (! skip) {
        string acc;
        accid->GetLabel(&acc, CSeq_id::eContent);
        string acc_prefix = acc.substr(0, 2);
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
        auto               snp_status = m_SNPTrackStub->ForSeqId(&context, request, &reply);
        if (snp_status.ok()) {
            string na_acc = reply.na_track_acc_with_filter();
            if (! na_acc.empty())
                ff_context.m_FFGenerator->SetAnnotSelector().IncludeNamedAnnotAccession(na_acc);
        }
    }
#endif

    return rc;
}

void CAsn2FlatApp::x_FFGenerate(CSeq_entry_Handle seh, TFFContext& context) const
{
    const CArgs& args = GetArgs();
    if (args["from"] || args["to"] || args["strand"] || args["location"]) {
        CSeq_loc loc;
        x_GetLocation(seh, args, loc);
        auto* flatfile_os = context.m_streams[eFlatFileCodes::all].get();
        context.m_FFGenerator->Generate(loc, seh.GetScope(), *flatfile_os, { flatfile_os });
    } else {
        auto* all  = context.m_streams[eFlatFileCodes::all].get();
        auto* nuc  = context.m_streams[eFlatFileCodes::nuc].get();
        auto* gen  = context.m_streams[eFlatFileCodes::gen].get();
        auto* rna  = context.m_streams[eFlatFileCodes::rna].get();
        auto* prot = context.m_streams[eFlatFileCodes::prot].get();
        auto* unk  = context.m_streams[eFlatFileCodes::unk].get();
        context.m_FFGenerator->Generate(seh, *all, { all, nuc, gen, rna, prot, unk });
    }
}

void CAsn2FlatApp::x_ResetContext(TFFContext& context)
{
    context.m_streams.Reset();

    if (context.m_FFGenerator.NotEmpty()) {
        context.m_FFGenerator->SetFeatTree(nullptr);
    }

    if (context.m_Scope.NotEmpty()) {
        context.m_Scope->ResetDataAndHistory();
    }
}

void CAsn2FlatApp::x_InitNewContext(TFFContext& context)
{
    if (context.m_Scope.Empty()) {
        context.m_Scope.Reset(new CScope(*m_Objmgr));
        context.m_Scope->AddDefaults();

#ifdef USE_SNPLOADER
        if (m_SNPDataLoader) {
            context.m_Scope->AddDataLoader(m_SNPDataLoader->GetLoaderNameFromArgs());
        }
#endif
#ifdef USE_CDDLOADER
        if (m_CDDDataLoader) {
            context.m_Scope->AddDataLoader(m_CDDDataLoader->GetLoaderNameFromArgs());
        }
#endif
    }

    // create the flat-file generator
    if (context.m_FFGenerator.Empty())
        x_CreateFlatFileGenerator(context, GetArgs());


    context.m_streams = m_writers.MakeNewFileset();
}

bool CAsn2FlatApp::WrapINSDSet(bool unwrap)
{
    auto& args = GetArgs();
    if (args["o"] && args["format"] && args["format"].AsString() == "insdseq") {
        auto streams = m_writers.MakeNewFileset();
        auto os      = streams[eFlatFileCodes::all].get();
        if (os)
            *os << (unwrap ? "</INSDSet>" : "<INSDSet>") << endl;
        return true;
    }
    return false;
}


END_NCBI_SCOPE

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//
// Main

int main(int argc, const char** argv)
{
    // this code converts single argument into multiple, just to simplify testing
    list<string> split_args;
    vector<const char*> new_argv;

    if (argc==2 && argv && argv[1] && strchr(argv[1], ' '))
    {
        NStr::Split(argv[1], " ", split_args);

        auto it = split_args.begin();
        while (it != split_args.end())
        {
            auto next = it; ++next;
            if (next != split_args.end() &&
                ((it->front() == '"' && it->back() != '"') ||
                 (it->front() == '\'' && it->back() != '\'')))
            {
                it->append(" "); it->append(*next);
                next = split_args.erase(next);
            } else it = next;
        }
        for (auto& rec: split_args)
        {
            if (rec.front()=='\'' && rec.back()=='\'')
                rec=rec.substr(1, rec.length()-2);
        }
        argc = 1 + int(split_args.size());
        new_argv.reserve(argc);
        new_argv.push_back(argv[0]);
        for (const string& s : split_args)
        {
            new_argv.push_back(s.c_str());
            std::cerr << s.c_str() << " ";
        }
        std::cerr << "\n";


        argv = new_argv.data();
    }

    CScope::SetDefaultKeepExternalAnnotsForEdit(true);
    return CAsn2FlatApp().AppMain(argc, argv);
}
