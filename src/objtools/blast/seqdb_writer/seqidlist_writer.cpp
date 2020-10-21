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
* Author:  Amelia Fong
*
*/

#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_writer/seqidlist_writer.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbgeneral.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/*
 * This function creates v5 seqidlist file
 * File Format:
 * Header
 * ------------------------------------------------------------------------------------
 * char 			NULL byte
 * Uint8			File Size
 * Uint8			Total Num of Ids
 * Uint4			Title Length (in bytes)
 * char[]			Title
 * char				Seqidlist create date string size (in bytes)
 * char[]			Seqidlist Create Date
 * Uint8 			Totol db length (0 if seqid list not associated with a specific db)
 *
 * If seqidlist is associated with a particular db:-
 * char 			DB create date string size (in bytes)
 * char[]			DB create date
 * Uint4			DB vol names string size (in bytes)
 * char[]			DB vol names
 *
 * Data
 * ------------------------------------------------------------------------------------
 * char 			ID string size (in bytes) if size >= byte max then byte set to 0xFF
 * Uint4 			ID string size if id size >= byte max
 * char[]			ID string
 *
 */

int WriteBlastSeqidlistFile(const vector<string> & idlist, CNcbiOstream & os, const string & title, const CSeqDB * seqdb)
{
	const char null_byte = 0;
	Uint8 file_size = 0;
	Uint8 total_num_ids = 0;
	string create_date = kEmptyStr;
	char create_date_size = 0;
	Uint8 total_length = 0;
	string vol_names = kEmptyStr;
	Uint4 vol_names_size = 0;
	Uint4 title_size = title.size();
	string db_date = kEmptyStr;
	char db_date_size = 0;
	const unsigned char byte_max = 0xFF;

	vector<string> tmplist;
	tmplist.reserve(idlist.size());
	if(seqdb != NULL) {
		if (seqdb->GetBlastDbVersion() < eBDB_Version5) {
			NCBI_THROW(CSeqDBException, eArgErr, "Seqidlist requires v5 database ");
		}
		total_length = seqdb->GetVolumeLength();
		vector<string> vols;
		seqdb->FindVolumePaths(vols, true);
		ITERATE(vector<string>, itr, vols) {
			if(itr != vols.begin()) {
				vol_names += " ";
			}
			string v = kEmptyStr;
			CSeqDB_Path(*itr).FindBaseName().GetString(v);
			vol_names += v;
		}
		vol_names_size = vol_names.size();

		db_date = seqdb->GetDate();
		db_date_size = db_date.size();
	}


	for(unsigned int i=0; i < idlist.size(); i++) {
		try {
		CSeq_id seqid(idlist[i], CSeq_id::fParse_RawText | CSeq_id::fParse_AnyLocal | CSeq_id::fParse_PartialOK);
		if(seqid.IsGi()) {
		   	continue;
		}

		if(seqid.IsPir() || seqid.IsPrf()) {
		   	string id = seqid.AsFastaString();
		   	tmplist.push_back(id);
		   	continue;
		}
		tmplist.push_back(seqid.GetSeqIdString(true));
		} catch (CException & e) {
            ERR_POST(e.GetMsg());
			NCBI_THROW(CSeqDBException, eArgErr, "Invalid seq id: " + idlist[i]);

		}
	}

	if (tmplist.size() == 0) {
        ERR_POST("Empty seqid list");
		return -1;
	}
	sort(tmplist.begin(), tmplist.end());
	vector<string>::iterator it = unique (tmplist.begin(), tmplist.end());
	tmplist.resize(distance(tmplist.begin(),it));
	if(seqdb != NULL) {
		vector<blastdb::TOid> oids;
		vector<string>  check_ids(tmplist);
		seqdb->AccessionsToOids(check_ids, oids);
		tmplist.clear();
		for(unsigned int i=0; i < check_ids.size(); i++) {
			if(oids[i] != kSeqDBEntryNotFound){
				tmplist.push_back(check_ids[i]);
			}
		}
	}

	total_num_ids = tmplist.size();
	CTime now(CTime::eCurrent);
	create_date = now.AsString("b d, Y  H:m P");
	create_date_size = create_date.size();

	os.write(&null_byte, 1);
	os.write((char *)&file_size, 8);
	os.write((char *)&total_num_ids, 8);
	os.write((char *) &title_size, 4);
	os.write(title.c_str(), title_size);
	os.write(&create_date_size, 1);
	os.write(create_date.c_str(), create_date_size);
	os.write((char *)&total_length, 8);
	if(vol_names != kEmptyStr) {
		os.write(&db_date_size, 1);
		os.write(db_date.c_str(), db_date_size);
		os.write((char *) &vol_names_size, 4);
		os.write(vol_names.c_str(), vol_names_size);
	}

	for(unsigned int i=0; i < tmplist.size(); i++) {
		Uint4 id_len = tmplist[i].size();

		if(id_len >= byte_max) {
			os.write((char *)&byte_max, 1);
			os.write((char *)&id_len, 4);
		}
		else {
			char l = byte_max & id_len;
			os.write(&l,1);
		}
		os.write(tmplist[i].c_str(), id_len);
	}

	os.flush();
	file_size = (Uint8) os.tellp();
	os.seekp(1);
	os.write((char *)&file_size, 8);
	os.flush();

	return 0;

}

END_NCBI_SCOPE
