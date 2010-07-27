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

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

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
#include <objtools/writers/gff_writer.hpp>
#include <objtools/writers/gtf_writer.hpp>
#include <objtools/writers/wiggle_writer.hpp>

#include <time.h>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
class CAnnotWriterApp : public CNcbiApplication
//  ----------------------------------------------------------------------------
{
public:
    void Init();
    int Run ();

private:
    CObjectIStream* x_OpenIStream(
        const CArgs& args );

    bool Write(
        const CSeq_annot& annot,
        CNcbiOstream& );

    bool WriteGff(
        const CSeq_annot& annot,
        CNcbiOstream& );

    bool WriteGtf(
        const CSeq_annot& annot,
        CNcbiOstream& );

    bool WriteWiggle(
        const CSeq_annot& annot,
        CNcbiOstream& );

    CGffWriter::TFlags GffFlags( 
        const CArgs& );
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
        arg_desc->AddOptionalKey( "i", "InputFile", 
            "Input file name", CArgDescriptions::eInputFile );
    }}

    // format
    {{
    arg_desc->AddDefaultKey(
        "format", 
        "STRING",
        "Output format",
        CArgDescriptions::eString, 
        "gff" );
    arg_desc->SetConstraint(
        "format", 
        &(*new CArgAllow_Strings, 
            "gff", "gtf", "wiggle" ) );
    }}

    // output
    {{ 
        arg_desc->AddOptionalKey( "o", "OutputFile", 
            "Output file name", CArgDescriptions::eOutputFile );
    }}

    //  flags
    {{
        arg_desc->AddDefaultKey(
            "tracksize",
            "INTEGER",
            "Records per track",
            CArgDescriptions::eInteger,
            "0" );

    arg_desc->AddFlag(
        "so-quirks",
        "recreate sequence ontology funniness in output",
        true );
        
    arg_desc->AddFlag(
        "structibutes",
        "limit attributes to inter feature relationships",
        true );
        
    }}
    
    SetupArgDescriptions(arg_desc.release());
}


//  ----------------------------------------------------------------------------
int CAnnotWriterApp::Run()
//  ----------------------------------------------------------------------------
{
	// initialize conn library
	CONNECT_Init(&GetConfig());

    const CArgs& args = GetArgs();

    CNcbiOstream* pOs = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
    if ( pOs == 0 ) {
        NCBI_THROW(CFlatException, eInternal, "Could not open output stream");
    }

    auto_ptr<CObjectIStream> pIs;
    pIs.reset( x_OpenIStream( args ) );
    if ( pIs.get() == NULL ) {
        string msg = args["i"]? "Unable to open input file" + args["i"].AsString() :
                        "Unable to read data from stdin";
        NCBI_THROW(CFlatException, eInternal, msg);
    }

    while ( true ) {
        try {
            CRef<CSeq_annot> annot(new CSeq_annot);
            *pIs >> *annot;
            Write( *annot, *pOs );
        }
        catch ( CEofException& ) {
            break;
        }
        catch ( ... ) {
            try {
                pIs.reset( x_OpenIStream( args ) );
                CRef<CSeq_entry> entry( new CSeq_entry );
                *pIs >> *entry;
                CTypeIterator<CSeq_annot> annot_iter( *entry );
                for ( ;  annot_iter;  ++annot_iter ) {
                    CRef< CSeq_annot > annot( annot_iter.operator->() );
                    Write( *annot, *pOs );
                }
                break;
            }
            catch ( ... ) {
                cerr << "Object type not supported!" << endl;
            }
        }
    
    } 
    pOs->flush();

    pIs.reset();
    return 0;
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
    return pI;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::Write( 
    const CSeq_annot& annot,
    CNcbiOstream& os )
//  -----------------------------------------------------------------------------
{
    if ( GetArgs()[ "format" ].AsString() == "gff" ) { 
        return WriteGff( annot, os );
    }
    if ( GetArgs()[ "format" ].AsString() == "gtf" ) {
        return WriteGtf( annot, os );
    }
    if ( GetArgs()[ "format" ].AsString() == "wiggle" ) {
        return WriteWiggle( annot, os );
    }
    cerr << "Unexpected!" << endl;
    return false;    
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::WriteGff( 
    const CSeq_annot& annot,
    CNcbiOstream& os )
//  -----------------------------------------------------------------------------
{
    CRef< CObjectManager > pObjMngr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager( *pObjMngr );
    CRef< CScope > pScope( new CScope( *pObjMngr ) );
    pScope->AddDefaults();

    CGffWriter writer( *pScope, os, GffFlags( GetArgs() ) );
    return writer.WriteAnnot( annot );
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::WriteGtf( 
    const CSeq_annot& annot,
    CNcbiOstream& os )
//  -----------------------------------------------------------------------------
{
    CRef< CObjectManager > pObjMngr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager( *pObjMngr );
    CRef< CScope > pScope( new CScope( *pObjMngr ) );
    pScope->AddDefaults();

    CGtfWriter writer( *pScope, os, GffFlags( GetArgs() ) );
    return writer.WriteAnnot( annot );
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::WriteWiggle( 
    const CSeq_annot& annot,
    CNcbiOstream& os )
//  -----------------------------------------------------------------------------
{
    CWiggleWriter writer( os, GetArgs()["tracksize"].AsInteger() );
    return writer.WriteAnnot( annot );
}

//  -----------------------------------------------------------------------------
CGffWriter::TFlags CAnnotWriterApp::GffFlags(
    const CArgs& args )
//  -----------------------------------------------------------------------------
{
    CGffWriter::TFlags eFlags = CGffWriter::fNormal;
    if ( args["so-quirks"] ) {
        eFlags = CGffWriter::TFlags( eFlags | CGffWriter::fSoQuirks );
    }
    if ( args["structibutes"] ) {
        eFlags = CGtfWriter::TFlags( eFlags | CGtfWriter::fStructibutes );
    }
    return eFlags;
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
