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

#include <algo/ms/formats/unimod/unimod__.hpp>
#include <objects/omssa/MSModSpecSet.hpp>
#include <objects/omssa/MSModSpec.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);
USING_SCOPE(unimod);

class CUpdateOmssaModApplication : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};


void CUpdateOmssaModApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "updates omssa's mods.xml info from unimod.xml");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("reg", "RegistryFile",
         "name of file to read equivalent modification numbers from (omssa_unimod.ini by default)",
         CArgDescriptions::eInputFile, "omssa_unimod.ini", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey
        ("unimod", "UnimodFile",
         "name of Unimod XML file to read (unimod.xml by default)",
         CArgDescriptions::eInputFile, "unimod.xml", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey
        ("mods", "ModsFile",
         "name of OMSSA mods XML file to read (mods.xml by default)",
         CArgDescriptions::eInputFile, "mods.xml", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey
        ("output", "OutputFile",
         "name of file to write Omssa mods XML data to (standard output by default)",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CUpdateOmssaModApplication::Run(void)
{
    // Open conversion registry
    CNcbiIstream& regFile = GetArgs()["reg"].AsInputFile();
    CNcbiRegistry reg(regFile);

    // Open Unimod file
    CNcbiIstream& unimodFile = GetArgs()["unimod"].AsInputFile();
    CUnimod unimod;
    unimodFile >> MSerial_Xml >> unimod;

    // Open OMSSA mods file
    CNcbiIstream& modsFile = GetArgs()["mods"].AsInputFile();
    CMSModSpecSet mods;
    modsFile >> MSerial_Xml >> mods;

    // Open output file
    CNcbiOstream& outFile = GetArgs()["output"].AsOutputFile();    

    ITERATE(CMSModSpecSet::Tdata, iMod, mods.Get()) {
        CRef<CMSModSpec> oMod = *iMod;
        int oModNum = oMod->GetMod();
        string ans = reg.Get("OmssaToUnimod", NStr::IntToString(oModNum));

        if (!ans.empty()) {
            int uniAcc = NStr::StringToInt(ans);        
            oMod->SetUnimod(uniAcc);

            CRef<CMod> uMod = unimod.FindMod(uniAcc);
            //outFile << "OMSSA: " << oModNum << "\tUnimod: " << uniAcc << endl;
            if (uMod) {
                oMod->SetPsi_ms(uMod->GetAttlist().GetTitle());
                //outFile << "\tPSI-MS name: " << uMod->GetAttlist().GetTitle() << endl;
            }
        }
    }

    outFile << MSerial_Xml << mods;

    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CUpdateOmssaModApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
