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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#ifndef WGS_ASN_H
#define WGS_ASN_H

#include <objects/submit/Seq_submit.hpp>

#include "wgs_seqentryinfo.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace wgsparse
{

bool FixSeqSubmit(CRef<CSeq_submit>& seq_submit, int& accession_ver, bool first, bool &reject);
bool CheckSeqEntry(const CSeq_entry& entry, const string& file, CSeqEntryInfo& info, CSeqEntryCommonInfo& common_info);
EDateIssues CheckDates(const CSeq_entry& entry, CSeqdesc::E_Choice choice, CDate& date);
void StripAuthors(CSeq_entry& entry);
bool IsScaffoldPrefix(const string& accession, size_t prefix_len);

}

#endif // WGS_ASN_H
