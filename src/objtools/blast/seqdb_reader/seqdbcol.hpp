#ifndef OBJTOOLS_READERS_SEQDB__SEQDBCOL_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBCOL_HPP

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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdbcol.hpp
/// Defines database column access classes.
///
/// Defines classes:
///     CSeqDBColumn
///
/// Implemented for: UNIX, MS-Windows

#include "seqdbatlas.hpp"
#include "seqdbfile.hpp"
#include <objects/seq/seq__.hpp>

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )

/// CSeqDBColumnFlush class
/// 
/// This object provides a call back to return memory holds to the
/// atlas from lease objects stored under CSeqDBColumn.

class CSeqDBColumnFlush : public CSeqDBFlushCB {
public:
    /// Constructor.
    CSeqDBColumnFlush()
        : m_Column(0)
    {
    }
    
    /// Destructor.
    virtual ~CSeqDBColumnFlush()
    {
    }
    
    /// Specify the implementation layer object.
    /// 
    /// This method sets the SeqDB implementation layer object
    /// pointer.  Until this pointer is set, this object will ignore
    /// attempts to flush unused data.  This pointer should not bet
    /// set until object construction is complete enough to permit the
    /// memory lease flushing to happen safely.
    /// 
    /// @param col A pointer to the column object.
    void SetColumn(class CSeqDBColumn * col)
    {
        m_Column = col;
    }
    
    /// Flush any held memory leases.
    ///
    /// At the beginning of garbage collection, this method is called
    /// to tell the column to release any held memory leases.  If the
    /// SetColumn() method has not been called, this method will do
    /// nothing.  This method assumes the atlas lock is held.
    virtual void operator()();
    
private:
    /// A pointer to the column object.
    class CSeqDBColumn * m_Column;
};


/// CSeqDBColumn class.
/// 
/// This code supports arbitrary user-defined data columns.  These can
/// be produced as part of a database volume, and accessed via SeqDB,
/// or can be independent entities, not associated with any database.

class CSeqDBColumn : public CObject {
public:
    /// Constructor.
    /// 
    /// The constructor verifies the existence and some of the data of
    /// the files making up this database column.  Since column files
    /// may be external to a database volume, this objects manages its
    /// own CSeqDBAtlasHolder object and `flush' callback.  For the
    /// same reason, the lock holder is optional here (an internal one
    /// is used if one is not provided.)
    /// 
    /// @param basename
    ///   The base name of the volume. [in]
    /// @param index_extn
    ///   The file extension for the index file. [in]
    /// @param data_extn
    ///   The file extension for the data file. [in]
    /// @param lockedp
    ///   The lock holder object for this thread (or NULL). [in]
    CSeqDBColumn(const string   & basename,
                 const string   & index_extn,
                 const string   & data_extn,
                 CSeqDBLockHold * lockedp);
    
    /// Destructor.
    ~CSeqDBColumn();
    
    /// Determine if the column exists.
    /// 
    /// This method tests whether a column with the given name and
    /// file extensions exists.  An alternative to calling this method
    /// is to try to construct the column and catch an exception if
    /// the construction attempt fails.
    /// 
    /// @param basename
    ///   The base name of the volume. [in]
    /// @param index_extn
    ///   The file extension for the index file. [in]
    /// @param data_extn
    ///   The file extension for the data file. [in]
    /// @param atlas
    ///   A reference to the memory management layer. [in]
    /// @param lockedp
    ///   The lock holder object for this thread. [in]
    static bool ColumnExists(const string   & basename,
                             const string   & index_extn,
                             const string   & data_extn,
                             CSeqDBAtlas    & atlas,
                             CSeqDBLockHold & locked);
    
    /// Get the column title.
    /// @return The column title.
    const string & GetTitle() const;
    
    /// Get the column's Key/Value meta data.
    /// @return All key/value meta data stored in this column.
    const map<string,string> & GetMetaData();
    
    /// Get the number of OIDs stored here.
    /// @return The number of OIDs stored here.
    int GetNumOIDs() const;
    
    /// Fetch the data blob for the given oid.
    ///
    /// This version fetches the data for the given blob, optionally
    /// incrementing the memory region so that it will not be garbage
    /// collected until the blob in question refers to another memory
    /// region.  If `keep' is true, the blob will contain an object
    /// designed to maintain the memory mapping until the next time
    /// the blob data is assigned or modified (which must be done when
    /// this thread does not hold the Atlas lock).  Otherwise the
    /// memory mapping will only be guaranteed held until the lock is
    /// released or the atlas is asked to provide another memory
    /// region.
    ///
    /// @param oid     The OID of the blob. [in]
    /// @param blob    The data will be returned here. [out]
    /// @param keep    If true, increment the memory region. [in]
    /// @param lockedp The lock holder object for this thread. [in]
    void GetBlob(int              oid,
                 CBlastDbBlob   & blob,
                 bool             keep,
                 CSeqDBLockHold * lockedp);
    
    /// Flush any held memory.
    void Flush();
    
private:
    /// String format used by column files.
    static const CBlastDbBlob::EStringFormat
        kStringFmt = CBlastDbBlob::eSizeVar;
    
    /// File offset type.
    typedef CSeqDBAtlas::TIndx TIndx;
    
    /// Prevent copy construction.
    CSeqDBColumn(const CSeqDBColumn&);
    
    /// Prevent copy assignment.
    CSeqDBColumn& operator=(CSeqDBColumn&);
    
    /// Open files and read field data from the atlas.
    /// @param locked The lock holder object for this thread. [in]
    void x_ReadFields(CSeqDBLockHold & locked);
    
    /// Open files and read field data from the atlas.
    /// @param locked The lock holder object for this thread. [in]
    void x_ReadMetaData(CSeqDBLockHold & locked);
    
    /// Which file to access.
    enum ESelectFile {
        e_Index = 101, ///< Use index file.
        e_Data = 102   ///< Use data file.
    };
    
    /// Get a range of the index or data file.
    ///
    /// A range of the index or data file is acquired and returned in
    /// the provided blob.
    ///
    /// @param begin The start offset for this range of data. [in]
    /// @param end The end (post) offset for this range of data. [in]
    /// @param select_file Whether to use the index or data file. [in]
    /// @param lifetime Should the blob maintain the memory mapping? [in]
    /// @param blob The data will be returned here. [out]
    /// @param locked The lock holder object for this thread. [in]
    void x_GetFileRange(TIndx            begin,
                        TIndx            end,
                        ESelectFile      select_file,
                        bool             lifetime,
                        CBlastDbBlob   & blob,
                        CSeqDBLockHold & locked);
    
    //
    // Data
    //
    
    /// This callback functor allows the atlas code to flush any
    /// cached region holds prior to garbage collection.
    CSeqDBColumnFlush m_FlushCB;
    
    /// Insures that a copy of the atlas exists.
    CSeqDBAtlasHolder m_AtlasHolder;
    
    /// Reference to the atlas.
    CSeqDBAtlas & m_Atlas;
    
    /// Index file.
    CSeqDBRawFile m_IndexFile;
    
    /// Index file lease.
    CSeqDBMemLease m_IndexLease;
    
    /// Data file.
    CSeqDBRawFile m_DataFile;
    
    /// Data file lease.
    CSeqDBMemLease m_DataLease;
    
    /// Number of OIDs (Blobs) in this column.
    Int4 m_NumOIDs;
    
    /// Total length of data stored in the data file.
    Int8 m_DataLength;
    
    /// Start offset (in the index file) of the metadata section.
    Int4 m_MetaDataStart;
    
    /// Start offset (in the index file) of the offset array.
    Int4 m_OffsetArrayStart;
    
    /// The title identifies this column's purpose.
    string m_Title;
    
    /// The create date of the column files.
    string m_Date;
    
    /// All key/value metadata for this column.
    map<string,string> m_MetaData;
};


/// Database-wide column information.
///
/// Users of a SeqDB database treat the column title and column ID as
/// corresponding to one database column spanning the entire OID range
/// of the database.  This class holds data used by CSeqDBImpl to map
/// the functionality of all columns with the same title from all
/// database volumes into one conceptual database-wide column.

class CSeqDB_ColumnEntry : public CObject {
public:
    /// Constructor.
    /// @param indices The indices of this column in each volume.
    CSeqDB_ColumnEntry(const vector<int> & indices);
    
    /// Get a volume-specific column ID.
    /// @param volnum The index of the volume.
    /// @return The column ID for this column entry's column.
    int GetVolumeIndex(int volnum)
    {
        _ASSERT(volnum < (int)m_VolIndices.size());
        return m_VolIndices[volnum];
    }
    
    /// Determine if we have the metadata map yet.
    /// @return true If the metadata map is computed yet.
    bool HaveMap()
    {
        return m_HaveMap;
    }
    
    /// Indicate that the metadata map is now complete.
    void SetHaveMap()
    {
        _ASSERT(! m_HaveMap);
        m_HaveMap = true;
    }
    
    /// Get the metadata map.
    ///
    /// This method returns the database-wide metadata map for this
    /// column, which is a potentially lossy combination of the maps
    /// for all of the per-volume columns with this title.
    ///
    /// @return The combined metadata map for this column.
    const map<string,string> & GetMap()
    {
        _ASSERT(m_HaveMap);
        return m_Map;
    }
    
    /// Add a meta-data key/value association.
    /// 
    /// Where volumes disagree on the value of a given metadata key,
    /// the policy is to use the first value we find for each key.
    /// 
    /// @param k The key to look up. [in]
    /// @param v The value to read for this key. [in]
    void SetMapValue(const string & k, const string & v);
    
private:
    /// The indices of columns with this title in each volume.
    vector<int> m_VolIndices;
    
    /// True if the metadata map is stored.
    bool m_HaveMap;
    
    /// The combined metadata map for this column.
    map<string,string> m_Map;
};

#endif

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBCOL_HPP


