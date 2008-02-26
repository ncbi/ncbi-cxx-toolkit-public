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
 * Authors:  Douglas Slotta
 *
 * File Description:
 *   Command line utility to convert OMSSA output to the PepXML format
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <objects/omssa/omssa__.hpp>

#include "omssa.hpp"
#include "pepxml.hpp"

USING_SCOPE(ncbi);
USING_SCOPE(objects);
USING_SCOPE(omssa);

// Helper function for debugging, might need this again in the future
// void PrintModInfo(CRef <CMSModSpecSet> Modset) {
//     for (unsigned int modNum = 0; modNum < Modset->Get().size(); modNum++) {
//         cout << MSSCALE2DBL(Modset->GetModMass(modNum)) << "\t";
//         cout << MSSCALE2DBL(Modset->GetNeutralLoss(modNum)) << "\t";
//         cout << Modset->GetModNumChars(modNum) << "\t";
//         for (int i=0; i< Modset->GetModNumChars(modNum); i++) {
//             cout << ConvertAA(Modset->GetModChar(modNum, i)) << " ";
//             cout << MonoMass[static_cast <int> (Modset->GetModChar(modNum,i))] << " ";
//         }
//         cout << "\t" << Modset->GetModType(modNum) << "\t";
//         cout << Modset->GetModName(modNum) << endl;
//     }
// }

/////////////////////////////////////////////////////////////////////////////
//  COmssa2pepxmlApplication::


class COmssa2pepxmlApplication : public CNcbiApplication
{
public:
    COmssa2pepxmlApplication();
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


COmssa2pepxmlApplication::COmssa2pepxmlApplication() {
    SetVersion(CVersionInfo(1, 0, 0));
}

/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments
void COmssa2pepxmlApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);    
    arg_desc->PrintUsageIfNoArgs();

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Convert OMSSA ASN.1 and XML output files to PepXML");

    // Describe the expected command-line arguments
    arg_desc->AddPositional
        ("filename",
         "The name of the XML file to load",
         CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)
int COmssa2pepxmlApplication::Run ( void )
{
    CMSSearch inOMSSA;
    CPepXML outPepXML;
    ESerialDataFormat format;

    // Get arguments
    const CArgs& args = GetArgs();

    string filename = args["filename"].AsString();
    CFile file(filename);
    string basename = file.GetBase();
    string ext = file.GetExt();
    string newname = basename + ".pep.xml";

    cout << "Reading " << filename << " as ";
    if (ext == ".oms") {
        cout << "ASN" << endl;
        format = eSerial_AsnBinary;
    } else if (ext == ".omx") {
        cout << "XML" << endl;
        format = eSerial_Xml;
    } else if (ext == ".omt") {
        cout << "ASN text" << endl;
        format = eSerial_AsnText;
    }
    auto_ptr<CObjectIStream> file_in(CObjectIStream::Open(filename, format));
    auto_ptr<CObjectOStream> file_out(CObjectOStream::Open(newname, eSerial_Xml));


    *file_in >> inOMSSA;

    CRef <CMSModSpecSet> Modset(new CMSModSpecSet);
    CSearchHelper::ReadModFiles("mods.xml","usermods.xml",GetProgramExecutablePath(), Modset); 
    Modset->CreateArrays();
    //PrintModInfo(Modset);

    outPepXML.ConvertFromOMSSA(inOMSSA, Modset, basename);

    *file_out << outPepXML;

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void COmssa2pepxmlApplication::Exit ( void )
{
    SetDiagStream ( 0 );
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main ( int argc, const char* argv[] )
{
    // Execute main application function
    return COmssa2pepxmlApplication().AppMain ( argc, argv, 0, eDS_Default, 0 );
}
