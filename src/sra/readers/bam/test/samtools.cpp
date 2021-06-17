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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Interface to SAMTOOLS library.
 *
 */

#include <ncbi_pch.hpp>
#include "samtools.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(samtools)

bool SBamAlignment::operator<(const SBamAlignment& a) const
{
#define ORDER_BY(field) if ( field != a.field ) return field < a.field
    ORDER_BY(ref_seq_index);
    ORDER_BY(ref_range);
    ORDER_BY(short_seq_id);
    ORDER_BY(short_range);
    ORDER_BY(flags);
    ORDER_BY(quality);
    ORDER_BY(short_sequence);
    ORDER_BY(CIGAR);
#undef ORDER_BY
    return 0;
}


bool SBamAlignment::operator==(const SBamAlignment& a) const
{
#define ORDER_BY(field) if ( field != a.field ) return false
    ORDER_BY(ref_seq_index);
    ORDER_BY(ref_range);
    ORDER_BY(short_seq_id);
    ORDER_BY(short_range);
    ORDER_BY(flags);
    ORDER_BY(quality);
    ORDER_BY(short_sequence);
    ORDER_BY(CIGAR);
#undef ORDER_BY
    return true;
}

#define SAM_BASES "-ACMGRSVTWYHKDBN"
#define SAM_CIGAR "MIDNSHP=X???????"

ostream& operator<<(ostream& out, const SBamAlignment& a)
{
    out << a.ref_seq_index << " " << a.ref_range << " "
        << a.short_seq_id << " " << a.short_range << " "
        << "0x" << hex << a.flags << dec << " "
        << a.quality << " " << a.short_sequence;
    out << " ";
    ITERATE ( vector<Uint4>, i, a.CIGAR ) {
        int type = *i&15;
        int len = *i>>4;
        out << SAM_CIGAR[type] << len;
    }
    return out;
}


namespace {

    class CFetchCountOverflowException : public exception {
    };

    struct SFetchParams
    {
        CBamFile::TRefSeqIndex m_Id;
        CRange<TSeqPos>        m_Range;
        size_t                 m_Count;
        size_t                 m_LimitCount;
        CBamFile::TAlignments* m_Alns;

        void CheckCountOverflow(size_t count) const
            {
                if ( m_LimitCount && count >= m_LimitCount ) {
                    throw CFetchCountOverflowException();
                }
            }
    };

}


extern "C" {

    static int bam_count_func(const bam1_t*, void* data)
    {
        SFetchParams* params = static_cast<SFetchParams*>(data);
        params->CheckCountOverflow(++params->m_Count);
        return 1;
    }
        

    static int bam_collect_func(const bam1_t* bam, void* data)
    {
        SFetchParams* params = static_cast<SFetchParams*>(data);
        SBamAlignment aln;
        aln.ref_seq_index = bam->core.tid;
        aln.ref_range.SetFrom(bam->core.pos);
        aln.short_seq_id = bam1_qname(bam);
        aln.short_range.SetFrom(0);
        aln.short_sequence.resize(bam->core.l_qseq);
        const unsigned char* seq = bam1_seq(bam);
        for ( int i = 0; i < bam->core.l_qseq; ++i ) {
            aln.short_sequence[i] = SAM_BASES[bam1_seqi(seq, i)];
        }
        int cigar_len = bam->core.n_cigar;
        const Uint4* cigar = bam1_cigar(bam);
        if ( cigar_len && (cigar[0]&15) == 4 /* S */ ) {
            aln.short_range.SetFrom(cigar[0]>>4);
            --cigar_len;
            ++cigar;
        }
        if ( cigar_len && (cigar[cigar_len-1]&15) == 4 /* S */ ) {
            --cigar_len;
        }
        aln.CIGAR.assign(cigar, cigar+cigar_len);
        int ref_len = 0, short_len = 0;
        ITERATE ( vector<Uint4>, i, aln.CIGAR ) {
            int type = *i&15;
            int len = *i>>4;
            if ( type == 0 || type == 7 || type == 8 ) { // both: "M=X"
                ref_len += len;
                short_len += len;
            }
            else if ( type == 2 || type == 3 ) { // ref only: "DN"
                ref_len += len;
            }
            else if ( type == 1 || type == 4 ) { // short only: "M=XIS"
                short_len += len;
            }
        }
        aln.ref_range.SetLength(ref_len);
        aln.short_range.SetLength(short_len);
        aln.flags = bam->core.flag;
        aln.quality = bam->core.qual;
        params->m_Alns->push_back(aln);
        params->CheckCountOverflow(params->m_Alns->size());
        return 1;
    }

}


CBamFile::CBamFile(void)
    : m_File(0), m_Index(0)
{
}


CBamFile::CBamFile(const string& path,
                   EIndexType index_type)
    : m_File(0),
      m_Index(0),
      m_Header(0)
{
    Open(path, index_type);
}


CBamFile::~CBamFile(void)
{
    Close();
}


void CBamFile::Open(const string& path,
                    EIndexType index_type)
{
    Close();
    m_File = bam_open(path.c_str(), "r");
    if ( !m_File ) {
        Close();
        NCBI_THROW(CException, eUnknown,
                   "Cannot open BAM file "+path);
    }
    if ( index_type == eWithIndex ) {
        m_Index = bam_index_load(path.c_str());
        if ( !m_Index ) {
            Close();
            NCBI_THROW(CException, eUnknown,
                       "Cannot open BAM file index "+path);
        }
    }
    m_Header = bam_header_read(m_File);
    if ( !m_Header ) {
        Close();
        NCBI_THROW(CException, eUnknown,
                   "Cannot read BAM file header"+path);
    }
}


void CBamFile::Close(void)
{
    if ( m_Header ) {
        bam_header_destroy(m_Header);
        m_Header = 0;
    }
    if ( m_Index ) {
        bam_index_destroy(m_Index);
        m_Index = 0;
    }
    if ( m_File ) {
        bam_close(m_File);
        m_File = 0;
    }
}


CBamFile::TRefSeqIndex CBamFile::GetRefSeqIndex(const string& id)
{
    for ( TRefSeqIndex i = 0; i < m_Header->n_targets; ++i ) {
        if ( id == m_Header->target_name[i] ) {
            return i;
        }
    }
    NCBI_THROW(CException, eUnknown,
               "Unknown ref seq id: "+id);
}


size_t CBamFile::Count(TRefSeqIndex id, TSeqPos from, TSeqPos to,
                       size_t limit_count)
{
    SFetchParams params;
    params.m_Id = id;
    params.m_Range.SetFrom(from).SetTo(to);
    params.m_Count = 0;
    params.m_Alns = 0;
    params.m_LimitCount = limit_count;
    if ( to != kInvalidSeqPos ) {
        ++to;
    }
    try {
        bam_fetch(m_File, m_Index, id, from, to, &params, bam_count_func);
    }
    catch ( CFetchCountOverflowException& /*ignored*/ ) {
    }
    return params.m_Count;
}


void CBamFile::Fetch(TRefSeqIndex id, TSeqPos from, TSeqPos to,
                     TAlignments& alns, size_t limit_count)
{
    SFetchParams params;
    params.m_Id = id;
    params.m_Range.SetFrom(from).SetTo(to);
    params.m_Count = 0;
    params.m_Alns = &alns;
    params.m_LimitCount = limit_count;
    if ( to != kInvalidSeqPos ) {
        ++to;
    }
    try {
        bam_fetch(m_File, m_Index, id, from, to, &params, bam_collect_func);
    }
    catch ( CFetchCountOverflowException& /*ignored*/ ) {
    }
}


END_SCOPE(samtools)
END_NCBI_SCOPE
