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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   Reader for selected data file formats
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbitime.hpp>
#include <util/format_guess.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/bed_reader.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
class CMultiReaderApp
//  ============================================================================
     : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    virtual int ReadSeqAnnot(
        CNcbiIstream&,
        CRef<CSeq_annot>& );
        
    virtual int ReadSeqEntry(
        CNcbiIstream&,
        CRef<CSeq_entry>& );
    
    CReaderBase* m_pReader;

};

//  ============================================================================
void CMultiReaderApp::Init(void)
//  ============================================================================
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "C++ multi format file reader");

    //
    //  shared flags and parameters:
    //        
    arg_desc->AddKey
        ("i", "InputFile",
         "Input File Name",
         CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey
        ("o", "OutputFile",
         "Output File Name",
         CArgDescriptions::eOutputFile, "-"); 

    arg_desc->AddDefaultKey(
        "g", "Flags",
        "Processing Bit Flags",
        CArgDescriptions::eInteger, "0" );

    arg_desc->AddDefaultKey
        ("f", "Format",
         "Input File Format",
         CArgDescriptions::eString, "guess");
    arg_desc->SetConstraint
        ("f", &(*new CArgAllow_Strings, 
            "bed", 
            "microarray", "bed15", 
            "wig", "wiggle",
            "gff",
            "guess"));

    //
    //  wiggle specific flags and parameters:
    //
    arg_desc->AddDefaultKey( "map",
        "map",
        "User defined mappings, format: \"src1:target1;...;srcN:targetN\" ",
        CArgDescriptions::eString,
        "" );
        
    arg_desc->AddDefaultKey( "usermap",
        "usermap",
        "Source for user defined mappings",
        CArgDescriptions::eInputFile,
        "" );
        
    arg_desc->AddDefaultKey( "sitemap",
        "sitemap",
        "Source for site defined mappings",
        CArgDescriptions::eInputFile,
        "" );
        
    arg_desc->AddDefaultKey( "dbmap",
        "dbmap",
        "Source for database provided mappings",
        CArgDescriptions::eString,
        "" );

    //
    //  gff specific flags and parameters:
    //
    arg_desc->AddFlag(
        "all-ids-to-local", 
        "All identifiers are local IDs",
        true );

    arg_desc->AddFlag(
        "numeric-ids-to-local", 
        "Numeric identifiers are local IDs",
        true );
    
    arg_desc->AddFlag(
        "attribute-to-gbqual", 
        "Attribute tags are GenBank qualifiers",
        true );
    
    arg_desc->AddFlag(
        "id-to-product", 
        "Move protein_id and transcript_id to products for mRNA and CDS",
        true );
    
    arg_desc->AddFlag(
        "no-gtf", 
        "Don't honor or recognize GTF conventions",
        true );

    SetupArgDescriptions(arg_desc.release());
}

//  ============================================================================
int
CMultiReaderApp::ReadSeqAnnot(
    CNcbiIstream& ip,
    CRef<CSeq_annot>& annot )
//  ============================================================================
{
    try {
        m_pReader->Read( ip, annot );
    }
    catch (...) {
        const CArgs& args = GetArgs();
        cerr << "Bad source file: Maybe not be " << args["f"].AsString() << endl;
        return 1;
    }
    return 0;
}
    
//  ============================================================================
int
CMultiReaderApp::ReadSeqEntry(
    CNcbiIstream& ip,
    CRef<CSeq_entry>& entry )
//  ============================================================================
{
    try {
        m_pReader->Read( ip, entry );
    }
    catch (...) {
        const CArgs& args = GetArgs();
        cerr << "Bad source file: Maybe not be " << args["f"].AsString() << endl;
        return 1;
    }
    return 0;
}

//  ============================================================================
int 
CMultiReaderApp::Run(void)
//  ============================================================================
{
    //
    //  Setup:
    //
    const CArgs& args = GetArgs();
    CNcbiIstream& ip = args["i"].AsInputFile();
    CNcbiOstream& op = args["o"].AsOutputFile();
    
    //
    //  Create a suitable reader object:
    //
    string format = args["f"].AsString();
    if ( NStr::StartsWith( GetProgramDisplayName(), "wig" ) ) {
        format = "wig";
    }
    if ( NStr::StartsWith( GetProgramDisplayName(), "gff" ) ) {
        format = "gff";
    }
    if ( NStr::StartsWith( GetProgramDisplayName(), "bed" ) ) {
        format = "bed";
    }
    if ( NStr::StartsWith( GetProgramDisplayName(), "b15" ) ) {
        format = "bed15";
    }
    if ( format == "guess" ) {
        m_pReader = CReaderBase::GetReader( 
            CFormatGuess::Format( ip ), args );
    }
    else {
        m_pReader = CReaderBase::GetReader( format, args );
    }
    if ( !m_pReader ) {
        cerr << "Bad format string: " << args["f"].AsString() << endl;
        return 1;
    }

    //
    //  Read and dump:
    //
    switch ( m_pReader->ObjectType() ) {
    
        default: {
            cerr << "Bad reader type: Cannot produce object of requested type" << endl;
            return 1;
        }   
        case CReaderBase::OT_SEQANNOT: {
            CRef<CSeq_annot> annot( new CSeq_annot );
            ReadSeqAnnot( ip, annot );
            op << MSerial_AsnText << *annot << endl;
            break;
        }    
        case CReaderBase::OT_SEQENTRY: {
            CObjectOStreamAsn AsnOut( op );
            CRef<CSeq_entry> entry;
            ReadSeqEntry( ip, entry );
            AsnOut << *entry;
            break;
        }
    }
    
    //
    //  Cleanup:
    //
    delete m_pReader;
    return 0;
}

//  ============================================================================
void CMultiReaderApp::Exit(void)
//  ============================================================================
{
    SetDiagStream(0);
}

//  ============================================================================
int main(int argc, const char* argv[])
//  ============================================================================
{
    // Execute main application function
    return CMultiReaderApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

