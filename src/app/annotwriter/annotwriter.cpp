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
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
class CAnnotRecord
//  ----------------------------------------------------------------------------
{
public:
    CAnnotRecord() {};
    ~CAnnotRecord() {};
    
    bool SetRecord(
        const CRef< CSeq_feat > );
        
    void DumpRecord(
        CNcbiOstream& );
};

//  ----------------------------------------------------------------------------
bool CAnnotRecord::SetRecord(
    const CRef< CSeq_feat > pFeature )
//  ----------------------------------------------------------------------------
{
    if ( pFeature->CanGetId() ) {
        cerr << ":id:";
    }
    if ( pFeature->CanGetData() ) {
        cerr << ":data:";
    }
    if ( pFeature->CanGetPartial() ) {
        cerr << ":partial:";
    }
    if ( pFeature->CanGetExcept() ) {
        cerr << ":except:";
    }
    if ( pFeature->CanGetComment() ) {
        cerr << ":comment:";
    }
    if ( pFeature->CanGetProduct() ) {
        cerr << ":product:";
    }
    if ( pFeature->CanGetLocation() ) {
        cerr << ":location:";
    }
    if ( pFeature->CanGetQual() ) {
        cerr << ":qual:";
    }
    if ( pFeature->CanGetTitle() ) {
        cerr << ":title:";
    }
    if ( pFeature->CanGetExt() ) {
        cerr << ":ext:";
    }
    if ( pFeature->CanGetCit() ) {
        cerr << ":cit:";
    }
    if ( pFeature->CanGetExp_ev() ) {
        cerr << ":exp:";
    }
    if ( pFeature->CanGetXref() ) {
        cerr << ":xref:";
    }
    if ( pFeature->CanGetDbxref() ) {
        cerr << ":dbxref:";
    }
    if ( pFeature->CanGetExcept_text() ) {
        cerr << ":xpttxt:";
    }
    if ( pFeature->CanGetPseudo() ) {
        cerr << ":pseudo:";
    }
    if ( pFeature->CanGetIds() ) {
        cerr << ":ids:";
    }
    if ( pFeature->CanGetExts() ) {
        cerr << ":exts:";
    }
    cerr << endl;
    return true;
}

//  ----------------------------------------------------------------------------
void CAnnotRecord::DumpRecord(
    CNcbiOstream& out )
//  ----------------------------------------------------------------------------
{
}


//  ----------------------------------------------------------------------------
class CAnnotWriterApp : public CNcbiApplication
//  ----------------------------------------------------------------------------
{
public:
    void Init(void);
    int  Run (void);

    bool HandleSeqAnnot(CRef<CSeq_annot>& se);
        
private:
    bool HandleSeqAnnotFtable(CRef<CSeq_annot>& se);
    bool HandleSeqAnnotAlign(CRef<CSeq_annot>& se);
    CObjectIStream* x_OpenIStream(const CArgs& args);

    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CNcbiOstream*               m_Os;           // Output stream
};

//  ----------------------------------------------------------------------------
void CAnnotWriterApp::Init()
//  ----------------------------------------------------------------------------
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Convert an ASN.1 to alternative file formats",
        false);
    
    // input
    {{
        // name
        arg_desc->AddOptionalKey("i", "InputFile", 
            "Input file name", CArgDescriptions::eInputFile);
    }}

    // output
    {{ 
        // name
        arg_desc->AddOptionalKey("o", "OutputFile", 
            "Output file name", CArgDescriptions::eOutputFile);
    }}
    
    SetupArgDescriptions(arg_desc.release());
}


//  ----------------------------------------------------------------------------
int CAnnotWriterApp::Run()
//  ----------------------------------------------------------------------------
{
	// initialize conn library
	CONNECT_Init(&GetConfig());

    const CArgs&   args = GetArgs();

    // create object manager
    m_Objmgr = CObjectManager::GetInstance();
    if ( !m_Objmgr ) {
        NCBI_THROW(CFlatException, eInternal, "Could not create object manager");
    }

    // open the output stream
    m_Os = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
    if ( m_Os == 0 ) {
        NCBI_THROW(CFlatException, eInternal, "Could not open output stream");
    }

    auto_ptr<CObjectIStream> is;
    is.reset( x_OpenIStream( args ) );
    if (is.get() == NULL) {
        string msg = args["i"]? "Unable to open input file" + args["i"].AsString() :
                        "Unable to read data from stdin";
        NCBI_THROW(CFlatException, eInternal, msg);
    }

    while ( true ) {
        try {
            CRef<CSeq_annot> annot(new CSeq_annot);
            *is >> *annot;
            HandleSeqAnnot( annot );
        }
        catch ( CEofException& ) {
            break;
        }
    } 
    
    m_Os->flush();

    is.reset();
    return 0;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::HandleSeqAnnot( 
    CRef<CSeq_annot>& annot )
//  -----------------------------------------------------------------------------
{
    if ( annot->IsFtable() ) {
        return HandleSeqAnnotFtable( annot );
    }
    if ( annot->IsAlign() ) {
        return HandleSeqAnnotAlign( annot );
    }
    cerr << "Unexpected!" << endl;
    return false;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::HandleSeqAnnotFtable( 
    CRef<CSeq_annot>& annot )
//  -----------------------------------------------------------------------------
{
    if ( ! annot->IsFtable() ) {
        return false;
    }
    cerr << "SeqAnnot.Ftable" << endl;
    
    const list< CRef< CSeq_feat > >& table = annot->GetData().GetFtable();
    list< CRef< CSeq_feat > >::const_iterator it = table.begin();
    CAnnotRecord record;
    while ( it != table.end() ) {
        record.SetRecord( *it );
        record.DumpRecord( *m_Os );
        it++;
    }
    return true;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::HandleSeqAnnotAlign( 
    CRef<CSeq_annot>& annot )
//  -----------------------------------------------------------------------------
{
    if ( ! annot->IsAlign() ) {
        return false;
    }
    cerr << "SeqAnnot.Align" << endl;
    return true;
}

//  -----------------------------------------------------------------------------
CObjectIStream* CAnnotWriterApp::x_OpenIStream( 
    const CArgs& args )
//  -----------------------------------------------------------------------------
{
    ESerialDataFormat serial = eSerial_AsnText;
    CNcbiIstream* pInputStream = &NcbiCin;
    bool bDeleteOnClose = false;
    if ( args["i"] ) {
        pInputStream = new CNcbiIfstream( args["i"].AsString().c_str(), ios::binary  );
        bDeleteOnClose = true;
    }
    CObjectIStream* pI = CObjectIStream::Open( serial, *pInputStream, bDeleteOnClose );
//    if ( 0 != pI ) {
//        pI->UseMemoryPool();
//    }
    return pI;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
//
// Main

int main(int argc, const char** argv)
{
    return CAnnotWriterApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
