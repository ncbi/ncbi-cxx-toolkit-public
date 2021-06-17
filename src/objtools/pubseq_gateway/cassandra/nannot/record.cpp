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
 * File Description:
 *
 * Blob storage: blob record
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/nannot/record.hpp>

#include <corelib/ncbistr.hpp>
#include <sstream>
#include <utility>
#include <string>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

string CNAnnotRecord::ToString() const
{
    stringstream s;
    s << "SeqId: " << m_Accession << "." << m_Version << "|" << m_SeqIdType << endl
      << "\tm_SatKey: " << m_SatKey << endl
      << "\tm_AnnotName: " << m_AnnotName << endl
      << "\tm_Start: " << m_Start << endl
      << "\tm_Stop: " << m_Stop << endl
      << "\tm_AnnotInfo: " << m_AnnotInfo << endl;
    return s.str();
}

END_IDBLOB_SCOPE
