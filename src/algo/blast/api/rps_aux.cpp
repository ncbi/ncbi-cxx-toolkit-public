#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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
 * Author:  Christiam Camacho
 *
 */

/** @file rps_aux.cpp
 * Implements auxiliary classes to manage RPS-BLAST related C-structures
 */


#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>
#include <algo/blast/api/rps_aux.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/////////////////////////////////////////////////////////////////////////////
//
// CBlastRPSAuxInfo
//
/////////////////////////////////////////////////////////////////////////////

/// Wrapper class to manage the BlastRPSAuxInfo structure, as currently
/// there aren't any allocation or deallocation functions for this structure in
/// the CORE of BLAST. This class is meant to be kept in a CRef<>. Note that
/// only the fields currently used are being passed to the constructor
class CBlastRPSAuxInfo : public CObject {
public:
    /// Parametrized constructor
    /// @param matrix name of the scoring matrix used to build the RPS-BLAST
    /// database
    /// @param gap_open gap opening cost
    /// @param gap_exten gap extension cost
    /// @param scale_factor scaling factor
    /// @param karlin_k statistical K parameter calculated when building the
    /// RPS-BLAST database
    CBlastRPSAuxInfo(const string& matrix,
                     int gap_open,
                     int gap_extend,
                     double scale_factor,
                     const vector<double>& karlin_k);

    /// Destructor 
    ~CBlastRPSAuxInfo();

    /// Lend the caller the pointer to the data structure this object manages.
    /// Caller MUST NOT deallocate the return value.
    const BlastRPSAuxInfo* operator()() const; 
private:
    /// Prohibit copy-constructor
    CBlastRPSAuxInfo(const CBlastRPSAuxInfo& rhs);
    /// Prohibit assignment operator
    CBlastRPSAuxInfo& operator=(const CBlastRPSAuxInfo& rhs);

    /// Deallocates the structure owned by this class
    void x_DoDestroy();

    /// The data structure this class manages
    BlastRPSAuxInfo* m_Data;
};

CBlastRPSAuxInfo::CBlastRPSAuxInfo(const string& matrix,
                                   int gap_open, 
                                   int gap_extend,
                                   double scale_factor,
                                   const vector<double>& karlin_k)
    : m_Data(0)
{
    ASSERT(!matrix.empty());
    ASSERT(!karlin_k.empty());

    try {
        m_Data = new BlastRPSAuxInfo;
        memset(m_Data, 0, sizeof(BlastRPSAuxInfo));
        m_Data->orig_score_matrix = strdup(matrix.c_str());
        m_Data->gap_open_penalty = gap_open;
        m_Data->gap_extend_penalty = gap_extend;
        m_Data->scale_factor = scale_factor;
        m_Data->karlin_k = new double[karlin_k.size()];
        copy(karlin_k.begin(), karlin_k.end(), &m_Data->karlin_k[0]);
    } catch (const bad_alloc&) {
        x_DoDestroy();
        NCBI_THROW(CBlastSystemException, eOutOfMemory, 
               "Failed to allocate memory for BlastRPSAuxInfo structure");
    }
}

CBlastRPSAuxInfo::~CBlastRPSAuxInfo()
{
    x_DoDestroy();
}

const BlastRPSAuxInfo*
CBlastRPSAuxInfo::operator()() const
{
    return m_Data;
}

void
CBlastRPSAuxInfo::x_DoDestroy()
{
    if ( !m_Data ) {
        return;
    }
    if (m_Data->orig_score_matrix) {
        sfree(m_Data->orig_score_matrix);
    }
    if (m_Data->karlin_k) {
        delete [] m_Data->karlin_k;
        m_Data->karlin_k = NULL;
    }
    delete m_Data;
    m_Data = NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
// CRpsAuxFile
//
/////////////////////////////////////////////////////////////////////////////

/// This class represents the .aux file in a RPS-BLAST file, which contains
/// information about the scoring matrix to be used during the RPS-BLAST
/// search, the scaling factor, an array of K statistical values (karlin_k), 
/// as well as various fields that are currently unused.
class CRpsAuxFile : public CObject {
public:
    /// Extension associated with the RPS-BLAST database auxiliary file
    static const string kExtension;

    /// Parametrized constructor
    /// @param filename_no_extn name of the file without extension
    CRpsAuxFile(const string& filename_no_extn);

    /// Lend the caller the pointer to the data structure this object manages.
    /// Caller MUST NOT deallocate the return value.
    const BlastRPSAuxInfo* operator()() const; 

private:
    /// Auxiliary method to read the contents of the file into m_Data
    CRef<CBlastRPSAuxInfo> x_ReadFromFile(CNcbiIfstream& input);
    CRef<CBlastRPSAuxInfo> m_Data;
};

const string CRpsAuxFile::kExtension(".aux");

CRpsAuxFile::CRpsAuxFile(const string& filename_no_extn)
{
    // Open the file
    const string file2open(filename_no_extn + kExtension);
    CNcbiIfstream auxfile(file2open.c_str());
    if (auxfile.bad() || auxfile.fail()) {
        NCBI_THROW(CBlastException, eRpsInit, 
                   "Cannot open RPS-BLAST auxiliary file: " + file2open);
    }
    // Read the file
    m_Data = x_ReadFromFile(auxfile);
    auxfile.close();
}

CRef<CBlastRPSAuxInfo>
CRpsAuxFile::x_ReadFromFile(CNcbiIfstream& input)
{
    // Declare data to read
    string matrix;
    int gap_open;
    int gap_extend;
    double scale_factor;
    vector<double> karlinK;
    double ignore_me_d;
    int ignore_me_i;

    // Read the data
    input >> matrix;
    input >> gap_open;
    input >> gap_extend;
    input >> ignore_me_d; // corresponds to BlastRPSAuxInfo::ungapped_k
    input >> ignore_me_d; // corresponds to BlastRPSAuxInfo::ungapped_h
    input >> ignore_me_i; // corresponds to BlastRPSAuxInfo::max_db_seq_length
    input >> ignore_me_i; // corresponds to BlastRPSAuxInfo::db_length
    input >> scale_factor;
    while (input) {
        int seq_size;   // unused value
        double k;
        input >> seq_size;
        input >> k;
        karlinK.push_back(k);
    }
    
    CRef<CBlastRPSAuxInfo> retval(new CBlastRPSAuxInfo(matrix, 
                                                       gap_open,
                                                       gap_extend,
                                                       scale_factor,
                                                       karlinK));
    return retval;
}

const BlastRPSAuxInfo*
CRpsAuxFile::operator()() const
{
    return (*m_Data)();
}

/////////////////////////////////////////////////////////////////////////////
//
// CRpsMmappedFile
//
/////////////////////////////////////////////////////////////////////////////

/// Encapsulates logic of mmap'ing and performing sanity checks on RPS-BLAST 
/// database files
class CRpsMmappedFile : public CObject {
public:
    /// Parametrized constructor
    /// @param filename name of the file to mmap
    CRpsMmappedFile(const string& filename);
protected:
    /// The data structure this class manages
    auto_ptr<CMemoryFile> m_MmappedFile;
};

CRpsMmappedFile::CRpsMmappedFile(const string& filename)
{
    try { m_MmappedFile.reset(new CMemoryFile(filename)); }
    catch (const CFileException& e) {
        NCBI_RETHROW(e, CBlastException, eRpsInit,
             "Cannot memory map RPS-BLAST database file: " + filename);
    }
}

/////////////////////////////////////////////////////////////////////////////
//
// CRpsLookupTblFile
//
/////////////////////////////////////////////////////////////////////////////

/// This class represents the .loo file in a RPS-BLAST file, which contains the
/// pre-computed lookup table
class CRpsLookupTblFile : public CRpsMmappedFile {
public:
    /// Extension associated with the RPS-BLAST database lookup table file
    static const string kExtension;

    /// Parametrized constructor
    /// @param filename_no_extn name of the file without extension
    CRpsLookupTblFile(const string& filename_no_extn);

    /// Lend the caller the pointer to the data structure this object manages.
    /// Caller MUST NOT deallocate the return value.
    const BlastRPSLookupFileHeader* operator()() const;
private:
    /// The data structure this class manages
    BlastRPSLookupFileHeader* m_Data;
};

const string CRpsLookupTblFile::kExtension(".loo");

CRpsLookupTblFile::CRpsLookupTblFile(const string& filename_no_extn)
    : CRpsMmappedFile(filename_no_extn + kExtension)
{
    m_Data = (BlastRPSLookupFileHeader*) m_MmappedFile->GetPtr();
    if (m_Data->magic_number != RPS_MAGIC_NUM) {
        NCBI_THROW(CBlastException, eRpsInit,
               "RPS BLAST profile file (" + filename_no_extn + kExtension + 
               ") is either corrupt or constructed for an incompatible "
               "architecture");
        m_Data = NULL;
    }
}

const BlastRPSLookupFileHeader*
CRpsLookupTblFile::operator()() const
{
    return m_Data;
}

/////////////////////////////////////////////////////////////////////////////
//
// CRpsPssmFile
//
/////////////////////////////////////////////////////////////////////////////

/// This class represents the .rps file in a RPS-BLAST file, which contains the
/// PSSMs for the database
class CRpsPssmFile : public CRpsMmappedFile {
public:
    /// Extension associated with the RPS-BLAST database PSSM file
    static const string kExtension;

    /// Parametrized constructor
    /// @param filename_no_extn name of the file without extension
    CRpsPssmFile(const string& filename_no_extn);

    /// Lend the caller the pointer to the data structure this object manages.
    /// Caller MUST NOT deallocate the return value.
    const BlastRPSProfileHeader* operator()() const;
private:
    /// The data structure this class manages
    BlastRPSProfileHeader* m_Data;
};

const string CRpsPssmFile::kExtension(".rps");

CRpsPssmFile::CRpsPssmFile(const string& filename_no_extn)
    : CRpsMmappedFile(filename_no_extn + kExtension)
{
    m_Data = (BlastRPSProfileHeader*) m_MmappedFile->GetPtr();
    if (m_Data->magic_number != RPS_MAGIC_NUM) {
        NCBI_THROW(CBlastException, eRpsInit,
               "RPS BLAST profile file (" + filename_no_extn + kExtension + 
               ") is either corrupt or constructed for an incompatible "
               "architecture");
        m_Data = NULL;
    }
}

const BlastRPSProfileHeader*
CRpsPssmFile::operator()() const
{
    return m_Data;
}

CBlastRPSInfo::CBlastRPSInfo(const string& rps_dbname)
    : m_RpsInfo(0)
{
    auto_ptr<BlastRPSInfo> rps_info;

    // Allocate the core data structure
    try { rps_info.reset(new BlastRPSInfo); }
    catch (const bad_alloc&) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory,
                   "RPSInfo allocation failed");
    }

    // Obtain the full path to the database
    string path;
    try {
        vector<string> dbpath;
        CSeqDB::FindVolumePaths(rps_dbname, CSeqDB::eProtein, dbpath);
        path = *dbpath.begin();
    } catch (const CSeqDBException& e) {
        NCBI_RETHROW(e, CBlastException, eRpsInit,
                     "Cannot retrieve path to RPS database");
    }
    ASSERT(!path.empty());

    // Load the various files
    m_AuxFile.Reset(new CRpsAuxFile(path));
    m_LutFile.Reset(new CRpsLookupTblFile(path));
    m_PssmFile.Reset(new CRpsPssmFile(path));

    // Assign the pointers to the core data structure
    m_RpsInfo = rps_info.release();
    // Note that these const_casts are only needed because the data structure
    // doesn't take const pointers, but these won't be modified at all
    m_RpsInfo->lookup_header = 
        const_cast<BlastRPSLookupFileHeader*>((*m_LutFile)());
    m_RpsInfo->profile_header = 
        const_cast<BlastRPSProfileHeader*>((*m_PssmFile)());
    m_RpsInfo->aux_info = 
        *const_cast<BlastRPSAuxInfo*>((*m_AuxFile)());
}

CBlastRPSInfo::~CBlastRPSInfo()
{
    if ( !m_RpsInfo ) {
        return;
    }
    delete m_RpsInfo;
}

const BlastRPSInfo*
CBlastRPSInfo::operator()() const 
{
    return m_RpsInfo;
}

double
CBlastRPSInfo::GetScalingFactor() const
{
    return m_RpsInfo->aux_info.scale_factor;
}

const char*
CBlastRPSInfo::GetMatrixName() const
{
    return m_RpsInfo->aux_info.orig_score_matrix;
}

int
CBlastRPSInfo::GetGapOpeningCost() const
{
    return m_RpsInfo->aux_info.gap_open_penalty;
}

int
CBlastRPSInfo::GetGapExtensionCost() const
{
    return m_RpsInfo->aux_info.gap_extend_penalty;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
