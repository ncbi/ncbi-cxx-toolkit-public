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
*   Test application for the CFormatGuess component
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
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <util/format_guess.hpp>

typedef std::map<unsigned int, std::string> FormatMap;
typedef FormatMap::iterator FormatIter;

USING_NCBI_SCOPE;
//USING_SCOPE(objects);

//  ============================================================================
class CFormatGuessApp
//  ============================================================================
     : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};

//  ============================================================================
void CFormatGuessApp::Init(void)
//  ============================================================================
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "CFormatGuess front end: Guess various file formats");

    //
    //  shared flags and parameters:
    //        
    arg_desc->AddKey
        ("i", "InputFile",
         "Input File Name",
         CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());
}

//  ============================================================================
int 
CFormatGuessApp::Run(void)
//  ============================================================================
{
    const CArgs& args = GetArgs();
    const string strFileName = args["i"].AsString();

    FormatMap FormatStrings;
    FormatStrings[ CFormatGuess::eUnknown ] = "Format not recognized";
    FormatStrings[ CFormatGuess::eBinaryASN ] = "Binary ASN.1";
    FormatStrings[ CFormatGuess::eTextASN ] = "Text ASN.1";
    FormatStrings[ CFormatGuess::eFasta ] = "FASTA sequence record";
    FormatStrings[ CFormatGuess::eXml ] = "XML";
    FormatStrings[ CFormatGuess::eRmo ] = "RepeatMasker Out";
    FormatStrings[ CFormatGuess::eGlimmer3 ] = "Glimmer3 prediction";
    FormatStrings[ CFormatGuess::ePhrapAce ] = "Phrap ACE assembly file";
    FormatStrings[ CFormatGuess::eGtf ] = "GFF/GTF style annotation";
    FormatStrings[ CFormatGuess::eAgp ] = "AGP format assembly";
    FormatStrings[ CFormatGuess::eNewick ] = "Newick tree";
    FormatStrings[ CFormatGuess::eDistanceMatrix ] = "Distance matrix";
    FormatStrings[ CFormatGuess::eFiveColFeatureTable ] = 
        "Five column feature table";
    FormatStrings[ CFormatGuess::eTaxplot ] = "Tax plot";
    FormatStrings[ CFormatGuess::eTable ] = "Generic table";
    FormatStrings[ CFormatGuess::eAlignment ] = "Text alignment";
    FormatStrings[ CFormatGuess::eFlatFileSequence ] = 
        "Flat file sequence portion";
    FormatStrings[ CFormatGuess::eSnpMarkers ] = "SNP marker flat file";
    FormatStrings[ CFormatGuess::eWiggle ] = "UCSC Wiggle file";
    FormatStrings[ CFormatGuess::eBed ] = "UCSC BED file";
    FormatStrings[ CFormatGuess::eBed15 ] = "UCSC microarray file";
    FormatStrings[ CFormatGuess::eHgvs ] = "HGVS Variation file";
                
    CFormatGuess Guesser( strFileName );
    CFormatGuess::EFormat uFormat = Guesser.GuessFormat();

    cout << strFileName << " :   ";
    
    FormatIter it = FormatStrings.find( uFormat );
    if ( it == FormatStrings.end() ) {
        cout << "Unmapped format [" << uFormat << "]";
    }
    else {
        cout << it->second;
    }
    
    cout << endl;
            
    return 0;
}

//  ============================================================================
void CFormatGuessApp::Exit(void)
//  ============================================================================
{
    SetDiagStream(0);
}

//  ============================================================================
int main(int argc, const char* argv[])
//  ============================================================================
{
    // Execute main application function
    return CFormatGuessApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

