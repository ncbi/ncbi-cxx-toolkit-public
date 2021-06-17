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
 * Blob storage: si2csi record
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/si2csi/record.hpp>

#include <corelib/ncbistr.hpp>
#include <sstream>
#include <utility>
#include <string>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

string CSI2CSIRecord::ToString(void) const
{
    string s;
    return s.append("SecSeqId: ")
            .append(m_SecSeqId)
            .append(" SeqSeqIdType: ")
            .append(NStr::NumericToString(m_SecSeqIdType))
            .append(" Accession: ")
            .append(m_Accession)
            .append(" GI: ")
            .append(NStr::NumericToString(m_GI))
            .append(" SecSeqState: ")
            .append(NStr::NumericToString(m_SecSeqState))
            .append(" SeqIdType: ")
            .append(NStr::NumericToString(m_SeqIdType))
            .append(" Version: ")
            .append(NStr::NumericToString(m_Version));
}

END_IDBLOB_SCOPE
