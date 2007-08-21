#ifndef OBJTOOLS_WRITERS_WRITEDB__WRITEDB_ISAM_HPP
#define OBJTOOLS_WRITERS_WRITEDB__WRITEDB_ISAM_HPP

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

/// @file writedb_isam.hpp
/// Code for database isam construction.
///
/// Defines classes:
///     CWriteDB_IsamIndex, CWriteDB_IsamData, CWriteDB_Isam
///
/// Implemented for: UNIX, MS-Windows

#include <objects/seq/seq__.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbifile.hpp>
#include "writedb_files.hpp"

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);

/// Type of ISAM index.
enum EWriteDBIsamType {
    /// Protein Identifier Group.
    ePig,
    
    /// Accession (string) Index.
    eAcc,
    
    /// GI Index.
    eGi,
    
    /// Trace ID Index.
    eTrace,
    
    /// Hash Index.
    eHash
};

/// Forward definition for CWriteDB_IsamData class.
class CWriteDB_IsamData;

/// CWriteDB_IsamIndex class
/// 
/// Manufacture an isam index file from sequence IDs.

class CWriteDB_IsamIndex : public CWriteDB_File {
public:
    /// Type of identifier stored in this ISAM index.
    typedef EWriteDBIsamType EIsamType;
    
    /// Type used for lists of sequence identifiers.
    typedef vector< CRef<CSeq_id> > TIdList;
    
    // Setup and control
    
    /// Constructor for an ISAM index file.
    ///
    /// @param itype    Type (GI, PIG, or string).
    /// @param dbname   Database name (same for all volumes).
    /// @param protein  True for protein, false for nucleotide.
    /// @param index    Index of the associated volume.
    /// @param datafile Corresponding ISAM data file.
    /// @param sparse   Set to true if sparse mode should be used.
    CWriteDB_IsamIndex(EIsamType               itype,
                       const string          & dbname,
                       bool                    protein,
                       int                     index,
                       CRef<CWriteDB_IsamData> datafile,
                       bool                    sparse);
    
    /// Destructor.
    ~CWriteDB_IsamIndex();
    
    /// Add sequence IDs to the index file.
    ///
    /// If this is a GI index, GIs are collected and added to an
    /// internal list.  If this is a string index, various strings are
    /// constructed from the identifiers and added to a sorting class.
    /// How many and what kind of strings are indexed depends on
    /// whether the "sparse" flag is specified.
    ///
    /// @param oid OID of the sequence.
    /// @param List of sequence identifiers.
    void AddIds(int oid, const TIdList & ids);
    
    /// Set PIG for a protein sequence.
    ///
    /// The PIG identifier corresponds to a specific string of protein
    /// residues rather than to a single defline or GI, and as such is
    /// not treated as a true sequence identifier.  Each PIG will be
    /// associated with one OID in a non-redundant protein database.
    ///
    /// @param oid OID of the sequence.
    /// @param pig PIG identifier for this sequence.
    void AddPig(int oid, int pig);
    
    /// Set a sequence's hash value.
    ///
    /// The sequence hash is a hash of a sequence.  This adds that
    /// hash for the purpose of building an ISAM file mapping hash
    /// values to OIDs.
    ///
    /// @param oid OID of the sequence.
    /// @param hash Sequence hash for this sequence.
    void AddHash(int oid, int hash);
    
    /// Tests whether there is room for a given number of IDs.
    /// 
    /// This returns true if the specified number of identifiers can
    /// be added to this volume without overflowing the maximum file
    /// size.  For strings, the estimating technique used is very
    /// rough (to avoid excess computation) and will tend to return
    /// false several disk blocks before the ISAM file actually
    /// reaches the maximum file size.  This is done for performance
    /// reasons.
    ///
    /// @param num Number of IDs that would be added.
    /// @return Returns true if the IDs can fit into the volume.
    bool CanFit(int num);
    
    /// Tests whether the index file is empty (has no entries).
    bool Empty() const;
    
private:
    enum {
        eKeyOffset       = 9*4,  ///< Offset of the key offset table.
        eKeyDelim        = 2,    ///< Byte indicating end of key.
        eRecordDelim     = 10,   ///< Byte indicating end of data.
        eMaxStringLine   = 4096, ///< Maximum line size for string.
        eIsamNumericType = 0,    ///< Numeric ISAM file with Int4 key.
        eIsamStringType  = 2,    ///< String ISAM file.
        eIsamNumericLong = 5     ///< Numeric ISAM file with Int8 key.
    };
    
    /// Flush index data for a numeric ISAM file.
    void x_FlushNumericIndex();
    
    /// Flush index data for a string ISAM file.
    void x_FlushStringIndex();
    
    /// Flush index data in preparation for Close().
    void x_Flush();
    
    /// Store GIs found in Seq-id list.
    /// @param oid OID of the sequence.
    /// @param idlist Identifiers for this sequence.
    void x_AddGis(int oid, const TIdList & idlist);
    
    /// Store GIs found in Seq-id list.
    /// @param oid OID of the sequence.
    /// @param idlist Identifiers for this sequence.
    void x_AddTraceIds(int oid, const TIdList & idlist);
    
    /// Compute and store string IDs from Seq-ids.
    ///
    /// For each identifier in the idlist, zero or more strings are
    /// added to the table of strings for this database.
    ///
    /// @param oid OID of the sequence.
    /// @param idlist Identifiers for this sequence.
    void x_AddStringIds(int oid, const TIdList & idlist);
    
    /// Add the index strings for the specified PDB identifier.
    ///
    /// For each Seq-id type, there is a defined set of strings that
    /// are constructed from the ID and added to the index file.  This
    /// method handles the definition for Seq-ids of the PDB type.
    ///
    /// @param oid OID of the sequence.
    /// @param seqid The sequence identifier.
    void x_AddPdb(int oid, const CSeq_id & seqid);
    
    /// Add GI String.
    /// 
    /// In non-sparse mode, the set of string index objects includes
    /// GIs with the format "gi|#|".  This adds such a string to the
    /// string table.
    ///
    /// @param oid OID of the sequence.
    /// @param gi GI to stringify.
    void x_AddGiString(int oid, int gi);
    
    /// Add a 'local' type Seq-id.
    /// 
    /// For each Seq-id type, there is a defined set of strings that
    /// are constructed from the ID and added to the index file.  This
    /// method handles the 'local' ID, i.e. those IDs that would be
    /// printed as "lcl|...".
    /// 
    /// @param oid OID for this sequence.
    /// @param seqid The sequence identifier.
    /// @param objid The data type for the local ID.
    void x_AddLocal(int oid, const CSeq_id & seqid);
    
    /// Add a 'patent' type Seq-id.
    /// 
    /// For each Seq-id type, there is a defined set of strings that
    /// are constructed from the ID and added to the index file.  This
    /// method handles the 'patent' ID, i.e. those IDs that would be
    /// something similar to "pat|xy|1234|56".  Here, the 'xy' is the
    /// issuing authority (country of origin), the 1234 is the patent
    /// number and 56 is the sequence number (within the 1234 patent).
    /// 
    /// @param oid OID for this sequence.
    /// @param seqid The sequence identifier.
    void x_AddPatent(int oid, const CSeq_id & seqid);
    
    /// Add a text ID.
    ///
    /// For each Seq-id type, there is a defined set of strings that
    /// are constructed from the ID and added to the index file.
    /// Several Seq-id types follow a 'Textseq-id' pattern.  This
    /// method handles those types.  This method can also optionally
    /// add 'genbank' strings, which look like "gb|...".
    ///
    /// @param oid OID for this sequence.
    /// @param typestr The string prefix for this type, e.g. "dbj".
    /// @param seqid The sequence identifier.
    /// @param id The Textseq_id object.
    /// @param add_gb Specify true to add the "gb|" strings.
    void x_AddTextId(int                 oid,
                     const char        * typestr,
                     const CTextseq_id & id,
                     bool                add_gb);
    
    /// Add a string to the string table.
    /// 
    /// This is a convenience method for using x_AddStringData() with
    /// normal c++ strings.  This uses a different name than the other
    /// methods to prevent automatic conversions between strings and
    /// CTempStrings from accidentally occurring.
    /// 
    /// @param oid OID for this sequence.
    /// @param s String to add.
    void x_AddStdString(int oid, const string & s)
    {
        x_AddStringData(oid, s.data(), (int) s.size());
    }
    
    /// Add a string to the string table.
    /// 
    /// This is a convenience method for using x_AddStringData() with
    /// CTempString objects.  This uses a different name than the
    /// other methods to prevent automatic conversions between strings
    /// and CTempStrings from accidentally occurring.
    /// 
    /// @param oid OID for this sequence.
    /// @param s String to add.
    void x_AddStringData(int oid, const CTempString & s)
    {
        x_AddStringData(oid, s.data(), (int) s.size());
    }
    
    /// Add a string to the string table.
    /// 
    /// This is the bottom-most level of the AddString() design; it
    /// just adds a record mapping the string to the OID.  Every
    /// string added to the string table goes through this method.
    /// 
    /// @param oid OID for this sequence.
    /// @param s String to add.
    /// @param size Size of string to add.
    void x_AddStringData(int oid, const char * s, int size);
    
    /// Add an accession with a version.
    ///
    /// If the provided version is zero (or the string is zero
    /// length), this method does nothing; otherwise it combines the
    /// accession and version and adds the resulting string to the
    /// string table.
    ///
    /// @param oid OID for this sequence.
    /// @param s Accession string to add.
    /// @param ver Version to use, or zero.
    void x_AddString(int oid, const CTempString & s, int ver);
    
    /// Add several strings for the given identifier.
    ///
    /// In non-sparse mode, this method is used to add up to 12
    /// strings to the key, each having a different combination of
    /// accession, locus, and version information.  If the input ID
    /// looks like "T|A.V|N", then the strings added will include the
    /// following permutations of those components: ("A", "A.V",
    /// "T|A.V|" "T|A.V|N", "T|A|", "T|A|N", "T||N", "gb|A.V|",
    /// "gb|A.V|N", "gb|A|", "gb|A|N", "gb||N").  (I am calling A the
    /// accession, V the version, N the name, and T the type prefix.
    /// This may not correspond to the standard jargon for these
    /// elements.)  Some of these combinations will be omitted if one
    /// or more of elements (such as the name or version) are not
    /// specified.  Duplications will be removed at a later stage of
    /// processing.
    ///
    /// @param oid OID for this sequence.
    /// @param typestr The type prefix for this identifier.
    /// @param nm The name component of the identifier.
    /// @param ver Version to use, or zero.
    /// @param acc The accession string.
    void x_AddString(int                 oid,
                     const char        * typestr,
                     const CTempString & nm,
                     int                 ver,
                     const CTempString & acc);
    
    /// Write the ISAM index header to disk.
    void x_WriteHeader();
    
    /// Convert a string to lower case.
    void x_ToLower(string & s)
    {
        for(unsigned i = 0; i < s.size(); i++) {
            char ch = s[i];
            
            if (ch != tolower(ch)) {
                s[i] = tolower(ch);
            }
        }
    }
    
    /// Free no longer needed array and string memory.
    void x_Free();
    
    // Configuration
    
    EIsamType m_Type;         ///< Type of identifier indexed here.
    bool      m_Sparse;       ///< If true, fewer strings are used.
    int       m_PageSize;     ///< Ratio of samples to data records.
    int       m_BytesPerElem; ///< Byte (over)estimate per Seq-id.
    Uint8     m_DataFileSize; ///< Accumulated size of data file.
    
    // Table data
    
    // pair<> is sortable by first then second, so there is not really
    // a need to build special structs here.  I only do this so I can
    // name the fields for readability purposes.
    
    /// Element type for numeric tables.
    ///
    /// This associates a GI (or PIG) with a given OID.  Deriving from
    /// pair<int,int> provides correct equality and order operators.
    struct SIdOid : public pair<Int8, int> {
        /// Construct an object from oid and ident.
        /// @param i Numeric identifier (GI or PIG).
        /// @param o OID of the sequence.
        SIdOid(Int8 i, int o)
            : pair<Int8,int>(i,o)
        {
        }
        
        /// Return the numeric identifier.
        Int8 id()  const
        {
            return first;
        }
        
        /// Return the oid.
        int oid() const
        {
            return second;
        }
    };
    
    CWriteDB_PackedSemiTree m_StringSort;  ///< Sorted list of strings.
    vector<SIdOid>          m_NumberTable; ///< Sorted list of numbers.
    
    bool m_UseInt8; ///< Use an Int8 table for numeric IDs.
    
    /// The data file associated with this index file.
    CRef<CWriteDB_IsamData> m_DataFile;
};

/// CWriteDB_IsamData class
/// 
/// This manufactures isam index files from input data.

class CWriteDB_IsamData : public CWriteDB_File {
public:
    /// Type of identifier stored in this ISAM index.
    typedef EWriteDBIsamType EIsamType;
    
    // Setup and control
    
    /// Constructor for an ISAM data file.
    ///
    /// @param itype         Type (GI, PIG, or string).
    /// @param dbname        Database name (same for all volumes).
    /// @param protein       True for protein, false for nucleotide.
    /// @param index         Index of the associated volume.
    /// @param max_file_size Maximum size of any generated file in bytes.
    CWriteDB_IsamData(EIsamType      itype,
                      const string & dbname,
                      bool           protein,
                      int            index,
                      Uint8          max_file_size);
    
    /// Destructor.
    ~CWriteDB_IsamData();
    
private:
    void x_Flush();
};


/// CWriteDB_Isam class
/// 
/// This manufactures isam indices.

class CWriteDB_Isam : public CObject {
public:
    /// Type of identifier stored in this ISAM index.
    typedef EWriteDBIsamType EIsamType;
    
    /// Type used for lists of sequence identifiers.
    typedef vector< CRef<CSeq_id> > TIdList;
    
    // Setup and control
    
    /// Constructor for an ISAM index file.
    ///
    /// @param itype         Type (GI, PIG, or string).
    /// @param dbname        Database name (same for all volumes).
    /// @param protein       True for protein, false for nucleotide.
    /// @param index         Index of the associated volume.
    /// @param max_file_size Maximum size of any generated file in bytes.
    /// @param sparse        Set to true if sparse mode should be used.
    CWriteDB_Isam(EIsamType      itype,
                  const string & dbname,
                  bool           protein,
                  int            index,
                  Uint8          max_file_size,
                  bool           sparse);
    
    /// Destructor.
    ~CWriteDB_Isam();
    
    /// Add sequence IDs to the index file.
    ///
    /// If this is a GI index, GIs are collected and added to an
    /// internal list.  If this is a string index, various strings are
    /// constructed from the identifiers and added to a sorting class.
    /// How many and what kind of strings are indexed depends on
    /// whether the "sparse" flag is specified.
    ///
    /// @param oid OID of the sequence.
    /// @param List of sequence identifiers.
    void AddIds(int oid, const TIdList & ids);
    
    /// Set PIG for a protein sequence.
    ///
    /// The PIG identifier corresponds to a specific string of protein
    /// residues rather than to a single defline or GI, and as such is
    /// not treated as a true sequence identifier.  Each PIG will be
    /// associated with one OID in a non-redundant protein database.
    ///
    /// @param oid OID of the sequence.
    /// @param pig PIG identifier for this sequence.
    void AddPig(int oid, int pig);
    
    /// Set a sequence's hash value.
    ///
    /// The sequence hash is a hash of a sequence.  This adds that
    /// hash for the purpose of building an ISAM file mapping hash
    /// values to OIDs.
    ///
    /// @param oid OID of the sequence.
    /// @param hash Sequence hash for this sequence.
    void AddHash(int oid, int hash);
    
    /// Rename files to single-volume names.
    /// 
    /// When volume component files are generated by WriteDB, the
    /// names include a volume index.  This method renames the files
    /// to names that do not include the volume index.  This method
    /// should not be called until after Close() is called.
    void RenameSingle();
    
    /// Flush data to disk and close all associated files.
    void Close();
    
    /// Tests whether there is room for a given number of IDs.
    /// 
    /// This returns true if the specified number of identifiers can
    /// be added to this volume without overflowing the maximum file
    /// size.  For strings, the estimating technique used is very
    /// rough (to avoid excess computation) and will tend to return
    /// false several disk blocks before the ISAM file actually
    /// reaches the maximum file size.  This is done for performance
    /// reasons.
    ///
    /// @param num Number of IDs that would be added.
    /// @return Returns true if the IDs can fit into the volume.
    bool CanFit(int num);
    
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
    
private:
    /// Index file, contains meta data and samples of the key/oid pairs.
    CRef<CWriteDB_IsamIndex> m_IFile;
    
    /// Data file, contains one record for each key/oid pair.
    CRef<CWriteDB_IsamData> m_DFile;
};

END_NCBI_SCOPE


#endif // OBJTOOLS_WRITERS_WRITEDB__WRITEDB_ISAM_HPP

