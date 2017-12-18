#ifndef OBJTOOLS_BLAST_SEQDB_READER___SEQIDLIST_READER__HPP
#define OBJTOOLS_BLAST_SEQDB_READER___SEQIDLIST_READER__HPP

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
 * Author:  Amelia FOng
 *
 */

/// @file seqidlist_reader.hpp



#include <corelib/ncbistre.hpp>
#include <corelib/ncbifile.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>

BEGIN_NCBI_SCOPE


class NCBI_XOBJREAD_EXPORT CBlastSeqidlistFile
{

public:

	/// Get seqidlist from dbv5 seqidlist file
	/// @param 	 file 	   seqidlist file
	/// @param   idlist    list of ids read from file
	/// @param   list_info info about seqidlist
	/// @return  num of ids in the file, return 0 if list is in v4 format (plain text)
	static int GetSeqidlist(CMemoryFile & file, vector<CSeqDBGiList::SSiOid> & idlist, SBlastSeqIdListInfo & list_info);

	/// Get seqidlist info only from dbv5 seqidlist file
	/// @param 	 file 	   seqidlist filename (excpetion if file path cannot be resolved)
	/// @param   list_info info about seqidlist
	/// @return  num of ids in the file, return 0 if list is in v4 format (plain text)
	static int GetSeqidlistInfo(const string & filename, SBlastSeqIdListInfo & list_info);

	/// @param 	 file 	   seqidlist filename (excpetion if file path cannot be resolved)
	/// @param   os		   output stream
	static void PrintSeqidlistInfo(const string & filename, CNcbiOstream & os);
};

END_NCBI_SCOPE


#endif //OBJTOOLS_BLAST_SEQDB_READER___SEQIDLIST_READER__HPP
