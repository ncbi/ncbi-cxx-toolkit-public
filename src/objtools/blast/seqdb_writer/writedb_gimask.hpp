#ifndef OBJTOOLS_WRITERS_WRITEDB__WRITEDB_GIMASK_HPP
#define OBJTOOLS_WRITERS_WRITEDB__WRITEDB_GIMASK_HPP

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

/// @file writedb_gimask.hpp
/// Code for gi-based database mask file construction.
///
/// Defines classes:
///     CWriteDB_GiMaskIndex, CWriteDB_GiMaskOffset, CWriteDB_GiMaskData,
///     and CWriteDB_GiMask
///
/// Implemented for: UNIX, MS-Windows

#include <objtools/blast/seqdb_writer/writedb_files.hpp>

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);


#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
/// CWriteDB_GiMaskData class
///
/// Manufacture gi based mask data files from input data.

class CWriteDB_GiMaskData : public CWriteDB_File {
public:
    typedef vector< pair<TSeqPos, TSeqPos> > TPairVector;
    typedef pair<int, int> TOffset;

    /// Constructor for an gimask data file.
    ///
    /// @param maskname      Database name (same for all volumes).
    /// @param extn          File extension to use for index file.
    /// @param max_file_size Maximum size of any generated file in bytes.
    /// @param le            Use little endian format
    CWriteDB_GiMaskData(const string     & maskname,
                        const string     & extn,
                        int                index,
                        Uint8              max_file_size,
                        bool               le = false);

    /// Destructor.
    ~CWriteDB_GiMaskData() { };

    /// Write a new data blob.
    /// @param mask Mask data to write.
    void WriteMask(const TPairVector & mask);

    /// Get current index
    int GetIndex() const
    {
        return m_Index;
    }

    /// Get current index/offset pair
    /// @return The current volume index and offset in byte
    TOffset GetOffset() const
    {
        return pair<int, int>(m_Index, m_DataLength);
    }

    /// Tests whether there is room for another batch
    /// @param the number of masks to be added. [in]
    /// @return Return TRUE if masks can fit into the volume.
    bool CanFit(int num_masks) const
    {
        return (m_DataLength + (num_masks *2+1)*4 < m_MaxFileSize);
    }

private:
    /// Length of data written so far.
    Uint8 m_DataLength;

    /// Use little endian?
    bool m_UseLE;

    /// Current index
    int m_Index;

    /// Flush any stored data.
    void x_Flush() { };
};


/// CWriteDB_GiMaskOffset class
///
/// Manufacture gi based mask offset files from input data.

class CWriteDB_GiMaskOffset : public CWriteDB_File {
public:
    typedef pair<int, int> TOffset;
    typedef vector< pair<TGi, TOffset> > TGiOffset;

    /// Constructor for gimask offset file.
    ///
    /// @param maskname      Database name (same for all volumes).
    /// @param extn          File extension to use for offset file.
    /// @param max_file_size Maximum size in bytes for component files.
    /// @param le            Use little endian format
    CWriteDB_GiMaskOffset(const string        & maskname,
                          const string        & extn,
                          Uint8                 max_file_size,
                          bool                  le = false);

    /// Destructor.
    ~CWriteDB_GiMaskOffset() {};

    /// Add sequence GI to the offset file.
    ///
    /// @param gi_offset GI->offset array [in]
    void AddGIs(const TGiOffset &gi_offset);

protected:
    /// Size of a GI
    static const int kGISize = 4;

    /// Size of offset entry
    static const int kOffsetSize = 8;

    /// Page size
    static const int kPageSize = 512;

    /// Flush offset data in preparation for Close().
    void x_Flush() { };

    /// Use little endian?
    bool m_UseLE;
};

/// CWriteDB_GiMaskIndex class
///
/// Manufacture gimask index files from input data.

class CWriteDB_GiMaskIndex : public CWriteDB_GiMaskOffset {
public:
    /// Constructor for gimask index file.
    ///
    /// @param maskname      Database name (same for all volumes).
    /// @param extn          File extension to use for index file.
    /// @param desc          Description of the mask algo
    /// @param max_file_size Maximum size in bytes for component files.
    /// @param le            Use little endian format
    CWriteDB_GiMaskIndex(const string                & maskname,
                         const string                & extn,
                         const string                & desc,
                         Uint8                       max_file_size,
                         bool                        le = false);

    /// Destructor.
    ~CWriteDB_GiMaskIndex() {};

    /// Add sequence GI to the offset file.
    ///
    /// @param gi_offset GI->offset array [in]
    /// @param num_vols Number of volumes [in]
    void AddGIs(const TGiOffset &gi_offset, int num_vols);

private:
    /// String format used by gimask files.
    static const CBlastDbBlob::EStringFormat
        kStringFmt = CBlastDbBlob::eSizeVar;

    /// Build fixed length header fields.
    void x_BuildHeaderFields(int num_vols);

    /// Creation timestamp for this gimask.
    string m_Date;

    /// Description of this gimask.
    string m_Desc;

    /// Number of GIs
    Int4 m_NumGIs;

    /// Number of GIs indexed
    Int4 m_NumIndex;
};


// GiMask files have extensions like: g[mn][iod]
//

/// CWriteDB_GiMask class
///
/// This manages construction of WriteDB gi mask files.

class CWriteDB_GiMask : public CObject {
public:
    typedef vector< pair<TSeqPos, TSeqPos> > TPairVector;
    typedef pair<int, int> TOffset;
    typedef vector< pair<TGi, TOffset> > TGiOffset;

    /// Construct WriteDB style database gimask.
    /// @param maskname Name of the mask data
    /// @param desc  Description of this mask.
    CWriteDB_GiMask(const string      & maskname,
                    const string      & desc,
                    Uint8             max_file_size);

    /// Destructor.
    ~CWriteDB_GiMask() {};

    /// Flush data to disk and close all associated files.
    void Close();

    /// List Filenames
    ///
    /// Returns a list of the files constructed by this class; the
    /// returned list may not be complete until Close() has been
    /// called.  The list is not cleared; instead names are appended
    /// to existing contents.
    ///
    /// @param files
    ///   The set of resolved database path names.
    void ListFiles(vector<string> & files) const;

    /// Get Mask Name
    ///
    /// Returns the name of the mask
    const string & GetName() const {
        return m_MaskName;
    }

    /// Add a mask data for a sequence represented by a set of GIs.
    ///
    /// @param GIs The GIs of the sequence
    /// @param masks The masks represented as ranges
    void AddGiMask(const vector<TGi> & GIs,
                   const TPairVector & masks);

private:
    string m_MaskName;    // the name of the mask data
    Uint8  m_MaxFileSize; // the maximum file size

    /// Data file
    CRef<CWriteDB_GiMaskData> m_DFile;
    CRef<CWriteDB_GiMaskData> m_DFile_LE; // little endian

    /// Offset file
    CRef<CWriteDB_GiMaskOffset> m_OFile;
    CRef<CWriteDB_GiMaskOffset> m_OFile_LE; // little endian

    /// Index file
    CRef<CWriteDB_GiMaskIndex> m_IFile;
    CRef<CWriteDB_GiMaskIndex> m_IFile_LE; // little endian

    /// Sorted list of (GI, offset) pairs
    TGiOffset m_GiOffset;
};
#endif

END_NCBI_SCOPE


#endif // OBJTOOLS_WRITERS_WRITEDB__WRITEDB_GIMASK_HPP

