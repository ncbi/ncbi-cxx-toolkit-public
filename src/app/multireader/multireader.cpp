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

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/bed_reader.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);
//USING_SCOPE(sequence);

//  ============================================================================
class CMultiReaderApp
//  ============================================================================
     : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    CReader* m_pReader;

};

//  ============================================================================
void CMultiReaderApp::Init(void)
//  ============================================================================
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "C++ speed test program");

    arg_desc->AddKey
        ("i", "InputFile",
         "Input File Name",
         CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey
        ("o", "OutputFile",
         "Output File Name",
         CArgDescriptions::eOutputFile, "-");

    arg_desc->AddDefaultKey
        ("f", "Format",
         "Input File Format",
         CArgDescriptions::eString, "guess");
    arg_desc->SetConstraint
        ("f", &(*new CArgAllow_Strings, "bed", "guess"));

    arg_desc->AddDefaultKey(
        "g", "Flags",
        "Processing Bit Flags",
        CArgDescriptions::eInteger, "0" );

    SetupArgDescriptions(arg_desc.release());
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
    if ( format == "guess" ) {
        m_pReader = CReader::GetReader( CReader::GuessFormat( ip ), args["g"].AsInteger() );
    }
    else {
        m_pReader = CReader::GetReader( format, args["g"].AsInteger() );
    }
    if ( !m_pReader ) {
        cerr << "Bad format string: " << args["f"].AsString() << endl;
        return 1;
    }

    //
    //  Read:
    //
    CRef<CSeq_annot> annot( new CSeq_annot );
    try {
        m_pReader->Read( ip, annot );
    }
    catch (...) {
        cerr << "Bad source file: Maybe not " << args["f"].AsString() << endl;
        return 1;
    }

    //
    //  Dump:
    //
    op << MSerial_AsnText << *annot << endl;

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

