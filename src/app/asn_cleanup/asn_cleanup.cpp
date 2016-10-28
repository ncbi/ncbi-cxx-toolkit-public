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
* Author:  Aaron Ucko, Mati Shomrat, Colleen Bollin, NCBI
*
* File Description:
*   runs ExtendedCleanup on ASN.1 files
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <serial/objectio.hpp>


#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#ifdef HAVE_NCBI_VDB
#  include <sra/data_loaders/wgs/wgsloader.hpp>
#endif
#include <objtools/data_loaders/genbank/readers.hpp>
#include <dbapi/driver/drivers.hpp>

#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objects/seqset/gb_release_file.hpp>

#include <objects/entrez2/Entrez2_boolean_element.hpp>
#include <objects/entrez2/Entrez2_boolean_reply.hpp>
#include <objects/entrez2/Entrez2_boolean_exp.hpp>
#include <objects/entrez2/Entrez2_eval_boolean.hpp>
#include <objects/entrez2/Entrez2_id_list.hpp>
#include <objects/entrez2/entrez2_client.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include <objtools/edit/autodef.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

#include "read_hooks.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CCleanupApp : public CNcbiApplication, public CGBReleaseFile::ISeqEntryHandler
{
public:
    CCleanupApp();
    void Init(void);
    int  Run (void);

    bool HandleSeqEntry(CRef<CSeq_entry>& se);
    bool HandleSeqEntry(CSeq_entry_Handle entry);
    bool HandleSeqID( const string& seqID );
    
    bool ObtainSeqEntryFromSeqEntry( 
        auto_ptr<CObjectIStream>& is, 
        CRef<CSeq_entry>& se );
    bool ObtainSeqEntryFromBioseq( 
        auto_ptr<CObjectIStream>& is, 
        CRef<CSeq_entry>& se );
    bool ObtainSeqEntryFromBioseqSet( 
        auto_ptr<CObjectIStream>& is, 
        CRef<CSeq_entry>& se );
  
private:
    // types

    CObjectIStream* x_OpenIStream(const CArgs& args, const string& filename);
    void x_OpenOStream(const string& filename, const string& dir = kEmptyStr, bool remove_orig_dir = true);
    void x_CloseOStream();
    bool x_ProcessSeqSubmit(auto_ptr<CObjectIStream>& is);
    void x_ProcessOneFile(auto_ptr<CObjectIStream> is, bool batch, const string& asn_type, bool first_only);
    void x_ProcessOneFile(const string& filename);
    void x_ProcessOneDirectory(const string& dirname, const string& suffix);

    TGi x_SeqIdToGiNumber( const string& seq_id, const string database );

    void x_FeatureOptionsValid(const string& opt);
    void x_KOptionsValid(const string& opt);
    void x_XOptionsValid(const string& opt);
    bool x_ProcessFeatureOptions(const string& opt, CSeq_entry_Handle seh);
    bool x_ProcessXOptions(const string& opt, CSeq_entry_Handle seh);
    bool x_GFF3Batch(CSeq_entry_Handle seh);
    enum EFixCDSOptions {
        eFixCDS_FrameFromLoc = 0x1,
        eFixCDS_Retranslate = 0x2,
        eFixCDS_ExtendToStop = 0x4
    };
    const Uint4 kGFF3CDSFixOptions = eFixCDS_FrameFromLoc | eFixCDS_Retranslate | eFixCDS_ExtendToStop;

    bool x_FixCDS(CSeq_entry_Handle seh, Uint4 options, const string& missing_prot_name);
    bool x_BatchExtendCDS(CSeq_feat& sf, CBioseq_Handle b);
    bool x_BasicAndExtended(CSeq_entry_Handle entry, const string& label, bool do_basic = true, bool do_extended = false, Uint4 options = 0);

    template<typename T> void x_WriteToFile(const T& s);

    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CRef<CScope>                m_Scope;
    CRef<CFlatFileGenerator>    m_FFGenerator;  // Flat-file generator
    auto_ptr<CObjectOStream>    m_Out;          // output
};


CCleanupApp::CCleanupApp()
{
  SetVersionByBuild(1);
}

void CCleanupApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Perform ExtendedCleanup on an ASN.1 Seq-entry into a flat report",
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
        
        // path
        arg_desc->AddOptionalKey("p", "path", "Path to files", CArgDescriptions::eDirectory);
        
        // suffix
        arg_desc->AddDefaultKey("x", "suffix", "File Selection Suffix", CArgDescriptions::eString, ".ent");

        // results
        arg_desc->AddOptionalKey("r", "results", "Path for Results", CArgDescriptions::eDirectory);

    }}

    // batch processing
    {{
        arg_desc->AddFlag("batch", "Process NCBI release file");
        // compression
        arg_desc->AddFlag("c", "Compressed file");

        // imitate limitation of C Toolkit version
        arg_desc->AddFlag("firstonly", "Process only first element");
    }}
    
    // output
    {{ 
        // name
        arg_desc->AddOptionalKey("o", "OutputFile", 
            "Output file name", CArgDescriptions::eOutputFile);
    }}
    
    // report
    {{

        // html
        arg_desc->AddFlag("html", "Produce HTML output");
    }}

    // normal cleanup options (will replace -nocleanup and -basic)
    {{
        arg_desc->AddOptionalKey("K", "Cleanup", "Systemic Cleaning Options\n"
                                 "\tb Basic\n"
                                 "\ts Extended\n"
                                 "\tn Normalize Descriptor Order\n"
                                 "\tu Remove Cleanup User-object\n",
                                 CArgDescriptions::eString);
    }}

    // extra cleanup options
    {{
        arg_desc->AddOptionalKey("F", "Feature", "Feature Cleaning Options\n"
                                 "\tr Remove Redundant Gene xref\n"
                                 "\ta Adjust for Missing Stop Codon\n"
                                 "\tp Clear internal partials\n"
                                 "\tz Delete or Update EC Numbers\n",
                                  CArgDescriptions::eString);

        arg_desc->AddOptionalKey("X", "Miscellaneous", "Other Cleaning Options",
            CArgDescriptions::eString);

        arg_desc->AddFlag("T", "TaxonomyLookup");
    }}
    
    // misc
    {{
        // no-cleanup
        arg_desc->AddFlag("nocleanup",
            "Do not perform extended data cleanup prior to formatting");
        arg_desc->AddFlag("basic",
            "Perform basic data cleanup prior to formatting");
        arg_desc->AddFlag("noobj",
            "Do not create Ncbi_cleanup object");

        // remote
        arg_desc->AddFlag("gbload", "Use CGBDataLoader");
        arg_desc->AddFlag("R", "Remote fetching");
        // show progress
        arg_desc->AddFlag("showprogress",
            "List ID for which cleanup is occuring");
        arg_desc->AddFlag("debug", "Save before.sqn");
    }}
    SetupArgDescriptions(arg_desc.release());
}


void CCleanupApp::x_FeatureOptionsValid(const string& opt)
{
    if (NStr::IsBlank(opt)){
        return;
    }
    string unrecognized = "";
    string::const_iterator s = opt.begin();
    while (s != opt.end()) {
        if (!isspace(*s)) {
            if (*s != 'r' && *s != 'a' && *s != 'p' && *s != 'z') {
                unrecognized += *s;
            }
        }
        s++;
    }
    if (unrecognized.length() > 0) {
        NCBI_THROW(CFlatException, eInternal, "Invalid -F arguments:" + unrecognized);
    }
}


void CCleanupApp::x_KOptionsValid(const string& opt)
{
    if (NStr::IsBlank(opt)){
        return;
    }
    string unrecognized = "";
    string::const_iterator s = opt.begin();
    while (s != opt.end()) {
        if (!isspace(*s)) {
            if (*s != 'b' && *s != 's' && *s != 'u' && *s != 'n') {
                unrecognized += *s;
            }
        }
        s++;
    }
    if (unrecognized.length() > 0) {
        NCBI_THROW(CFlatException, eInternal, "Invalid -K arguments:" + unrecognized);
    }
}


void CCleanupApp::x_XOptionsValid(const string& opt)
{
    if (NStr::IsBlank(opt)){
        return;
    }
    string unrecognized = "";
    string::const_iterator s = opt.begin();
    while (s != opt.end()) {
        if (!isspace(*s)) {
            if (*s != 'w' && *s != 'r' && *s != 'b') {
                unrecognized += *s;
            }
        }
        s++;
    }
    if (unrecognized.length() > 0) {
        NCBI_THROW(CFlatException, eInternal, "Invalid -X arguments:" + unrecognized);
    }
}


static CObjectOStream* CreateTmpOStream(const std::string& outFileName, const std::string& tmpFileName)
{
    if (outFileName.empty()) // cout
        return CObjectOStream::Open(eSerial_AsnText, std::cout);

    return CObjectOStream::Open(eSerial_AsnText, tmpFileName, eSerial_StdWhenAny);
}


static auto_ptr<CObjectTypeInfo> GetEntryTypeInfo()
{
    // 'data' member of CSeq_submit ...
    CObjectTypeInfo submitTypeInfo = CType<CSeq_submit>();
    CObjectTypeInfoMI data = submitTypeInfo.FindMember("data");

    // points to a container (pointee may has different types) ...
    const CPointerTypeInfo* dataPointerType = CTypeConverter<CPointerTypeInfo>::SafeCast(data.GetMemberType().GetTypeInfo());
    const CChoiceTypeInfo* dataChoiceType = CTypeConverter<CChoiceTypeInfo>::SafeCast(dataPointerType->GetPointedType());

    // that is a list of pointers to 'CSeq_entry' (we process only that case)
    const CItemInfo* entries = dataChoiceType->GetItemInfo("entrys");

    return auto_ptr<CObjectTypeInfo>(new CObjectTypeInfo(entries->GetTypeInfo()));
}


static CObjectTypeInfoMI GetSubmitBlockTypeInfo()
{
    CObjectTypeInfo submitTypeInfo = CType<CSeq_submit>();
    return submitTypeInfo.FindMember("sub");
}


static void CompleteOutputFile(CObjectOStream& out)
{
    out.EndContainer(); // ends the list of entries

    out.EndChoiceVariant(); // ends 'entrys'
    out.PopFrame();
    out.EndChoice(); // ends 'data'
    out.PopFrame();

    out.EndClass(); // ends 'CSeq_submit'

    Separator(out);

    fflush(NULL);
}


static std::string GetOutputFileName(const CArgs& args)
{
    std::string ret = !args["o"] ? "" : args["o"].AsString();
    return ret;
}

// returns false if fails to read object of expected type, throws for all other errors
bool CCleanupApp::x_ProcessSeqSubmit(auto_ptr<CObjectIStream>& is)
{
    CRef<CSeq_submit> sub(new CSeq_submit);
    if (sub.Empty()) {
        NCBI_THROW(CFlatException, eInternal,
            "Could not allocate Seq-submit object");
    }
    try {

        //CTmpFile tmpFile;
        //auto_ptr<CObjectOStream> out(CreateTmpOStream(outFileName, tmpFile.GetFileName()));

        CObjectTypeInfoMI submitBlockObj = GetSubmitBlockTypeInfo();
        submitBlockObj.SetLocalReadHook(*is, new CReadSubmitBlockHook(*m_Out));

        auto_ptr<CObjectTypeInfo> entryObj = GetEntryTypeInfo();
        entryObj->SetLocalReadHook(*is, new CReadEntryHook(*this, *m_Out));

        *is >> *sub;

        entryObj->ResetLocalReadHook(*is);
        submitBlockObj.ResetLocalReadHook(*is);

        CompleteOutputFile(*m_Out);
    }
    catch (...) {
        return false;
    }

    if (!sub->IsSetSub() || !sub->IsSetData()) {
        NCBI_THROW(CFlatException, eInternal, "No data in Seq-submit");
    }
    else if (!sub->GetData().IsEntrys()) {
        NCBI_THROW(CFlatException, eInternal, "Wrong data in Seq-submit");
    }
    
    return true;
}


void CCleanupApp::x_ProcessOneFile(auto_ptr<CObjectIStream> is, bool batch, const string& asn_type, bool first_only)
{
    if (batch) {
        CGBReleaseFile in(*is.release());
        in.RegisterHandler(this);
        in.Read();  // HandleSeqEntry will be called from this function
    } else {
        if (asn_type == "seq-submit") {  // submission
            if (!x_ProcessSeqSubmit(is)) {
                NCBI_THROW(CFlatException, eInternal, "Unable to read Seq-submit");
            }
        } else {
            CRef<CSeq_entry> se(new CSeq_entry);

            if (asn_type == "seq-entry") {
                //
                //  Straight through processing: Read a seq_entry, then process
                //  a seq_entry:
                //
                if (!ObtainSeqEntryFromSeqEntry(is, se)) {
                    NCBI_THROW(
                        CFlatException, eInternal, "Unable to construct Seq-entry object");
                }
                HandleSeqEntry(se);
                x_WriteToFile(*se);
            } else if (asn_type == "bioseq") {
                //
                //  Read object as a bioseq, wrap it into a seq_entry, then process
                //  the wrapped bioseq as a seq_entry:
                //
                if (!ObtainSeqEntryFromBioseq(is, se)) {
                    NCBI_THROW(
                        CFlatException, eInternal, "Unable to construct Seq-entry object");
                }
                HandleSeqEntry(se);
                x_WriteToFile(se->GetSeq());
            } else if (asn_type == "bioseq-set") {
                //
                //  Read object as a bioseq_set, wrap it into a seq_entry, then 
                //  process the wrapped bioseq_set as a seq_entry:
                //
                if (!ObtainSeqEntryFromBioseqSet(is, se)) {
                    NCBI_THROW(
                        CFlatException, eInternal, "Unable to construct Seq-entry object");
                }
                HandleSeqEntry(se);
                x_WriteToFile(se->GetSet());
            } else if (asn_type == "any") {
                size_t num_cleaned = 0;
                while (true) {
                    CNcbiStreampos start = is->GetStreamPos();
                    //
                    //  Try the first three in turn:
                    //
                    string strNextTypeName = is->PeekNextTypeName();
                    if (ObtainSeqEntryFromSeqEntry(is, se)) {
                        HandleSeqEntry(se);
                        x_WriteToFile(*se);
                    } else {
                        is->SetStreamPos(start);
                        if (ObtainSeqEntryFromBioseqSet(is, se)) {
                            HandleSeqEntry(se);
                            x_WriteToFile(se->GetSet());
                        } else {
                            is->SetStreamPos(start);
                            if (ObtainSeqEntryFromBioseq(is, se)) {
                                HandleSeqEntry(se);
                                x_WriteToFile(se->GetSeq());
                            } else {
                                is->SetStreamPos(start);
                                if (!x_ProcessSeqSubmit(is)) {
                                    if (num_cleaned == 0) {
                                        NCBI_THROW(
                                            CFlatException, eInternal,
                                            "Unable to construct Seq-entry object"
                                            );
                                    } else {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    num_cleaned++;
                    if (first_only) {
                        break;
                    }
                }
            }
        }
    }


}


void CCleanupApp::x_ProcessOneFile(const string& filename)
{
    const CArgs&   args = GetArgs();

    auto_ptr<CObjectIStream> is;

    // open file
    is.reset(x_OpenIStream(args, filename));
    if (is.get() == NULL) {
        string msg = NStr::IsBlank(filename) ? "Unable to read data from stdin" : "Unable to open input file" + filename;
        NCBI_THROW(CFlatException, eInternal, msg);
    }

    // need to set output if -o not specified
    bool opened_output = false;

    if (!args["o"] && args["r"]) {
        x_OpenOStream(filename, args["r"].AsString(), true);
        opened_output = true;
    }

    x_ProcessOneFile(is, args["batch"], args["type"].AsString(), args["firstonly"]);

    is.reset();
    if (opened_output) {
        // close output file if we opened one
        x_CloseOStream();
    }
}


void CCleanupApp::x_ProcessOneDirectory(const string& dirname, const string& suffix)
{
    const CArgs& args = GetArgs();

    CDir dir(dirname);

    string mask = "*" + suffix;
    size_t num_files = 0;

    CDir::TEntries files(dir.GetEntries(mask, CDir::eFile));
    ITERATE(CDir::TEntries, ii, files) {
        string fname = (*ii)->GetName();
        if ((*ii)->IsFile()) {
            string fname = CDirEntry::MakePath(dirname, (*ii)->GetName());
            x_ProcessOneFile(fname);
            num_files++;
        }
    }
    if (num_files == 0) {
        NCBI_THROW(CFlatException, eInternal, "No files found!");
    }
}


int CCleanupApp::Run(void)
{
	// initialize conn library
	CONNECT_Init(&GetConfig());

    const CArgs&   args = GetArgs();

    // flag validation
    if (args["F"]) {
        x_FeatureOptionsValid(args["F"].AsString());
    }
    if (args["K"]) {
        x_KOptionsValid(args["K"].AsString());
    }

    // create object manager
    m_Objmgr = CObjectManager::GetInstance();
    if ( !m_Objmgr ) {
        NCBI_THROW(CFlatException, eInternal, "Could not create object manager");
    }

    m_Scope.Reset(new CScope(*m_Objmgr));
    if (args["gbload"] || args["R"] || args["id"]) {
#ifdef HAVE_PUBSEQ_OS
        // we may require PubSeqOS readers at some point, so go ahead and make
        // sure they are properly registered
        GenBankReaders_Register_Pubseq();
        GenBankReaders_Register_Pubseq2();
        DBAPI_RegisterDriver_FTDS();
#endif

        CGBDataLoader::RegisterInObjectManager(*m_Objmgr);
#ifdef HAVE_NCBI_VDB
        CWGSDataLoader::RegisterInObjectManager(*m_Objmgr,
                                            CObjectManager::eDefault,
                                            88);
#endif

    }
    m_Scope->AddDefaults();

    // need to set output (-o) if specified, if not -o and not -r need to use standard output
    bool opened_output = false;
    if (args["o"]) {
        x_OpenOStream(args["o"].AsString(),
                      args["r"] ? args["r"].AsString() : kEmptyStr,
                      false);
        opened_output = true;
    } else if (!args["r"] || args["id"]) {
        x_OpenOStream(kEmptyStr);
        opened_output = true;
    }

    if (args["id"]) {
        string seqID = args["id"].AsString();
        HandleSeqID(seqID);
    } else if (args["i"]) {
        if (args["p"]) {
            string fname = CDirEntry::MakePath(args["p"].AsString(), args["i"].AsString());
            x_ProcessOneFile(fname);
        } else {
            x_ProcessOneFile(args["i"].AsString());
        }
    } else if (args["r"]) {
        x_ProcessOneDirectory(args["p"].AsString(), args["x"].AsString());
    } else {
        x_ProcessOneFile("");
    }

    if (opened_output) {
        // close output file if we opened one
        x_CloseOStream();
    }
    return 0;
}

bool CCleanupApp::ObtainSeqEntryFromSeqEntry( 
    auto_ptr<CObjectIStream>& is, 
    CRef<CSeq_entry>& se )
{
    try {
        *is >> *se;
        if (se->Which() == CSeq_entry::e_not_set) {
            return false;
        }
        return true;
    }
    catch( ... ) {
        return false;
    }
}

bool CCleanupApp::ObtainSeqEntryFromBioseq( 
    auto_ptr<CObjectIStream>& is, 
    CRef<CSeq_entry>& se )
{
    try {
		CRef<CBioseq> bs( new CBioseq );
		if ( ! bs ) {
            NCBI_THROW(CFlatException, eInternal, 
            "Could not allocate Bioseq object");
		}
	    *is >> *bs;

        se->SetSeq( bs.GetObject() );
        return true;
    }
    catch( ... ) {
        return false;
    }
}

bool CCleanupApp::ObtainSeqEntryFromBioseqSet( 
    auto_ptr<CObjectIStream>& is, 
    CRef<CSeq_entry>& se )
{
    try {
		CRef<CBioseq_set> bss( new CBioseq_set );
		if ( ! bss ) {
            NCBI_THROW(CFlatException, eInternal, 
            "Could not allocate Bioseq object");
		}
	    *is >> *bss;

        se->SetSet( bss.GetObject() );
        return true;
    }
    catch( ... ) {
        return false;
    }
}

TGi CCleanupApp::x_SeqIdToGiNumber( 
    const string& seq_id,
    const string database_name )
{
    CEntrez2Client m_E2Client;

    CRef<CEntrez2_boolean_element> e2_element (new CEntrez2_boolean_element);
    e2_element->SetStr(seq_id);
        
    CEntrez2_eval_boolean eb;
    eb.SetReturn_UIDs(true);
    CEntrez2_boolean_exp& query = eb.SetQuery();
    query.SetExp().push_back(e2_element);
    query.SetDb() = CEntrez2_db_id( database_name );
    
    CRef<CEntrez2_boolean_reply> reply = m_E2Client.AskEval_boolean(eb);
    
    switch ( reply->GetCount() ) {
    
    case 0:
        // no hits whatever:
        return ZERO_GI;
        
    case 1: {
        //  one hit; the expected outcome:
        //
        //  "it" declared here to keep the WorkShop compiler from whining.
        CEntrez2_id_list::TConstUidIterator it 
            = reply->GetUids().GetConstUidIterator();        
        return GI_FROM(TIntGi, (*it));
    }    
    default:
        // multiple hits? Unexpected and definitely not a good thing...
        ERR_FATAL("Unexpected: The ID " << seq_id.c_str() 
                  << " turned up multiple hits." );
    }

    return ZERO_GI;
};


bool CCleanupApp::HandleSeqID( const string& seq_id )
{
    CRef<CScope> scope(new CScope(*m_Objmgr));
    scope->AddDefaults();

    CBioseq_Handle bsh;
    try {
        CSeq_id SeqId(seq_id);
        bsh = scope->GetBioseqHandle(SeqId);
    }
    catch ( CException& ) {
        ERR_FATAL("The ID " << seq_id.c_str() << " is not a valid seq ID." );
    }

    if (!bsh) {
        ERR_FATAL("Sequence for " << seq_id.c_str() << " cannot be retrieved.");
        return false;
    }

    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->Assign(*(bsh.GetSeq_entry_Handle().GetCompleteSeq_entry()));
    HandleSeqEntry(entry);
    x_WriteToFile(*entry);

    return true;
}

bool CCleanupApp::x_ProcessFeatureOptions(const string& opt, CSeq_entry_Handle seh)
{
    if (NStr::IsBlank(opt)) {
        return false;
    }
    bool any_changes = false;
    if (NStr::Find(opt, "r") != string::npos) {
        any_changes |= CCleanup::RemoveUnnecessaryGeneXrefs(seh);
    }
    if (NStr::Find(opt, "a") != string::npos) {
        any_changes |= x_FixCDS(seh, eFixCDS_ExtendToStop, kEmptyStr);
    }
    if (NStr::Find(opt, "p") != string::npos) {
        any_changes |= CCleanup::ClearInternalPartials(seh);
    }
    if (NStr::Find(opt, "z") != string::npos) {
        any_changes |= CCleanup::FixECNumbers(seh);
    }
    return any_changes;
}

bool CCleanupApp::x_ProcessXOptions(const string& opt, CSeq_entry_Handle seh)
{
    bool any_changes = false;
    if (NStr::Find(opt, "w") != string::npos) {
        any_changes = CCleanup::WGSCleanup(seh);
    }
    if (NStr::Find(opt, "r") != string::npos) {
        bool change_defline = CAutoDef::RegenerateDefLines(seh);
        if (change_defline) {
            any_changes = true;
            CCleanup::NormalizeDescriptorOrder(seh);
        }
        
    }
    if (NStr::Find(opt, "b") != string::npos) {
        any_changes = x_GFF3Batch(seh);
    }
    return any_changes;
}


bool CCleanupApp::x_BatchExtendCDS(CSeq_feat& sf, CBioseq_Handle b)
{
    if (!sf.GetData().IsCdregion()) {
        // not coding region
        return false;
    }
    if (CCleanup::IsPseudo(sf, b.GetScope())) {
        return false;
    }

    // check for existing stop codon
    string translation;
    try {
        CSeqTranslator::Translate(sf, b.GetScope(), translation, true);
    } catch (CSeqMapException& e) {
        cout << e.what() << endl;
        return false;
    } catch (CSeqVectorException& e) {
        cout << e.what() << endl;
        return false;
    }
    if (NStr::EndsWith(translation, "*")) {
        //already has stop codon
        return false;
    }
    

    if (CCleanup::ExtendToStopCodon(sf, b, 50)) {
        feature::RetranslateCDS(sf, b.GetScope());
        return true;
    } else {
        return false;
    }
}


bool s_LocationShouldBeExtendedToMatch(const CSeq_loc& orig, const CSeq_loc& improved)
{
    if ((orig.GetStrand() == eNa_strand_minus &&
        orig.GetStop(eExtreme_Biological) > improved.GetStop(eExtreme_Biological)) ||
        (orig.GetStrand() != eNa_strand_minus &&
        orig.GetStop(eExtreme_Biological) < improved.GetStop(eExtreme_Biological))) {
        return true;
    } else {
        return false;
    }
}


bool CCleanupApp::x_FixCDS(CSeq_entry_Handle seh, Uint4 options, const string& missing_prot_name)
{
    bool any_changes = false;
    for (CBioseq_CI bi(seh, CSeq_inst::eMol_na); bi; ++bi) {
        any_changes |= CCleanup::SetGeneticCodes(*bi);
        for (CFeat_CI fi(*bi, CSeqFeatData::eSubtype_cdregion); fi; ++fi) {
            CConstRef<CSeq_feat> orig = fi->GetSeq_feat();
            CRef<CSeq_feat> sf(new CSeq_feat());
            sf->Assign(*orig);
            bool feat_change = false;
            if ((options & eFixCDS_FrameFromLoc) &&
                CCleanup::SetFrameFromLoc(sf->SetData().SetCdregion(), sf->GetLocation(), bi.GetScope())) {
                feat_change = true;
            }
            if ((options & eFixCDS_Retranslate)) {
                feat_change |= feature::RetranslateCDS(*sf, bi.GetScope());
            }
            if ((options & eFixCDS_ExtendToStop) &&
                x_BatchExtendCDS(*sf, *bi)) {
                CConstRef<CSeq_feat> mrna = sequence::GetmRNAforCDS(*orig, seh.GetScope());
                if (mrna && s_LocationShouldBeExtendedToMatch(mrna->GetLocation(), sf->GetLocation())) {
                    CRef<CSeq_feat> new_mrna(new CSeq_feat());
                    new_mrna->Assign(*mrna);
                    if (CCleanup::ExtendToStopCodon(*new_mrna, *bi, 50, sf)) {
                        CSeq_feat_EditHandle efh(seh.GetScope().GetSeq_featHandle(*mrna));
                        efh.Replace(*new_mrna);
                    }
                }
                CConstRef<CSeq_feat> gene = CCleanup::GetGeneForFeature(*orig, seh.GetScope());
                if (gene && s_LocationShouldBeExtendedToMatch(gene->GetLocation(), sf->GetLocation())) {
                    CRef<CSeq_feat> new_gene(new CSeq_feat());
                    new_gene->Assign(*gene);
                    if (CCleanup::ExtendToStopCodon(*new_gene, *bi, 50, sf)) {
                        CSeq_feat_EditHandle efh(seh.GetScope().GetSeq_featHandle(*gene));
                        efh.Replace(*new_gene);
                    }
                }

                feat_change = true;
            }
            if (feat_change) {
                CSeq_feat_EditHandle ofh = CSeq_feat_EditHandle(seh.GetScope().GetSeq_featHandle(*orig));
                ofh.Replace(*sf);
                any_changes = true;
            }
            //also set protein name if missing, change takes place on protein bioseq
            if (!NStr::IsBlank(missing_prot_name)) {
                string current_name = CCleanup::GetProteinName(*sf, seh.GetScope());
                if (NStr::IsBlank(current_name)) {
                    CCleanup::SetProteinName(*sf, missing_prot_name, false, seh.GetScope());
                    any_changes = true;
                }
            }
        }
    }
    return any_changes;
}


bool CCleanupApp::x_GFF3Batch(CSeq_entry_Handle seh)
{
    bool any_changes = x_FixCDS(seh, kGFF3CDSFixOptions, kEmptyStr);
    CCleanup cleanup;
    cleanup.SetScope(&(seh.GetScope()));
    Uint4 options = CCleanup::eClean_NoNcbiUserObjects;
    CConstRef<CCleanupChange> changes = cleanup.BasicCleanup(seh, options);
    any_changes |= (!changes.Empty());
    changes = cleanup.ExtendedCleanup(seh, options);
    any_changes |= (!changes.Empty());
    any_changes |= x_FixCDS(seh, 0, "unnamed protein product");

    return any_changes;
}


bool CCleanupApp::x_BasicAndExtended(CSeq_entry_Handle entry, const string& label, 
                                     bool do_basic, bool do_extended, Uint4 options)
{
    if (!do_basic && !do_extended) {
        return false;
    }

    bool any_changes = false;
    CCleanup cleanup;
    cleanup.SetScope(&(entry.GetScope()));

    if (do_basic) {
        // perform BasicCleanup
        try {
            CConstRef<CCleanupChange> changes = cleanup.BasicCleanup(entry, options);
            vector<string> changes_str = changes->GetAllDescriptions();
            if (changes_str.size() == 0) {
                LOG_POST(Error << "No changes from BasicCleanup\n");
            }
            else {
                LOG_POST(Error << "Changes from BasicCleanup:\n");
                ITERATE(vector<string>, vit, changes_str) {
                    LOG_POST(Error << (*vit).c_str());
                }
                any_changes = true;
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error in basic cleanup: " << e.GetMsg() << label);
        }
    }

    if (do_extended) {
        // perform ExtendedCleanup
        try {
            CConstRef<CCleanupChange> changes = cleanup.ExtendedCleanup(entry, options);
            vector<string> changes_str = changes->GetAllDescriptions();
            if (changes_str.size() == 0) {
                LOG_POST(Error << "No changes from ExtendedCleanup\n");
            }
            else {
                LOG_POST(Error << "Changes from ExtendedCleanup:\n");
                ITERATE(vector<string>, vit, changes_str) {
                    LOG_POST(Error << (*vit).c_str());
                }
                any_changes = true;
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error in extended cleanup: " << e.GetMsg() << label);
        }
    }
    return any_changes;
}


bool CCleanupApp::HandleSeqEntry(CSeq_entry_Handle entry)
{
    string label;    
    entry.GetCompleteSeq_entry()->GetLabel(&label, CSeq_entry::eBoth);

    const CArgs& args = GetArgs();

    if (args["showprogress"]) {
        LOG_POST(Error << label + "\n");
    }

    ESerialDataFormat outFormat = eSerial_AsnText;

    if (args["debug"]) {
        auto_ptr<CObjectOStream> debug_out(CObjectOStream::Open(outFormat, "before.sqn",
            eSerial_StdWhenAny));

        *debug_out << *(entry.GetCompleteSeq_entry());
    }

    bool any_changes = false;

    if (args["T"]) {
        any_changes |= CCleanup::TaxonomyLookup(entry);
    }

    if (args["K"] && NStr::Find(args["K"].AsString(), "u") != string::npos) {
        CRef<CSeq_entry> se(const_cast<CSeq_entry *>(entry.GetCompleteSeq_entry().GetPointer()));
        any_changes |= CCleanup::RemoveNcbiCleanupObject(*se);
    }

    bool do_basic = false;
    bool do_extended = false;
    if (args["K"]) {
        if (NStr::Find(args["K"].AsString(), "b") != string::npos) {
            do_basic = true;
        }
        if (NStr::Find(args["K"].AsString(), "s") != string::npos) {
            do_basic = true;
            do_extended = true;
        }
    }
    else if (args["X"]) {
        do_basic = true;
        if (NStr::Find(args["X"].AsString(), "w") != string::npos) {
            //Extended Cleanup is part of -X w
            do_extended = false;
        }
    } else if (args["F"]) {
        do_basic = true;
    } else {
        if (args["basic"]) {
            do_basic = true;
        }
        if (!args["nocleanup"]) {
            do_extended = true;
        }
    }

    Uint4 options = 0;
    if (args["noobj"]) {
        options = CCleanup::eClean_NoNcbiUserObjects;
    }

    any_changes |= x_BasicAndExtended(entry, label, do_basic, do_extended, options);    

    if (args["F"]) {
        any_changes |= x_ProcessFeatureOptions(args["F"].AsString(), entry);
    }
    if (args["X"]) {
        any_changes |= x_ProcessXOptions(args["X"].AsString(), entry);
    }
    if (args["K"] && NStr::Find(args["K"].AsString(), "n") != string::npos && !do_extended) {
        any_changes |= CCleanup::NormalizeDescriptorOrder(entry);
    }

    return true;
}

bool CCleanupApp::HandleSeqEntry(CRef<CSeq_entry>& se)
{
    if (!se) {
        return false;
    }

    CSeq_entry_Handle entry = m_Scope->AddTopLevelSeqEntry(*se);


    if ( !entry ) {
        NCBI_THROW(CFlatException, eInternal, "Failed to insert entry to scope.");
    }

    if (HandleSeqEntry(entry)) {
        if (entry.GetCompleteSeq_entry().GetPointer() != se.GetPointer()) {
            se->Assign(*entry.GetCompleteSeq_entry());
        }
        m_Scope->RemoveTopLevelSeqEntry(entry);
        return true;
    } else {
        m_Scope->RemoveTopLevelSeqEntry(entry);
        return false;
    }
}


template<typename T>void CCleanupApp::x_WriteToFile(const T& obj)
{
    *m_Out << obj;

    fflush(NULL);
}


CObjectIStream* CCleanupApp::x_OpenIStream(const CArgs& args, const string& filename)
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
    if ( !NStr::IsBlank(filename)) {
        pInputStream = new CNcbiIfstream(filename.c_str(), ios::binary  );
        bDeleteOnClose = true;
    }
        
    // if -c was specified then wrap the input stream into a gzip decompressor before 
    // turning it into an object stream:
    CObjectIStream* pI = 0;
    if ( args["c"] ) {
        CZipStreamDecompressor* pDecompressor = new CZipStreamDecompressor(
            512, 512, kZlibDefaultWbits, CZipCompression::fCheckFileHeader );
        CCompressionIStream* pUnzipStream = new CCompressionIStream(
            *pInputStream, pDecompressor, CCompressionIStream::fOwnProcessor );
        pI = CObjectIStream::Open( serial, *pUnzipStream, eTakeOwnership );
    }
    else {
        pI = CObjectIStream::Open( serial, *pInputStream, bDeleteOnClose ? eTakeOwnership : eNoOwnership );
    }
    
    if ( 0 != pI ) {
        pI->UseMemoryPool();
        pI->SetDelayBufferParsingPolicy(CObjectIStream::eDelayBufferPolicyAlwaysParse);
    }
    return pI;
}


void CCleanupApp::x_OpenOStream(const string& filename, const string& dir, bool remove_orig_dir)
{
    ESerialDataFormat outFormat = eSerial_AsnText;
    if (NStr::IsBlank(filename)) {
        m_Out.reset(CObjectOStream::Open(outFormat, cout));
    } else if (!NStr::IsBlank(dir)) {
        string base = filename;
        if (remove_orig_dir) {
            char buf[2];
            buf[1] = 0;
            buf[0] = CDirEntry::GetPathSeparator();
            size_t pos = NStr::Find(base, buf, 0, base.length(), NStr::eLast);
            if (pos != string::npos) {
                base = base.substr(pos + 1);
            }
        }
        string fname = CDirEntry::MakePath(dir, base);
        m_Out.reset(CObjectOStream::Open(outFormat, fname, eSerial_StdWhenAny));
    } else {
        m_Out.reset(CObjectOStream::Open(outFormat, filename, eSerial_StdWhenAny));
    }
}


void CCleanupApp::x_CloseOStream()
{
    m_Out->Close();
    m_Out.reset(NULL);
}


END_NCBI_SCOPE

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//
// Main

int main(int argc, const char** argv)
{
    return CCleanupApp().AppMain(argc, argv);
}
