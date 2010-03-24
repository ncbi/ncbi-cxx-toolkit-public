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
* Author: Douglas
*
* File Description:
*   Utility to convert a HDF5 file to one or more mzXML file(s)
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrxml.hpp>
#include <algo/ms/formats/mzxml/mzXML__.hpp>

#include <algo/ms/formats/mshdf5/MsHdf5.hpp>

USING_SCOPE(ncbi);

class CHdf2mzXMLApplication : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};

void CHdf2mzXMLApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Convert a set of mzXML files to hdf5");

    // Describe the expected command-line arguments
    arg_desc->AddPositional
        ("input", 
         "name of msHdf5 file to convert",
         CArgDescriptions::eInputFile);

    arg_desc->AddExtra
        (0, kMax_UInt,
         "Optional subset of mzXML files to extract",
         CArgDescriptions::eOutputFile);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CHdf2mzXMLApplication::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();
    string inFile = args["input"].AsString();

    CRef<CMsHdf5> msHdf5(new CMsHdf5(inFile));

    set<string> groups;    
    if (args.GetNExtra()) {
        for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
            groups.insert(args[extra].AsString());
        }
    } else {
        msHdf5->getGroups(groups);
    }

    ITERATE(set<string>, iGroup, groups) {
        string group = *iGroup;
        string outFile = group + ".mzXML";
        cerr << "Extracting " << group << " as as " << outFile << endl;
        
        objects::SPC::CMzXML mzXML;
        mzXML.SetIndexOffset("0");
        string mdStr = msHdf5->readMetadata(group);
        CNcbiIstrstream bufStream(mdStr.c_str());
        bufStream >> MSerial_Xml >> mzXML.SetMsRun();
        
        CMsHdf5::TSpecMap specMap;
        msHdf5->getSpectraMap(group, specMap);

        map<Uint4, CRef<objects::SPC::CScan> > specTree;
        
        ITERATE(CMsHdf5::TSpecMap, iSMap, specMap) {
            Uint4 scanNum = (*iSMap).second.scan;
            Uint4 parentNum = (*iSMap).second.parentScan;
            int msLevel =  (*iSMap).second.msLevel;
            int idx = (*iSMap).second.idx;
            CRef<objects::SPC::CScan> pScan(new objects::SPC::CScan);
            CMsHdf5::TSpectrum spec;
            
            string specName = group + ":" 
                + NStr::IntToString(scanNum) + "\t" 
                + NStr::IntToString(idx);

            msHdf5->getSpectrum(specName, spec, *pScan, NStr::IntToString(msLevel));
            string b64peaks;
            CMsHdf5::encodePeaks(spec, b64peaks);
            
            pScan->SetPeaks().SetAttlist().SetPrecision(32);
            pScan->SetPeaks().SetAttlist().SetByteOrder("network");
            pScan->SetPeaks().SetAttlist().SetPairOrder("m/z-int");
            pScan->SetPeaks().SetPeaks(b64peaks);

            
            if (parentNum == 0) {
                specTree[scanNum] = pScan;
                mzXML.SetMsRun().SetScan().push_back(pScan);
            } else {
                specTree[parentNum]->SetScan().push_back(pScan);
            }   
        }
        auto_ptr<CObjectOStream> oStream(CObjectOStream::Open(outFile, eSerial_Xml));
        CObjectOStreamXml *xml_out = dynamic_cast <CObjectOStreamXml *> (oStream.get());
        xml_out->SetDefaultSchemaNamespace("http://sashimi.sourceforge.net/schema_revision/mzXML_2.1");
        xml_out->SetReferenceSchema();
        *xml_out << mzXML;

        CFile(outFile).SetMode(ncbi::CDirEntry::fDefault,
                               ncbi::CDirEntry::fWrite | ncbi::CDirEntry::fRead,
                               ncbi::CDirEntry::fDefault,
                               ncbi::CDirEntry::fDefault);
    }


    // Exit successfully
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CHdf2mzXMLApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
