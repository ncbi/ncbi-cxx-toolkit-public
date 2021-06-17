#ifndef ALGO_COBALT___RESFREQ__HPP
#define ALGO_COBALT___RESFREQ__HPP

/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: resfreq.hpp

Author: Jason Papadopoulos

Contents: Interface for CProfileData

******************************************************************************/

#include <corelib/ncbifile.hpp>
#include <algo/cobalt/base.hpp>

/// @file resfreq.hpp
/// Interface for CProfileData

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Represent databases of PSSM data and residue frequencies
class NCBI_COBALT_EXPORT CProfileData
{
public:
    /// Which collection to retrieve
    typedef enum {
        eGetResFreqs,    ///< Retrieve residue frequencies
        eGetPssm         ///< Retrieve PSSMs
    } EMapChoice;

    /// Default constructor
    ///
    CProfileData() {}

    /// Destructor
    ///
    ~CProfileData() { Clear(); }

    /// Retrieve a list of offsets where database sequences begin.
    /// @return Pointer to list of offsets
    ///
    Int4 * GetSeqOffsets() const { return m_SeqOffsets; }

    /// Assuming the database is a list of PSSM columns,
    /// retrieve a list of all of the PSSMs in the database
    /// concatenated together. GetSeqOffsets() can be used to
    /// retrieve a list of the offset of the first PSSM column
    /// of each database sequence. The returned matrix is 
    /// (db length) x kAlphabetSize
    /// @return Pointer to PSSM data
    ///
    Int4 ** GetPssm() const { return m_PssmRows; }

    /// Assuming the database is a list of columns of profiles,
    /// frequencies, retrieve a list of all of the profiles
    /// in the database concatenated together. GetSeqOffsets() 
    /// can be used to retrieve a list of the offset of the 
    /// first profile column of each database sequence. The
    /// returned matrix is (db length) x kAlphabetSize
    /// @return Pointer to profile data
    ///
    double ** GetResFreqs() const { return m_ResFreqRows; }

    /// Load information from a given database
    /// @param choice Specifies whether PSSMs or profiles are loaded [in]
    /// @param dbname Base name of the database to load [in]
    /// @param resfreq_file If residue frequencies are desired, the
    ///                     name of the file that contains them
    ///
    void Load(EMapChoice choice, 
              string dbname,
              string resfreq_file = "");

    /// Free previously loaded PSSM or profile data
    ///
    void Clear();

private:
    CMemoryFile *m_ResFreqMmap; ///< Memory-mapped residue frequency data
    CMemoryFile *m_PssmMmap;    ///< Memory-mapped PSSM data
    Int4 *m_SeqOffsets;         ///< List of the first offset of each
                                ///  database sequence
    Int4 **m_PssmRows;          ///< List of pointers to PSSM columns
    double **m_ResFreqRows;     ///< List of pointers to profile columns
};

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif /* ALGO_COBALT___RESFREQ__HPP */
