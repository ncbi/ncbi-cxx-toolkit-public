#ifndef NCBI_OBJMGR_SPLIT_ID_RANGE__HPP
#define NCBI_OBJMGR_SPLIT_ID_RANGE__HPP

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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Utility class for collecting ranges of sequences
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <util/range.hpp>

#include <map>

BEGIN_NCBI_SCOPE

class CObjectOStream;

BEGIN_SCOPE(objects)

class CSeq_feat;
class CSeq_align;
class CSeq_graph;
class CSeq_loc;
class CSeq_id;
class CSeq_point;
class CSeq_interval;
class CPacked_seqpnt;
class CDense_seg;
class CDense_diag;
class CPacked_seg;
class CSpliced_seg;
class CSparse_seg;

class COneSeqRange
{
public:
    typedef CRange<TSeqPos> TRange;

    COneSeqRange(void)
        : m_TotalRange(TRange::GetEmpty())
        {
        }

    TRange GetTotalRange(void) const
        {
            return m_TotalRange;
        }

    void Add(const COneSeqRange& range);
    void Add(const TRange& range);
    void Add(TSeqPos start, TSeqPos stop_exclusive);

private:
    TRange m_TotalRange;
};


class CSeqsRange
{
public:
    CSeqsRange(void);
    ~CSeqsRange(void);

    CNcbiOstream& Print(CNcbiOstream& out) const;

    typedef COneSeqRange::TRange TRange;
    typedef map<CSeq_id_Handle, COneSeqRange> TRanges;
    typedef TRanges::const_iterator const_iterator;

    const_iterator begin(void) const
        {
            return m_Ranges.begin();
        }
    const_iterator end(void) const
        {
            return m_Ranges.end();
        }

    size_t size(void) const
        {
            return m_Ranges.size();
        }
    bool empty(void) const
        {
            return m_Ranges.empty();
        }
    void clear(void)
        {
            m_Ranges.clear();
        }

    CSeq_id_Handle GetSingleId(void) const;

    void Add(const CSeq_id_Handle& id, const COneSeqRange& loc);
    void Add(const CSeq_id_Handle& id, const TRange& range);
    void Add(const CSeqsRange& seqs_range);

    void Add(const CSeq_loc& loc);
    void Add(const CSeq_id& id);
    void Add(const CSeq_point& p);
    void Add(const CSeq_interval& i);
    void Add(const CPacked_seqpnt& pp);
    void Add(const CSeq_feat& obj);
    void Add(const CSeq_align& obj);
    void Add(const CSeq_graph& obj);
    void Add(const CDense_seg& denseg);
    void Add(const CDense_diag& diag);
    void Add(const CPacked_seg& packed);
    void Add(const CSpliced_seg& spliced);
    void Add(const CSparse_seg& sparse);

private:
    TRanges m_Ranges;
};


inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CSeqsRange& info)
{
    return info.Print(out);
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//NCBI_OBJMGR_SPLIT_ID_RANGE__HPP
