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
* Author:  Aaron Ucko, Mati Shomrat, Mike DiCuccio, NCBI
*
* File Description:
*   flat-file generator application
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_signal.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/readers/fasta.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objects/seqset/gb_release_file.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CAsn2FlatApp : public CNcbiApplication, public CGBReleaseFile::ISeqEntryHandler
{
public:
    void Init(void);
    int  Run (void);

    bool HandleSeqEntry(CRef<CSeq_entry>& se);
    bool HandleSeqEntry(const CSeq_entry_Handle& seh);

protected:
    void HandleSeqSubmit(CObjectIStream& is );
    void HandleSeqId(const string& id);

    CSeq_entry_Handle ObtainSeqEntryFromSeqEntry(CObjectIStream& is);
    CSeq_entry_Handle ObtainSeqEntryFromBioseq(CObjectIStream& is);
    CSeq_entry_Handle ObtainSeqEntryFromBioseqSet(CObjectIStream& is);

private:
    // types
    typedef CFlatFileConfig::TFormat        TFormat;
    typedef CFlatFileConfig::TMode          TMode;
    typedef CFlatFileConfig::TStyle         TStyle;
    typedef CFlatFileConfig::TFlags         TFlags;
    typedef CFlatFileConfig::TView          TView;
    typedef CFlatFileConfig::TGffOptions    TGffOptions;
    typedef CFlatFileConfig::TGenbankBlocks TGenbankBlocks;
    typedef CFlatFileConfig::CGenbankBlockCallback TGenbankBlockCallback;

    CObjectIStream* x_OpenIStream(const CArgs& args);

    CFlatFileGenerator* x_CreateFlatFileGenerator(const CArgs& args);
    TFormat         x_GetFormat(const CArgs& args);
    TMode           x_GetMode(const CArgs& args);
    TStyle          x_GetStyle(const CArgs& args);
    TFlags          x_GetFlags(const CArgs& args);
    TView           x_GetView(const CArgs& args);
    TGenbankBlocks  x_GetGenbankBlocks(const CArgs& args);
    TGenbankBlockCallback* x_GetGenbankCallback(const CArgs& args);
    TSeqPos x_GetFrom(const CArgs& args);
    TSeqPos x_GetTo  (const CArgs& args);
    void x_GetLocation(const CSeq_entry_Handle& entry,
        const CArgs& args, CSeq_loc& loc);
    CBioseq_Handle x_DeduceTarget(const CSeq_entry_Handle& entry);
    void x_CreateCancelBenchmarkCallback(void);

    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CRef<CScope>                m_Scope;
    CNcbiOstream*               m_Os;           // Output stream
    CRef<CFlatFileGenerator>    m_FFGenerator;  // Flat-file generator
    auto_ptr<ICanceled>         m_pCanceledCallback;
    bool                        m_do_cleanup;
};


void CAsn2FlatApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
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

         // output
         arg_desc->AddOptionalKey("o", "OutputFile",
                                  "Output file name",
                                  CArgDescriptions::eOutputFile);
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

    // report
    {{
         arg_desc->SetCurrentGroup("Formatting Options");
         // format (default: genbank)
         arg_desc->AddDefaultKey("format", "Format",
                                 "Output format",
                                 CArgDescriptions::eString, "genbank");
         arg_desc->SetConstraint("format",
                                 &(*new CArgAllow_Strings,
                                   "genbank", "embl", "ddbj", "gbseq", "ftable", "gff", "gff3"));

         // mode (default: dump)
         arg_desc->AddDefaultKey("mode", "Mode",
                                 "Restriction level",
                                 CArgDescriptions::eString, "gbench");
         arg_desc->SetConstraint("mode",
                                 &(*new CArgAllow_Strings, "release", "entrez", "gbench", "dump"));

         // style (default: normal)
         arg_desc->AddDefaultKey("style", "Style",
                                 "Formatting style",
                                 CArgDescriptions::eString, "normal");
         arg_desc->SetConstraint("style",
                                 &(*new CArgAllow_Strings, "normal", "segment", "master", "contig"));

         // flags (default: 0)
         arg_desc->AddDefaultKey("flags", "Flags",
                                 "Flags controlling flat file output.  The value is the bitwise OR (logical addition) of:\n"
                                 "         1 - show HTML report\n"
                                 "         2 - show contig features\n"
                                 "         4 - show contig sources\n"
                                 "         8 - show far translations\n"
                                 "        16 - show translations if there are no products\n"
                                 "        32 - always translate CDS\n"
                                 "        64 - show only near features\n"
                                 "       128 - show far features on segs\n"
                                 "       256 - copy CDS feature from cDNA\n"
                                 "       512 - copy gene to cDNA\n"
                                 "      1024 - show contig in master\n"
                                 "      2048 - hide imported features\n"
                                 "      4096 - hide remote imported features\n"
                                 "      8192 - hide SNP features\n"
                                 "     16384 - hide exon features\n"
                                 "     32768 - hide intron features\n"
                                 "     65536 - hide misc features\n"
                                 "    131072 - hide CDS product features\n"
                                 "    262144 - hide CDD features\n"
                                 "    542288 - show transcript sequence\n"
                                 "   1048576 - show peptides\n"
                                 "   2097152 - hide GeneRIFs\n"
                                 "   4194304 - show only GeneRIFs\n"
                                 "   8388608 - show only the latest GeneRIFs\n"
                                 "  16777216 - show contig and sequence\n"
                                 "  33554432 - hide source features\n"
                                 "  67108864 - show feature table references\n"
                                 " 134217728 - use the old feature sort order\n"
                                 " 268435456 - hide gap features\n"
                                 " 536870912 - do not translate the CDS\n"
                                 "1073741824 - show javascript sequence spans",

                                 CArgDescriptions::eInteger, "0");

         arg_desc->AddOptionalKey("showblocks", "COMMA_SEPARATED_BLOCK_LIST", 
             "Use this to only show certain parts of the flatfile (e.g. '-showblocks locus,defline').  "
             "These are all possible values for block names: " + NStr::Join(CFlatFileConfig::GetAllGenbankStrings(), ", "),
             CArgDescriptions::eString );
         arg_desc->AddOptionalKey("skipblocks", "COMMA_SEPARATED_BLOCK_LIST", 
             "Use this to skip certain parts of the flatfile (e.g. '-skipblocks sequence,origin').  "
             "These are all possible values for block names: " + NStr::Join(CFlatFileConfig::GetAllGenbankStrings(), ", "),
             CArgDescriptions::eString );
         // don't allow both because it's not really clear what the user intended.
         arg_desc->SetDependency("showblocks", CArgDescriptions::eExcludes, "skipblocks");

         arg_desc->AddFlag("demo-genbank-callback",
             "When set (and genbank mode is used), this program will demonstrate the use of "
             "genbank callbacks via a very simple callback that just prints its output to stderr, then "
             "prints some statistics.  To demonstrate halting of flatfile generation, the genbank callback "
             "will halt flatfile generation if it encounters an item with the words 'HALT TEST'.  To demonstrate skipping a block, it will skip blocks with the words 'SKIP TEST' in them.  Also, blocks with the words 'MODIFY TEST' in them will have the text 'MODIFY TEST' turned into 'WAS MODIFIED TEST'.");

         arg_desc->AddFlag("no-external",
                           "Disable all external annotation sources");

         arg_desc->AddFlag("resolve-all",
                           "Resolves all, e.g. for contigs.");

         arg_desc->AddFlag("show-flags",
                           "Describe the current flag set in ENUM terms");

         // view (default: nucleotide)
         arg_desc->AddDefaultKey("view", "View", "View",
                                 CArgDescriptions::eString, "nuc");
         arg_desc->SetConstraint("view",
                                 &(*new CArgAllow_Strings, "all", "prot", "nuc"));

         // from
         arg_desc->AddOptionalKey("from", "From",
                                  "Begining of shown range", CArgDescriptions::eInteger);

         // to
         arg_desc->AddOptionalKey("to", "To",
                                  "End of shown range", CArgDescriptions::eInteger);

         // strand
         arg_desc->AddDefaultKey("count", "Count", "Number of runs",
                                 CArgDescriptions::eInteger, "1");

         // accession to extract

         // html
         arg_desc->AddFlag("html", "Produce HTML output");
     }}

    // misc
    {{
         // no-cleanup
         arg_desc->AddFlag("nocleanup",
                           "Do not perform data cleanup prior to formatting");
         // remote
         arg_desc->AddFlag("gbload", "Use GenBank data loader");
     }}

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

     arg_desc->SetCurrentGroup(kEmptyStr);
     SetupArgDescriptions(arg_desc.release());
}


int CAsn2FlatApp::Run(void)
{
    // initialize conn library
    CONNECT_Init(&GetConfig());

    const CArgs&   args = GetArgs();

    CSignal::TrapSignals(CSignal::eSignal_USR1);

    // create object manager
    m_Objmgr = CObjectManager::GetInstance();
    if ( !m_Objmgr ) {
        NCBI_THROW(CException, eUnknown, "Could not create object manager");
    }
    if (args["gbload"]  ||  args["id"]  ||  args["ids"]) {
        CGBDataLoader::RegisterInObjectManager(*m_Objmgr);
    }
    m_Scope.Reset(new CScope(*m_Objmgr));
    m_Scope->AddDefaults();

    // open the output stream
    m_Os = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
    if ( m_Os == 0 ) {
        NCBI_THROW(CException, eUnknown, "Could not open output stream");
    }

    // so we don't fail silently if, e.g., the output disk gets full
    m_Os->exceptions( ios::failbit | ios::badbit );

    // create the flat-file generator
    m_FFGenerator.Reset(x_CreateFlatFileGenerator(args));
    if (args["no-external"]) {
        m_FFGenerator->SetAnnotSelector()
            .SetExcludeExternal(true);
    }
    if( args["resolve-all"]) {
        m_FFGenerator->SetAnnotSelector().SetResolveAll();
    }

    m_do_cleanup = ( ! args["nocleanup"]);

    auto_ptr<CObjectIStream> is;
    is.reset( x_OpenIStream( args ) );
    if (is.get() == NULL) {
        string msg = args["i"]? "Unable to open input file" + args["i"].AsString() :
            "Unable to read data from stdin";
        NCBI_THROW(CException, eUnknown, msg);
    }

    if ( args[ "sub" ] ) {
        HandleSeqSubmit( *is );
        return 0;
    }

    if ( args[ "batch" ] ) {
        CGBReleaseFile in( *is.release() );
        in.RegisterHandler( this );
        in.Read();  // HandleSeqEntry will be called from this function
        return 0;
    }

    if ( args[ "ids" ] ) {
        CNcbiIstream& istr = args["ids"].AsInputFile();
        string id_str;
        while (NcbiGetlineEOL(istr, id_str)) {
            id_str = NStr::TruncateSpaces(id_str);
            if (id_str.empty()  ||  id_str[0] == '#') {
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
        return 0;
    }

    if ( args[ "id" ] ) {
        HandleSeqId( args[ "id" ].AsString() );
        return 0;
    }

    string asn_type = args["type"].AsString();

    if ( asn_type == "seq-entry" ) {
        //
        //  Straight through processing: Read a seq_entry, then process
        //  a seq_entry:
        //
        CSeq_entry_Handle seh = ObtainSeqEntryFromSeqEntry(*is);
        if ( !seh ) {
            NCBI_THROW(CException, eUnknown,
                       "Unable to construct Seq-entry object" );
        }
        HandleSeqEntry(seh);
    }
    else if ( asn_type == "bioseq" ) {                
        //
        //  Read object as a bioseq, wrap it into a seq_entry, then process
        //  the wrapped bioseq as a seq_entry:
        //
        CSeq_entry_Handle seh = ObtainSeqEntryFromBioseq(*is);
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
        CSeq_entry_Handle seh = ObtainSeqEntryFromBioseqSet(*is);
        if ( !seh ) {
            NCBI_THROW(CException, eUnknown,
                       "Unable to construct Seq-entry object" );
        }
        HandleSeqEntry(seh);
    }
    else if ( asn_type == "seq-submit" ) {
        HandleSeqSubmit( *is );
    }
    else if ( asn_type == "any" ) {
        //
        //  Try the first four in turn:
        //
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
                    if (sub->IsSetSub()  &&  sub->IsSetData()) {
                        CRef<CScope> scope(new CScope(*m_Objmgr));
                        scope->AddDefaults();
                        if ( m_do_cleanup ) {
                            CCleanup cleanup;
                            cleanup.BasicCleanup( *sub );
                        }
                        m_FFGenerator->Generate(*sub, *scope, *m_Os);
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
    }

    return 0;
}

//  ============================================================================
void CAsn2FlatApp::HandleSeqSubmit(CObjectIStream& is )
//  ============================================================================
{
    CRef<CSeq_submit> sub(new CSeq_submit);
    is >> *sub;
    if (sub->IsSetSub()  &&  sub->IsSetData()) {
        CRef<CScope> scope(new CScope(*m_Objmgr));
        scope->AddDefaults();
        if ( m_do_cleanup ) {
            CCleanup cleanup;
            cleanup.BasicCleanup( *sub );
        }
        m_FFGenerator->Generate(*sub, *scope, *m_Os);
    }
}

//  ============================================================================
void CAsn2FlatApp::HandleSeqId(
    const string& strId )
//  ============================================================================
{
    CConstRef<CSeq_entry> entry;

    // This C++-scope gets a raw CSeq_entry that has no attachment
    // to any CScope and puts it into entry.
    {
        CSeq_id id(strId);
        CRef<CScope> scope(new CScope(*m_Objmgr));
        scope->AddDefaults();
        CBioseq_Handle bsh = scope->GetBioseqHandle( id );
        if ( ! bsh ) {
            NCBI_THROW(
                CException, eUnknown,
                "Unable to retrieve data for the given ID"
                );
        }
        CSeq_entry_Handle entry_h = bsh.GetParentEntry();

        entry = entry_h.GetCompleteSeq_entry();
    }

    // we have to remove the Genbank loader before we call AddTopLevelSeqEntry,
    // or else we'll get an "already added to scope" exception.
    // we add it back afterwards.
    // The idea of this maneuver is so the GenBank loader isn't permanently
    // locked by CAsn2FlatApp::HandleSeqId.
    m_Scope->RemoveDataLoader("GBLOADER");
    CSeq_entry_Handle new_entry_h = m_Scope->AddTopLevelSeqEntry(*entry);
    m_Scope->AddDataLoader("GBLOADER");

    //
    //  ... and use that to generate the flat file:
    //
    HandleSeqEntry( new_entry_h );
}

//  ============================================================================
bool CAsn2FlatApp::HandleSeqEntry(const CSeq_entry_Handle& seh )
//  ============================================================================
{
    const CArgs& args = GetArgs();

    CSeq_entry_EditHandle seeh = seh.GetEditHandle();
    CRef<CSeq_entry> tmp_se (new CSeq_entry);
    tmp_se->Assign(*seeh.GetCompleteSeq_entry());

    if ( m_do_cleanup ) {
        CCleanup cleanup;
        cleanup.BasicCleanup( *tmp_se );

        seeh.SelectNone();
        switch (tmp_se->Which()) {
            case CSeq_entry::e_Seq:  seeh.SelectSeq(tmp_se->SetSeq());  break;
            case CSeq_entry::e_Set:  seeh.SelectSet(tmp_se->SetSet());  break;
            default:                 _TROUBLE;
        }
    }

    // generate flat file
    if ( args["from"]  ||  args["to"] ) {
        CSeq_loc loc;
        x_GetLocation( seh, args, loc );
        m_FFGenerator->Generate(loc, seh.GetScope(), *m_Os);
    }
    else {
        int count = args["count"].AsInteger();
        for ( int i = 0; i < count; ++i ) {
            m_FFGenerator->Generate( seh, *m_Os);
        }

    }
    return true;
}

CSeq_entry_Handle CAsn2FlatApp::ObtainSeqEntryFromSeqEntry(CObjectIStream& is)
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
        ERR_POST(Error << e);
    }
    return CSeq_entry_Handle();
}

CSeq_entry_Handle CAsn2FlatApp::ObtainSeqEntryFromBioseq(CObjectIStream& is)
{
    try {
        CRef<CBioseq> bs(new CBioseq);
        is >> *bs;
        CBioseq_Handle bsh = m_Scope->AddBioseq(*bs);
        return bsh.GetTopLevelEntry();
    }
    catch (CException& e) {
        ERR_POST(Error << e);
    }
    return CSeq_entry_Handle();
}

CSeq_entry_Handle CAsn2FlatApp::ObtainSeqEntryFromBioseqSet(CObjectIStream& is)
{
    try {
        CRef<CSeq_entry> entry(new CSeq_entry);
        is >> entry->SetSet();
        return m_Scope->AddTopLevelSeqEntry(*entry);
    }
    catch (CException& e) {
        ERR_POST(Error << e);
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
    CObjectIStream* pI = 0;
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


CFlatFileGenerator* CAsn2FlatApp::x_CreateFlatFileGenerator(const CArgs& args)
{
    TFormat        format         = x_GetFormat(args);
    TMode          mode           = x_GetMode(args);
    TStyle         style          = x_GetStyle(args);
    TFlags         flags          = x_GetFlags(args);
    TView          view           = x_GetView(args);
    TGffOptions    gff_options    = CFlatFileConfig::fGffGTFCompat;
    TGenbankBlocks genbank_blocks = x_GetGenbankBlocks(args);
    CRef<TGenbankBlockCallback> genbank_callback( x_GetGenbankCallback(args) );

    if( args["benchmark-cancel-checking"] ) {
        x_CreateCancelBenchmarkCallback();
    }

    CFlatFileConfig cfg(
        format, mode, style, flags, view, gff_options, genbank_blocks, 
        genbank_callback.GetPointerOrNull(), m_pCanceledCallback.get() );
    return new CFlatFileGenerator(cfg);
}


CAsn2FlatApp::TFormat CAsn2FlatApp::x_GetFormat(const CArgs& args)
{
    const string& format = args["format"].AsString();
    if ( format == "genbank" ) {
        return CFlatFileConfig::eFormat_GenBank;
    } else if ( format == "embl" ) {
        return CFlatFileConfig::eFormat_EMBL;
    } else if ( format == "ddbj" ) {
        return CFlatFileConfig::eFormat_DDBJ;
    } else if ( format == "gbseq" ) {
        return CFlatFileConfig::eFormat_GBSeq;
    } else if ( format == "ftable" ) {
        return CFlatFileConfig::eFormat_FTable;
    }
    if (format == "gff"  ||  format == "gff3") {
        string msg = 
            "Asn2flat no longer supports GFF and GFF3 generation. "
            "For state-of-the-art GFF output, use annotwriter.";
        NCBI_THROW(CException, eInvalid, msg);
    }
    // default
    return CFlatFileConfig::eFormat_GenBank;
}


CAsn2FlatApp::TMode CAsn2FlatApp::x_GetMode(const CArgs& args)
{
    const string& mode = args["mode"].AsString();
    if ( mode == "release" ) {
        return CFlatFileConfig::eMode_Release;
    } else if ( mode == "entrez" ) {
        return CFlatFileConfig::eMode_Entrez;
    } else if ( mode == "gbench" ) {
        return CFlatFileConfig::eMode_GBench;
    } else if ( mode == "dump" ) {
        return CFlatFileConfig::eMode_Dump;
    }

    // default
    return CFlatFileConfig::eMode_GBench;
}


CAsn2FlatApp::TStyle CAsn2FlatApp::x_GetStyle(const CArgs& args)
{
    const string& style = args["style"].AsString();
    if ( style == "normal" ) {
        return CFlatFileConfig::eStyle_Normal;
    } else if ( style == "segment" ) {
        return CFlatFileConfig::eStyle_Segment;
    } else if ( style == "master" ) {
        return CFlatFileConfig::eStyle_Master;
    } else if ( style == "contig" ) {
        return CFlatFileConfig::eStyle_Contig;
    }

    // default
    return CFlatFileConfig::eStyle_Normal;
}


CAsn2FlatApp::TFlags CAsn2FlatApp::x_GetFlags(const CArgs& args)
{
    TFlags flags = args["flags"].AsInteger();
    if (args["html"]) {
        flags |= CFlatFileConfig::fDoHTML;
    }

    if (args["show-flags"]) {

        typedef pair<CFlatFileConfig::EFlags, const char*> TFlagDescr;
        static const TFlagDescr kDescrTable[] = {
            TFlagDescr(CFlatFileConfig::fDoHTML,
                       "CFlatFileConfig::fDoHTML"),
            TFlagDescr(CFlatFileConfig::fShowContigFeatures,
                       "CFlatFileConfig::fShowContigFeatures"),
            TFlagDescr(CFlatFileConfig::fShowContigSources,
                       "CFlatFileConfig::fShowContigSources"),
            TFlagDescr(CFlatFileConfig::fShowFarTranslations,
                       "CFlatFileConfig::fShowFarTranslations"),
            TFlagDescr(CFlatFileConfig::fTranslateIfNoProduct,
                       "CFlatFileConfig::fTranslateIfNoProduct"),
            TFlagDescr(CFlatFileConfig::fAlwaysTranslateCDS,
                       "CFlatFileConfig::fAlwaysTranslateCDS"),
            TFlagDescr(CFlatFileConfig::fOnlyNearFeatures,
                       "CFlatFileConfig::fOnlyNearFeatures"),
            TFlagDescr(CFlatFileConfig::fFavorFarFeatures,
                       "CFlatFileConfig::fFavorFarFeatures"),
            TFlagDescr(CFlatFileConfig::fCopyCDSFromCDNA,
                       "CFlatFileConfig::fCopyCDSFromCDNA"),
            TFlagDescr(CFlatFileConfig::fCopyGeneToCDNA,
                       "CFlatFileConfig::fCopyGeneToCDNA"),
            TFlagDescr(CFlatFileConfig::fShowContigInMaster,
                       "CFlatFileConfig::fShowContigInMaster"),
            TFlagDescr(CFlatFileConfig::fHideImpFeatures,
                       "CFlatFileConfig::fHideImpFeatures"),
            TFlagDescr(CFlatFileConfig::fHideRemoteImpFeatures,
                       "CFlatFileConfig::fHideRemoteImpFeatures"),
            TFlagDescr(CFlatFileConfig::fHideSNPFeatures,
                       "CFlatFileConfig::fHideSNPFeatures"),
            TFlagDescr(CFlatFileConfig::fHideExonFeatures,
                       "CFlatFileConfig::fHideExonFeatures"),
            TFlagDescr(CFlatFileConfig::fHideIntronFeatures,
                       "CFlatFileConfig::fHideIntronFeatures"),
            TFlagDescr(CFlatFileConfig::fHideMiscFeatures,
                       "CFlatFileConfig::fHideMiscFeatures"),
            TFlagDescr(CFlatFileConfig::fHideCDSProdFeatures,
                       "CFlatFileConfig::fHideCDSProdFeatures"),
            TFlagDescr(CFlatFileConfig::fHideCDDFeatures,
                       "CFlatFileConfig::fHideCDDFeatures"),
            TFlagDescr(CFlatFileConfig::fShowTranscript,
                       "CFlatFileConfig::fShowTranscript"),
            TFlagDescr(CFlatFileConfig::fShowPeptides,
                       "CFlatFileConfig::fShowPeptides"),
            TFlagDescr(CFlatFileConfig::fHideGeneRIFs,
                       "CFlatFileConfig::fHideGeneRIFs"),
            TFlagDescr(CFlatFileConfig::fOnlyGeneRIFs,
                       "CFlatFileConfig::fOnlyGeneRIFs"),
            TFlagDescr(CFlatFileConfig::fLatestGeneRIFs,
                       "CFlatFileConfig::fLatestGeneRIFs"),
            TFlagDescr(CFlatFileConfig::fShowContigAndSeq,
                       "CFlatFileConfig::fShowContigAndSeq"),
            TFlagDescr(CFlatFileConfig::fHideSourceFeatures,
                       "CFlatFileConfig::fHideSourceFeatures"),
            TFlagDescr(CFlatFileConfig::fShowFtableRefs,
                       "CFlatFileConfig::fShowFtableRefs"),
            TFlagDescr(CFlatFileConfig::fOldFeaturesOrder,
                       "CFlatFileConfig::fOldFeaturesOrder"),
            TFlagDescr(CFlatFileConfig::fHideGapFeatures,
                       "CFlatFileConfig::fHideGapFeatures"),
            TFlagDescr(CFlatFileConfig::fNeverTranslateCDS,
                       "CFlatFileConfig::fNeverTranslateCDS")
        };
        static size_t kArraySize = sizeof(kDescrTable) / sizeof(TFlagDescr);
        for (size_t i = 0;  i < kArraySize;  ++i) {
            if (flags & kDescrTable[i].first) {
                LOG_POST(Error << "flag: "
                         << std::left << setw(40) << kDescrTable[i].second
                         << " = "
                         << std::right << setw(10) << kDescrTable[i].first
                        );
            }
        }
    }

    return flags;
}


CAsn2FlatApp::TView CAsn2FlatApp::x_GetView(const CArgs& args)
{
    const string& view = args["view"].AsString();
    if ( view == "all" ) {
        return CFlatFileConfig::fViewAll;
    } else if ( view == "prot" ) {
        return CFlatFileConfig::fViewProteins;
    } else if ( view == "nuc" ) {
        return CFlatFileConfig::fViewNucleotides;
    }

    // default
    return CFlatFileConfig::fViewNucleotides;
}

CAsn2FlatApp::TGenbankBlocks CAsn2FlatApp::x_GetGenbankBlocks(const CArgs& args)
{
    const static CAsn2FlatApp::TGenbankBlocks kDefault = 
        CFlatFileConfig::fGenbankBlocks_All;

    string blocks_arg;
    // set to true if we're hiding the blocks given instead of showing them
    bool bInvertFlags = false; 
    if( args["showblocks"] ) {
        blocks_arg = args["showblocks"].AsString();
    } else if( args["skipblocks"] ) {
        blocks_arg = args["skipblocks"].AsString();
        bInvertFlags = true;
    } else {
        return kDefault;
    }

    // turn the blocks into one mask
    CAsn2FlatApp::TGenbankBlocks fBlocksGiven = 0;
    vector<string> vecOfBlockNames;
    NStr::Tokenize(blocks_arg, ",", vecOfBlockNames);
    ITERATE(vector<string>, name_iter, vecOfBlockNames) {
        // Note that StringToGenbankBlock throws an
        // exception if it gets an illegal value.
        CAsn2FlatApp::TGenbankBlocks fThisBlock =
            CFlatFileConfig::StringToGenbankBlock(
            NStr::TruncateSpaces(*name_iter));
        fBlocksGiven |= fThisBlock;
    }

    return ( bInvertFlags ? ~fBlocksGiven : fBlocksGiven );
}

CAsn2FlatApp::TGenbankBlockCallback* 
CAsn2FlatApp::x_GetGenbankCallback(const CArgs& args)
{
    class CSimpleCallback : public TGenbankBlockCallback {
    public:
        CSimpleCallback(void) { }
        virtual ~CSimpleCallback(void) 
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

        virtual EBioseqSkip notify_bioseq( CBioseqContext & ctx )
        {
            cerr << "notify_bioseq called." << endl;
            return eBioseqSkip_No;
        }

        // this macro is the lesser evil compared to the messiness that
        // you would see here otherwise. (plus it's less error-prone)
#define SIMPLE_CALLBACK_NOTIFY(TItemClass) \
        virtual EAction notify( string & block_text, \
                                const CBioseqContext& ctx, \
                                const TItemClass & item ) { \
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
        typedef map<string, int> TTypeToCountMap;
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

        void x_PrintAverageStats(void) {
            ITERATE( TTypeToCountMap, map_iter, m_TypeAppearancesMap ) {
                const string sType = map_iter->first;
                const int iAppearances = map_iter->second;
                const int iTotalCharacters = m_TypeCharsMap[sType];
                cerr << setw(25) << left << (sType + ':')
                     << " " << (iTotalCharacters / iAppearances) << endl;
            }
        }
    };

    if( args["demo-genbank-callback"] ) {
        return new CSimpleCallback;
    } else {
        return NULL;
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

    if ( from == CRange<TSeqPos>::GetWholeFrom()  &&  to == length ) {
        // whole
        loc.SetWhole().Assign(*h.GetSeqId());
    } else {
        // interval
        loc.SetInt().SetId().Assign(*h.GetSeqId());
        loc.SetInt().SetFrom(from);
        loc.SetInt().SetTo(to);
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
                if ( h  &&  CSeq_inst::IsNa(h.GetInst_Mol()) ) {
                    return h;
                }
            }
        }
        break;
    case CBioseq_set::eClass_gen_prod_set:
        // return the genomic
        for ( CSeq_entry_CI it(bsst); it; ++it ) {
            if ( it->IsSeq()  &&
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
    default:
        break;
    }
    NCBI_THROW(CFlatException, eInvalidParam,
            "Cannot deduce target bioseq.");
}

void
CAsn2FlatApp::x_CreateCancelBenchmarkCallback(void)
{
    // This ICanceled interface always says "keep going"
    // unless SIGUSR1 is received,
    // and it keeps statistics on how often it is checked
    class CCancelBenchmarking : public ICanceled {
    public:
        CCancelBenchmarking(void)
            : m_TimeOfLastCheck(CTime::eCurrent)
        {
        }


        ~CCancelBenchmarking(void)
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


        virtual bool IsCanceled(void) const 
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
        size_t x_kGetNumToSaveAtStartAndEnd(void) const { return 10; }

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

END_NCBI_SCOPE

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//
// Main

int main(int argc, const char** argv)
{
    return CAsn2FlatApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}

