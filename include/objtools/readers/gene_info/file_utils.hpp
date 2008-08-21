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
 * Author:  Vahram Avagyan
 *
 */

/// @file file_utils.hpp
/// General file processing routines and structures.
///
/// Defines a class combining various general file processing routines
/// and structures necessary for reading and writing binary files used
/// for Gene information retrieval.

#ifndef OBJTOOLS_READERS_GENE_INFO___FILE_UTILS__HPP
#define OBJTOOLS_READERS_GENE_INFO___FILE_UTILS__HPP

//==========================================================================//

#include <objtools/readers/gene_info/gene_info.hpp>

#include <corelib/ncbifile.hpp>

BEGIN_NCBI_SCOPE


//==========================================================================//

/// CGeneFileUtils
///
/// Class that combines Gene info file processing routines and structures.
///
/// The class combines various general file processing routines
/// and structures necessary for reading and writing binary files used
/// for Gene information retrieval. These include opening, reading and
/// writing uniformly structured binary files (e.g. consisting of integer
/// tuples), as well as providing a uniform storage/retrieval mechanism for
/// Gene info objects that is independent of any text formatting issues.

class NCBI_XOBJREAD_EXPORT CGeneFileUtils
{
public:
    /// STwoIntRecord - a pair of integers.
    ///
    /// Structure to read/write to binary files
    /// such as Gi to Offset, Gi to Gene ID, etc.
    struct STwoIntRecord
    {
        /// First integer field of the record.
        int n1;

        /// Second integer field of the record.
        int n2;
    };

    /// SMultiIntRecord - an n-tuple of integers.
    ///
    /// Structure to read/write to binary files
    /// such as Gene ID to Gi (storing fixed n integers per record).
    template <int k_nFields>
    struct SMultiIntRecord
    {
        /// Array of integer fields of the record.
        int n[k_nFields];
    };

public:
    /// Check if a directory exists, given its name.
    static bool CheckDirExistence(const string& strDir);

    /// Check if a file exists, given its name.
    static bool CheckExistence(const string& strFile);

    /// Get the length of a file, given its name.
    static Int8 GetLength(const string& strFile);

    /// Open the given text file for reading.
    static bool OpenTextInputFile(const string& strFileName,
                                  CNcbiIfstream& in);

    /// Open the given binary file for reading.
    static bool OpenBinaryInputFile(const string& strFileName,
                                    CNcbiIfstream& in);

    /// Open the given text file for writing.
    static bool OpenTextOutputFile(const string& strFileName,
                                   CNcbiOfstream& out);

    /// Open the given binary file for writing.
    static bool OpenBinaryOutputFile(const string& strFileName,
                                     CNcbiOfstream& out);

    /// Write a pair of integers to the file.
    static void WriteRecord(CNcbiOfstream& out,
                            STwoIntRecord& record);

    /// Read a pair of integers from the file.
    static void ReadRecord(CNcbiIfstream& in,
                           STwoIntRecord& record);

    /// Write an n-tuple of integers to the file.
    template <int k_nFields>
    static void WriteRecord(CNcbiOfstream& out,
                            SMultiIntRecord<k_nFields>& record);

    /// Read an n-tuple of integers from the file.
    template <int k_nFields>
    static void ReadRecord(CNcbiIfstream& in,
                            SMultiIntRecord<k_nFields>& record);

    /// Write a Gene info object to the file.
    ///
    /// Writes the Gene info object to a binary file, in a format
    /// that is independent of Gene info text formatting. Updates
    /// the current offset variable to point to the end of the
    /// written record.
    static void WriteGeneInfo(CNcbiOfstream& out,
                              CRef<CGeneInfo> info,
                              int& nCurrentOffset);

    /// Read a Gene info object from the file.
    ///
    /// Reads a Gene info object from a binary file, assuming
    /// it was written using WriteGeneInfo. The object is read
    /// from the location pointed at by the offset variable.
    static void ReadGeneInfo(CNcbiIfstream& in,
                             int nOffset,
                             CRef<CGeneInfo>& info);
};

//==========================================================================//

inline void CGeneFileUtils::
    WriteRecord(CNcbiOfstream& out,
                STwoIntRecord& record)
{
    out.write((char*)(&record.n1), sizeof(int));
    out.write((char*)(&record.n2), sizeof(int));
}

inline void CGeneFileUtils::
    ReadRecord(CNcbiIfstream& in,
               STwoIntRecord& record)
{
    in.read((char*)(&record.n1), sizeof(int));
    in.read((char*)(&record.n2), sizeof(int));
}

template <int k_nFields>
inline void CGeneFileUtils::
    WriteRecord(CNcbiOfstream& out,
                SMultiIntRecord<k_nFields>& record)
{
    for (int iField = 0; iField < k_nFields; iField++)
        out.write((char*)(&record.n[iField]), sizeof(int));
}

template <int k_nFields>
inline void CGeneFileUtils::
    ReadRecord(CNcbiIfstream& in,
               SMultiIntRecord<k_nFields>& record)
{
    for (int iField = 0; iField < k_nFields; iField++)
        in.read((char*)(&record.n[iField]), sizeof(int));
}

//==========================================================================//

END_NCBI_SCOPE

#endif

