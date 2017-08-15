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
 * Author:  Greg Boratyn
 *
 */

/** @file blast_asn1_input.cpp
 * Convert ASN1-formatted files into blast sequence input
 */

#include <ncbi_pch.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/User_object.hpp>

#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/blast_sra_input/blast_sra_input.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include <vfs/resolver.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);


CSraInputSource::CSraInputSource(const vector<string>& accessions,
                                 bool check_for_paires /* = true */,
                                 bool cache_enabled /* = false */)
    : m_Accessions(accessions),
      m_IsPaired(check_for_paires)
{
    m_ItAcc = m_Accessions.begin();

    // initialize SRA access
    CVDBMgr mgr;

    // disable SRA cache
    if (!cache_enabled) {
        CVResolver resolver = CVResolver(CVFSManager(mgr));
        VResolverCacheEnable(resolver.GetPointer(), vrAlwaysDisable);
    }

    m_SraDb.reset(new CCSraDb(mgr, *m_ItAcc));
    m_It.reset(new CCSraShortReadIterator(*m_SraDb));
}


bool
CSraInputSource::End(void)
{
    if (!*m_It) {
        x_NextAccession();
    }
    return m_ItAcc == m_Accessions.end();
}


int
CSraInputSource::GetNextSequence(CBioseq_set& bioseq_set)
{
    m_BasesAdded = 0;

    // read sequences
    if (m_IsPaired) {
        x_ReadPairs(bioseq_set);
    }
    else {
        if (!*m_It) {
            x_NextAccession();
        }
        if (m_ItAcc != m_Accessions.end()) {
            x_ReadOneSeq(bioseq_set);
            ++(*m_It);
        }
    }

    return m_BasesAdded;
}


CRef<CSeq_entry>
CSraInputSource::x_ReadOneSeq(void)
{
    _ASSERT(m_It.get() && *m_It);
    if (m_It->IsTechnicalRead()) {
        return CRef<CSeq_entry>();
    }
    CTempString sequence = m_It->GetReadData();
    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    seq_entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_na);
    seq_entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);

    CBioseq& bioseq = seq_entry->SetSeq();
    bioseq.SetId().push_back(m_It->GetShortSeq_id());
    bioseq.SetInst().SetLength(sequence.length());
    bioseq.SetInst().SetSeq_data().SetIupacna(CIUPACna((string)sequence));

    m_BasesAdded += sequence.length();
    return seq_entry;
}


CSeq_entry*
CSraInputSource::x_ReadOneSeq(CBioseq_set& bioseq_set)
{
    CRef<CSeq_entry> seq_entry = x_ReadOneSeq();
    bioseq_set.SetSeq_set().push_back(seq_entry);
    return bioseq_set.SetSeq_set().back().GetNonNullPointer();
}


void
CSraInputSource::x_ReadPairs(CBioseq_set& bioseq_set)
{
    CRef<CSeqdesc> seqdesc_first(new CSeqdesc);
    seqdesc_first->SetUser().SetType().SetStr("Mapping");
    seqdesc_first->SetUser().AddField("has_pair", eFirstSegment);

    CRef<CSeqdesc> seqdesc_last(new CSeqdesc);
    seqdesc_last->SetUser().SetType().SetStr("Mapping");
    seqdesc_last->SetUser().AddField("has_pair", eLastSegment);

    TVDBRowId spot = -1;
    if (!*m_It) {
        x_NextAccession();
    }

    if (m_ItAcc == m_Accessions.end()) {
        return;
    }

    // read one sequence and rember spot id
    spot = m_It->GetSpotId();
    CSeq_entry* first = x_ReadOneSeq(bioseq_set);

    // move to the next sequence
    ++(*m_It);
    if (!(*m_It)) {
        return;
    }
        
    // if the current sequence has the same spot id, read current
    // sequence as a mate, otherwise continue the loop
    if (m_It->GetSpotId() == spot) {
        CSeq_entry* second = x_ReadOneSeq(bioseq_set);
        // set pair information for sequences
        if (first && second) {

            first->SetSeq().SetDescr().Set().push_back(seqdesc_first);
            second->SetSeq().SetDescr().Set().push_back(seqdesc_last);
        }
        ++(*m_It);
    }
}


void CSraInputSource::x_NextAccession(void)
{
    if (m_ItAcc == m_Accessions.end()) {
        return;
    }

    ++m_ItAcc;
    if (m_ItAcc != m_Accessions.end()) {
        CVDBMgr mgr;
        m_SraDb.reset(new CCSraDb(mgr, *m_ItAcc));
        m_It.reset(new CCSraShortReadIterator(*m_SraDb));
    }
}


CRef<CSeq_loc> CSraInputSource::x_GetNextSeq_loc(CScope& scope)
{
    m_BasesAdded = 0;
    CRef<CSeq_entry> seq_entry;

    // technical reads result in empty seq_entry
    while (m_ItAcc != m_Accessions.end() && seq_entry.Empty()) {
        if (!*m_It) {
            x_NextAccession();
        }
        if (m_ItAcc != m_Accessions.end()) {
            seq_entry = x_ReadOneSeq();
            ++(*m_It);
        }
    }

    if (seq_entry.Empty()) {
        return CRef<CSeq_loc>();
    }

    scope.AddTopLevelSeqEntry(*seq_entry);

    CRef<CSeq_loc> seqloc(new CSeq_loc());
    seqloc->SetInt().SetStrand(eNa_strand_both);

    seqloc->SetInt().SetFrom(0);
    seqloc->SetInt().SetTo(seq_entry->GetSeq().GetInst().GetLength() - 1);
    seqloc->SetInt().SetId().Assign(*seq_entry->GetSeq().GetFirstId());

    return seqloc;
}


SSeqLoc CSraInputSource::GetNextSSeqLoc(CScope& scope)
{
    CRef<CSeq_loc> seqloc = x_GetNextSeq_loc(scope);
    SSeqLoc retval(*seqloc, scope);

    return retval;
}

CRef<CBlastSearchQuery> CSraInputSource::GetNextSequence(CScope& scope)
{
    CRef<CSeq_loc> seqloc = x_GetNextSeq_loc(scope);
    CRef<CBlastSearchQuery> retval(new CBlastSearchQuery(*seqloc, scope));
    return retval;
}


END_SCOPE(blast)
END_NCBI_SCOPE
