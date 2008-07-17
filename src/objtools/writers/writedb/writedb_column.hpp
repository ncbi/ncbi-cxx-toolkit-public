#ifndef OBJTOOLS_WRITERS_WRITEDB__WRITEDB_COLUMN_HPP
#define OBJTOOLS_WRITERS_WRITEDB__WRITEDB_COLUMN_HPP

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

/// @file writedb_column.hpp
/// Code for arbitrary data `column' file construction.
///
/// Defines classes:
///     CWriteDB_ColumnIndex, CWriteDB_ColumnData, CWriteDB_Column
///
/// Implemented for: UNIX, MS-Windows

#include "writedb_files.hpp"

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);


#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
/// CWriteDB_ColumnData class
/// 
/// Manufacture column data files from input data.

class CWriteDB_ColumnData : public CWriteDB_File {
public:
    /// Constructor for an column data file.
    ///
    /// @param dbname        Database name (same for all volumes).
    /// @param extn          File extension for data file.
    /// @param index         Index of the associated volume.
    /// @param max_file_size Maximum size of any generated file in bytes.
    CWriteDB_ColumnData(const string     & dbname,
                        const string     & extn,
                        int                index,
                        Uint8              max_file_size);
    
    /// Destructor.
    ~CWriteDB_ColumnData();
    
    /// Write a new data blob.
    /// @param blob Blob data to write.
    /// @return The file size after this write.
    Int8 WriteBlob(const CBlastDbBlob & blob);
    
    /// Get size of data file (so far).
    /// @return The total file size in bytes.
    Int8 GetDataLength() const
    {
        return m_DataLength;
    }
    
    /// Tests whether the data file is empty.
    bool Empty() const
    {
        return ! GetDataLength();
    }
    
    /// Tests whether there is room for some number of bytes.
    ///
    /// This returns true if adding the specified number of bytes
    /// would cause this file to exceed the maximum file size limit.
    ///
    /// @return Returns true if enough space is available.
    bool CanFit(int size) const;
    
private:
    /// Length of data written so far.
    Uint8 m_DataLength;
    
    /// Flush any stored data.
    void x_Flush();
};


/// CWriteDB_ColumnIndex class
/// 
/// Manufacture column index files from input data.

class CWriteDB_ColumnIndex : public CWriteDB_File {
public:
    /// Type used for database column meta-data.
    typedef map<string,string> TColumnMeta;
    
    /// Constructor for column index file.
    ///
    /// @param dbname        Database name (same for all volumes).
    /// @param extn          File extension to use for index file.
    /// @param index         Index of the associated volume.
    /// @param datafile      Corresponding column data file.
    /// @param title         Title of the database column.
    /// @param meta          Meta data for this column.
    /// @param max_file_size Maximum size in bytes for component files.
    CWriteDB_ColumnIndex(const string        & dbname,
                         const string        & extn,
                         int                   index,
                         CWriteDB_ColumnData & datafile,
                         const string        & title,
                         const TColumnMeta   & meta,
                         Uint8                 max_file_size);
    
    /// Destructor.
    ~CWriteDB_ColumnIndex();
    
    /// Write the offset of a new data blob.
    /// @param offset The offset of the blob.
    /// @param size The size of the blob.
    void WriteBlobIndex(Int8 offset);
    
    /// Tests whether there is room for another entry.
    /// 
    /// This returns true if there is room for another entry in this
    /// file without exceeding the maximum file size.  When this file
    /// is part of a CWriteDB volume, it is unlikely that the column
    /// index will return false here (before some other file does so).
    ///
    /// @return Returns true if one more row can fit.
    bool CanFit() const;
    
    /// Add meta data to the column.
    ///
    /// In addition to normal blob data, database columns can store a
    /// `dictionary' of user-defined metadata in key/value form.  This
    /// method adds one such key/value pair to the column.  Specifying
    /// a key a second time causes replacement of the previous value.
    /// Using this mechanism to store large amounts of data may have a
    /// negative impact on performance.
    ///
    /// @param key   Key string.
    /// @param value Value string.
    void AddMetaData(const string & key, const string & value);
    
private:
    /// String format used by column files.
    static const CBlastDbBlob::EStringFormat
        kStringFmt = CBlastDbBlob::eSizeVar;
    
    /// Size of an entry in the index file.
    static const int kEntrySize = 4;
    
    /// Flush index data in preparation for Close().
    void x_Flush();
    
    /// Build fixed length header fields.
    void x_BuildHeaderFields();
    
    /// Build header string data section.
    void x_BuildHeaderStrings();
    
    /// Serialize meta data strings into header object.
    void x_BuildMetaData();
    
    
    // Data
    
    /// The data file associated with this index file.
    CRef<CWriteDB_ColumnData> m_DataFile;
    
    /// Header data.
    CRef<CBlastDbBlob> m_Header;
    
    /// Offsets of sequences in the data file.
    CRef<CBlastDbBlob> m_Offsets;
    
    /// Column meta data.
    TColumnMeta m_MetaData;
    
    /// Creation timestamp for this column.
    string m_Date;
    
    /// Title of this column.
    string m_Title;
    
    /// OID Count.
    int m_OIDs;
    
    /// Length of data accounted for so far.
    Uint8 m_DataLength;
};


// Column files have extensions like: [pnx][a-z][ab]
//
// Char 1: Protein, nucleotide, other.
// Char 2: Any value (up to 26 columns)
// Char 3: 'a' for index data, 'b' for blob data.


/// CWriteDB_Column class
///
/// This manages construction of a WriteDB column, which maps OIDs to
/// blob data.

class CWriteDB_Column : public CObject {
public:
    /// Type used for database column meta-data.
    typedef map<string, string> TColumnMeta;
    
    /// Construct WriteDB style database column.
    /// @param dbname Base of filename.
    /// @param extn1  Extension used for index file.
    /// @param extn1  Extension used for data file.
    /// @param index  Volume index.
    /// @param title  Title of this column.
    /// @param meta   User-defined meta data for this file.
    CWriteDB_Column(const string      & dbname,
                    const string      & extn1,
                    const string      & extn2,
                    int                 index,
                    const string      & title,
                    const TColumnMeta & meta,
                    Uint8             max_file_size);
    
    /// Destructor.
    ~CWriteDB_Column();
    
    /// Rename files to single-volume names.
    /// 
    /// When volume component files are generated by WriteDB, the
    /// names include a volume index.  This method renames the files
    /// to names that do not include the volume index.  This method
    /// should not be called until after Close() is called.
    void RenameSingle();
    
    /// Flush data to disk and close all associated files.
    void Close();
    
    /// Tests whether there is room for a given blob.
    /// 
    /// @param bytes Size of the blob that would be added.
    /// @return Returns true if the IDs can fit into the volume.
    bool CanFit(int bytes) const;
    
    /// List Filenames
    ///
    /// Returns a list of the files constructed by this class; the
    /// returned list may not be complete until Close() has been
    /// called.  The list is not cleared; instead names are appended
    /// to existing contents.
    ///
    /// @param files
    ///   The set of resolved database path names.
    void ListFiles(vector<string> & files, bool skip_empty) const;
    
    /// Add a blob to the column.
    ///
    /// The data described by `blob' is added to the column.  If the
    /// blob is empty, no data is stored but the OID is incremented.
    ///
    /// @param blob The blob to add to the column.
    void AddBlob(const CBlastDbBlob & blob);
    
    /// Add meta data to the column.
    ///
    /// In addition to normal blob data, database columns can store a
    /// `dictionary' of user-defined metadata in key/value form.  This
    /// method adds one such key/value pair to the column.  Specifying
    /// a key a second time causes replacement of the previous value.
    /// Using this mechanism to store large amounts of data may have a
    /// negative impact on performance.
    ///
    /// @param key   Key string.
    /// @param value Value string.
    void AddMetaData(const string & key, const string & value);
    
private:
    /// Index file, contains meta data and samples of the key/oid pairs.
    CRef<CWriteDB_ColumnIndex> m_IFile;
    
    /// Data file, contains one record for each key/oid pair.
    CRef<CWriteDB_ColumnData> m_DFile;
};
#endif

END_NCBI_SCOPE


#endif // OBJTOOLS_WRITERS_WRITEDB__WRITEDB_COLUMN_HPP

