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
* Author:  Douglas Slotta
*
* File Description: Used to test the parsimony library
* 
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>

#include <algo/ms/formats/unimod/unimod__.hpp>


USING_SCOPE(ncbi);
USING_SCOPE(objects);
USING_SCOPE(unimod);

class CUnimodApplication : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};


void CUnimodApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "unimod demo program");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("input", "InputFile",
         "name of file to read tab-delimited data from (standard input by default)",
         CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey
        ("output", "OutputFile",
         "name of file to write XML data to (standard output by default)",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);
	arg_desc->AddFlag("xml", "Output file as XML");
	arg_desc->AddFlag("asn", "Output file as ASN.1 (default)");
	arg_desc->AddFlag("asntext", "Output file as ASN.1 text");


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CUnimodApplication::Run(void)
{
    const CArgs& args = GetArgs();
    
    CNcbiIstream& inFile = args["input"].AsInputFile();
    CNcbiOstream& outFile = args["output"].AsOutputFile();
    inFile >> MSerial_Xml;
    outFile << MSerial_AsnBinary;


    string inFilename = args["input"].AsString();
    if (inFilename == "-") {
        cerr << "Reading from STDIN, assuming XML" << endl;
    } else {
        CFile file(inFilename);
        string ext = file.GetExt();
        cerr << "Reading " << inFilename << " as ";
        if (ext == ".xml") {
            cerr << "XML" << endl;
            inFile >> MSerial_Xml;
        } else if (ext == ".asn") {
            cerr << "ASN" << endl;
            inFile >>  MSerial_AsnBinary;
        } else if (ext == ".asntxt") {
            cerr << "ASN Text" << endl;
            inFile >> MSerial_AsnText;
        }
    }

	if (args["xml"]) {
        outFile << MSerial_Xml;
	} else if (args["asn"]) {
        outFile << MSerial_AsnBinary;
	} else if (args["asntext"]) {
        outFile << MSerial_AsnText;
	}

    CUnimod unimod;

    inFile >> unimod;
    outFile << unimod;

    //CRef<CMod> mod = unimod.FindMod(10);
    //outFile << mod->GetAttlist().GetTitle() << endl;

    //outFile << "[unimodtranslate]" << endl;
    
    //ITERATE(CModifications::TMod, iMod, unimod.GetModifications().GetMod()) {
    //    string id = (*iMod)->GetAttlist().GetRecord_id();
    //    string title = (*iMod)->GetAttlist().GetTitle();
    //    outFile << id << "=" << title << endl;
    //}

    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CUnimodApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
