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
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/flat_expt.hpp>

#include <objects/entrez2/Entrez2_boolean_element.hpp>
#include <objects/entrez2/Entrez2_boolean_reply.hpp>
#include <objects/entrez2/Entrez2_boolean_exp.hpp>
#include <objects/entrez2/Entrez2_eval_boolean.hpp>
#include <objects/entrez2/Entrez2_id_list.hpp>
#include <objects/entrez2/entrez2_client.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

#include <objtools/format/items/feature_item.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//	=============================================================================
class CAsn2GffApp : 
    public CNcbiApplication
//	=============================================================================
{
public:
    void Init(void);
    int  Run (void);

    bool HandleSeqEntry(CRef<CSeq_entry>& se);
    bool HandleSeqID( const string& seqID );
    bool HandleSeqAnnot( CRef<CSeq_annot>& annot );
        
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
    typedef CFlatFileConfig::TFormat    TFormat;
    typedef CFlatFileConfig::TMode      TMode;
    typedef CFlatFileConfig::TStyle     TStyle;
    typedef CFlatFileConfig::TFlags     TFlags;
    typedef CFlatFileConfig::TView      TView;

    CObjectIStream* x_OpenIStream(const CArgs& args);

    CFlatFileGenerator* x_CreateFlatFileGenerator(const CArgs& args);
    TFormat         x_GetFormat(const CArgs& args);
    TMode           x_GetMode(const CArgs& args);
    TFlags          x_GetFlags(const CArgs& args);
    TView           x_GetView(const CArgs& args);
    TSeqPos x_GetFrom(const CArgs& args);
    TSeqPos x_GetTo  (const CArgs& args);
    void x_GetLocation(const CSeq_entry_Handle& entry,
        const CArgs& args, CSeq_loc& loc);
    CBioseq_Handle x_DeduceTarget(const CSeq_entry_Handle& entry);
    int x_SeqIdToGiNumber( const string& seq_id, const string database );
    
    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CNcbiOstream*               m_Os;           // Output stream
    CRef<CFlatFileGenerator>    m_FFGenerator;  // Flat-file generator
};


//	=============================================================================
void CAsn2GffApp::Init()
//	=============================================================================
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Convert an ASN.1 Seq-entry into a gff file",
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
        arg_desc->AddFlag("c", "Compressed file");
        // propogate top descriptors
        arg_desc->AddFlag("p", "Propogate top descriptors");
    }}
    
    // output
    {{ 
        // name
        arg_desc->AddOptionalKey("o", "OutputFile", 
            "Output file name", CArgDescriptions::eOutputFile);
    }}
    
    // report
    {{
        // format (default: genbank)
        arg_desc->AddDefaultKey("format", "Format", "Output format",
            CArgDescriptions::eString, "gff");
        arg_desc->SetConstraint("format",
            &(*new CArgAllow_Strings,
              "gff", "gff3"));

        // mode (default: dump)
        arg_desc->AddDefaultKey("mode", "Mode", "Restriction level",
            CArgDescriptions::eString, "gbench");
        arg_desc->SetConstraint("mode", 
            &(*new CArgAllow_Strings, "release", "entrez", "gbench", "dump"));

        // style (default: normal)
        arg_desc->AddDefaultKey("style", "Style", "Formatting style",
            CArgDescriptions::eString, "normal");
        arg_desc->SetConstraint("style", 
            &(*new CArgAllow_Strings, "normal", "segment", "master", "contig"));

        // flags (default: 0)
        arg_desc->AddDefaultKey("flags", "Flags", "Custom flags",
            CArgDescriptions::eInteger, "0");

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
        // remote
        arg_desc->AddFlag("gbload", "Use CGBDataLoader");

    }}
    SetupArgDescriptions(arg_desc.release());
}

//  ============================================================================
int CAsn2GffApp::Run()
//  ============================================================================
{
	// initialize conn library
	CONNECT_Init(&GetConfig());

    const CArgs&   args = GetArgs();

    // create object manager
    m_Objmgr = CObjectManager::GetInstance();
    if ( !m_Objmgr ) {
        NCBI_THROW(CFlatException, eInternal, "Could not create object manager");
    }
    if (args["gbload"]) {
        CGBDataLoader::RegisterInObjectManager(*m_Objmgr);
    }

    // open the output stream
    m_Os = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
    if ( m_Os == 0 ) {
        NCBI_THROW(CFlatException, eInternal, "Could not open output stream");
    }

    // create the flat-file generator
    m_FFGenerator.Reset(x_CreateFlatFileGenerator(args));

    auto_ptr<CObjectIStream> is;
    is.reset( x_OpenIStream( args ) );
    if (is.get() == NULL) {
        string msg = args["i"]? "Unable to open input file" + args["i"].AsString() :
                        "Unable to read data from stdin";
        NCBI_THROW(CFlatException, eInternal, msg);
    }
    
    if ( args["id"] ) {
    
        //
        //  Implies gbload; otherwise this feature would be pretty 
        //  useless...
        //
        if ( ! args[ "gbload" ] ) {
            CGBDataLoader::RegisterInObjectManager(*m_Objmgr);
        }   
        string seqID = args["id"].AsString();
        HandleSeqID( seqID );
        
    } else {
        string asn_type = args["type"].AsString();
        CRef<CSeq_entry> se(new CSeq_entry);
        
        if ( asn_type == "seq-entry" ) {
            //
            //  Straight through processing: Read a seq_entry, then process
            //  a seq_entry:
            //
            if ( ! ObtainSeqEntryFromSeqEntry( is, se ) ) {
                NCBI_THROW( 
                    CFlatException, eInternal, "Unable to construct Seq-entry object" );
            }
            HandleSeqEntry(se);
			//m_Objmgr.Reset();
		}
		else if ( asn_type == "bioseq" ) {				
			//
			//  Read object as a bioseq, wrap it into a seq_entry, then process
			//  the wrapped bioseq as a seq_entry:
			//
            if ( ! ObtainSeqEntryFromBioseq( is, se ) ) {
                NCBI_THROW( 
                    CFlatException, eInternal, "Unable to construct Seq-entry object" );
            }
            HandleSeqEntry( se );
		}
		else if ( asn_type == "bioseq-set" ) {
			//
			//  Read object as a bioseq_set, wrap it into a seq_entry, then 
			//  process the wrapped bioseq_set as a seq_entry:
			//
            if ( ! ObtainSeqEntryFromBioseqSet( is, se ) ) {
                NCBI_THROW( 
                    CFlatException, eInternal, "Unable to construct Seq-entry object" );
            }
            HandleSeqEntry( se );
		}
        else if ( asn_type == "any" ) {
            //
            //  Try the first three in turn:
            //
            string strNextTypeName = is->PeekNextTypeName();
            
            if ( ! ObtainSeqEntryFromSeqEntry( is, se ) ) {
                is->Close();
                is.reset( x_OpenIStream( args ) );
                if ( ! ObtainSeqEntryFromBioseqSet( is, se ) ) {
                    is->Close();
                    is.reset( x_OpenIStream( args ) );
                    if ( ! ObtainSeqEntryFromBioseq( is, se ) ) {
                        NCBI_THROW( 
                            CFlatException, eInternal, 
                            "Unable to construct Seq-entry object" 
                        );
                    }
                }
            }
            HandleSeqEntry(se);
        }
    }

    m_Os->flush();

    is.reset();
    return 0;
}


//	=============================================================================
bool CAsn2GffApp::ObtainSeqEntryFromSeqEntry( 
    auto_ptr<CObjectIStream>& is, 
    CRef<CSeq_entry>& se )
//	=============================================================================
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

//	=============================================================================
bool CAsn2GffApp::ObtainSeqEntryFromBioseq( 
    auto_ptr<CObjectIStream>& is, 
    CRef<CSeq_entry>& se )
//	=============================================================================
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

//	=============================================================================
bool 
CAsn2GffApp::ObtainSeqEntryFromBioseqSet( 
    auto_ptr<CObjectIStream>& is, 
    CRef<CSeq_entry>& se )
//	=============================================================================
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

//	=============================================================================
int 
CAsn2GffApp::x_SeqIdToGiNumber( 
    const string& seq_id,
    const string database_name )
//	=============================================================================
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
        return 0;
        
    case 1: {
        //  one hit; the expected outcome:
        //
        //  "it" declared here to keep the WorkShop compiler from whining.
        CEntrez2_id_list::TConstUidIterator it 
            = reply->GetUids().GetConstUidIterator();
        return ( *it );
    }    
    default:
        // multiple hits? Unexpected and definitely not a good thing...
        ERR_FATAL("Unexpected: The ID " << seq_id
                  << " turned up multiple hits." );
    }

    return 0;
};


//	=============================================================================
bool 
CAsn2GffApp::HandleSeqAnnot( 
    CRef<CSeq_annot>& annot )
//	=============================================================================
{
    CRef< CSeq_entry > se( new CSeq_entry );
    CBioseq& bioseq = se->SetSeq();
    bioseq.SetAnnot().push_back( annot );
    CRef< CSeq_id > id( new CSeq_id( "lcl|1" ) );
    bioseq.SetId().push_back( id );
    return HandleSeqEntry( se );
}

//	=============================================================================
bool 
CAsn2GffApp::HandleSeqID( 
    const string& seq_id )
//	=============================================================================
{
    //
    //  Let's make sure we are dealing with something that qualifies a seq id
    //  in the first place:
    //
    try {
        CSeq_id SeqId( seq_id );
    }
    catch ( CException& ) {
        ERR_FATAL("The ID " << seq_id << " is not a valid seq ID." );
    }
    
    unsigned int gi_number = NStr::StringToUInt( seq_id, NStr::fConvErr_NoThrow );
 
    //
    //  We need a gi number for the remote fetching. So if seq_id does not come
    //  as a gi number already, we have to go through a lookup step first. 
    //
    const char* database_names[] = { "Nucleotide", "Protein" };
    const int num_databases = sizeof( database_names ) / sizeof( const char* );
    
    for ( int i=0; (gi_number == 0) && (i < num_databases); ++ i ) {
        gi_number = x_SeqIdToGiNumber( seq_id, database_names[ i ] );
    }
    if ( 0 == gi_number ) {
        ERR_FATAL("Given ID \"" << seq_id
          << "\" does not resolve to a GI number." );
    }
       
    //
    //  Now use the gi_number to get the actual seq object...
    //
    CSeq_id id;
    id.SetGi( gi_number );
    CRef<CScope> scope(new CScope(*m_Objmgr));
    scope->AddDefaults();
    CBioseq_Handle bsh = scope->GetBioseqHandle( id );
    if ( ! bsh ) {
        ERR_FATAL("Unable to obtain data on ID \"" << seq_id << "\".");
    }
    
    //
    //  ... and use that to generate the flat file:
    //
    CArgs args = GetArgs();
    CSeq_entry_Handle seh = bsh.GetTopLevelEntry();
    if ( args["from"]  ||  args["to"] ) {
        CSeq_loc loc;
        x_GetLocation( seh, args, loc );
        m_FFGenerator->Generate(loc, *scope, *m_Os);
    } else {
        try {
            int count = args["count"].AsInteger();
            for ( int i = 0; i < count; ++i ) {
                m_FFGenerator->Generate( bsh.GetTopLevelEntry(), *m_Os);
            }
        } catch (CException& ) {
            ERR_FATAL("Flat file generation failed on " << id.DumpAsFasta() );
        }
    }
    return true;
}

//	=============================================================================
bool 
CAsn2GffApp::HandleSeqEntry(
    CRef<CSeq_entry>& se )
//	=============================================================================
{
    if (!se) {
        return false;
    }

    const CArgs&   args = GetArgs();

    string label;
    //se->GetLabel(&label, CSeq_entry::eBoth);

    // create new scope
    CRef<CScope> scope(new CScope(*m_Objmgr));
    if ( !scope ) {
        NCBI_THROW(CFlatException, eInternal, "Could not create scope");
    }
    scope->AddDefaults();

    // add entry to scope   
    CSeq_entry_Handle entry = scope->AddTopLevelSeqEntry(*se);
    if ( !entry ) {
        NCBI_THROW(CFlatException, eInternal, "Failed to insert entry to scope.");
    }

    // generate flat file
    if ( args["from"]  ||  args["to"] ) {
        CSeq_loc loc;
        x_GetLocation(entry, args, loc);
        m_FFGenerator->Generate(loc, *scope, *m_Os);
    } else {
        try {
            int count = args["count"].AsInteger();
            for ( int i = 0; i < count; ++i ) {
                m_FFGenerator->Generate(entry, *m_Os);
            }
        } catch (CException& e) {
            ERR_POST(e.ReportThis() + " " + label);
        }
    }

    return true;
}


//	=============================================================================
CObjectIStream* 
CAsn2GffApp::x_OpenIStream(
    const CArgs& args )
//	=============================================================================
{
    
    // determine the file serialization format.
    // default for batch files is binary, otherwise text.
    ESerialDataFormat serial = eSerial_AsnText;
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


//	=============================================================================
CFlatFileGenerator* 
CAsn2GffApp::x_CreateFlatFileGenerator(
    const CArgs& args )
//	=============================================================================
{
    TFormat    format = x_GetFormat(args);
    TMode      mode   = x_GetMode(args);
    TStyle     style  = CFlatFileConfig::eStyle_Master;
    TFlags     flags  = x_GetFlags(args);
    TView      view   = x_GetView(args);

    CFlatFileConfig cfg(format, mode, style, flags, view);
    return new CFlatFileGenerator(cfg);
}


//	=============================================================================
CAsn2GffApp::TFormat CAsn2GffApp::x_GetFormat(
    const CArgs& args )
//	=============================================================================
{
    const string& format = args["format"].AsString();
    if ( format == "gff" ) {
        return CFlatFileConfig::eFormat_GFF;
    } else if ( format == "gff3" ) {
        return CFlatFileConfig::eFormat_GFF3;
    }

    return CFlatFileConfig::eFormat_GFF;
}


//	=============================================================================
CAsn2GffApp::TMode 
CAsn2GffApp::x_GetMode(
    const CArgs& args )
//	=============================================================================
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


//	=============================================================================
CAsn2GffApp::TFlags 
CAsn2GffApp::x_GetFlags(
    const CArgs& args )
//	=============================================================================
{
    TFlags flags = args["flags"].AsInteger();
    if (args["html"]) {
        flags |= CFlatFileConfig::fDoHTML;
    }

    return flags;
}


//	=============================================================================
CAsn2GffApp::TView 
CAsn2GffApp::x_GetView(
    const CArgs& args )
//	=============================================================================
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


//	=============================================================================
TSeqPos 
CAsn2GffApp::x_GetFrom(
    const CArgs& args )
//	=============================================================================
{
    return args["from"] ? 
        static_cast<TSeqPos>(args["from"].AsInteger() - 1) :
        CRange<TSeqPos>::GetWholeFrom(); 
}


//	=============================================================================
TSeqPos 
CAsn2GffApp::x_GetTo(
    const CArgs& args )
//	=============================================================================
{
    return args["to"] ? 
        static_cast<TSeqPos>(args["to"].AsInteger() - 1) :
        CRange<TSeqPos>::GetWholeTo();
}


//	=============================================================================
void 
CAsn2GffApp::x_GetLocation(
    const CSeq_entry_Handle& entry,
    const CArgs& args,
    CSeq_loc& loc )
//	=============================================================================
{
    _ASSERT(entry);
        
    CBioseq_Handle h = x_DeduceTarget(entry);
    if ( !h ) {
        NCBI_THROW(CFlatException, eInvalidParam,
            "Cannot deduce target bioseq.");
    }
    TSeqPos length = h.GetInst_Length();
    TSeqPos from   = x_GetFrom(args);
    TSeqPos to     = min(x_GetTo(args), length);
    
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


//	=============================================================================
CBioseq_Handle 
CAsn2GffApp::x_DeduceTarget(
    const CSeq_entry_Handle& entry )
//
//  If the 'from' or 'to' flags are specified try to guess the bioseq.
//	=============================================================================
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

//	=============================================================================
int 
main(
    int argc, 
    const char** argv )
//	=============================================================================
{
    return CAsn2GffApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
