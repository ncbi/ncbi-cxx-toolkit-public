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
#include <util/compress/bzip2.hpp> 

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
    SetVersion(CVersionInfo(2, 1, 4));
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
	arg_desc->AddFlag("xml", "Input file is XML");
    arg_desc->AddFlag("bz2", "Input file is bzipped XML");
    arg_desc->AddFlag("asn", "Input file is ASN.1");
	arg_desc->AddFlag("asntext", "Input file is ASN.1 text");

	arg_desc->AddOptionalKey("o", "outfile", 
		"filename for pepXML formatted search results",
		CArgDescriptions::eString);

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
    ESerialDataFormat format = eSerial_Xml;  // assume xml

    // Get arguments
    const CArgs& args = GetArgs();

    string filename = args["filename"].AsString();
	CFile file(filename);

	string fullpath, path, base, ext;
	if (CFile::IsAbsolutePath(file.GetPath())) {
		fullpath = file.GetPath();
	} else {
		fullpath = CFile::CreateAbsolutePath(file.GetPath());
	}
	CFile::SplitPath(fullpath, &path, &base, &ext);
	string basename = path + base;
	string newname;
	if (args["o"].HasValue()) {
		newname = args["o"].AsString();
	} else {
		newname = basename + ".pep.xml";
	}

	// figure out input file type
	bool notSet = true;
	if (args["xml"] || args["bz2"]) {
		format = eSerial_Xml;
		notSet = false;
	} else if (args["asn"]) {
		format = eSerial_AsnBinary;
		notSet = false;
	} else if (args["asntext"]) {
		format = eSerial_AsnText;
		notSet = false;
	}

	if (notSet) { // Not explict, maybe extension gives us a clue?
		if (ext == ".oms") {
			format = eSerial_AsnBinary;
		} else if (ext == ".omx") {
			format = eSerial_Xml;
		} else if (ext == ".omt") {
			format = eSerial_AsnText;
		}
	}
	cout << "Reading " << filename << " as ";
	switch (format) {
    case eSerial_AsnBinary:
        cout << "ASN" << endl; break;
    case eSerial_Xml:
        cout << "XML" << endl; break;
    case eSerial_AsnText:
        cout << "ASN text" << endl; break;
    default:
        cout << "Unable to determine type of file" << endl; 
        return 0;
	}

    CSearchHelper::ReadCompleteSearch(filename, format, args["bz2"], inOMSSA);

    if (!inOMSSA.CanGetRequest()) {
		cout << "Sorry, this file cannot be converted." << endl;
		cout << "The original search needs to have been executed with the '-w' flag set." << endl;
		cout << "The search settings are not availiable in this file. Aborting" << endl;
		return 0;
	}

    CRef <CMSModSpecSet> Modset(new CMSModSpecSet);
    CSearchHelper::ReadModFiles("mods.xml","usermods.xml",GetProgramExecutablePath(), Modset); 
    Modset->CreateArrays();
    //PrintModInfo(Modset);

    outPepXML.ConvertFromOMSSA(inOMSSA, Modset, basename, newname);

    auto_ptr<CObjectOStream> file_out(CObjectOStream::Open(newname, eSerial_Xml));
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
