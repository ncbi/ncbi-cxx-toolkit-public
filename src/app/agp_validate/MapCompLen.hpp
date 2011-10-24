#ifndef AGP_VALIDATE_MapCompLen
#define AGP_VALIDATE_MapCompLen

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
 */

#include <corelib/ncbistd.hpp>
#include <iostream>
#include <map>

BEGIN_NCBI_SCOPE

typedef map<string, int> TMapStrInt ;
class CMapCompLen : public TMapStrInt
{
public:
  // may be less than size() because we add some names twice, e.g.: lcl|id1 and id1
  int m_count;

  typedef pair<TMapStrInt::iterator, bool> TMapStrIntResult;
  // returns 0 on success, or a previous length not equal to the new one
  int AddCompLen(const string& acc, int len, bool increment_count=true);
  CMapCompLen()
  {
    m_count=0;
  }

};

END_NCBI_SCOPE

#endif /* AGP_VALIDATE_MapCompLen */

