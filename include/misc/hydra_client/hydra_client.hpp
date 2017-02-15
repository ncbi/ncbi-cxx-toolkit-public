#ifndef MISC_HYDRA_CLIENT___HYDRA_CLIENT__HPP
#define MISC_HYDRA_CLIENT___HYDRA_CLIENT__HPP

/*
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
* Author: Jonathan Kans, Aaron Ucko
*
* ===========================================================================
*/

/// @file hydra_client.hpp
/// API (CHydraSearch) for citation lookup.

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////

class CHydraSearch
{
public:
    // Group 1 PUBMED and Group 2 PMC have different training sets.
    // In Group 2 PMC, matches in the abstract text are ignored.
    enum class ESearch {
        // Group 1 PUBMED 
        ePUBMED_TOP_20,  // up to 20 results with any score (original default)
        ePUBMED,         // up to 15 results with score above 0.5

        // Group 2 PMC 
        eCITATION,       // only the top result in case its score is > 0.5, otherwise no results
        ePMC,            // up to 6 results with score above 0.5
        ePMC_TOP_6       // maximum of 6 results, any positive score
    };

    // Do not report matches with a score below specified threshold
    enum class EScoreCutoff {
        eCutoff_Low,      // 0.80 or higher (original default)
        eCutoff_Medium,   // 0.90 or higher
        eCutoff_High,     // 0.95 or higher
        eCutoff_VeryHigh  // 0.99 or higher
    };

    bool DoHydraSearch(const string& query, vector<int>& uids,
                       ESearch search = ESearch::ePUBMED_TOP_20,
                       EScoreCutoff cutoff = EScoreCutoff::eCutoff_Low);
};


END_NCBI_SCOPE


#endif /* MISC_HYDRA_CLIENT___HYDRA_CLIENT__HPP */
