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
 * Authors:
 *      Victor Sapojnikov
 *
 * File Description:
 *      Map of string->int populated by sequence ids->lengths from a FASTA file.
 *
 *
 */

#include <ncbi_pch.hpp>

#include "MapCompLen.hpp"

using namespace ncbi;
BEGIN_NCBI_SCOPE

//// class CMapCompLen
int CMapCompLen::AddCompLen(const string& acc, int len, bool increment_count)
{
  TMapStrInt::value_type acc_len(acc, len);
  TMapStrIntResult insert_result = insert(acc_len);
  if(insert_result.second == false) {
    if(insert_result.first->second != len)
      return insert_result.first->second; // error: already have a different length
  }
  if(increment_count) m_count++;
  return 0; // success
}



END_NCBI_SCOPE

