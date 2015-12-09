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
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_signal.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <dbapi/driver/drivers.hpp>
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
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>
#include <objtools/readers/fasta.hpp>

#include <sra/data_loaders/wgs/wgsloader.hpp>

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
    CAsn2FlatApp (void);
    ~CAsn2FlatApp (void);

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
    void x_CreateCancelBenchmarkCallback(void);

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
    auto_ptr<ICanceled>         m_pCanceledCallback;
    bool                        m_do_cleanup;
};


// constructor
CAsn2FlatApp::CAsn2FlatApp (void)
{
    SetVersion(CVersionInfo(0, 9, 16));
}

// destructor
CAsn2FlatApp::~CAsn2FlatApp (void)

{
}

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

     arg_desc->SetCurrentGroup(kEmptyStr);
     SetupArgDescriptions(arg_desc.release());
}


CNcbiOstream* CAsn2FlatApp::OpenFlatfileOstream(const string& name)
{
    const CArgs& args = GetArgs();

    if (! args[name]) return NULL;

    CNcbiOstream* flatfile_os = &(args[name].AsOutputFile());

    // so we don't fail silently if, e.g., the output disk gets full
    flatfile_os->exceptions( ios::failbit | ios::badbit );

    return flatfile_os;
}


int CAsn2FlatApp::Run(void)
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

    CSignal::TrapSignals(CSignal::eSignal_USR1);

    // create object manager
    m_Objmgr = CObjectManager::GetInstance();
    if ( !m_Objmgr ) {
        NCBI_THROW(CException, eUnknown, "Could not create object manager");
    }
    if (args["gbload"]  ||  args["id"]  ||  args["ids"]) {
        CGBDataLoader::RegisterInObjectManager(*m_Objmgr);
        CWGSDataLoader::RegisterInObjectManager(*m_Objmgr,
                                                CObjectManager::eDefault,
                                                88);
    }
    m_Scope.Reset(new CScope(*m_Objmgr));
    m_Scope->AddDefaults();

    // open the output streams
    m_Os = OpenFlatfileOstream ("o");
    m_On = OpenFlatfileOstream ("on");
    m_Og = OpenFlatfileOstream ("og");
    m_Or = OpenFlatfileOstream ("or");
    m_Op = OpenFlatfileOstream ("op");
    m_Ou = OpenFlatfileOstream ("ou");

    m_OnlyNucs = false;
    m_OnlyProts = false;
    if (m_Os != NULL) {
        const string& view = args["view"].AsString();
        m_OnlyNucs = (view == "nuc");
        m_OnlyProts = (view == "prot");
    }

    if (m_Os == NULL  &&  m_On == NULL  &&  m_Og == NULL  &&
        m_Or == NULL  &&  m_Op == NULL  &&  m_Ou == NULL) {
        // No output (-o*) argument given - default to stdout
        m_Os = &cout;
    }

    // create the flat-file generator
    m_FFGenerator.Reset(x_CreateFlatFileGenerator(args));
    if (args["no-external"]) {
        m_FFGenerator->SetAnnotSelector().SetExcludeExternal(true);
    }
    if( args["resolve-all"]) {
        m_FFGenerator->SetAnnotSelector().SetResolveAll();
    }
    if( args["depth"] ) {
        m_FFGenerator->SetAnnotSelector().SetResolveDepth(args["depth"].AsInteger());
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
        bool propagate = args[ "p" ];
        CGBReleaseFile in( *is.release(), propagate );
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
        while ( !is->EndOfData() ) {
            CSeq_entry_Handle seh = ObtainSeqEntryFromSeqEntry(*is);
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
            CSeq_entry_Handle seh = ObtainSeqEntryFromBioseq(*is);
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
            CSeq_entry_Handle seh = ObtainSeqEntryFromBioseqSet(*is);
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
            m_Scope->RemoveTopLevelSeqEntry(seh);
        }
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

    m_FFGenerator->SetFeatTree(new feature::CFeatTree(seh));
    
    for (CBioseq_CI bioseq_it(seh);  bioseq_it;  ++bioseq_it) {
        CBioseq_Handle bsh = *bioseq_it;
        CConstRef<CBioseq> bsr = bsh.GetCompleteBioseq();

        CNcbiOstream* flatfile_os = NULL;

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

        if ( m_Os != NULL ) {
            if ( m_OnlyNucs && ! bsh.IsNa() ) continue;
            if ( m_OnlyProts && ! bsh.IsAa() ) continue;
            flatfile_os = m_Os;
        } else if ( bsh.IsNa() ) {
            if ( m_On != NULL ) {
                flatfile_os = m_On;
            } else if ( (is_genomic || ! closest_molinfo) && m_Og != NULL ) {
                flatfile_os = m_Og;
            } else if ( is_RNA && m_Or != NULL ) {
                flatfile_os = m_Or;
            } else {
                continue;
            }
        } else if ( bsh.IsAa() ) {
            if ( m_Op != NULL ) {
                flatfile_os = m_Op;
            }
        } else {
            if ( m_Ou != NULL ) {
                flatfile_os = m_Ou;
            } else if ( m_On != NULL ) {
                flatfile_os = m_On;
            } else {
                continue;
            }
        }

        if ( flatfile_os == NULL ) continue;

        // generate flat file
        if ( args["from"]  ||  args["to"]  ||  args["strand"] ) {
            CSeq_loc loc;
            x_GetLocation( seh, args, loc );
            m_FFGenerator->Generate(loc, seh.GetScope(), *flatfile_os);
        }
        else {
            int count = args["count"].AsInteger();
            for ( int i = 0; i < count; ++i ) {
                m_FFGenerator->Generate( bsh, *flatfile_os);
            }

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
    CFlatFileConfig cfg;
    cfg.FromArguments(args);
    m_do_cleanup = ( ! args["nocleanup"]);
    cfg.BasicCleanup(false);

    CRef<TGenbankBlockCallback> genbank_callback( x_GetGenbankCallback(args) );

    if( args["benchmark-cancel-checking"] ) {
        x_CreateCancelBenchmarkCallback();
    }

    //CFlatFileConfig cfg(
    //    format, mode, style, flags, view, gff_options, genbank_blocks,
    //    genbank_callback.GetPointerOrNull(), m_pCanceledCallback.get(),
    //    args["cleanup"] );
    return new CFlatFileGenerator(cfg);
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

    if ( from == CRange<TSeqPos>::GetWholeFrom()  &&  to == length ) {
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
    return CAsn2FlatApp().AppMain(argc, argv);
}

