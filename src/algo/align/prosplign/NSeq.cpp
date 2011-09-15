/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Boris Kiryutin
*
* =========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <algo/align/prosplign/prosplign_exception.hpp>

#include "NSeq.hpp"

#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/seq_vector.hpp>

#include <objtools/alnmgr/nucprot.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)
USING_SCOPE(ncbi::objects);

CNSeq::CNSeq(void)
{
    m_size = 0;
}

CNSeq::~CNSeq(void)
{
}

// letter by position
char CNSeq::Upper(int pos) const
{
    return CTranslationTable::NucToChar(seq[pos]);
}


void CNSeq::Init(const CNSeq& fullseq, const vector<pair<int, int> >& igi)
{
    seq.clear();
    NSEQ::const_iterator beg, end;
    beg = fullseq.seq.begin();
    m_size = fullseq.m_size;
    _ASSERT((int)fullseq.seq.size() >= fullseq.size());
    for(vector<pair<int, int> >::const_iterator it = igi.begin(); it != igi.end(); ++it) {
        if(it->first < 0 || it->second < 1) NCBI_THROW(CProSplignException, eGenericError, "Intron coordinates are invalid");
        if(it->first + it->second > fullseq.size()) NCBI_THROW(CProSplignException, eGenericError, "Intron coordinate is out of sequence");
        end = fullseq.seq.begin() + it->first;
        if(end < beg) NCBI_THROW(CProSplignException, eGenericError, "Intron coordinates have wrong order");
        seq.insert(seq.end(), beg, end);
        beg = end + it->second;
        m_size -= it->second;
    }
    if(beg < fullseq.seq.end()) seq.insert(seq.end(), beg, fullseq.seq.end());
}

void CNSeq::Init(CScope& scope, CSeq_loc& genomic)
{
    CRef<CSeq_id> seqid( new CSeq_id );
    seqid->Assign(*genomic.GetId());

    TSeqPos loc_end = sequence::GetStop(genomic, &scope);
    TSeqPos seq_end = sequence::GetLength(*genomic.GetId(), &scope)-1;

    CRef<CSeq_loc> extended_seqloc(new CSeq_loc);
    if (loc_end<=seq_end)
        extended_seqloc->Assign(genomic);
    else {
        CRef<CSeq_loc> extra_seqloc(new CSeq_loc(*seqid,seq_end+1,loc_end,genomic.GetStrand()) );
        extended_seqloc = sequence::Seq_loc_Subtract(genomic,*extra_seqloc,CSeq_loc::fMerge_All|CSeq_loc::fSort,&scope);
        extended_seqloc->SetId(*seqid); // Seq_loc_Subtract might change the id, e.g. replace accession with gi
        genomic.Assign(*extended_seqloc);
    }

    m_size = sequence::GetLength(*extended_seqloc,&scope);

    if (IsForward(genomic.GetStrand())) {
        TSeqPos pos = sequence::GetStop(*extended_seqloc, &scope);
        if (pos<seq_end) {
            TSeqPos pos1 = pos + 3;
            if (pos1 > seq_end)
                pos1 = seq_end;
            CRef<CSeq_loc> extra_seqloc(new CSeq_loc(*seqid,pos,pos1,eNa_strand_plus) );
            extended_seqloc = sequence::Seq_loc_Add(*extended_seqloc,*extra_seqloc,CSeq_loc::fMerge_All|CSeq_loc::fSort,&scope);
        }
    } else {
        TSeqPos pos = sequence::GetStart(*extended_seqloc, &scope);
        if (pos > 0) {
            TSeqPos  pos0 = pos < 3 ? 0 : pos - 3;
            CRef<CSeq_loc> extra_seqloc(new CSeq_loc(*seqid,pos0,pos-1,eNa_strand_minus) );
            extended_seqloc = sequence::Seq_loc_Add(*extended_seqloc,*extra_seqloc,CSeq_loc::fMerge_All|CSeq_loc::fSort,&scope);
        }
    }
    
    CSeqVector seq_vec(*extended_seqloc,scope,CBioseq_Handle::eCoding_Ncbi);

    vector<char> convert(16,nN);
    convert[1] = nA;
    convert[2] = nC;
    convert[4] = nG;
    convert[8] = nT;

    seq.clear();
    for (CSeqVector_CI i(seq_vec); i; ++i) {
        seq.push_back(convert[*i&0xf]);
    }
}


END_SCOPE(prosplign)
END_NCBI_SCOPE
