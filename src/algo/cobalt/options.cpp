static char const rcsid[] = "$Id$";

/*
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

File name: options.cpp

Author: Greg Boratyn

Contents: Implementation of options for CMultiAligner

******************************************************************************/

/// @file options.cpp
/// Implementation of the CMultiAlignerOptions class

#include <ncbi_pch.hpp>
#include <algo/blast/core/blast_options.h>
#include <algo/cobalt/exception.hpp>
#include <algo/cobalt/options.hpp>
#include <algo/cobalt/patterns.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)


const CMultiAlignerOptions::TMode CMultiAlignerOptions::kQClustersModeMask
= (CMultiAlignerOptions::fNoQueryClusters 
   | CMultiAlignerOptions::fConservativeQueryClusters 
   | CMultiAlignerOptions::fMediumQueryClusters 
   | CMultiAlignerOptions::fLargeQueryClusters);

const int CMultiAlignerOptions::kDefaultGapOpen = BLAST_GAP_OPEN_PROT;
const int CMultiAlignerOptions::kDefaultGapExtend = BLAST_GAP_EXTN_PROT;

CMultiAlignerOptions::CMultiAlignerOptions(void)
{
    x_InitParams(CMultiAlignerOptions::fMediumQueryClusters
                 | CMultiAlignerOptions::fNoRpsBlast);
}

CMultiAlignerOptions::CMultiAlignerOptions(TMode mode)
{
    x_InitParams(mode);
}

CMultiAlignerOptions::CMultiAlignerOptions(const string& rps_db_name,
                                           TMode mode)
{
    x_InitParams(mode);
    m_RpsDb = rps_db_name;
}


void CMultiAlignerOptions::SetUserConstraints(
                               const vector<SConstraint>& constraints)
{
    m_UserHits.resize(constraints.size());
    copy(constraints.begin(), constraints.end(), m_UserHits.begin());
    m_Mode = fNonStandard;
}


void CMultiAlignerOptions::SetCddPatterns(const vector<char*>& patterns)
{
    m_Patterns.resize(patterns.size());
    copy(patterns.begin(), patterns.end(), m_Patterns.begin());

    m_Mode = fNonStandard;
}

void CMultiAlignerOptions::AddCddPatterns(const vector<char*>& patterns)
{
    size_t old_size = m_Patterns.size();
    m_Patterns.resize(m_Patterns.size() + patterns.size());
    for (size_t i=0; i < m_Patterns.size();i++) {
        m_Patterns[old_size + i] = patterns[i];
    }

    m_Mode = fNonStandard;
}

bool CMultiAlignerOptions::Validate(void)
{
    // Check whether m_UseQieryClusters and m_Mode are consistent
    if (((m_Mode & kQClustersModeMask) != fNoQueryClusters) 
        != m_UseQueryClusters && !(m_Mode & fNonStandard)) {

            NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                       "Conflicting use query clusters setting");
    }

    // Check query clustering params
    if (m_UseQueryClusters) {
        if (m_KmerAlphabet < TKMethods::eRegular
            || m_KmerAlphabet < TKMethods::eSE_V10) {

            NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                       "Invalid alphabet for query cludtering");
        }

        if (m_ClustDistMeasure != TKMethods::eFractionCommonKmersGlobal
            && m_ClustDistMeasure != TKMethods::eFractionCommonKmersLocal) {

            NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                       "Invalid distance measure for query clustering");
        }
    }

    // Check if data base name is specified if option selected
    if (!(m_Mode & fNoRpsBlast) && !(m_Mode & fNonStandard)
        && m_RpsDb.empty()) {

        NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                   "RPS BLAST database name not specified");
    }

    // Check if RPS e-value allowed if RPS BLAST used
    if (!m_RpsDb.empty() && m_RpsEvalue < 0.0) {
        NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                   "Incorrect RPS BLAST e-value");
    }

    // Check if Blastp e-value allowed
    if (m_BlastpEvalue < 0.0) {
        NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                   "Incorrect Blastp e-value");
    }

    // Check if patterns are specified if option selected
    if (!(m_Mode & fNoPatterns) && !(m_Mode & fNonStandard)
        && (m_Patterns.size() == 0)) {

        NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                   "CDD patterns not specified");
    }

    // Check if pseudocount value allowed if iterative alignment is selected
    if (!(m_Mode & fNoIterate) && !(m_Mode & fNonStandard)
        && m_Pseudocount < 0.0) {

        NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                   "Invalid pseudocount value");
    }

    // Check tree computation method
    if (m_TreeMethod != eNJ && m_TreeMethod != eFastME) {
        NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                   "Invalid tree computation method for progressive alignment");
    }

    return true;
}


void CMultiAlignerOptions::x_InitParams(TMode mode)
{
    if (mode & fNonStandard) {
        NCBI_THROW(CMultiAlignerException, eInvalidOptions,
                   "Invalid options mode value");
    }

    m_Mode = mode;

    // Query clusters
    m_UseQueryClusters = (mode & kQClustersModeMask) != 0;

    switch (mode & kQClustersModeMask) {
    case fConservativeQueryClusters :
        m_MaxInClusterDist = 0.6;
        break;

    case fMediumQueryClusters :
        m_MaxInClusterDist = 0.85;
        break;

    case fLargeQueryClusters :
        m_MaxInClusterDist = 0.95;
        break;

    // Do noting for eNoQueryClusters
    }
    m_KmerLength = 6;
    m_KmerAlphabet = TKMethods::eSE_B15;
    m_ClustDistMeasure = TKMethods::eFractionCommonKmersGlobal;

    // RPS Blast
    m_UsePreRpsHits = true;
    m_RpsEvalue = 0.01;
    m_DomainResFreqBoost = 0.5;

    // Blatp
    m_BlastpEvalue = 0.01;


    // Patterns
    if (!(mode & fNoPatterns)) {
        AssignDefaultPatterns(m_Patterns);
    }

    // Iterate
    m_Iterate = !(mode & fNoIterate);
    m_ConservedCutoff = 0.67;
    m_Pseudocount = 2.0;

    m_TreeMethod = eNJ;
    m_LocalResFreqBoost = 1.0;

    m_MatrixName = "BLOSUM62";
    m_EndGapOpen = kDefaultGapOpen;
    m_EndGapExtend = kDefaultGapExtend;
    m_GapOpen = kDefaultGapOpen;
    m_GapExtend = kDefaultGapExtend;
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
