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
 * Author:  Douglas J. Slotta
 *
 * File Description:
 *   Code for converting Spectra of various formats to HDF5
 *
 */

// standard includes
#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbistre.hpp>
#include <util/xregexp/regexp.hpp>
#include <connect/ncbi_socket.h>

#include <algo/ms/formats/mshdf5/MsHdf5.hpp>

#define AVG_HYD 1.00794
#define MONO_HYD 1.007825035
#define MAX_MSLEVEL 3
#define MSLEVEL_BOUND MAX_MSLEVEL + 1

BEGIN_NCBI_SCOPE
USING_SCOPE(H5);

void CMsHdf5::init(string filename, unsigned int flags) 
{
    H5::Exception::dontPrint();
    h5file = new H5File(filename, flags);

    m_curGroup = NULL;
    m_groups.clear();
    m_pathStack.clear();
    m_msLevels.resize(MSLEVEL_BOUND);
    m_msLevelsInfo.resize(MSLEVEL_BOUND);
}

CMsHdf5::~CMsHdf5(void)
{
    if (m_curGroup) delete m_curGroup;
    
    delete h5file;
}

void CMsHdf5::newSpectraSet(string name) 
{
    m_curSetRoot = "/" + name;
    if (m_curGroup) delete m_curGroup;
    m_curGroup = new Group(h5file->createGroup(m_curSetRoot));
    m_pathStack.clear();
    m_path = getCurrentPath();
    m_specMap.clear();
    for (Uint4 i=0; i<MSLEVEL_BOUND; i++) {
        m_msLevels[i].clear();
        m_msLevelsInfo[i].clear();
    }
}

void CMsHdf5::addSpectrum(Uint4 scan, Uint4 msLevel, Uint4 parentScan, vector<float> &mz, vector<float> &it, string &info) 
{
    MSMapEntry mapEntry;
    mapEntry.scan = scan;
    mapEntry.msLevel = msLevel;
    mapEntry.idx = m_msLevels[msLevel].size();
    mapEntry.parentScan = parentScan;
    m_specMap.push_back(mapEntry);

    addSpectrumData(msLevel, mz, it);
    addSpectrumInfo(msLevel, info);
}

void CMsHdf5::addSpectrumData(Uint4 msLevel, vector<float> &mz, vector<float> &it) 
{
    TSpectrum spec;
    spec.push_back(mz);
    spec.push_back(it);

    if (m_msLevels.size() < (msLevel)) {
        m_msLevels.resize(msLevel+1);
    }

    m_msLevels[msLevel].push_back(spec);
}

void CMsHdf5::addSpectrumInfo(Uint4 msLevel, string &info) 
{
    if (m_msLevelsInfo.size() < (msLevel)) {
        m_msLevelsInfo.resize(msLevel+1);
    }

    m_msLevelsInfo[msLevel].push_back(info);
}

bool mapEntryComp(CMsHdf5::MSMapEntry i, CMsHdf5::MSMapEntry j) { return (i.scan < j.scan); }

void CMsHdf5::writeSpectra() 
{
    // Create spectra map
    sort(m_specMap.begin(), m_specMap.end(), mapEntryComp);
    
    Uint4 nRows = m_specMap.size();
    Uint4* data = new Uint4[ nRows * 4 ];
    
    for (Uint4 x=0; x<nRows; x++) {
        Uint4 point = (x*4);
        data[point] = m_specMap[x].scan;
        data[point+1] = m_specMap[x].msLevel;
        data[point+2] = m_specMap[x].idx;
        data[point+3] = m_specMap[x].parentScan;
    }
    m_specMap.clear();
    
    // Array size
    hsize_t dimensions[2];
    dimensions[0] = nRows;
    dimensions[1] = 4;
    DataSpace dataspace(2, dimensions);

    // HDF5 Creation properties
    DSetCreatPropList parameters;
    parameters.setDeflate(6);
    hsize_t chunk_dims[2];
    Uint4 chunk_size = computeNumChunkElements(sizeof(Uint4)*4, nRows);
    chunk_dims[0] = chunk_size;
    chunk_dims[1] = 4;
    parameters.setChunk(2, chunk_dims);

    // Define datatype
    FloatType datatype(PredType::STD_U32LE);
    datatype.setOrder(H5T_ORDER_LE);

    // New dataset
    DataSet dataset = h5file->createDataSet(m_path + "/map", datatype, dataspace, parameters);

    dataset.write(data, PredType::NATIVE_UINT);

    delete [] data;

    // write ms peak data for all levels
    for (Uint4 w=0; w<m_msLevels.size(); w++) {
        if (m_msLevels[w].size() > 0) {
            writeSpectra(w);
        }
    }
}

void CMsHdf5::writeSpectra(Uint4 msLevel) 
{

    Uint4 nSpec, mLen;
    spectraDimensions(msLevel, nSpec, mLen);

    if (nSpec < 1) return;

    writeSpectraInfo(msLevel);

    float* data = new float[ nSpec * mLen * 2];
    for (Uint4 i=0; i<(nSpec*mLen*2); i++) {
        data[i] = 0;
    }
    
    for (Uint4 x=0; x<nSpec; x++) {
        for (Uint4 y=0; y<m_msLevels[msLevel][x][0].size(); y++) {
            Uint4 point = (x*mLen*2)+(y*2);
            data[point] = m_msLevels[msLevel][x][0][y];
            data[point+1] = m_msLevels[msLevel][x][1][y];
        }
    }

    // Array size
    hsize_t dimensions[3];
    dimensions[0] = nSpec;
    dimensions[1] = mLen;
    dimensions[2] = 2;
    DataSpace dataspace(3, dimensions);

    // HDF5 Creation properties
    DSetCreatPropList parameters;
    parameters.setDeflate(6);
    hsize_t chunk_dims[3];
    Uint4 chunk_size = computeNumChunkElements(sizeof(float)*mLen*2, nSpec);
    chunk_dims[0] = chunk_size;
    chunk_dims[1] = mLen;
    chunk_dims[2] = 2;
    parameters.setChunk(3, chunk_dims);

    // Define datatype
    FloatType datatype(PredType::IEEE_F32LE);
    datatype.setOrder(H5T_ORDER_LE);

    // New dataset
    DataSet dataset = h5file->createDataSet(m_path + "/ms" + NStr::IntToString(msLevel) + "peaks", datatype, dataspace, parameters);

    //dataset.write(data, PredType::IEEE_F32LE);
    dataset.write(data, PredType::NATIVE_FLOAT);

    m_msLevels[msLevel].clear();
    delete [] data;
}

void CMsHdf5::writeSpectraInfo(Uint4 msLevel) 
{

    Uint4 nSpec, mLen;
    spectraInfoDimensions(msLevel, nSpec, mLen);

    if (nSpec < 1) return;

    char* data = new char[ nSpec * mLen ];
    for (Uint4 i=0; i<(nSpec*mLen); i++) {
        data[i] = '\0';

    }
    
    for (Uint4 x=0; x<nSpec; x++) {
        Uint4 point = (x*mLen);
        strcpy(&data[point], m_msLevelsInfo[msLevel][x].c_str());
    }

    // Array size
    hsize_t dimensions[2];
    dimensions[0] = nSpec;
    dimensions[1] = mLen;
    DataSpace dataspace(2, dimensions);

    // HDF5 Creation properties
    DSetCreatPropList parameters;
    parameters.setDeflate(6);
    hsize_t chunk_dims[2];
    Uint4 chunk_size = computeNumChunkElements(sizeof(char)*mLen, nSpec);
    //if (chunk_size > nSpec) chunk_size = nSpec;
    chunk_dims[0] = chunk_size;
    chunk_dims[1] = mLen;
    parameters.setChunk(2, chunk_dims);

    // Define datatype
    FloatType datatype(PredType::NATIVE_CHAR);
    datatype.setOrder(H5T_ORDER_LE);

    // New dataset
    DataSet dataset = h5file->createDataSet(m_path + "/ms" + NStr::IntToString(msLevel) + "info", datatype, dataspace, parameters);

    dataset.write(data, PredType::NATIVE_CHAR);

    m_msLevelsInfo[msLevel].clear();
    delete [] data;
}

void CMsHdf5::addMetadata(string data)
{
    size_t size = data.size();

    // Array size
    hsize_t dimensions[] = {size};
    DataSpace dataspace(1, dimensions);

    // HDF5 Creation properties
    DSetCreatPropList parameters;
    parameters.setDeflate(6);
    parameters.setChunk(1, dimensions);
    
    // Define datatype
    StrType datatype(PredType::NATIVE_CHAR);
    datatype.setOrder(H5T_ORDER_LE);

    // New dataset
    DataSet dataset = h5file->createDataSet(m_curSetRoot + "/" + "metadata", datatype, dataspace, parameters);

    dataset.write(data.c_str(), PredType::NATIVE_CHAR);    
}

string CMsHdf5::readMetadata(string spectraSetName)
{
    DataSet dataset = this->openDataSet(spectraSetName, "metadata");
    DataType datatype = dataset.getDataType();
    DataSpace dataspace = dataset.getSpace();

    hsize_t size;        // dataset dimensions
    dataspace.getSimpleExtentDims( &size );
    
    char *buf = new char[size];
    dataset.read((void*)buf, datatype);

    string val(buf, size);
    delete [] buf;
    
    return val;
}

void CMsHdf5::addAttribute(string key, string value)
{
    // Create new dataspace for attribute
    DataSpace dataspace = DataSpace(H5S_SCALAR);
    
    try {
        int iVal = NStr::StringToInt(value);
        IntType datatype(PredType::STD_I32LE);
        datatype.setOrder(H5T_ORDER_LE);
        // Create attribute and write to it
        Attribute attr = m_curGroup->createAttribute(key, datatype, dataspace);
        attr.write(datatype, &iVal);
    } catch (...) {
        try {
            double dVal = NStr::StringToDouble(value);
            IntType datatype(PredType::IEEE_F64LE);
            datatype.setOrder(H5T_ORDER_LE);
            Attribute attr = m_curGroup->createAttribute(key, datatype, dataspace);
            attr.write(datatype, &dVal);
        } catch (...) {
            StrType datatype(PredType::C_S1, value.size()+1);
            datatype.setOrder(H5T_ORDER_LE);
            Attribute attr = m_curGroup->createAttribute(key, datatype, dataspace);
            attr.write(datatype, value.c_str());
        }
    }

}

string CMsHdf5::getCurrentPath() 
{
    string path(m_curSetRoot);

    list<string>::iterator it;
    for (it=m_pathStack.begin(); it!=m_pathStack.end(); it++) {
        path += "/" + *it;
    }
    return path;
}

void CMsHdf5::pushDir(string name, bool isScan)
{
    m_path += "/" + name;

    if (m_curGroup) delete m_curGroup;
    m_curGroup = new Group(h5file->createGroup(m_path));

    if (isScan) {
        m_pathStack.push_back(name);
        if (m_pathStack.size() > 1) {
            h5file->link(H5G_LINK_SOFT, m_path, m_curSetRoot + "/" + name);
        }
    }
}

void CMsHdf5::popDir(bool isScan)
{
    if (isScan) {
        m_pathStack.pop_back();
    }
    m_path = getCurrentPath();    
}

void CMsHdf5::printSpectra()
{
    for (Uint4 w=0; w<m_msLevels.size(); w++) {
        cout << "MS" << w+1 << " spectra - " << m_msLevels[w].size() << endl;
        for (Uint4 x=0; x<m_msLevels[w].size(); x++) {
            cout << "  Spectrum " << x << ", ";
            cout << m_msLevels[w][x].size() << "x" << m_msLevels[w][x][0].size() << endl;
            
            //for (Uint4 y=0; y<m_msLevels[w][x][0].size(); y++) {
            //    cout << "    Mass: " << m_msLevels[w][x][0][y] << "\t";
            //    cout << "Int: " << m_msLevels[w][x][1][y] << endl;
            //}
        }
    }
    cout << endl << endl;
}

void CMsHdf5::getSpectrum(string src, TSpectrum& spectrum, objects::SPC::CScan& scan, string msLevel)
{
    string spec, idxS;
    NStr::SplitInTwo(src, "\t", spec, idxS);
    int idx = NStr::StringToInt(idxS);
    string specSetName, specId;
    NStr::SplitInTwo(src, ":", specSetName, specId);

    // Retrieve the spectrum info
    DataSet dsInfo = this->openDataSet(specSetName, "ms" + msLevel + "info");
    DataType dtInfo = dsInfo.getDataType();
    DataSpace dspaceInfo = dsInfo.getSpace();

    hsize_t sizeInfo[2];        // dataset dimensions
    dspaceInfo.getSimpleExtentDims(sizeInfo);
    
    hsize_t offsetInfo[2];	// hyperslab offset in the file
    hsize_t countInfo[2];	// size of the hyperslab in the file
    offsetInfo[0] = idx;
    offsetInfo[1] = 0;
    countInfo[0]  = 1;
    countInfo[1]  = sizeInfo[1];
    dspaceInfo.selectHyperslab( H5S_SELECT_SET, countInfo, offsetInfo );
    DataSpace mspaceInfo(1, &countInfo[1]);

    char *bufInfo = new char[sizeInfo[1]];
    dsInfo.read((void*)bufInfo, dtInfo, mspaceInfo, dspaceInfo);

    string valInfo(bufInfo, sizeInfo[1]);

    CNcbiIstrstream bufStream(valInfo.c_str());
    bufStream >> MSerial_Xml >> scan;
    delete [] bufInfo;

    // Retrieve the spectrum peaks
    DataSet dsPeaks = this->openDataSet(specSetName, "ms" + msLevel + "peaks");
    DataType dtPeaks = dsPeaks.getDataType();
    DataSpace dspacePeaks = dsPeaks.getSpace();

    hsize_t sizePeaks[3];        // dataset dimensions
    dspacePeaks.getSimpleExtentDims(sizePeaks);
    
    //cout << sizePeaks[0] << "X" << sizePeaks[1] << "X" << sizePeaks[2] << endl;
    Uint4 numPeaks = scan.GetAttlist().GetPeaksCount();
    if (numPeaks > sizePeaks[1]) {
        // should throw exception here
        cerr << "Trying to get more peaks than I should" << endl;
        return;
    }
    
    hsize_t offsetPeaks[3];	// hyperslab offset in the file
    hsize_t countPeaks[3];	// size of the hyperslab in the file
    offsetPeaks[0] = idx;
    offsetPeaks[1] = 0;
    offsetPeaks[2] = 0;
    countPeaks[0]  = 1;
    countPeaks[1]  = numPeaks;
    countPeaks[2]  = 2;
    int bufSize = numPeaks * 2;
    float* bufPeaks = new float[bufSize];

    if (numPeaks > 0) {
        dspacePeaks.selectHyperslab( H5S_SELECT_SET, countPeaks, offsetPeaks );
        DataSpace mspacePeaks(2, &countPeaks[1]);
        dsPeaks.read((void*)bufPeaks, dtPeaks, mspacePeaks, dspacePeaks);
    }
    
    spectrum.clear();
    vector<float> mz, it;
    mz.reserve(numPeaks);
    it.reserve(numPeaks);
    for (int i=0; i<bufSize; i+=2) {
        mz.push_back(bufPeaks[i]);
        it.push_back(bufPeaks[i+1]);
    }
    spectrum.push_back(mz);
    spectrum.push_back(it);
    delete [] bufPeaks;
}

void CMsHdf5::getSpectraMap(const string &spectraSetName, TSpecMap &specMap)
{
    // Retrieve the spectrum info
    DataSet dset = this->openDataSet(spectraSetName, "map");
    DataType dtype = dset.getDataType();
    DataSpace dspace = dset.getSpace();
    
    hsize_t size[2];        // dataset dimensions
    dspace.getSimpleExtentDims(size);
    
    int bufSize = size[0] * size[1];
    Uint4 *buf = new Uint4[bufSize];
    dset.read((void*)buf, dtype);
    
    for (Uint4 x=0; x<size[0]; x++) {
        Uint4 point = (x*4);
        MSMapEntry mapEntry;
        mapEntry.scan = buf[point];
        mapEntry.msLevel = buf[point+1];
        mapEntry.idx = buf[point+2];
        mapEntry.parentScan = buf[point+3];
        specMap[mapEntry.scan] = mapEntry;
    }

    delete [] buf;
}

void CMsHdf5::getPrecursorMzs(const string &spectraSetName, TSpecMap &specMap, TSpecPrecursorMap &specPreMap)
{
    // Retrieve the spectrum info
    DataSet dsInfo = this->openDataSet(spectraSetName, "ms2info");
    DataType dtInfo = dsInfo.getDataType();
    DataSpace dspaceInfo = dsInfo.getSpace();

    hsize_t sizeInfo[2];        // dataset dimensions
    dspaceInfo.getSimpleExtentDims(sizeInfo);
    
    hsize_t offsetInfo[2];	// hyperslab offset in the file
    hsize_t countInfo[2];	// size of the hyperslab in the file
    //offsetInfo[0] = idx;
    offsetInfo[1] = 0;
    countInfo[0]  = 1;
    countInfo[1]  = sizeInfo[1];
    DataSpace mspaceInfo(1, &countInfo[1]);

    char *bufInfo = new char[sizeInfo[1]];
    
    // use reg expression to decode
    // sample: ">463.9961243</precursorMz>
    CRegexp RxpParse(".*>.*?(\\d+\\.?\\d*).*</precursorMz>.*", CRegexp::fCompile_ignore_case | CRegexp::fCompile_dotall);
    
    ITERATE(TSpecMap, iMapEntry, specMap) {
        if ((*iMapEntry).second.msLevel == 2) {
            Uint4 scanNum, idx;
            scanNum = (*iMapEntry).second.scan;
            idx  = (*iMapEntry).second.idx;
            offsetInfo[0] = idx;
            if (idx >= sizeInfo[0]) {
                throw OutOfBounds("Group: " + spectraSetName + 
                                  ", rows: " + NStr::IntToString(sizeInfo[0]) + 
                                  ", index: " + NStr::IntToString(idx));
            }
            dspaceInfo.selectHyperslab( H5S_SELECT_SET, countInfo, offsetInfo );
            dsInfo.read((void*)bufInfo, dtInfo, mspaceInfo, dspaceInfo);
            //cout << bufInfo << endl;
            string valInfo(bufInfo, sizeInfo[1]);

            // use serial object to decode, broken?
            //CNcbiIstrstream bufStream(valInfo.c_str());
            //objects::CScan scan;
            //bufStream >> MSerial_Xml >> scan;
            //double preMz = scan.GetPrecursorMz().front()->GetPrecursorMz();
            //specPreMap[scanNum] = preMz;

            // use reg expression to decode
            string value = RxpParse.GetMatch(valInfo, 0, 1);
            specPreMap[scanNum] = NStr::StringToDouble(value);
        }
    }

    delete [] bufInfo;
}

herr_t CMsHdf5::get_groups_helper(hid_t loc_id, const char *name, void *opdata)
{    
    iter_info *info=(iter_info *)opdata;
    int obj_type = H5Gget_objtype_by_idx(loc_id, info->index);

    if(obj_type == H5G_GROUP) {
        info->groups->insert(name);
    }

    (info->index)++;
    return 0;
}

void CMsHdf5::getGroups(set<string> &groups)
{
    iter_info info;
    int idx = 0;
    info.index = 0;
    info.groups= &groups;
    idx = h5file->iterateElems("/", NULL, CMsHdf5::get_groups_helper, &info);
}

const set<string>& CMsHdf5::getGroups()
{
    if (m_groups.size() < 1) {
        this->getGroups(m_groups);
    }
    return m_groups;
}

bool CMsHdf5::hasGroup(const string &groupName)
{
    return getGroups().count(groupName);
}

void CMsHdf5::encodePeaks(const TSpectrum& spectrum, string &b64Str) 
{
    size_t num_peaks = spectrum[0].size();
    Uint4 elements;
    if (num_peaks > 0) elements = num_peaks*2;
    else elements = 2;
            
    Uint4 *data = new Uint4[elements];
    Uint4 dSize = sizeof(Uint4)*elements;
    data[0] = 0;
    data[1] = 0;
    for (size_t i=0; i<num_peaks; i++) {
        data[i*2] = SOCK_HostToNetLong(*reinterpret_cast<const Uint4 *>(&(spectrum[0][i])));
        data[(i*2)+1] = SOCK_HostToNetLong(*reinterpret_cast<const Uint4 *>(&(spectrum[1][i])));
    }
    //size_t oSize = dSize + dSize/2;
    size_t oSize = dSize * 2;
    char *outbuf = new char[oSize];
    size_t dRead, dWritten, linesz;
    linesz = 0;
    BASE64_Encode(data, dSize, &dRead, outbuf, oSize, &dWritten, &linesz);
    if ((dSize != dRead) || (oSize < dWritten) || (dSize < 1)) {
        cerr << "Read: " << dRead << "\t" << "Written: " << dWritten << endl;
    }  

    b64Str.assign(outbuf);
    
    delete [] outbuf;
    delete [] data;
}

H5File* CMsHdf5::raw()
{
    return h5file;
}

void CMsHdf5::spectraDimensions(const Uint4 msLevel, Uint4 &nSpectra, Uint4 &maxLen)
{
    maxLen = 0;
    nSpectra = m_msLevels[msLevel].size();
    for (Uint4 x=0; x<nSpectra; x++) {
        Uint4 y = m_msLevels[msLevel][x][0].size();
        if (y > maxLen) maxLen = y;
    }
}

void CMsHdf5::spectraInfoDimensions(const Uint4 msLevel, Uint4 &nSpectra, Uint4 &maxLen)
{
    maxLen = 0;
    nSpectra = m_msLevelsInfo[msLevel].size();
    for (Uint4 x=0; x<nSpectra; x++) {
        Uint4 y = m_msLevelsInfo[msLevel][x].size();
        if (y > maxLen) maxLen = y;
    }
    // make room for the null terminator
    maxLen++; 
}


Uint4 CMsHdf5::computeNumChunkElements(size_t elementSize, Uint4 maxElements)
{
    Uint4 chunk_size = 128 * 1024 * sizeof(char);
    Uint4 elements;
    if (elementSize > chunk_size) {
        elements = 1;
    } else {
        elements = chunk_size / elementSize;
    }
    return (elements > maxElements) ? maxElements : elements;
}

H5::DataSet CMsHdf5::openDataSet(string spectraSetName, string dataSetName)
{
    if (! this->hasGroup(spectraSetName)) {
        throw UnknownGroup("The group: " + spectraSetName + ", does not exist in file: " + m_filename);
    }
    return h5file->openDataSet("/" + spectraSetName + "/" + dataSetName);
}

double CMsHdf5::mzToAvgMass(double mz, int charge) 
{
    return ((mz * charge) - (charge * AVG_HYD));
}

double CMsHdf5::mzToMonoMass(double mz, int charge) 
{
    return ((mz * charge) - (charge * MONO_HYD));
}

END_NCBI_SCOPE
