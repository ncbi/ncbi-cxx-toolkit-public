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
* Author:  Aaron Ucko, Mati Shomrat, NCBI
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

#include <sra/data_loaders/wgs/wgsloader.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ==========================================================================
class CFilteringFastaOstream:
    public CFastaOstream
//  ==========================================================================
{
public:
    enum {
        INCLUDE_NONE = 0,
        INCLUDE_NUC = 1<<0,
        INCLUDE_PROT = 1<<1,
        INCLUDE_ALL = (INCLUDE_NUC | INCLUDE_PROT),
    };

public:
    CFilteringFastaOstream(
        CNcbiOstream& out,
        unsigned int includes = INCLUDE_ALL)
    : CFastaOstream(out) 
    {
        SetIncludes(includes);
    };

    virtual bool
    SkipBioseq(
        const CBioseq_Handle&);

    virtual bool
    SkipBioseq(
        const CBioseq&);
 
    void
    SetIncludes(
        unsigned int includes)
    {
        m_includes = includes;
    };

protected:
    int m_includes;
};

//  --------------------------------------------------------------------------
bool CFilteringFastaOstream::SkipBioseq(
    const ncbi::objects::CBioseq_Handle& bsh)
//  --------------------------------------------------------------------------
{
    if ((m_includes & INCLUDE_NUC)  &&  bsh.IsNucleotide()) {
        return false;
    }
    if ((m_includes & INCLUDE_PROT)  &&  bsh.IsProtein()) {
        return false;
    }
    return true;
}

//  --------------------------------------------------------------------------
bool CFilteringFastaOstream::SkipBioseq(
    const ncbi::objects::CBioseq& bs)
//  --------------------------------------------------------------------------
{
    if ((m_includes & INCLUDE_NUC)  &&  bs.IsNa()) {
        return false;
    }
    if ((m_includes & INCLUDE_PROT)  &&  bs.IsAa()) {
        return false;
    }
    return true;
}

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


private:
    CObjectIStream* x_OpenIStream(const CArgs& args);
    int x_GetFilterIncludes(const CArgs& args);

    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CRef<CScope>                m_Scope;
    CNcbiOstream*               m_Os;           // Output stream

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

        // filtering options:
        arg_desc->AddFlag("nucs-only", "Only emit nucleotide sequences");
        arg_desc->AddFlag("prots-only", "Only emit protein sequences");
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

        // name
        arg_desc->AddOptionalKey("o", "OutputFile",
            "Output file name", CArgDescriptions::eOutputFile);
    }}

    SetupArgDescriptions(arg_desc.release());
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

    // open the output stream
    m_Os = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
    if ( m_Os == 0 ) {
        NCBI_THROW(CException, eUnknown, "Could not open output stream");
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
                    streampos = is->GetStreamPos();
                }
            }
        }
    }

    m_Os->flush();

    is.reset();
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
    return ret;
}

//  --------------------------------------------------------------------------
bool CAsn2FastaApp::HandleSeqEntry(CSeq_entry_Handle& seh)
//  --------------------------------------------------------------------------
{
    CFilteringFastaOstream fasta_os( *m_Os, x_GetFilterIncludes(GetArgs()));

    fasta_os.SetAllFlags(
        CFastaOstream::fInstantiateGaps |
        CFastaOstream::fAssembleParts |
        CFastaOstream::fNoDupCheck |
        CFastaOstream::fKeepGTSigns |
        CFastaOstream::fNoExpensiveOps);
    if( GetArgs()["gap-mode"] ) {
        fasta_os.SetFlag(
            CFastaOstream::fInstantiateGaps);
        string gapmode = GetArgs()["gap-mode"].AsString();
        if ( gapmode == "one-dash" ) {
            fasta_os.SetGapMode(CFastaOstream::eGM_one_dash);
        } else if ( gapmode == "dashes" ) {
            fasta_os.SetGapMode(CFastaOstream::eGM_dashes);
        } else if ( gapmode == "letters" ) {
            fasta_os.SetGapMode(CFastaOstream::eGM_letters);
        } else if ( gapmode == "count" ) {
            fasta_os.SetGapMode(CFastaOstream::eGM_count);
        }
    }

    if( GetArgs()["show-mods"] ) {
        fasta_os.SetFlag(CFastaOstream::fShowModifiers);
    }
    if( GetArgs()["width"] ) {
        fasta_os.SetWidth( GetArgs()["width"].AsInteger() );
    }
    if (m_DeflineOnly) {
        for (CBioseq_CI bioseq_it(seh);  bioseq_it;  ++bioseq_it) {
            fasta_os.WriteTitle(*bioseq_it);
        }
    } else {
        fasta_os.Write(seh);
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

//  --------------------------------------------------------------------------
int CAsn2FastaApp::x_GetFilterIncludes(
    const CArgs& args)
//  --------------------------------------------------------------------------
{
    bool bIncludeNucs = args["nucs-only"];
    bool bIncludeProts = args["prots-only"];
    if (bIncludeNucs && bIncludeProts) {
        NCBI_THROW(CException, eUnknown,
            "\"nucs-only\" and \"prots-only\" are mutually exclusive");
    }
    if (bIncludeNucs) {
        return CFilteringFastaOstream::INCLUDE_NUC;
    }
    if (bIncludeProts) {
        return CFilteringFastaOstream::INCLUDE_PROT;
    }
    return CFilteringFastaOstream::INCLUDE_ALL;
}
         
END_NCBI_SCOPE

USING_NCBI_SCOPE;

//  ==========================================================================
int main(int argc, const char** argv)
//  ==========================================================================
{
    return CAsn2FastaApp().AppMain(argc, argv);
}
