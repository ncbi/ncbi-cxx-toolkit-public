/* 
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
 *  and reliability of the software and data, the NLM 
 *  Although all reasonable efforts have been taken to ensure the accuracyand the U.S.
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
 * Author:  Douglas J. Slotta
 *
 * File Description:
 *   Code for converting Spectra of various formats to HDF5
 *
 */

#ifndef MSHDF5_HPP
#define MSHDF5_HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>
#include <algo/ms/formats/mzxml/Scan.hpp>

#include <H5Cpp.h>
#include <set>

BEGIN_NCBI_SCOPE

class CMsHdf5 : public CObject {
    
  public:

    typedef vector<vector<float> > TSpectrum;
    typedef vector<TSpectrum> TSpectra;

    struct MSMapEntry {
        Uint4 scan;
        Uint4 msLevel;
        Uint4 idx;
        Uint4 parentScan;
    };  
    typedef map<Uint4, MSMapEntry> TSpecMap;
    typedef map<Uint4, double> TSpecPrecursorMap;

    class UnknownGroup : public std::runtime_error {
        public:
            UnknownGroup(const string &arg) : std::runtime_error(arg) { }
    };

    class OutOfBounds : public std::range_error {
        public:
            OutOfBounds(const string &arg) : std::range_error(arg) { }
    };

    // constructor
    CMsHdf5(string filename) { init(filename, H5F_ACC_RDONLY); }
    CMsHdf5(string filename, unsigned int flags) { init(filename, flags); }
    // destructor
    ~CMsHdf5(void);

    void newSpectraSet(string name);

    void addSpectrum(Uint4 scan, Uint4 msLevel, Uint4 parentScan, vector<float> &mz, vector<float> &it, string &info);

    void writeSpectra(void);
    void addAttribute(string key, string value);
    void addMetadata(string data);
    string readMetadata(string spectraSetName);

    string getCurrentPath(void);
    void pushDir(string name, bool isScan = false);
    void popDir(bool isScan = false);

    void printSpectra(void);

    // input string format: "spectraSetName:scan\tindex"
    void getSpectrum(string src, TSpectrum& spectrum, objects::CScan& scan, string msLevel="2");
    
    void getSpectraMap(const string &spectraSetName, TSpecMap &specMap);
    void getPrecursorMzs(const string &spectraSetName, TSpecMap &specMap, TSpecPrecursorMap &specPreMap);

    void getGroups(set<string> &groups);    
    const set<string>& getGroups();

    bool hasGroup(const string &groupName);

    static void encodePeaks(const TSpectrum& spectrum, string &b64Str);

    H5::H5File* raw(void); // Dangerous, use only when absolutely required 
    
  private:
    // Prohibit copy constructor and assignment operator
    CMsHdf5(const CMsHdf5& value);
    CMsHdf5& operator=(const CMsHdf5& value);

    void init(string filename, unsigned int flags);

    void addSpectrumData(Uint4 msLevel, vector<float> &mz, vector<float> &it);
    void addSpectrumInfo(Uint4 msLevel, string &info);

    void writeSpectra(Uint4 msLevel); // called from writeSpectra()
    void writeSpectraInfo(Uint4 msLevel); // called from writeSpectra(Uint4 msLevel)
    void spectraDimensions(const Uint4 msLevel, Uint4 &nSpectra, Uint4 &maxLen);
    void spectraInfoDimensions(const Uint4 msLevel, Uint4 &nSpectra, Uint4 &maxLen);

    Uint4 computeNumChunkElements(size_t elementSize, Uint4 maxElements);
    
    static herr_t get_groups_helper(hid_t loc_id, const char *name, void *opdata);
    
    H5::DataSet openDataSet(string spectraSetName, string dataSetName);

    H5::H5File* h5file;
    H5::Group* m_curGroup;

    set<string> m_groups;
        
    struct iter_info {
        int index; // index of the current object
        set<string> *groups;                     
    };                                 

    string m_filename;
    string m_curSetRoot;
    list<string> m_pathStack;
    string m_path;

    typedef vector<MSMapEntry> TSpecMatrix;
    typedef vector<TSpectra> TMsLevels;
    typedef vector<string> TSpectrumInfo;
    typedef vector<TSpectrumInfo> TMsLevelsInfo;
    
    TSpecMatrix m_specMap;
    TMsLevels m_msLevels;
    TMsLevelsInfo m_msLevelsInfo;
    
  public:
    static double mzToAvgMass(double mz, int charge);
    static double mzToMonoMass(double mz, int charge);    
};


END_NCBI_SCOPE

#endif // MSHDF5_HPP
