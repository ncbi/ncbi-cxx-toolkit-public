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
    CBDB_FileDumper& operator=(const CBDB_FileDumper& fdump);


    void SetColumnSeparator(const string& col_separator);

    /// Convert BDB file into text file
    void Dump(const string& dump_file_name, CBDB_File& db);

    /// Convert BDB file into text and write it into the specified stream
    void Dump(CNcbiOstream& out, CBDB_File& db);

protected:
    string     m_ColumnSeparator;
}; 


inline
CBDB_FileDumper::CBDB_FileDumper(const string& col_separator)
: m_ColumnSeparator(col_separator)
{
}

inline
CBDB_FileDumper::CBDB_FileDumper(const CBDB_FileDumper& fdump)
: m_ColumnSeparator(fdump.m_ColumnSeparator)
{
}

inline
CBDB_FileDumper& CBDB_FileDumper::operator=(const CBDB_FileDumper& fdump)
{
    m_ColumnSeparator = fdump.m_ColumnSeparator;
    return *this;
}

inline
void CBDB_FileDumper::SetColumnSeparator(const string& col_separator)
{
    m_ColumnSeparator = col_separator;
}

/* @} */

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/10/27 14:27:07  kuznets
 * Minor compilation bug fixed.
 *
 * Revision 1.1  2003/10/27 14:17:57  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif
