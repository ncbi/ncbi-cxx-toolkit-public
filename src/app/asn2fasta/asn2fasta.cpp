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

#include <objects/seqset/gb_release_file.hpp>
#include <objects/seq/seq_macros.hpp>

#include <sra/data_loaders/wgs/wgsloader.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>
#include <objmgr/util/sequence.hpp>

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

    CFastaOstream* OpenFastaOstream(const string& name, bool use_stdout);

private:
    CObjectIStream* x_OpenIStream(const CArgs& args);

    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CRef<CScope>                m_Scope;

    auto_ptr<CFastaOstream>     m_Os;           // all sequence output stream
    bool                        m_OnlyNucs;
    bool                        m_OnlyProts;

    auto_ptr<CFastaOstream>     m_On;           // nucleotide output stream
    auto_ptr<CFastaOstream>     m_Og;           // genomic output stream
    auto_ptr<CFastaOstream>     m_Or;           // RNA output stream
    auto_ptr<CFastaOstream>     m_Op;           // protein output stream
    auto_ptr<CFastaOstream>     m_Ou;           // unknown output stream
    CNcbiOstream*               m_Oq;           // quality score output stream

    bool m_DeflineOnly;
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
            &( *new CArgAllow_Strings, "any", "seq-entry", "bioseq", "bioseq-set" ) );
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

    SetupArgDescriptions(arg_desc.release());
}

//  --------------------------------------------------------------------------
CFastaOstream* CAsn2FastaApp::OpenFastaOstream(const string& name, bool use_stdout)
//  --------------------------------------------------------------------------
{
    const CArgs& args = GetArgs();

    if (! use_stdout) {
        if (! args[name]) return NULL;
    }

    auto_ptr<CFastaOstream> fasta_os
        ( new CFastaOstream( use_stdout? cout : args[name].AsOutputFile()
         ) );

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

    if( GetArgs()["show-mods"] ) {
        fasta_os->SetFlag(CFastaOstream::fShowModifiers);
    }
    if( GetArgs()["width"] ) {
        fasta_os->SetWidth( GetArgs()["width"].AsInteger() );
    }

    return fasta_os.release();
}

//  --------------------------------------------------------------------------
int CAsn2FastaApp::Run(void)
//  --------------------------------------------------------------------------
{
    // initialize conn library
    CONNECT_Init(&GetConfig());

    const CArgs&   args = GetArgs();

    // create object manager
    m_Objmgr = CObjectManager::GetInstance();
    if ( !m_Objmgr ) {
        NCBI_THROW(CException, eUnknown,
                   "Could not create object manager");
    }
    CGBDataLoader::RegisterInObjectManager(*m_Objmgr);
    CWGSDataLoader::RegisterInObjectManager(*m_Objmgr, CObjectManager::eDefault);
    m_Scope.Reset(new CScope(*m_Objmgr));
    m_Scope->AddDefaults();

    // open the output streams
    m_Os.reset( OpenFastaOstream ("o", false) );
    m_On.reset( OpenFastaOstream ("on", false) );
    m_Og.reset( OpenFastaOstream ("og", false) );
    m_Or.reset( OpenFastaOstream ("or", false) );
    m_Op.reset( OpenFastaOstream ("op", false) );
    m_Ou.reset( OpenFastaOstream ("ou", false) );
    m_Oq = args["oq"] ? &(args["oq"].AsOutputFile()) : NULL;

    m_OnlyNucs = args["nucs-only"];
    m_OnlyProts = args["prots-only"];

    if (m_Os.get() == NULL  &&  m_On.get() == NULL  &&  m_Og.get() == NULL  &&
        m_Or.get() == NULL  &&  m_Op.get() == NULL  &&  m_Ou.get() == NULL  &&
        m_Oq == NULL) {
        // No output (-o*) argument given - default to stdout
        m_Os.reset( OpenFastaOstream ("", true) );
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
                        cerr << "Resolution error: Sequence dropped." << endl;
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
            else if ( asn_type == "any" ) {
                //
                //  Try the first three in turn:
                //
                size_t streampos = is->GetStreamPos();
                while (!is->EndOfData()) {
                    seh = ObtainSeqEntryFromSeqEntry(*is);
                    if (!seh) {
                        if (is->EndOfData()) {
                            break;
                        }
                        is->ClearFailFlags(-1);
                        is->SetStreamPos(streampos);
                        seh = ObtainSeqEntryFromBioseqSet(*is);
                    }
                    if (!seh) {
                        if (is->EndOfData()) {
                            break;
                        }
                        is->ClearFailFlags(-1);
                        is->SetStreamPos(streampos);
                        seh = ObtainSeqEntryFromBioseq(*is);
                    }
                    if (!seh) {
                        NCBI_THROW(CException, eUnknown,
                            "Unable to construct Seq-entry object" );
                    }
                    HandleSeqEntry(seh);
                    m_Scope->RemoveTopLevelSeqEntry(seh);
                    m_Scope->ResetHistory();
                    streampos = is->GetStreamPos();
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
    catch (CException& e) {
        if (! (is.GetFailFlags() & is.eEOF)) {
            ERR_POST(Error << e);
        }
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
    catch (CException& e) {
        if (! (is.GetFailFlags() & is.eEOF)) {
            ERR_POST(Error << e);
        }
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
    catch (CException& e) {
        if (! (is.GetFailFlags() & is.eEOF)) {
            ERR_POST(Error << e);
        }
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
        ERR_POST(Fatal << "Unable to obtain data on ID \"" << seq_id.c_str()
          << "\"." );
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
bool CAsn2FastaApp::HandleSeqEntry(CSeq_entry_Handle& seh)
//  --------------------------------------------------------------------------
{
    for (CBioseq_CI bioseq_it(seh);  bioseq_it;  ++bioseq_it) {
        CBioseq_Handle bsh = *bioseq_it;
        CConstRef<CBioseq> bsr = bsh.GetCompleteBioseq();

        CFastaOstream* fasta_os;

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

        if ( m_Oq != NULL) {
            FOR_EACH_SEQANNOT_ON_BIOSEQ (sa_itr, *bsr) {
                const CSeq_annot& annot = **sa_itr;
                if (! annot.IsGraph()) continue;
                FOR_EACH_SEQGRAPH_ON_SEQANNOT (gr_itr, annot) {
                    const CSeq_graph& graph = **gr_itr;
                    if (graph.IsSetTitle()) {
                        const string& g_title = graph.GetTitle();
                        *m_Oq << "Title: " << g_title << endl;
                    }
                    if (graph.IsSetNumval()) {
                        *m_Oq << "Numval: " << graph.GetNumval() << endl;
                    }
                    if (graph.IsSetLoc()) {
                        const CSeq_loc& loc = graph.GetLoc();
                        *m_Oq << "Start: " << loc.GetStart(eExtreme_Positional) << ", Stop: " << loc.GetStop(eExtreme_Positional) << endl;
                    }
                }
            }
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
