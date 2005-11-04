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
 * Author:  Christiam Camacho
 *
 */

/// @file subject_psi_database.hpp
/// Declares an implementation of the IPsiBlastSubject interface which uses
/// CSeqDB

#ifndef ALGO_BLAST_API___SUBJECT_PSI_DATABASE__HPP
#define ALGO_BLAST_API___SUBJECT_PSI_DATABASE__HPP

#include "psiblast_subject.hpp"
#include <objtools/readers/seqdb/seqdb.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Interface to obtain subject data for PSI-BLAST database search.
class NCBI_XBLAST_EXPORT CBlastSubjectDb : public IPsiBlastSubject
{
public:
    /// Constructor
    /// @param dbinfo database description
    CBlastSubjectDb(const CSearchDatabase& dbinfo);

    /// Constructor
    /// @param seqdb BLAST database already initialized as a CSeqDB object
    CBlastSubjectDb(CRef<CSeqDB> seqdb);

    /// Verifies that the database exists or that the CSeqDB object is not NULL
    virtual void Validate();

protected:
    /// Creates a BlastSeqSrc from the input data
    virtual void x_MakeSeqSrc();

    /// Creates a IBlastSeqInfoSrc from the input data
    virtual void x_MakeSeqInfoSrc();

private:
    /// Copy of the database description
    const CSearchDatabase m_DbInfo;
    /// True if the resource above was provided
    bool m_DbInfoProvided;

    /// BLAST database
    CRef<CSeqDB> m_DbHandle;

    /// Returns true if the database is protein, otherwise returns false
    bool x_DetermineMoleculeType();

    /// Prohibit copy constructor
    CBlastSubjectDb(const CBlastSubjectDb& rhs);

    /// Prohibit assignment operator
    CBlastSubjectDb& operator=(const CBlastSubjectDb& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___SUBJECT_DATABASE__HPP */
