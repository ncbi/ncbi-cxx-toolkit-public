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
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
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
#include <objmgr/util/sequence.hpp>
#include <objtools/writers/gff_writer.hpp>
#include <objtools/writers/gtf_writer.hpp>
#include <objtools/writers/gff3_writer.hpp>
#include <objtools/writers/wiggle_writer.hpp>
#include <objtools/writers/bed_track_record.hpp>
#include <objtools/writers/bed_feature_record.hpp>
#include <objtools/writers/bed_writer.hpp>
#include <objtools/writers/gvf_writer.hpp>

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

    bool TrySeqAnnot(
        CObjectIStream&,
        CNcbiOstream& );

    bool TrySeqEntry(
        CObjectIStream&,
        CNcbiOstream& );

    bool TryBioseqSet(
        CObjectIStream&,
        CNcbiOstream& );

    bool TryBioseq(
        CObjectIStream&,
        CNcbiOstream& );

    bool TrySeqAlign(
        CObjectIStream&,
        CNcbiOstream& );

    bool Write(
        const CSeq_annot& annot,
        CNcbiOstream& );

    bool WriteGff2(
        const CSeq_annot& annot,
        CNcbiOstream& );

    bool WriteGff3(
        const CSeq_annot& annot,
        CNcbiOstream& );

    bool WriteGtf(
        const CSeq_annot& annot,
        CNcbiOstream& );

    bool WriteGvf(
        const CSeq_annot& annot,
        CNcbiOstream& );

    bool WriteWiggle(
        const CSeq_annot& annot,
        CNcbiOstream& );

    bool WriteBed(
        const CSeq_annot& annot,
        CNcbiOstream& );

    bool WriteGff3(
        const CSeq_align& align,
        CNcbiOstream& );

    bool WriteHandleGff3(
        CBioseq_Handle bsh,
        CNcbiOstream& );

    bool WriteHandleGvf(
        CBioseq_Handle bsh,
        CNcbiOstream& );

    CGff2Writer::TFlags GffFlags( 
        const CArgs& );

    bool TestHandles() const;
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
            "gvf",
            "gff", "gff2", 
            "gff3", 
            "gtf", 
            "wig", "wiggle",
            "bed" ) );
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

    //  testing
    arg_desc->AddFlag(
        "test-handle",
        "call writer interface with handle object",
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
        if ( TrySeqAnnot( *pIs, *pOs ) ) {
            continue;
        }
        if ( TrySeqEntry( *pIs, *pOs ) ) {
            continue;
        }
        if ( TryBioseqSet( *pIs, *pOs ) ) {
            continue;
        }
        if ( TryBioseq( *pIs, *pOs ) ) {
            continue;
        }
        if ( TrySeqAlign( *pIs, *pOs ) ) {
            continue;
        }
        if ( ! pIs->EndOfData() ) {
            cerr << "Object type not supported!" << endl;
        }
        break;
    } 
    pOs->flush();

    pIs.reset();
    return 0;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::TrySeqAnnot(
    CObjectIStream& istr,
    CNcbiOstream& ostr )
//  -----------------------------------------------------------------------------
{
    CNcbiStreampos curr = istr.GetStreamPos();
    try {
        CRef<CSeq_annot> annot(new CSeq_annot);
        istr >> *annot;
        return Write( *annot, ostr );
    }
    catch ( ... ) {
        istr.SetStreamPos ( curr );
        return false;
    }
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::TryBioseqSet(
    CObjectIStream& istr,
    CNcbiOstream& ostr )
//  -----------------------------------------------------------------------------
{
    CNcbiStreampos curr = istr.GetStreamPos();
    try {
        CRef<CBioseq_set> pBioset(new CBioseq_set);
        istr >> *pBioset;

        const CBioseq_set::TSeq_set& bss = pBioset->GetSeq_set();
        for ( CBioseq_set::TSeq_set::const_iterator it = bss.begin(); it != bss.end(); ++it ) {
            const CSeq_entry& se = **it;

            CRef< CObjectManager > pObjMngr = CObjectManager::GetInstance();
            CGBDataLoader::RegisterInObjectManager( *pObjMngr );
            CRef< CScope > pScope( new CScope( *pObjMngr ) );
            pScope->AddDefaults();
            const CBioseq& bs = se.GetSeq();
            pScope->AddBioseq( bs );
            this->WriteHandleGff3( pScope->GetBioseqHandle( bs ), ostr );
        }
        return true;
    }
    catch ( ... ) {
        istr.SetStreamPos ( curr );
        return false;
    }
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::TryBioseq(
    CObjectIStream& istr,
    CNcbiOstream& ostr )
//  -----------------------------------------------------------------------------
{
    CNcbiStreampos curr = istr.GetStreamPos();
    try {
        CRef<CBioseq> pBioseq(new CBioseq);
        istr >> *pBioseq;
        CTypeIterator<CSeq_annot> annot_iter( *pBioseq );
        for ( ;  annot_iter;  ++annot_iter ) {
            CRef< CSeq_annot > annot( annot_iter.operator->() );
            if ( ! Write( *annot, ostr ) ) {
                return false;
            }
        }
        return true;
    }
    catch ( ... ) {
        istr.SetStreamPos ( curr );
        return false;
    }
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::TrySeqEntry(
    CObjectIStream& istr,
    CNcbiOstream& ostr )
//  -----------------------------------------------------------------------------
{
    CNcbiStreampos curr = istr.GetStreamPos();
    try {
        // special case: test writer handling of bioseq handles
        CRef<CSeq_entry> pEntry( new CSeq_entry );
        istr >> *pEntry;
        if ( TestHandles() ) {
            CRef< CObjectManager > pObjMngr = CObjectManager::GetInstance();
            CGBDataLoader::RegisterInObjectManager( *pObjMngr );
            CRef< CScope > pScope( new CScope( *pObjMngr ) );
            pScope->AddDefaults();

            CSeq_entry_Handle seh = pScope->AddTopLevelSeqEntry( *pEntry );
            for ( CBioseq_CI bci( seh ); bci; ++bci ) {
                if ( ! WriteHandleGff3( *bci, ostr ) ) {
                    return false;
                }
            }
            return true;
        }

        // normal case: just dump all the annots
        CTypeIterator<CSeq_annot> annot_iter( *pEntry );
        for ( ;  annot_iter;  ++annot_iter ) {
            CRef< CSeq_annot > annot( annot_iter.operator->() );
            if ( ! Write( *annot, ostr ) ) {
                return false;
            }
        }
        return true;
    }
    catch ( ... ) {
        istr.SetStreamPos ( curr );
        return false;
    }
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::TrySeqAlign(
    CObjectIStream& istr,
    CNcbiOstream& ostr )
//  -----------------------------------------------------------------------------
{
    CNcbiStreampos curr = istr.GetStreamPos();
    try {
        CRef<CSeq_align> pAlign(new CSeq_align);
        istr >> *pAlign;
        return WriteGff3( *pAlign, ostr );
    }
    catch ( ... ) {
        istr.SetStreamPos ( curr );
        return false;
    }
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
        pInputStream = new CNcbiIfstream( 
            args["i"].AsString().c_str(), ios::binary  );
        bDeleteOnClose = true;
    }
    CObjectIStream* pI = CObjectIStream::Open( 
        serial, *pInputStream, bDeleteOnClose );
    return pI;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::Write( 
    const CSeq_annot& annot,
    CNcbiOstream& os )
//  -----------------------------------------------------------------------------
{
    const string strFormat = GetArgs()[ "format" ].AsString();

    if ( strFormat == "gff"  ||  strFormat == "gff2" ) { 
        return WriteGff2( annot, os );
    }
    if ( strFormat == "gff3" ) { 
        return WriteGff3( annot, os );
    }
    if ( strFormat == "gvf" ) { 
        return WriteGvf( annot, os );
    }
    if ( strFormat == "gtf" ) {
        return WriteGtf( annot, os );
    }
    if ( strFormat == "wiggle"  ||  strFormat == "wig" ) {
        return WriteWiggle( annot, os );
    }
    if ( strFormat == "bed" ) {
        return WriteBed( annot, os );
    }
    cerr << "Unexpected!" << endl;
    return false;    
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::WriteGff2( 
    const CSeq_annot& annot,
    CNcbiOstream& os )
//  -----------------------------------------------------------------------------
{
    CGff2Writer writer( os, GffFlags( GetArgs() ) );
    return writer.WriteAnnot( annot );
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::WriteGff3( 
    const CSeq_annot& annot,
    CNcbiOstream& os )
//  -----------------------------------------------------------------------------
{
    CRef< CObjectManager > pObjMngr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager( *pObjMngr );
    CRef< CScope > pScope( new CScope( *pObjMngr ) );
    pScope->AddDefaults();

    CGff3Writer writer( *pScope, os, GffFlags( GetArgs() ) );
    return writer.WriteAnnot( annot );
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::WriteGvf( 
    const CSeq_annot& annot,
    CNcbiOstream& os )
//  -----------------------------------------------------------------------------
{
    CRef< CObjectManager > pObjMngr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager( *pObjMngr );
    CRef< CScope > pScope( new CScope( *pObjMngr ) );
    pScope->AddDefaults();

    CGvfWriter writer( *pScope, os );
    return writer.WriteAnnot( annot );
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::WriteHandleGff3(
    CBioseq_Handle bsh,
    CNcbiOstream& ostr )
//  -----------------------------------------------------------------------------
{
    CGff3Writer writer( ostr, GffFlags( GetArgs() ) );
    return writer.WriteBioseqHandle( bsh ) || true;
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::WriteGff3( 
    const CSeq_align& align,
    CNcbiOstream& os )
//  -----------------------------------------------------------------------------
{
    CGff3Writer writer( os, GffFlags( GetArgs() ) );
    return writer.WriteAlign( align );
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::WriteGtf( 
    const CSeq_annot& annot,
    CNcbiOstream& os )
//  -----------------------------------------------------------------------------
{
    CGtfWriter writer( os, GffFlags( GetArgs() ) );
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
bool CAnnotWriterApp::WriteBed( 
    const CSeq_annot& annot,
    CNcbiOstream& os )
//  -----------------------------------------------------------------------------
{
    CRef< CObjectManager > pObjMngr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager( *pObjMngr );
    CRef< CScope > pScope( new CScope( *pObjMngr ) );
    pScope->AddDefaults();

    CBedWriter writer( *pScope, os );
    return writer.WriteAnnot( annot );
}

//  -----------------------------------------------------------------------------
CGff2Writer::TFlags CAnnotWriterApp::GffFlags(
    const CArgs& args )
//  -----------------------------------------------------------------------------
{
    CGff2Writer::TFlags eFlags = CGff2Writer::fNormal;
    if ( args["so-quirks"] ) {
        eFlags = CGff2Writer::TFlags( eFlags | CGff2Writer::fSoQuirks );
    }
    if ( args["structibutes"] ) {
        eFlags = CGtfWriter::TFlags( eFlags | CGtfWriter::fStructibutes );
    }
    return eFlags;
}

//  ----------------------------------------------------------------------------
bool CAnnotWriterApp::TestHandles() const
//  ----------------------------------------------------------------------------
{
    return GetArgs()["test-handle"];
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
