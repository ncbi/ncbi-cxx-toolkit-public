#ifndef OBJTOOLS_WRITERS_WRITEDB__WRITEDB_FILES_HPP
#define OBJTOOLS_WRITERS_WRITEDB__WRITEDB_FILES_HPP

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

/// @file writedb_files.hpp
/// Code for database files construction.
///
/// Defines classes:
///     CWriteDBHeader
///
/// Implemented for: UNIX, MS-Windows

#include "writedb_general.hpp"
#include "writedb_convert.hpp"
#include <objects/seq/seq__.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbifile.hpp>

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);

/// CWriteDB_IndexFile class
/// 
/// This manufactures blast database index files from input data.

class CWriteDB_File : public CObject {
public:
    // Setup and control
    
    /// Constructor.
    ///
    /// The filename is constructed from basename, extension, and
    /// index, but might be changed if the RenameSingle() method is
    /// called.  If zero is specified for maximum file size, a default
    /// size is provided by this class.  The maximum file size is not
    /// enforced by this class, instead each derived class must do its
    /// own enforcement.
    ///
    /// @param basename Database base name, shared by all files. [in]
    /// @param extension File name extension for this file. [in]
    /// @param index Volume index used in filename. [in]
    /// @param max_file_size File size limit (in bytes). [in]
    /// @param always_create If true the file will be created now. [in]
    CWriteDB_File(const string & basename,
                  const string & extension,
                  int            index,
                  Uint8          max_file_size,
                  bool           always_create);
    
    /// Create and open the file.
    ///
    /// This method must be called before the first time that data is
    /// written to the file.  If the constructor is passed 'true' for
    /// always_create, this method will be called during construction.
    /// It is an error to call this method more than once (including
    /// via the constructor) or to not call it but to call Write.  The
    /// rationale for making this explicit is to permit some files to
    /// be created optionally, such as ISAM files, which should only
    /// be created if the corresponding ID types are found.
    void Create();
    
    /// Write contents of a string to the file.
    /// @param data Data to write.
    /// @return File offset after write.
    int Write(const CTempString & data);
    
    /// Write an Int4 (in bigendian order) to the file.
    /// @param data String to write.
    /// @return File offset after write.
    int WriteInt4(int data)
    {
        s_WriteInt4(m_RealFile, data);
        m_Offset += 4;
        return m_Offset;
    }
    
    /// Write an Int8 (in bigendian order) to the file.
    /// @param data String to write.
    /// @return File offset after write.
    int WriteInt8(Int8 data)
    {
        s_WriteInt8BE(m_RealFile, data);
        m_Offset += 8;
        return m_Offset;
    }
    
    /// Write contents of a string to the file, appending a NUL.
    /// @param data String to write.
    /// @return File offset after write.
    int WriteWithNull(const CTempString & data)
    {
        Write(data);
        return Write(m_Nul);
    }
    
    /// Close the file, flushing any remaining data to disk.
    void Close();
    
    /// Rename this file, disincluding the volume index.
    virtual void RenameSingle();
    
    /// Construct the short name for a volume.
    ///
    /// Volume names consist of the database base name, ".", and the
    /// volume index in decimal.  The volume index is normally two
    /// digits, but if more than 100 volumes are needed, the filename
    /// will use three or more index digits as needed.
    ///
    /// @param base Base name to use.
    /// @param index Volume index.
    /// @return A short name.
    static string MakeShortName(const string & base, int index);
    
    /// Get the current filename for this file.
    ///
    /// The filename is returned.  The data returned by this method
    /// will reflect the change made by RenameSingle, so it probably
    /// best to call it after that method has been called (if it will
    /// be called).
    ///
    /// @return The filename.
    const string & GetFilename() const
    {
        return m_Fname;
    }
    
protected:
    /// True if the file has already been opened.
    bool m_Created;
    
    /// Underlying 'output file' type used here.
    typedef ofstream TFile;
    
    /// For convenience, a string containing one NUL character.
    string m_Nul; // init me
    
    /// The default value for max_file_size.
    /// @return The max file size used if otherwise unspecified.
    Uint8 x_DefaultByteLimit()
    {
        // 1 gb (marketing version) - 1; about a billion
        return 1000*1000*1000 - 1;
    }
    
    /// This should flush any unwritten data to disk.
    ///
    /// This method must be implemented by derived classes to flush
    /// any unwritten data to disk.  In the cases of sequence and
    /// header files, it will normally do nothing, because such files
    /// are written as the data is available.  For index (pin/nin) and
    /// ISAM files, this method does most of the disk I/O.
    virtual void x_Flush() = 0;
    
    /// Build the filename for this file.
    void x_MakeFileName();
    
    // Configuration
    
    string m_BaseName;    ///< Database base name for all files.
    string m_Extension;   ///< File extension for this file.
    int    m_Index;       ///< Volume index.
    int    m_Offset;      ///< Stream position.
    Uint8  m_MaxFileSize; ///< Maximum file size in bytes.
    
    // The file
    
    bool   m_UseIndex; ///< True if filenames should use volume index.
    string m_Fname;    ///< Current filename for output file.
    TFile  m_RealFile; ///< Actual stream implementing the output file.
};

// For index file format, see .cpp file.

/// This class builds the volume index file (pin or nin).
class CWriteDB_IndexFile : public CWriteDB_File {
public:
    /// Constructor.
    /// @param dbname Database base name.
    /// @param protein True for protein volumes.
    /// @param title Database title string.
    /// @param date Timestamp of database construction start.
    /// @param index Index of this volume.
    /// @param max_file_size Maximum file size in bytes (or zero).
    CWriteDB_IndexFile(const string & dbname,
                       bool           protein,
                       const string & title,
                       const string & date,
                       int            index,
                       Uint8          max_file_size);
    
    /// Returns true if another sequence can fit into the file.
    bool CanFit()
    {
        _ASSERT(m_MaxFileSize > 1024);
        
        if (! m_OIDs)
            return true;
        
        return m_DataSize < (m_MaxFileSize-12);
    }
    
    /// Add a sequence to a protein index file (pin).
    ///
    /// The index file does not need sequence data, so this method
    /// only needs offsets of the data in other files.
    ///
    /// @param Sequence length in letters.
    /// @param hdr Length of binary ASN.1 header data.
    /// @param seq Length in bytes of sequence data.
    void AddSequence(int length, int hdr, int seq)
    {
        if (length > m_MaxLength) {
            m_MaxLength = length;
        }
        
        m_OIDs ++;
        m_Letters += length;
        m_DataSize += 8;
        
        m_Hdr.push_back(hdr);
        m_Seq.push_back(seq);
    }
    
    /// Add a sequence to a nucleotide index file (nin).
    ///
    /// The index file does not need sequence data, so this method
    /// only needs offsets of the data in other files.
    ///
    /// @param Sequence length in letters.
    /// @param hdr Length of binary ASN.1 header data.
    /// @param seq Length in bytes of packed sequence data.
    /// @param seq Length in bytes of packed ambiguity data.
    void AddSequence(int length, int hdr, int seq, int amb)
    {
        if (length > m_MaxLength) {
            m_MaxLength = length;
        }
        
        m_OIDs ++;
        m_Letters += length;
        
        m_DataSize += 12;
        m_Hdr.push_back(hdr);
        m_Seq.push_back(amb); // Not a bug.
        m_Amb.push_back(seq); // Also not a bug.
    }
    
private:
    /// Compute index file overhead.  This is the overhead used by all
    /// fields of the index file, and does account for padding.
    ///
    /// @param T Title string.
    /// @param D Create time string.
    /// @return Combined size of all meta-data fields in nin/pin file.
    int x_Overhead(const string & T, const string & D);
    
    /// Flush index data to disk.
    virtual void x_Flush();
    
    bool   m_Protein;   ///< True if this is a protein database.
    string m_Title;     ///< Title string for all database volumes.
    string m_Date;      ///< Database creation time stamp.
    int    m_OIDs;      ///< OIDs added to database so far.
    int    m_Overhead;  ///< Amount of file used by metadata.
    Uint8  m_DataSize;  ///< Required space for data once written to disk.
    Uint8  m_Letters;   ///< Letters of sequence data accumulated so far.
    int    m_MaxLength; ///< Length of longest sequence.
    
    // Because the lengths are found via "next offset - this offset",
    // each array has an extra element.  (This is not necesary in the
    // case of m_Amb; the last element is never examined because of
    // the alternation of sequences and ambiguities.)
    
    /// Start offset in header file of each OID's headers.
    ///
    /// The end offset is given by the start offset of the following
    /// OID's headers.
    vector<int> m_Hdr;
    
    /// Offset in sequence file of each OID's sequence data.
    ///
    /// The end of the sequence data is given by the start offset of
    /// the ambiguity data for the same OID.
    vector<int> m_Seq;
    
    /// Offset in sequence file of each OID's ambiguity data.
    ///
    /// The end of the ambiguity data is given by the start offset of
    /// the sequence data for the next OID.
    vector<int> m_Amb;
};

/// This class builds the volume header file (phr or nhr).
class CWriteDB_HeaderFile : public CWriteDB_File {
public:
    /// Constructor.
    /// @param dbname Database base name.
    /// @param protein True for protein volumes.
    /// @param index Index of this volume.
    /// @param max_file_size Maximum file size in bytes (or zero).
    CWriteDB_HeaderFile(const string & dbname,
                        bool           protein,
                        int            index,
                        Uint8          max_file_size);
    
    /// Returns true if the specified amount of data would fit.
    ///
    /// If the specified amount of data (in bytes) would fit in the
    /// file without exceeding the max_file_size, this method returns
    /// true.
    ///
    /// @param size Size of new data in bytes.
    bool CanFit(int size)
    {
        if (! m_DataSize) {
            return true;
        }
        
        return (m_DataSize + size) < m_MaxFileSize;
    }
    
    /// Add binary header data to this file.
    /// @param binhdr Binary ASN.1 version of header data. [in]
    /// @param offset Offset of end of header data. [out]
    void AddSequence(const string & binhdr, int & offset)
    {
        m_DataSize = offset = Write(binhdr);
    }
    
private:
    /// Flush unwritten data to the output file.
    virtual void x_Flush()
    {
        // There is nothing to do here - header data is written as
        // soon as it is added.
    }
    
    /// Amount of data written so far.
    Uint8 m_DataSize;
};

class CWriteDB_SequenceFile : public CWriteDB_File {
public:
    /// Constructor.
    /// @param dbname Database base name.
    /// @param protein True for protein volumes.
    /// @param index Index of this volume.
    /// @param max_file_size Maximum file size in bytes (or zero).
    /// @param max_letter Maximum sequence letters per volume (or zero).
    CWriteDB_SequenceFile(const string & dbname,
                          bool           protein,
                          int            index,
                          Uint8          max_file_size,
                          Uint8          max_letters);
    
    /// Returns true if the specified amount of data would fit.
    ///
    /// If the specified amount of data (in bytes) would fit in the
    /// file without exceeding the max_file_size, and the specified
    /// number of letters would fit without exceeding the maximum
    /// letters limit, this method returns true.
    ///
    /// @param size Size of new data in bytes.
    /// @param letters Number of sequence letters in new data.
    bool CanFit(int size, int letters)
    {
        if (m_Offset <= 1) {
            return true;
        }
        
        if (m_BaseLimit &&
            ((m_Letters + letters) > m_BaseLimit)) {
            return false;
        }
        
        return ((m_Offset + (unsigned)size)  < m_MaxFileSize);
    }
    
    /// Add a protein sequence to this file.
    ///
    /// This method should only be called in the protein case.
    ///
    /// @param sequence Packed sequence data. [in]
    /// @param offset Offset of the end of the sequence data. [out]
    /// @param length Length of the sequence in letters. [in]
    void AddSequence(const string & sequence,
                     int          & offset,
                     int            length)
    {
        _ASSERT(m_Protein);
        offset = WriteWithNull(sequence);
        m_Letters += length;
    }
    
    /// Add a nucleotide sequence to this file.
    ///
    /// This method should only be called in the nucleotide case.
    ///
    /// @param sequence Packed sequence data. [in]
    /// @param ambig Packed ambiguity data. [in]
    /// @param off_seq Offset of the end of the sequence data. [out]
    /// @param off_amb Offset of the end of the ambiguity data. [out]
    /// @param length Length of the sequence in letters. [in]
    void AddSequence(const string & sequence,
                     const string & ambig,
                     int          & off_seq,
                     int          & off_amb,
                     int            length)
    {
        _ASSERT(! m_Protein);
        off_seq = Write(sequence);
        off_amb = Write(ambig);
        m_Letters += length;
    }
    
private:
    /// Flush unwritten data to the output file.
    virtual void x_Flush()
    {
        // There is nothing to do here - sequence data is written as
        // soon as it is added.
    }
    
    Uint8 m_Letters;   ///< Letters of sequence data added so far.
    Uint8 m_BaseLimit; ///< Limit on letters of sequence data.
    bool  m_Protein;   ///< True if this is a protein database.
};

END_NCBI_SCOPE


#endif // OBJTOOLS_WRITERS_WRITEDB__WRITEDB_FILES_HPP

