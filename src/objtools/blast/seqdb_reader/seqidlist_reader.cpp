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
#include <objtools/blast/seqdb_reader/seqidlist_reader.hpp>
#include <objects/seqloc/Seq_id.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CSeqidlistRead
{
public:
	CSeqidlistRead (CMemoryFile & file);

	void GetListInfo(SBlastSeqIdListInfo & info){ info = m_info;};
	int GetIds(vector<CSeqDBGiList::SSiOid>  & idlist);

private:
	inline Uint8 x_GetUint8() { Uint8 rv= *((Uint8 *) m_Ptr); m_Ptr +=8; return rv;}
	inline Uint4 x_GetUint4() { Uint4 rv= *((Uint4 *) m_Ptr); m_Ptr +=4; return rv;}
	inline char x_GetChar() {char rv = *m_Ptr; m_Ptr++; return rv;}
	inline void x_GetString(string & rv, Uint4 len) {rv.assign (m_Ptr, len); m_Ptr+= len;}

	char * m_Ptr;
	char * m_EndPtr;
	SBlastSeqIdListInfo m_info;
};

CSeqidlistRead::CSeqidlistRead (CMemoryFile & file) : m_Ptr((char*) file.GetPtr()), m_EndPtr((char*) file.GetPtr()) {
	if(m_Ptr == NULL) {
		NCBI_THROW(CSeqDBException, eArgErr, "Failed to map seqidlist file ");
	}

	char null_byte = x_GetChar();
	if (null_byte == 0) {
		m_info.is_v4 = false;
		Uint8 file_size = file.GetFileSize();
		m_info.file_size = x_GetUint8();
		if (m_info.file_size != file_size) {
			NCBI_THROW(CSeqDBException, eArgErr, "Invalid seqidlist file");
		}
		m_EndPtr += file_size;
		m_info.num_ids = x_GetUint8();
		Uint4 title_length = x_GetUint4();
		x_GetString(m_info.title, title_length);
		char file_create_date_length = x_GetChar();
		x_GetString(m_info.create_date, file_create_date_length);
		m_info.db_vol_length = x_GetUint8();
		if(m_info.db_vol_length != 0) {
			char file_db_create_date_length = x_GetChar();
			x_GetString(m_info.db_create_date, file_db_create_date_length);
			Uint4 file_vol_names_length = x_GetUint4();
			x_GetString(m_info.db_vol_names, file_vol_names_length);
		}
	}
}

int CSeqidlistRead::GetIds(vector<CSeqDBGiList::SSiOid>  & idlist)
{
	const unsigned char byte_max = 0xFF;
	unsigned int i = 0;
	idlist.clear();
	idlist.resize(m_info.num_ids);
	for(; (m_Ptr < m_EndPtr) && (i < m_info.num_ids); i++) {
		unsigned char id_len = (unsigned char) x_GetChar();
		if(id_len == byte_max) {
			Uint4 long_id_len = x_GetUint4();
			x_GetString(idlist[i].si, long_id_len);
		}
		else {
			x_GetString(idlist[i].si, id_len);
		}
	}
	if(i != m_info.num_ids) {
		NCBI_THROW(CSeqDBException, eArgErr, "Invalid total num of ids in seqidlist file");
	}

	return i;
}


int CBlastSeqidlistFile::GetSeqidlist(CMemoryFile & file, vector<CSeqDBGiList::SSiOid>  & idlist,
		                              SBlastSeqIdListInfo & list_info)
{

	CSeqidlistRead list(file);
	list.GetListInfo(list_info);
	list.GetIds(idlist);

	return list_info.num_ids;
}

int CBlastSeqidlistFile::GetSeqidlistInfo(const string & filename, SBlastSeqIdListInfo & list_info)
{
	string file = SeqDB_ResolveDbPath(filename);
	CMemoryFile in(file);
	CSeqidlistRead list(in);
	list.GetListInfo(list_info);
	return list_info.num_ids;

}

void CBlastSeqidlistFile::PrintSeqidlistInfo(const string & filename, CNcbiOstream & os)
{
	SBlastSeqIdListInfo list_info;
	if (CBlastSeqidlistFile::GetSeqidlistInfo(filename, list_info) > 0) {
		os <<"Num of Ids: " <<  list_info.num_ids << "\n";
		os <<"Title: " << list_info.title << "\n";
		os <<"Create Date: " << list_info.create_date << "\n";
		if(list_info.db_vol_length > 0) {
			os << "DB Info: \n";
			os << "\t" << "Total Vol Length: " << list_info.db_vol_length  << "\n";
			os << "\t" << "DB Create Date: " << list_info.db_create_date  << "\n";
			os << "\t" << "DB Vols: ";
			vector<string> vols;
			NStr::Split(list_info.db_vol_names, " ", vols);
			for(unsigned int i=0; i < vols.size(); i ++ ) {
				os << "\n\t\t" << vols[i];
			}
		}
	}
	else {
		os << "Seqidlist file is not in blast db version 5 format";
	}
	os << endl;
}

END_NCBI_SCOPE
