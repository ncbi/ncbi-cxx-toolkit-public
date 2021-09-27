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
* Author:  Justin Foley
*
* File Description:
*   Unit tests for CSubSource class
*
*/

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(IsPlasmidNameValid) 
{
    // See RW-1436 
    list<pair<string,string>> plasmidTaxnamePairs = 
    {
        {"pTB913","Plasmid pTB913"},
        {"PDS075","Plasmid PDS075"},
        {"NR79","Plasmid NR79"},
        {"R387", "Plasmid R387"},
        {"pFA6", "Plasmid pFA6"},
        {"pWP113a", "Plasmid pWP113a"},
        {"pWW100","Plasmid pWW100"},
        {"R46","Plasmid R46"},
        {"pJHC-MW1","Plasmid pJHC-MW1"},
        {"pIE1120", "IncQ plasmid pIE1120"},
        {"Plasmid F", "Plasmid F"},
        {"Plasmid R", "Plasmid R"},
        {"Plasmid pIP630","Plasmid pIP630"},
        {"Plasmid pNG2","Plasmid pNG2"},     
        {"Plasmid pGT633","Plasmid pGT633"},   
        {"Plasmid pE5","Plasmid pE5"},      
        {"Plasmid pIP1527","Plasmid pIP1527"},  
        {"Plasmid pAM77","Plasmid pAM77"},
        {"Plasmid pAZ1","Plasmid pAZ1"},     
        {"Plasmid RP4","Plasmid RP4"}     
    };

    for (const auto& entry : plasmidTaxnamePairs) {
        auto success = CSubSource::IsPlasmidNameValid(entry.first,entry.second);
        BOOST_CHECK(success);
    }
}

