#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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

/** @file subject_psi_database.cpp
 * Defines an implementation of the IPsiBlastSubject interface which uses
 * CSeqDB
 */

#include <ncbi_pch.hpp>
#include "subject_psi_database.hpp"
#include <algo/blast/api/seqsrc_seqdb.hpp>
#include <algo/blast/api/seqinfosrc_seqdb.hpp>
#include <algo/blast/api/blast_exception.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CBlastSubjectDb::CBlastSubjectDb(const CSearchDatabase& dbinfo)
: m_DbInfo(dbinfo), m_DbInfoProvided(true), m_DbHandle(0)
{}

CBlastSubjectDb::CBlastSubjectDb(CRef<CSeqDB> seqdb)
: m_DbInfo("dummy db", CSearchDatabase::eBlastDbIsProtein),
  m_DbInfoProvided(false), m_DbHandle(seqdb)
{}

void
CBlastSubjectDb::Validate()
{
    if (m_DbInfoProvided) {
        string db_name = m_DbInfo.GetDatabaseName();
        bool is_prot = x_DetermineMoleculeType();

        try {
            CSeqDB::ESeqType mol_type = is_prot 
                ? CSeqDB::eProtein 
                : CSeqDB::eNucleotide;
            vector<string> paths;
            CSeqDB::FindVolumePaths(db_name, mol_type, paths);
        } catch (const CSeqDBException& e) {
            string msg("Could not find ");
            msg += is_prot ? "protein" : "nucleotide";
            msg += " database '" + db_name + "'";
            NCBI_THROW(CBlastException, eInvalidArgument, msg);
        }
    } else {
        if (m_DbHandle.Empty()) {
            NCBI_THROW(CBlastException, eInvalidArgument, 
                       "Empty CSeqDB database");
        }
    }
}

bool
CBlastSubjectDb::x_DetermineMoleculeType()
{
    bool retval = false;
    if (m_DbInfoProvided) {
        if  (m_DbInfo.GetMoleculeType() == CSearchDatabase::eBlastDbIsProtein)
            retval = true;
        else
            retval = false;
    } else {
        retval = (m_DbHandle->GetSequenceType() == CSeqDB::eProtein)
            ? true : false;
    }
    return retval;
}

void
CBlastSubjectDb::x_MakeSeqSrc()
{
    if (m_DbInfoProvided) {
        m_SeqSrc = SeqDbBlastSeqSrcInit(m_DbInfo.GetDatabaseName(), 
                                        x_DetermineMoleculeType());
        m_OwnSeqSrc = true;
    } else {
        m_SeqSrc = SeqDbBlastSeqSrcInit(m_DbHandle);
        m_OwnSeqSrc = false;
    }
}

void
CBlastSubjectDb::x_MakeSeqInfoSrc()
{
    if (m_DbInfoProvided) {
        m_SeqInfoSrc = new CSeqDbSeqInfoSrc(m_DbInfo.GetDatabaseName(),
                                            x_DetermineMoleculeType());
    } else {
        m_SeqInfoSrc = new CSeqDbSeqInfoSrc(m_DbHandle);
    }
    m_OwnSeqInfoSrc = true;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

