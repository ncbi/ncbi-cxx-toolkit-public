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

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
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

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CCleanupApp : public CNcbiApplication, public CGBReleaseFile::ISeqEntryHandler
{
public:
    void Init(void);
    int  Run (void);

    bool HandleSeqEntry(CRef<CSeq_entry>& se);
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

    CObjectIStream* x_OpenIStream(const CArgs& args);

    int x_SeqIdToGiNumber( const string& seq_id, const string database );
    
    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CRef<CFlatFileGenerator>    m_FFGenerator;  // Flat-file generator
};


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
        arg_desc->AddFlag("sub", "Submission");
        // id
        arg_desc->AddOptionalKey("id", "ID", 
            "Specific ID to display", CArgDescriptions::eString);
            
        // input type:
        arg_desc->AddDefaultKey( "type", "AsnType", "ASN.1 object type",
            CArgDescriptions::eString, "any" );
        arg_desc->SetConstraint( "type", 
            &( *new CArgAllow_Strings, "any", "seq-entry", "bioseq", "bioseq-set", "seq-submit" ) );
        
    }}

    // batch processing
    {{
        arg_desc->AddFlag("batch", "Process NCBI release file");
        // compression
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

        // html
        arg_desc->AddFlag("html", "Produce HTML output");
    }}
    
    // misc
    {{
        // no-cleanup
        arg_desc->AddFlag("nocleanup",
            "Do not perform extended data cleanup prior to formatting");
        arg_desc->AddFlag("basic",
            "Perform basic data cleanup prior to formatting");

        // remote
        arg_desc->AddFlag("gbload", "Use CGBDataLoader");
        // show progress
        arg_desc->AddFlag("showprogress",
            "List ID for which cleanup is occuring");
        arg_desc->AddFlag("debug", "Save before.sqn");
    }}
    SetupArgDescriptions(arg_desc.release());
}


int CCleanupApp::Run(void)
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
#ifdef HAVE_PUBSEQ_OS
        // we may require PubSeqOS readers at some point, so go ahead and make
        // sure they are properly registered
        GenBankReaders_Register_Pubseq();
        GenBankReaders_Register_Pubseq2();
        DBAPI_RegisterDriver_FTDS();
#endif

        CGBDataLoader::RegisterInObjectManager(*m_Objmgr);
    }

    auto_ptr<CObjectIStream> is;
    is.reset( x_OpenIStream( args ) );
    if (is.get() == NULL) {
        string msg = args["i"]? "Unable to open input file" + args["i"].AsString() :
                        "Unable to read data from stdin";
        NCBI_THROW(CFlatException, eInternal, msg);
    }
    
    if ( args["batch"] ) {
        CGBReleaseFile in(*is.release());
        in.RegisterHandler(this);
        in.Read();  // HandleSeqEntry will be called from this function
    } 
    else {
        
            string asn_type = args["type"].AsString();
        if (args["sub"] || asn_type == "seq-submit") {  // submission
            CRef<CSeq_submit> sub(new CSeq_submit);
            if (sub.Empty()) {
                NCBI_THROW(CFlatException, eInternal, 
                    "Could not allocate Seq-submit object");
            }
            *is >> *sub;
            if (sub->IsSetSub()  &&  sub->IsSetData()) {
                CRef<CScope> scope(new CScope(*m_Objmgr));
                if ( !scope ) {
                    NCBI_THROW(CFlatException, eInternal, "Could not create scope");
                }
                scope->AddDefaults();
                CRef<CSeq_entry> se(new CSeq_entry);
                se.Reset( sub->SetData().SetEntrys().front() );
                HandleSeqEntry(se);
            }
        } else if ( args["id"] ) {
        
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
    }


    is.reset();
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

int CCleanupApp::x_SeqIdToGiNumber( 
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
        ERR_FATAL("Unexpected: The ID " << seq_id.c_str() 
                  << " turned up multiple hits." );
    }

    return 0;
};


bool CCleanupApp::HandleSeqID( const string& seq_id )
{
    //
    //  Let's make sure we are dealing with something that qualifies a seq id
    //  in the first place:
    //
    try {
        CSeq_id SeqId( seq_id );
    }
    catch ( CException& ) {
        ERR_FATAL("The ID " << seq_id.c_str() << " is not a valid seq ID." );
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
        ERR_FATAL("Given ID \"" << seq_id.c_str() 
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
    }

    CArgs args = GetArgs();

	if (args["basic"] || !args["nocleanup"]) {
        CCleanup cleanup;
        cleanup.SetScope(scope);
        
		if (args["basic"]) {
			// perform BasicCleanup
			try {
				CConstRef<CCleanupChange> changes = cleanup.BasicCleanup (const_cast<CSeq_entry& >(*(bsh.GetSeq_entry_Handle().GetCompleteSeq_entry())));
				vector<string> changes_str = changes->GetAllDescriptions();
				if (changes_str.size() == 0) {
					printf ("No changes from BasicCleanup\n");
				} else {
					printf ("Changes from BasicCleanup:\n");
				    ITERATE(vector<string>, vit, changes_str) {
					    printf ("%s\n", (*vit).c_str());
				    }
				}
			}
			catch (CException& e) {
				LOG_POST(Error << "error in basic cleanup: " << e.GetMsg());
			}
		}

		if (!args["nocleanup"]) {
			// perform ExtendedCleanup
			try {
				CConstRef<CCleanupChange> changes = cleanup.ExtendedCleanup (const_cast<CSeq_entry& >(*(bsh.GetSeq_entry_Handle().GetCompleteSeq_entry())));
				vector<string> changes_str = changes->GetAllDescriptions();
				if (changes_str.size() == 0) {
				    printf ("No changes from ExtendedCleanup\n");
				} else {
					printf ("Changes from ExtendedCleanup:\n");
				    ITERATE(vector<string>, vit, changes_str) {
					    printf ("%s\n", (*vit).c_str());
				    }
				}
			}
			catch (CException& e) {
				LOG_POST(Error << "error in extended cleanup: " << e.GetMsg());
			}
		}
    }
    
    ESerialDataFormat outFormat = eSerial_AsnText;
    auto_ptr<CObjectOStream> out(!args["o"]? 0:
                                 CObjectOStream::Open(outFormat, args["o"].AsString(),
                                                      eSerial_StdWhenAny));

    *out << *(bsh.GetSeq_entry_Handle().GetCompleteSeq_entry());
    
    return true;
}

bool CCleanupApp::HandleSeqEntry(CRef<CSeq_entry>& se)
{
    if (!se) {
        return false;
    }

    string label;
    se->GetLabel(&label, CSeq_entry::eBoth);

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

    CArgs args = GetArgs();

    if (args["showprogress"]) {
        printf ("%s\n", label.c_str());
    }   
    
    ESerialDataFormat outFormat = eSerial_AsnText;

    if (args["debug"]) {
      auto_ptr<CObjectOStream> debug_out(CObjectOStream::Open(outFormat, "before.sqn",
                                                      eSerial_StdWhenAny));

      *debug_out << *(entry.GetCompleteSeq_entry());
    }

    string file_name = args["o"].AsString();
    bool any_changes = false;

	if (args["basic"] || !args["nocleanup"]) {
        CCleanup cleanup;
        cleanup.SetScope(scope);

		if (args["basic"]) {
			// perform BasicCleanup
			try {
				CConstRef<CCleanupChange> changes = cleanup.BasicCleanup (entry);
				vector<string> changes_str = changes->GetAllDescriptions();
				if (changes_str.size() == 0) {
					printf ("No changes from BasicCleanup\n");
				} else {
					printf ("Changes from BasicCleanup:\n");
				    ITERATE(vector<string>, vit, changes_str) {
					    printf ("%s\n", (*vit).c_str());
				    }
				    any_changes = true;
				}
			}
			catch (CException& e) {
				LOG_POST(Error << "error in basic cleanup: " << e.GetMsg() << label);
				file_name = "last_error.sqn";
			}
		}
    
		if (!args["nocleanup"]) {
			// perform ExtendedCleanup
			try {
				CConstRef<CCleanupChange> changes = cleanup.ExtendedCleanup (const_cast<CSeq_entry& >(*(entry.GetCompleteSeq_entry())));
				vector<string> changes_str = changes->GetAllDescriptions();
				if (changes_str.size() == 0) {
				    printf ("No changes from ExtendedCleanup\n");
				} else {
					printf ("Changes from ExtendedCleanup:\n");
				    ITERATE(vector<string>, vit, changes_str) {
					    printf ("%s\n", (*vit).c_str());
				    }
				    any_changes = true;
				}
			}
			catch (CException& e) {
				LOG_POST(Error << "error in extended cleanup: " << e.GetMsg() << label);
				file_name = "last_error.sqn";
			}
		}
    }

    auto_ptr<CObjectOStream> out(!args["o"]? 0:
                                 CObjectOStream::Open(outFormat, file_name,
                                                      eSerial_StdWhenAny));

    *out << *(entry.GetCompleteSeq_entry());

    fflush(NULL);

    if (any_changes && args["debug"]) {
        string diff_cmd = "diff before.sqn " + file_name;
        system (diff_cmd.c_str());
    }
    printf ("generated asn.1\n");
    return true;
}


CObjectIStream* CCleanupApp::x_OpenIStream(const CArgs& args)
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
        pI = CObjectIStream::Open( serial, *pInputStream, bDeleteOnClose ? eTakeOwnership : eNoOwnership );
    }
    
    if ( 0 != pI ) {
        pI->UseMemoryPool();
    }
    return pI;
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
