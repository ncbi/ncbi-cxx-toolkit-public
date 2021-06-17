#ifndef OBJTOOLS_BLAST_SEQDB_READER___COLUMN_READER__HPP
#define OBJTOOLS_BLAST_SEQDB_READER___COLUMN_READER__HPP

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

/// @file column_reader.hpp
/// Defines column reader class for SeqDB.
/// 
/// Defines classes:
///     CSeqDB_ColumnReader
/// 
/// Implemented for: UNIX, MS-Windows

#include <ncbiconf.h>
#include <corelib/ncbiobj.hpp>
#include <objtools/blast/seqdb_reader/seqdbblob.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE

/// Include definitions from the objects namespace.
USING_SCOPE(objects);


/// Reader for BlastDb format column files.
///
/// This class supports reading of BlastDb format column files.  To
/// read column files attached to a volume, use SeqDB's column related
/// methods.  This class is intended for column data not associated
/// with specific BlastDb volumes.

class NCBI_XOBJREAD_EXPORT CSeqDB_ColumnReader : public CObject {
public:
    /// Read a BlastDb format column.
    ///
    /// The BlastDb format column with the given base name and file id
    /// is opened.  The file_id character must be alphanumeric.
    ///
    /// @param basename Column filename (minus extension).
    /// @param file_id  Identifier for this column.
    CSeqDB_ColumnReader(const string & basename, char file_id = 'a');
    
    /// Destructor.
    ~CSeqDB_ColumnReader();
    
    /// Get the column title.
    ///@return The column title.
    const string & GetTitle() const;
    
    /// Get the column's key/value meta data.
    /// @return All key/value meta data stored here.
    const map<string,string> & GetMetaData();
    
    /// Look up one metadata value.
    /// @param key The key to look up.
    /// @return The value if found, or "" if not.
    const string & GetValue(const string & key);
    
    /// Get the number of rows stored in this column.
    /// @return The number of rows stored in this column.
    int GetNumOIDs() const;
    
    /// Fetch the data blob for the given oid.
    /// @param oid  The OID of the blob. [in]
    /// @param blob The data will be returned here. [out]
    void GetBlob(int oid, CBlastDbBlob & blob);
    
private:
    /// Prevent copy construction.
    CSeqDB_ColumnReader(const CSeqDB_ColumnReader&);
    
    /// Prevent copy assignment.
    CSeqDB_ColumnReader & operator= (CSeqDB_ColumnReader&);
    
    /// Implementation object.
    class CSeqDBColumn * m_Impl;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_BLAST_SEQDB_READER___COLUMN_READER__HPP

