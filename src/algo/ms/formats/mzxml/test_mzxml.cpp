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
* File Description: Used to test the mzxml library
* 
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

#include <algo/ms/formats/mzxml/mzXML__.hpp>


USING_SCOPE(ncbi);
USING_SCOPE(objects);
USING_SCOPE(SPC);

class CMzxmlApplication : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};


void CMzxmlApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "mzxml demo program");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("input", "InputFile",
         "name of file to read tab-delimited data from (standard input by default)",
         CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey
        ("output", "OutputFile",
         "name of file to write XML data to (standard output by default)",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CMzxmlApplication::Run(void)
{
    CNcbiIstream& inFile = GetArgs()["input"].AsInputFile();
    CNcbiOstream& outFile = GetArgs()["output"].AsOutputFile();

    CMzXML mzxml;

    inFile >> MSerial_Xml >> mzxml;

    outFile << MSerial_Xml << mzxml;
    //outFile << MSerial_AsnBinary << mzxml;


    // Read as text ASN.1
    //GetArgs()["input"].AsInputFile() >> MSerial_AsnText >> obj;

    // Write as XML
    //GetArgs()["output"].AsOutputFile() << MSerial_Xml << pSet;
    // Write as ASN.1
    //GetArgs()["output"].AsOutputFile() << MSerial_AsnText << *mzxml;
    //GetArgs()["output"].AsOutputFile() << MSerial_AsnBinary << *mzxml;

    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CMzxmlApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
