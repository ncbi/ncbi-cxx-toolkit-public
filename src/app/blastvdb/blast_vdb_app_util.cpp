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
 * Author: Amelia Fong
 *
 */

/** @file blast_vdb_app_util.cpp
 *  Utility functions for BLAST VDB command line applications
 */


#include <ncbi_pch.hpp>
#include "blast_vdb_app_util.hpp"

#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <algo/blast/vdb/vdb2blast_util.hpp>
#include <algo/blast/vdb/vdbalias.hpp>

//#include <algo/blast/blastinput/blast_scope_src.hpp>
//#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(blast);

void SortAndFetchSeqData(const blast::CSearchResultSet& results, CRef<CScope> scope, CScope::TBioseqHandles & handles)
{
	static const CSeq_align::TDim kSubjRow = 1;
    _ASSERT(scope.NotEmpty());
    if (results.size() == 0) {
        return;
    }

   	std::set<CSeq_id_Handle> seqids;
    ITERATE(blast::CSearchResultSet, result, results) {
        if ( !(*result)->HasAlignments() ) {
            continue;
        }
        ITERATE(CSeq_align_set::Tdata, aln, (*result)->GetSeqAlign()->Get()) {
            seqids.insert(CSeq_id_Handle::GetHandle((*aln)->GetSeq_id(kSubjRow)));
        }
    }

    std::vector<CSeq_id_Handle> sorted_ids (seqids.begin(), seqids.end());

    handles = scope->GetBioseqHandles(sorted_ids);
}

static void s_RegisterLocalDataLoader(list<string> & search_list, set<string> & local_paths,
									  CRef<CObjectManager> & om, vector<string> & dl_local)
{
	for(set<string>::iterator s_itr=local_paths.begin(); s_itr != local_paths.end(); ++s_itr) {
		vector<string>  sra_files;
		vector<string>  wgs_files;
		list<string>::iterator itr=search_list.begin();
		while(itr != search_list.end()) {
			CFile f(*itr);
			if(f.GetDir() == *s_itr) {
				string fname = f.GetName();
				if (CVDBBlastUtil::IsSRA(fname)) {
				    sra_files.push_back(f.GetName());
				}
				else {
				    wgs_files.push_back(f.GetName());
				}
				itr = search_list.erase(itr);
				continue;
			}
			 ++itr;
		}
		if(sra_files.size()) {
		    dl_local.push_back(CCSRADataLoader::RegisterInObjectManager(*om, *s_itr, sra_files,
		    		                                                    CObjectManager::eDefault).GetLoader()->GetName());
		}
		if(wgs_files.size()) {
		    dl_local.push_back(CWGSDataLoader::RegisterInObjectManager(*om, *s_itr, wgs_files,
		    		                                                   CObjectManager::eDefault).GetLoader()->GetName());
		}
	}
}

CRef<CScope> GetVDBScope(string dbAllNames)
{
	if(dbAllNames == kEmptyStr) {
		NCBI_THROW(CException, eInvalid, "Empty DBs\n");
	}

	list<string> search_list;
	NStr::Split(dbAllNames, " ", search_list, NStr::fSplit_Tokenize);

	vector<string> db_names;
	set<string> local_paths;
	list<string>::iterator sp=search_list.begin();
	 while(sp != search_list.end()){
		CDirEntry local(*sp);
		if(local.Exists()) {
			local_paths.insert(local.GetDir());
			++sp;
		}
		else {
			db_names.push_back(*sp);
			sp = search_list.erase(sp);
		}
	}

	CRef<CObjectManager> om = CObjectManager::GetInstance();
	CObjectManager::TRegisteredNames names;
	om->GetRegisteredNames(names);
	ITERATE ( CObjectManager::TRegisteredNames, it, names ) {
		om->RevokeDataLoader(*it);
	}

	vector<string> dl_local;
	if(search_list.size() > 0) {
		s_RegisterLocalDataLoader(search_list, local_paths, om, dl_local);
	}

	unsigned int num_sra = 0;
	for(unsigned int i=0; i < db_names.size(); i++) {
		if(CVDBBlastUtil::IsSRA(db_names[i]))
			num_sra ++;
	}
    string wgs(kEmptyStr);
    string gb(kEmptyStr);
    string sra(kEmptyStr);
    if(num_sra < db_names.size()) {
    	wgs = CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault).GetLoader()->GetName();
    }
    if(num_sra > 0) {
    	sra = CCSRADataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault).GetLoader()->GetName();
    }

    CRef<CScope> scope(new CScope(*om));

    for(unsigned int i=0; i < dl_local.size(); i++) {
    	scope->AddDataLoader(dl_local[i], CObjectManager::kPriority_Local);
    }

    if( sra != kEmptyStr) {
    	scope->AddDataLoader(sra,CObjectManager::kPriority_Replace);
    }

    if(wgs != kEmptyStr) {
    	scope->AddDataLoader(wgs, CObjectManager::kPriority_Replace);
    }

   	scope->AddDataLoader(CGBDataLoader::RegisterInObjectManager(*om, 0, CObjectManager::eNonDefault).GetLoader()->GetName(),
   			                                                    CObjectManager::kPriority_Loader);

    return scope;
}

END_NCBI_SCOPE
