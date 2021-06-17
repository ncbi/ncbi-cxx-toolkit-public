#ifndef BDB_FILEDUMP__HPP
#define BDB_FILEDUMP__HPP

/* $Id$
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
 * Author:  Anatoliy Kuznetsov
 *   
 * File Description: Dumper for BDB files into text format.
 *
 */

/// @file bdb_filedump.hpp
/// BDB File covertion into text.


BEGIN_NCBI_SCOPE

class CBDB_Query;

/** @addtogroup BDB_Files
 *
 * @{
 */

class CBDB_File;

/// Utility class to convert DBD files into text files. 
///

class NCBI_BDB_EXPORT CBDB_FileDumper
{
public:
    /// Constructor.
    /// @param col_separator
    ///    Column separator
    CBDB_FileDumper(const string& col_separator = "\t");
    CBDB_FileDumper(const CBDB_FileDumper& fdump);
    ~CBDB_FileDumper();
    
    CBDB_FileDumper& operator=(const CBDB_FileDumper& fdump);


    void SetColumnSeparator(const string& col_separator);

    enum EPrintFieldNames
    {
        ePrintNames,
        eDropNames
    };

    /// Control field names printing
    void SetColumnNames(EPrintFieldNames print_names);

    /// Field value formatting controls
    /// @sa SetValueFormatting
    ///
    enum EValueFormatting
    {
        eNoQuotes,
        eQuoteStrings,
        eQuoteAll
    };
        
    void SetValueFormatting(EValueFormatting vf);
    
    /// BLOB value formatting controls
    /// @sa SetBlobFormatting
    ///
    enum EBlobFormatting
    {
        eBlobSummary = (1 << 0),
        eBlobAll     = (1 << 1),
        eBlobAsHex   = (1 << 2),
        eBlobAsTxt   = (1 << 3)
    };
        
    typedef unsigned int   TBlobFormat;
    
    void SetBlobFormat(TBlobFormat bf);   
    TBlobFormat GetBlobFormat() const { return m_BlobFormat; }
	
    /// Convert BDB file into text file
    void Dump(const string& dump_file_name, CBDB_File& db);

    /// Convert BDB file into text and write it into the specified stream
    void Dump(CNcbiOstream& out, CBDB_File& db);
    
    /// Dump BDB cursor to stream
    void Dump(CNcbiOstream& out, CBDB_FileCursor& cur);
    
    /// Return number of records processed by Dump
    unsigned GetRecordsDumped() const { return m_RecordsDumped; }
    
    /// Set query filter
    void SetQuery(const string& query_str);
    
    /// Set BLOB dump file name
    void SetBlobDumpFile(const string& fname) { m_BlobDumpFname = fname; }
    
    /// Set reference on output file 
    /// (mode when all dumped records are put into a separate database)
    /// Class does not take ownership on out_dbf
    void SetOutFile(CBDB_File* out_dbf) { m_OutFile = out_dbf; }
protected:
        
    void PrintHeader(CNcbiOstream& out,
                     const CBDB_BufferManager* key,
                     const CBDB_BufferManager* data);

private:
    void x_SetQuoteFlags(vector<unsigned>*         flags, 
                         const CBDB_BufferManager& bman);

    void x_DumpFields(CNcbiOstream&             out, 
                      const CBDB_BufferManager& bman,
                      const vector<unsigned>&   quote_flags, 
                      bool                      is_key);
        
protected:
    string               m_ColumnSeparator;
    string               m_BlobDumpFname;
    EPrintFieldNames     m_PrintNames;
    EValueFormatting     m_ValueFormatting;
    TBlobFormat          m_BlobFormat;
    unsigned int         m_RecordsDumped;
    
    string               m_QueryStr;
    CBDB_Query*          m_Query;
    
    CBDB_File*           m_OutFile;
}; 

/* @} */


inline
void CBDB_FileDumper::SetColumnSeparator(const string& col_separator)
{
    m_ColumnSeparator = col_separator;
}

inline
void CBDB_FileDumper::SetColumnNames(EPrintFieldNames print_names)
{
    m_PrintNames = print_names;
}

inline
void CBDB_FileDumper::SetValueFormatting(EValueFormatting vf)
{
    m_ValueFormatting = vf;
}

inline
void CBDB_FileDumper::SetBlobFormat(TBlobFormat bf)
{
    m_BlobFormat = bf;
}

END_NCBI_SCOPE

#endif
