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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Unit tests for SNP data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <sra/data_loaders/snp/snploader.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_table_ci.hpp>
#include <objects/seqtable/seqtable__.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

static string s_LoaderName;

static CRef<CScope> s_MakeScope(const string& file_name = kEmptyStr)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    if ( !s_LoaderName.empty() ) {
        om->RevokeDataLoader(s_LoaderName);
        s_LoaderName.erase();
    }
    if ( file_name.empty() ) {
        s_LoaderName =
            CSNPDataLoader::RegisterInObjectManager(*om)
            .GetLoader()->GetName();
    }
    else {
        s_LoaderName =
            CSNPDataLoader::RegisterInObjectManager(*om, file_name)
            .GetLoader()->GetName();
    }
    CRef<CScope> scope(new CScope(*om));
    scope->AddDataLoader(s_LoaderName);
    return scope;
}


BOOST_AUTO_TEST_CASE(FetchSeq1)
{
    CRef<CScope> scope =
        s_MakeScope("");
}
