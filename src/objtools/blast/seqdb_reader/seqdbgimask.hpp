#ifndef OBJTOOLS_READERS_SEQDB__SEQDBGIMASK_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBGIMASK_HPP

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
 * Author:  Ning Ma
 *
 */

/// @file seqdbgimask.hpp
/// Defines gi-based mask data files
///
/// Defines classes:
///     CSeqDBGiMask
///
/// Implemented for: UNIX, MS-Windows

#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbatlas.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbfile.hpp>

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )

/// CSeqDBGiMask class.
/// 
/// This code supports Gi-based database masks

class CSeqDBGiMask : public CObject {
public:
    /// Constructor.
    /// 
    /// @param atlas
    ///   The atlas layer managing memory lease [in]
    /// @param mask_name
    ///   The names of the mask files. [in]
    CSeqDBGiMask(CSeqDBAtlas           & atlas,
                 const vector <string> & mask_name);
    
    /// Destructor.
    ~CSeqDBGiMask() {
        m_IndexLease.Clear();
        m_OffsetLease.Clear();
        for (unsigned int i=0; i<m_DataFile.size(); i++) {
            m_DataLease[i]->Clear();
            delete m_DataFile[i];
            delete m_DataLease[i];
        }
    };
    
    /// Get the mask description for algo id
    /// @param algo_id The chosen algo id [in]
    /// @param locked
    ///   The lock holder object for this thread (or NULL). [in]
    /// @return The description of the masking algo.
    const string & GetDesc(int algo_id, CSeqDBLockHold & locked);

    /// Get the mask data for GI
    /// @param algo_id The chosen algo id [in]
    /// @param gi The chosen gi [in]
    /// @param ranges The masks for sequence with gi [out]
    /// @param locked
    ///   The lock holder object for this thread (or NULL). [in]
    void GetMaskData(int                     algo_id,
                     int                     gi,
                     CSeqDB::TSequenceRanges &ranges,
                     CSeqDBLockHold          &locked);

    /// Get the available mask algorithsm ids
    /// @param algo The avaiable algo ids [out]
    void GetAvailableMaskAlgorithms(vector <int> & algo) const {
        algo.clear();
        for (unsigned int i=0; i<m_MaskNames.size(); ++i) {
            algo.push_back(i);
        }
        return;
    }

    /// Get the mask algorithsm id for a string id
    /// @param algo_name The algorithm string [in]
    /// @return the algorithm id corresponding to the string
    int GetAlgorithmId(const string & algo_name) const {
        for (unsigned int i=0; i<m_MaskNames.size(); ++i) {
            if (m_MaskNames[i] == algo_name) return i;
        }
        CNcbiOstrstream oss;
        oss << "Filtering algorithm " << algo_name
            << " does not exist." << endl;
        oss << GetAvailableAlgorithmNames();
        NCBI_THROW(CSeqDBException, eArgErr,
                       CNcbiOstrstreamToString(oss));
    }

    /// Get the mask algorithsm name for a numeric id
    /// @param algo_id The algorithm id [in]
    /// @return the algorithm name 
    const string & GetAlgorithmName(int algo_id) const {
        x_VerifyAlgorithmId(algo_id);
        return m_MaskNames[algo_id];
    }
   
    /// Get the names of available mask algorithms as string
    string GetAvailableAlgorithmNames() const {
        CNcbiOstrstream retval;
        retval << endl
               << "Available filtering algorithm(s):"
               << endl << endl;
        retval << setw(14) << left << "Algorithm ID"
               << setw(40) << left << "Algorithm name" << endl;
        for (unsigned int id=0; id < m_MaskNames.size(); ++id) {
            retval << "    " << setw(10) << left << id
                   << setw(40) << left << m_MaskNames[id] << endl;
        }
        return CNcbiOstrstreamToString(retval);
    } 

private:
    /// Sgring format used by gi mask files
    static const CBlastDbBlob::EStringFormat
        kStringFmt = CBlastDbBlob::eSizeVar;

    /// File offset type.
    typedef CSeqDBAtlas::TIndx TIndx;
    
    /// Prevent copy construction.
    CSeqDBGiMask(const CSeqDBGiMask&);
    
    /// Prevent copy assignment.
    CSeqDBGiMask& operator=(CSeqDBGiMask&);
    
    /// Open file for a chosen algo_id
    /// @param algo_id The chosen algo_id [in]
    /// @param locked The lock holder object for this thread. [in]
    void x_Open(Int4 algo_id, CSeqDBLockHold & locked);

    /// Open files and read field data from the atlas.
    /// @param locked The lock holder object for this thread. [in]
    void x_ReadFields(CSeqDBLockHold & locked);

    /// Verify the algorithm exists.  If not, raise an exception
    void x_VerifyAlgorithmId(int algo_id) const {
        if (algo_id < 0 || algo_id >= (int)m_MaskNames.size()) {
            CNcbiOstrstream oss;
            oss << "Filtering algorithm ID " << algo_id
                << " does not exist." << endl;
            oss << GetAvailableAlgorithmNames();
            NCBI_THROW(CSeqDBException, eArgErr,
                       CNcbiOstrstreamToString(oss));
        }
    }
    
    /// Get a range of the index or data file.
    ///
    /// A range of file is acquired and returned in the provided blob.
    ///
    /// @param begin The start offset for this range of data. [in]
    /// @param end The end (post) offset for this range of data. [in]
    /// @param select_file Whether to use the index or data file. [in]
    /// @param lifetime Should the blob maintain the memory mapping? [in]
    /// @param blob The data will be returned here. [out]
    /// @param locked The lock holder object for this thread. [in]
    static void s_GetFileRange(TIndx            begin,
                               TIndx            end,
                               CSeqDBRawFile  & file,
                               CSeqDBMemLease & lease,
                               CBlastDbBlob   & blob,
                               CSeqDBLockHold & locked);
    
    /// Binary search for value associated with a key
    ///
    /// @param keys  The (sorted) key array [in]
    /// @param n  Number of keys [in]
    /// @param key The key to search for [in]
    /// @param idx The index to the key array where key is found. [out]
    /// @return TRUE if the key is found 
    static bool s_BinarySearch(const int *keys,
                               const int  n,
                               const int  key,
                               int       &idx);

    /// Reference to the atlas.
    CSeqDBAtlas & m_Atlas;
    
    /// The set of gi masks found in alias description
    const vector<string> m_MaskNames;

    /// The current used mask id
    Int4 m_AlgoId;
    
    /// Index file.
    CSeqDBRawFile m_IndexFile;

    /// Index file lease.
    CSeqDBMemLease m_IndexLease;

    /// Offset file.
    CSeqDBRawFile m_OffsetFile;

    /// Offset file lease.
    CSeqDBMemLease m_OffsetLease;

    /// Number of data volumes
    Int4 m_NumVols;
    
    /// Data file.
    vector<CSeqDBRawFile *> m_DataFile;
    
    /// Data file lease.
    vector<CSeqDBMemLease *> m_DataLease;
    
    /// GI size
    Int4 m_GiSize;

    /// Offset size
    Int4 m_OffsetSize;

    /// Page size
    Int4 m_PageSize;
    
    /// Number of Gi indices
    Int4 m_NumIndex;

    /// Number of Gis
    Int4 m_NumGi;
    
    /// Mapped Gi index
    const Int4 *m_GiIndex;    

    /// Start offset (in the index file) of the offset array.
    Int4 m_IndexStart;
    
    /// The description about the masking algo
    string m_Desc;
    
    /// The create date of the GI mask
    string m_Date;
    
};

#endif

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBCOL_HPP
