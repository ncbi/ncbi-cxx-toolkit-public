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
 *  Author: Christiam Camacho
 *
 * ===========================================================================
 */

/** @file local_blastdb_adapter.cpp
 * Defines the CLocalBlastDbAdapter class
 */
#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include "local_blastdb_adapter.hpp"
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_literal.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CSeqDB::ESeqType
CLocalBlastDbAdapter::GetSequenceType()
{
    return m_SeqDB->GetSequenceType();
}

int 
CLocalBlastDbAdapter::GetTaxId(const CSeq_id_Handle& idh)
{
    int retval = kInvalidSeqPos;
    CConstRef<CSeq_id> id = idh.GetSeqId();
    if (id.NotEmpty()) {
        int oid = 0;
        if (SeqidToOid(*id, oid)) {
            map<int, int> gi_to_taxid;
            m_SeqDB->GetTaxIDs(oid, gi_to_taxid);
            if (idh.IsGi()) {
                retval = gi_to_taxid[idh.GetGi()];
            } else {
                retval = gi_to_taxid.begin()->second;
            }
        }
    }
    return retval;
}

int 
CLocalBlastDbAdapter::GetSeqLength(int oid)
{
    return m_SeqDB->GetSeqLength(oid);
}

IBlastDbAdapter::TSeqIdList
CLocalBlastDbAdapter::GetSeqIDs(int oid)
{
    return m_SeqDB->GetSeqIDs(oid);
}

CRef<CBioseq> 
CLocalBlastDbAdapter::GetBioseqNoData(int oid, int target_gi /* = 0 */)
{
    return m_SeqDB->GetBioseqNoData(oid, target_gi);
}

/// Assigns a buffer of nucleotide sequence data as retrieved from CSeqDB into
/// the CSeq_data object
/// @param buffer contains the sequence data to assign [in]
/// @param seq_data object to assign the data to in ncbi4na format [in|out]
/// @param length sequence length [in]
static void
s_AssignBufferToSeqData(const char* buffer,
                        CSeq_data& seq_data,
                        TSeqPos length)
{
    // This code works around the fact that SeqDB
    // currently only produces 8 bit output -- it builds an array and
    // packs the output into it in 4 bit format. SeqDB should probably
    // provide more formats and combinations so that this code can
    // disappear.
    
    vector<char>& v4 = seq_data.SetNcbi4na().Set();
    v4.reserve((length+1)/2);

    const TSeqPos length_whole = length & ~1;

    for(TSeqPos i = 0; i < length_whole; i += 2) {
        v4.push_back((buffer[i] << 4) | buffer[i+1]);
    }
    if (length_whole != length) {
        _ASSERT((length_whole) == (length-1));
        v4.push_back(buffer[length_whole] << 4);
    }
}

CRef<CSeq_data> 
CLocalBlastDbAdapter::GetSequence(int oid, 
                                  int begin /* = 0 */, 
                                  int end /* = 0*/)
{
    const bool kIsProtein = (GetSequenceType() == CSeqDB::eProtein)
        ? true : false;
    const int kNuclCode(kSeqDBNuclNcbiNA8);
    CRef<CSeq_data> retval(new CSeq_data);
    const char* buffer = NULL;

    if (begin == end && begin == 0) {   
        // Get full sequence
        if (kIsProtein) {
            TSeqPos length = m_SeqDB->GetSequence(oid, &buffer);
            retval->SetNcbistdaa().Set().assign(buffer, buffer+length);
            m_SeqDB->RetSequence(&buffer);
        } else {
            TSeqPos length = m_SeqDB->GetAmbigSeq(oid, &buffer, kNuclCode);
            s_AssignBufferToSeqData(buffer, *retval, length); 
            m_SeqDB->RetAmbigSeq(&buffer);
        }
    } else {
        // Get parts of the sequence
        if (kIsProtein) {
            TSeqPos length = m_SeqDB->GetSequence(oid, &buffer);
            _ASSERT((end-begin) <= (int)length);
            retval->SetNcbistdaa().Set().assign(buffer + begin, buffer + end);
            m_SeqDB->RetSequence(&buffer);
            length += 0;    // to avoid compiler warning
        } else {
            CSeqDB::TRangeList ranges;
            ranges.insert(pair<int,int>(begin, end));
            m_SeqDB->SetOffsetRanges(oid, ranges, false, false);
            TSeqPos length = 
                m_SeqDB->GetAmbigSeq(oid, &buffer, kNuclCode, begin, end);
            _ASSERT((end-begin) == (int)length);
            s_AssignBufferToSeqData(buffer, *retval, length); 
            m_SeqDB->RetAmbigSeq(&buffer);
            m_SeqDB->RemoveOffsetRanges(oid);
        }
    }
    return retval;
}

bool 
CLocalBlastDbAdapter::SeqidToOid(const CSeq_id & id, int & oid)
{
    return m_SeqDB->SeqidToOid(id, oid);
}

END_SCOPE(objects)
END_NCBI_SCOPE

