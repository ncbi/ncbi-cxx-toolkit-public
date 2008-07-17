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

/** @file uniform_search.cpp
 * Implementation of the uniform BLAST search interface auxiliary classes
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CSearchDatabase::CSearchDatabase(const string& dbname, EMoleculeType mol_type)
    : m_DbName(dbname), m_MolType(mol_type)
{}

CSearchDatabase::CSearchDatabase(const string& dbname, EMoleculeType mol_type,
               const string& entrez_query)
    : m_DbName(dbname), m_MolType(mol_type),
      m_EntrezQueryLimitation(entrez_query)
{}

CSearchDatabase::CSearchDatabase(const string& dbname, EMoleculeType mol_type,
               const TGiList& gilist)
    : m_DbName(dbname), m_MolType(mol_type),
      m_GiListLimitation(gilist) 
{}

CSearchDatabase::CSearchDatabase(const string& dbname, EMoleculeType mol_type,
               const string& entrez_query, const TGiList& gilist)
    : m_DbName(dbname), m_MolType(mol_type),
      m_EntrezQueryLimitation(entrez_query), m_GiListLimitation(gilist)
{}

void 
CSearchDatabase::SetDatabaseName(const string& dbname) 
{ 
    m_DbName = dbname; 
}

string 
CSearchDatabase::GetDatabaseName() const 
{
    return m_DbName; 
}

void 
CSearchDatabase::SetMoleculeType(EMoleculeType mol_type)
{ 
    m_MolType = mol_type; 
}

CSearchDatabase::EMoleculeType 
CSearchDatabase::GetMoleculeType() const 
{ 
    return m_MolType; 
}

void 
CSearchDatabase::SetEntrezQueryLimitation(const string& entrez_query) 
{
    m_EntrezQueryLimitation = entrez_query;
}

string 
CSearchDatabase::GetEntrezQueryLimitation() const 
{ 
    return m_EntrezQueryLimitation; 
}

void 
CSearchDatabase::SetGiListLimitation(const TGiList& gilist) 
{
    if ( !m_NegativeGiListLimitation.empty() ) {
        NCBI_THROW(CBlastException, eInvalidArgument,
               "Cannot have a negative and a regular gi list simultaneously");
    }
    m_GiListLimitation = gilist; 
}

const CSearchDatabase::TGiList& 
CSearchDatabase::GetGiListLimitation() const 
{ 
    return m_GiListLimitation; 
}

void 
CSearchDatabase::SetFilteringAlgorithms(const TFilteringAlgorithms& flist) 
{
    m_FilteringAlgs = flist; 
}

const CSearchDatabase::TFilteringAlgorithms& 
CSearchDatabase::GetFilteringAlgorithms() const 
{ 
    return m_FilteringAlgs; 
}

void 
CSearchDatabase::SetNegativeGiListLimitation(const TGiList& gilist) 
{
    if ( !m_GiListLimitation.empty() ) {
        NCBI_THROW(CBlastException, eInvalidArgument,
               "Cannot have a regular and a negative gi list simultaneously");
    }
    m_NegativeGiListLimitation = gilist; 
}

const CSearchDatabase::TGiList& 
CSearchDatabase::GetNegativeGiListLimitation() const 
{ 
    return m_NegativeGiListLimitation; 
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
