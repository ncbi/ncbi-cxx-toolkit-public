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
* File Description:
*   Simple program demonstrating the use of serializable objects (in this
*   case, biological sequences).  Does NOT use the object manager.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/serial.hpp>
#include <serial/objostrxml.hpp>
#include <util/compress/zlib.hpp>
#include <util/compress/bzip2.hpp>

#include <algo/ms/formats/mshdf5/MsHdf5.hpp>
#include <algo/ms/formats/mzxml/mzXML__.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);
USING_SCOPE(SPC);

class CPkl2hdf5Application : public CNcbiApplication
{
    struct pklSpectrum {
        CRef<CScan> scan;
        double mass;
        vector<float> mz;
        vector<float> it;
    };

    virtual void Init(void);
    virtual int  Run(void);
    void parse_pkl(const string &groupName, CNcbiIstream &iStream, CRef<CMsHdf5> msHdf5);
    static bool pklSpecComp(pklSpectrum i, pklSpectrum j); 
    bool m_doSort;
};

void CPkl2hdf5Application::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Convert a set of PKL files to hdf5");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("out", "OutputFile",
         "name of msHdf5 file to create",
         CArgDescriptions::eOutputFile, "data.hdf5", CArgDescriptions::fPreOpen);

    arg_desc->AddFlag("sort", "Sort spectra by precursor mass");

    arg_desc->AddExtra
        (1, kMax_UInt,
         "One or more PKL files to add to the new HDF5 file",
         CArgDescriptions::eInputFile);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

bool CPkl2hdf5Application::pklSpecComp(pklSpectrum i, pklSpectrum j) { 
    return (i.mass < j.mass); 
}

void CPkl2hdf5Application::parse_pkl(const string &groupName, CNcbiIstream& iStream, CRef<CMsHdf5> msHdf5)
{
    string row;
    vector<pklSpectrum> pklSpectra;

    msHdf5->newSpectraSet(groupName);  // Need to catch existing path exception

    // Read spectra
    while (NcbiGetlineEOL(iStream, row)) {
        list<string> cols;
        NStr::Split(NStr::TruncateSpaces(row), " ", cols);
        int sz;

        switch (cols.size()) {
        case 0: // end of spectrum            
            sz = pklSpectra.back().mz.size();
            pklSpectra.back().scan->SetAttlist().SetPeaksCount(sz);
            break;
        case 2:  // m/z - intensity pair
            pklSpectra.back().mz.push_back(NStr::StringToDouble(cols.front()));
            cols.pop_front();
            pklSpectra.back().it.push_back(NStr::StringToDouble(cols.front()));
            break;
        case 3:  // start of spectrum
        {
            pklSpectrum pkSpec;
            pkSpec.scan = new CScan();
            CRef<CPrecursorMz> preCursor(new CPrecursorMz());
            double mz = NStr::StringToDouble(cols.front());
            preCursor->SetPrecursorMz(mz);
            cols.pop_front();
            preCursor->SetAttlist().SetPrecursorIntensity(NStr::StringToDouble(cols.front()));
            cols.pop_front();
            int charge = NStr::StringToInt(cols.front());
            preCursor->SetAttlist().SetPrecursorCharge(charge);
            pkSpec.scan->SetPrecursorMz().push_back(preCursor);
            pkSpec.scan->SetAttlist().SetMsLevel(2);
            pkSpec.mass = CMsHdf5::mzToMonoMass(mz, charge);
            pklSpectra.push_back(pkSpec);
        } break;
        default:
            cerr << "Improperly formatted PKL file, aborting" << endl;
            return;
        }
        
    }

    // Write spectra
    if (m_doSort) {
        sort(pklSpectra.begin(), pklSpectra.end(), pklSpecComp);
    }
    
    int scanNum = 1;
    NON_CONST_ITERATE (vector<pklSpectrum>, iPkSpec, pklSpectra) {
        iPkSpec->scan->SetAttlist().SetNum(scanNum);
        CNcbiOstrstream bufStream;
        bufStream << MSerial_Xml << *(iPkSpec->scan);
        bufStream.flush();
        string buf = bufStream.str();
        size_t start = NStr::Find(buf,"<scan");
        size_t stop = (NStr::Find(buf,"</scan>") + 7) - start;
        string info = buf.substr(start, stop);
        msHdf5->addSpectrum(scanNum, 2, 0, iPkSpec->mz, iPkSpec->it, info);
        scanNum++;
    }
    CMsRun msRun;
    CRef<CParentFile> pFile(new CParentFile);
    pFile->SetAttlist().SetFileName("file://" + groupName + ".pkl");
    pFile->SetAttlist().SetFileType(CParentFile::C_Attlist::eAttlist_fileType_processedData);
    pFile->SetAttlist().SetFileSha1("");
    msRun.SetParentFile().push_back(pFile);
    CRef<CDataProcessing> pDataProc(new CDataProcessing);
    pDataProc->SetSoftware().SetAttlist().SetType(CSoftware::C_Attlist::eAttlist_type_conversion);
    pDataProc->SetSoftware().SetAttlist().SetName("pkl2hdf5");
    pDataProc->SetSoftware().SetAttlist().SetVersion("1");
    msRun.SetDataProcessing().push_back(pDataProc);

    // Convert to string
    CNcbiOstrstream msRunStream;
    msRunStream << MSerial_Xml << msRun;
    msRunStream.flush();
    string mBuf = msRunStream.str();
    size_t mStart = NStr::Find(mBuf,"<msRun");
    size_t mStop = (NStr::Find(mBuf,"</msRun>") + 8) - mStart;
    string mInfo = mBuf.substr(mStart, mStop);   
    msHdf5->addMetadata(mInfo);
    msHdf5->writeSpectra();
}

int CPkl2hdf5Application::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();
    
    string outFile = args["out"].AsString();

    m_doSort = args["sort"];
    if (m_doSort) cerr <<  "Sorting spectra by Pre-cursor M/Z" << endl; 


    CRef<CMsHdf5> msHdf5(new CMsHdf5(outFile, H5F_ACC_TRUNC));

    CZipStreamDecompressor gzip(CZipCompression::fCheckFileHeader);
    CBZip2StreamDecompressor bzip2;

    for (size_t extra = 1;  extra <= args.GetNExtra();  extra++) {
        string fileName = args[extra].AsString();
        cerr << "Reading " << fileName << " as SpectraSet " ;
        size_t startPos = fileName.rfind("/");
        if (startPos == string::npos) {
            startPos = 0; 
        } else {
            startPos++;
        }
        size_t pos = fileName.rfind(".");
        string suffix(fileName.substr(pos));
        string base;

        CNcbiIfstream *ifStream = new CNcbiIfstream(fileName.c_str(), ios::binary);
        CNcbiIstream *pStream=NULL;

        if (suffix == ".bz2") {
            pStream = new CCompressionIStream(*ifStream, &bzip2);
            base = fileName.substr(startPos,fileName.rfind(".", pos-1)-startPos);
        }
        else if (suffix == ".gz") {
            pStream = new CCompressionIStream(*ifStream, &gzip);        
            base = fileName.substr(startPos,fileName.rfind(".", pos-1)-startPos);
        }
        else { // no compression
            if (fileName.empty() || fileName == "-") {
                // Should never be here!
                
            }
            else {
                pStream = ifStream;
                base = fileName.substr(startPos,pos-startPos);
            }
        }
        cerr << base << endl;
            
        parse_pkl(base, *pStream, msHdf5);
        delete pStream;
    }

    CFile(outFile).SetMode(ncbi::CDirEntry::fDefault,
                           ncbi::CDirEntry::fWrite | ncbi::CDirEntry::fRead,
                           ncbi::CDirEntry::fDefault,
                           ncbi::CDirEntry::fDefault);

    // Exit successfully
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CPkl2hdf5Application().AppMain(argc, argv, 0, eDS_Default, 0);
}
