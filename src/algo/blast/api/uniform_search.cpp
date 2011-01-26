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
    : m_DbName(dbname), m_MolType(mol_type), m_GiListSet(false),
      m_FilteringAlgorithmId(-1), m_MaskType(DB_MASK_NONE),
      m_NeedsFilteringTranslation(false), m_DbInitialized(false)
{}

CSearchDatabase::CSearchDatabase(const string& dbname, EMoleculeType mol_type,
               const string& entrez_query)
    : m_DbName(dbname), m_MolType(mol_type),
      m_EntrezQueryLimitation(entrez_query), m_GiListSet(false),
      m_FilteringAlgorithmId(-1), m_MaskType(DB_MASK_NONE),
      m_NeedsFilteringTranslation(false), m_DbInitialized(false)
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
CSearchDatabase::SetGiList(CSeqDBGiList * gilist) 
{
    if (m_GiListSet) NCBI_THROW(CBlastException, eInvalidArgument,
          "Cannot have more than one type of id list filtering.");
    m_GiListSet = true;
    m_GiList.Reset(gilist); 
}

const CRef<CSeqDBGiList>& 
CSearchDatabase::GetGiList() const
{
    return m_GiList;
}

const CSearchDatabase::TGiList 
CSearchDatabase::GetGiListLimitation() const 
{ 
    CSearchDatabase::TGiList retval;
    if (!m_GiList.Empty() && !m_GiList->Empty()) {
        m_GiList->GetGiList(retval);
    }
    return retval;
}

void 
CSearchDatabase::SetNegativeGiList(CSeqDBGiList * gilist) 
{
    if (m_GiListSet) NCBI_THROW(CBlastException, eInvalidArgument,
          "Cannot have more than one type of id list filtering.");
    m_GiListSet = true;
    m_NegativeGiList.Reset(gilist); 
}

const CRef<CSeqDBGiList>& 
CSearchDatabase::GetNegativeGiList() const
{ 
    return m_NegativeGiList; 
}

const CSearchDatabase::TGiList 
CSearchDatabase::GetNegativeGiListLimitation() const 
{ 
    CSearchDatabase::TGiList retval;
    if (!m_NegativeGiList.Empty() && !m_NegativeGiList->Empty()) {
        m_NegativeGiList->GetGiList(retval);
    }
    return retval;
}

void 
CSearchDatabase::SetFilteringAlgorithm(const string &filt_algorithm, int mask_type)
{
    m_FilteringAlgorithmId = NStr::StringToNumeric(filt_algorithm);
    m_MaskType = mask_type;
    if (m_FilteringAlgorithmId < 0) {
        // This is a string id, must translate to numeric id first
        m_FilteringAlgorithmString = filt_algorithm;
        m_NeedsFilteringTranslation = true;
    }
    x_ValidateMaskingAlgorithm();
}

void 
CSearchDatabase::SetFilteringAlgorithm(int filt_algorithm_id)
{
    SetFilteringAlgorithm(filt_algorithm_id, DB_MASK_SOFT);
}

void 
CSearchDatabase::SetFilteringAlgorithm(int filt_algorithm_id, int mask_type)
{
    m_FilteringAlgorithmId = filt_algorithm_id;
    m_MaskType = mask_type;
    m_NeedsFilteringTranslation = false;
    x_ValidateMaskingAlgorithm();
}

int 
CSearchDatabase::GetFilteringAlgorithm() const
{
    if (m_NeedsFilteringTranslation) {
        x_TranslateFilteringAlgorithm();
    } 
    return m_FilteringAlgorithmId;
}

int
CSearchDatabase::GetMaskType() const
{
    return m_MaskType;
}

void
CSearchDatabase::x_TranslateFilteringAlgorithm() const
{
    if (!m_DbInitialized) {
        x_InitializeDb();
    }
    m_FilteringAlgorithmId = 
        m_SeqDb->GetMaskAlgorithmId(m_FilteringAlgorithmString);
    m_NeedsFilteringTranslation = false;
}

void
CSearchDatabase::SetSeqDb(CRef<CSeqDB> seqdb) 
{
    m_SeqDb.Reset(seqdb);
    m_DbInitialized = true;
}

CRef<CSeqDB>
CSearchDatabase::GetSeqDb() const
{
    if (!m_DbInitialized) {
        x_InitializeDb();
    }
    return m_SeqDb;
}

void 
CSearchDatabase::x_InitializeDb() const
{
    const CSeqDB::ESeqType seq_type = IsProtein() ? CSeqDB::eProtein : CSeqDB::eNucleotide;
    if (! m_GiList.Empty() && ! m_GiList->Empty()) {
        m_SeqDb.Reset(new CSeqDB(m_DbName, seq_type, m_GiList));

    } else if (! m_NegativeGiList.Empty() && ! m_NegativeGiList->Empty()) {
        vector<int> gis;
        m_NegativeGiList->GetGiList(gis);
        CSeqDBIdSet idset(gis, CSeqDBIdSet::eGi, false);
        m_SeqDb.Reset(new CSeqDB(m_DbName, seq_type, idset));

    } else {
        m_SeqDb.Reset(new CSeqDB(m_DbName, seq_type));

    }
      
    x_ValidateMaskingAlgorithm();
    _ASSERT(m_SeqDb.NotEmpty());
    m_DbInitialized = true;
}

void 
CSearchDatabase::x_ValidateMaskingAlgorithm() const
{
    if (m_FilteringAlgorithmId <= 0 || m_SeqDb.Empty()) {
        return;
    }

    vector<int> supported_algorithms;
    m_SeqDb->GetAvailableMaskAlgorithms(supported_algorithms);
    if (find(supported_algorithms.begin(),
             supported_algorithms.end(),
             m_FilteringAlgorithmId) == supported_algorithms.end()) {
        CNcbiOstrstream oss;
        oss << "Masking algorithm ID " << m_FilteringAlgorithmId << " is "
            << "not supported in " << 
            (IsProtein() ? "protein" : "nucleotide") << " '" 
            << GetDatabaseName() << "' BLAST database";
        string msg = CNcbiOstrstreamToString(oss);
        NCBI_THROW(CBlastException, eInvalidOptions, msg);
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
