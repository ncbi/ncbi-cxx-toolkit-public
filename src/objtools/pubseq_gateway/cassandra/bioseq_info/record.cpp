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
 * Blob storage: bioseq_info record
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>

#include <corelib/ncbistr.hpp>
#include <sstream>
#include <utility>
#include <string>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

void CBioseqInfoRecord::Reset(void)
{
    m_Accession.clear();
    m_Version = -1;
    m_SeqIdType = -1;
    m_DateChanged = 0;
    m_Hash = -1;
    m_GI = -1;
    m_Length = -1;
    m_Mol = -1;
    m_Sat = -1;
    m_SatKey = -1;
    m_SeqIds.clear();
    m_SeqState = -1;
    m_State = -1;
    m_TaxId = -1;
    m_Writetime = -1;
}

string CBioseqInfoRecord::ToString(void) const
{
    string s;
    return s.append("Accession: ")
            .append(m_Accession)
            .append(" Version: ")
            .append(NStr::NumericToString(m_Version))
            .append(" SeqIdType: ")
            .append(NStr::NumericToString(m_SeqIdType))
            .append(" GI: ")
            .append(NStr::NumericToString(m_GI))
            .append(" Name: ")
            .append(m_Name)
            .append(" DateChanged: ")
            .append(NStr::NumericToString(m_DateChanged))
            .append(" Hash: ")
            .append(NStr::NumericToString(m_Hash))
            .append(" Length: ")
            .append(NStr::NumericToString(m_Length))
            .append(" Mol: ")
            .append(NStr::NumericToString(m_Mol))
            .append(" Sat: ")
            .append(NStr::NumericToString(m_Sat))
            .append(" SatKey: ")
            .append(NStr::NumericToString(m_SatKey))
            .append(" SeqState: ")
            .append(NStr::NumericToString(m_SeqState))
            .append(" State: ")
            .append(NStr::NumericToString(m_State))
            .append(" TaxId: ")
            .append(NStr::NumericToString(m_TaxId));
}

END_IDBLOB_SCOPE
