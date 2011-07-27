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
    typedef CFlatFileConfig::TFormat    TFormat;
    typedef CFlatFileConfig::TMode      TMode;
    typedef CFlatFileConfig::TStyle     TStyle;
    typedef CFlatFileConfig::TFlags     TFlags;
    typedef CFlatFileConfig::TView      TView;

    CObjectIStream* x_OpenIStream(const CArgs& args);

    CFlatFileGenerator* x_CreateFlatFileGenerator(const CArgs& args);
    TFormat         x_GetFormat(const CArgs& args);
    TMode           x_GetMode(const CArgs& args);
    TStyle          x_GetStyle(const CArgs& args);
    TFlags          x_GetFlags(const CArgs& args);
    TView           x_GetView(const CArgs& args);
    TSeqPos x_GetFrom(const CArgs& args);
    TSeqPos x_GetTo  (const CArgs& args);
    void x_GetLocation(const CSeq_entry_Handle& entry,
        const CArgs& args, CSeq_loc& loc);
    CBioseq_Handle x_DeduceTarget(const CSeq_entry_Handle& entry);

    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CRef<CScope>                m_Scope;
    CNcbiOstream*               m_Os;           // Output stream
    CRef<CFlatFileGenerator>    m_FFGenerator;  // Flat-file generator
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
                                  &( *new CArgAllow_Strings, "any", "seq-entry", "bioseq", "bioseq-set" ) );

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
                                 "        1 - show HTML report\n"
                                 "        2 - show contig features\n"
                                 "        4 - show contig sources\n"
                                 "        8 - show far translations\n"
                                 "       16 - show translations if there are no products\n"
                                 "       32 - always translate CDS\n"
                                 "       64 - show only near features\n"
                                 "      128 - show far features on segs\n"
                                 "      256 - copy CDS feature from cDNA\n"
                                 "      512 - copy gene to cDNA\n"
                                 "     1024 - show contig in master\n"
                                 "     2048 - hide imported features\n"
                                 "     4096 - hide remote imported features\n"
                                 "     8192 - hide SNP features\n"
                                 "    16384 - hide exon features\n"
                                 "    32768 - hide intron features\n"
                                 "    65536 - hide misc features\n"
                                 "   131072 - hide CDS product features\n"
                                 "   262144 - hide CDD features\n"
                                 "   542288 - show transcript sequence\n"
                                 "  1048576 - show peptides\n"
                                 "  2097152 - hide GeneRIFs\n"
                                 "  4194304 - show only GeneRIFs\n"
                                 "  8388608 - show only the latest GeneRIFs\n"
                                 " 16777216 - show contig and sequence\n"
                                 " 33554432 - hide source features\n"
                                 " 67108864 - show feature table references\n"
                                 "134217728 - use the old feature sort order\n"
                                 "268435456 - hide gap features\n"
                                 "536870912 - do not translate the CDS",

                                 CArgDescriptions::eInteger, "0");

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
    SetupArgDescriptions(arg_desc.release());
}


int CAsn2FlatApp::Run(void)
{
    // initialize conn library
    CONNECT_Init(&GetConfig());

    const CArgs&   args = GetArgs();

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
    else if ( asn_type == "any" ) {
        //
        //  Try the first three in turn:
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
                    NCBI_THROW(
                               CException, eUnknown,
                               "Unable to construct Seq-entry object"
                              );
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
        m_FFGenerator->Generate(*sub, *scope, *m_Os);
    }
}

//  ============================================================================
void CAsn2FlatApp::HandleSeqId(
    const string& strId )
//  ============================================================================
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

    //
    //  ... and use that to generate the flat file:
    //
    HandleSeqEntry(bsh.GetParentEntry() );
}

//  ============================================================================
bool CAsn2FlatApp::HandleSeqEntry(const CSeq_entry_Handle& seh )
//  ============================================================================
{
    const CArgs& args = GetArgs();

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
        pI = CObjectIStream::Open( serial, *pUnzipStream, true );
    }
    else {
        pI = CObjectIStream::Open( serial, *pInputStream, bDeleteOnClose );
    }

    if ( 0 != pI ) {
        pI->UseMemoryPool();
    }
    return pI;
}


CFlatFileGenerator* CAsn2FlatApp::x_CreateFlatFileGenerator(const CArgs& args)
{
    TFormat    format = x_GetFormat(args);
    TMode      mode   = x_GetMode(args);
    TStyle     style  = x_GetStyle(args);
    TFlags     flags  = x_GetFlags(args);
    TView      view   = x_GetView(args);

    CFlatFileConfig cfg(format, mode, style, flags, view);
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
    } else if ( format == "gff" ) {
        return CFlatFileConfig::eFormat_GFF;
    } else if ( format == "gff3" ) {
        return CFlatFileConfig::eFormat_GFF3;
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


END_NCBI_SCOPE

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//
// Main

int main(int argc, const char** argv)
{
//    return CAsn2FlatApp().AppMain(argc, argv, 0, eDS_ToStderr, "config.ini");
    return CAsn2FlatApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
