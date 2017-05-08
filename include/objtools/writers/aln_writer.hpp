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
 * Authors: Justin Foley
 *
 * File Description:  Write alignment file
 *
 */

#ifndef OBJTOOLS_WRITERS_ALN_WRITER_HPP
#define OBJTOOLS_WRITERS_ALN_WRITER_HPP

#include <objtools/writers/writer.hpp>
#include <util/sequtil/sequtil.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CDense_seg;
class CScope;
class CSparse_align;
class CSpliced_seg;

class NCBI_XOBJWRITE_EXPORT CAlnWriter:
    public CWriterBase
{

public:
    CAlnWriter(
        CScope& scope,
        CNcbiOstream& ostr,
        unsigned int uFlags);

    CAlnWriter(
        CNcbiOstream& ostr,
        unsigned int uFlags);

    virtual ~CAlnWriter() = default;

    virtual bool WriteAlign(
        const CSeq_align& align,
        const string& name="",
        const string& descr="");

private:
    bool xWriteAlignDenseSeg(const CDense_seg& denseg);

    bool xWriteAlignSplicedSeg(const CSpliced_seg& spliced_seg);

    bool xWriteSparseAlign(const CSparse_align& sparse_aln);

    bool xWriteSplicedExons(const list<CRef<CSpliced_exon>>& exons,
        CSpliced_seg::TProduct_type product_type,
        CRef<CSeq_id> default_genomic_id,
        ENa_strand default_genomic_strand,
        CRef<CSeq_id> default_product_id,
        ENa_strand default_product_strand);

    void xProcessSeqId(const CSeq_id& id, CBioseq_Handle& bsh, CRange<TSeqPos>& range);
   
    bool xGetSeqString(CBioseq_Handle bsh,
        const CRange<TSeqPos>& range,
        ENa_strand strand,
        string& seq);

    string xGetSegString(const string& seq_plus,
        CSeqUtil::ECoding coding,
        ENa_strand strand,
        int start,
        size_t len);

    CRef<CScope> m_pScope;
    
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS_ALN_WRITER_HPP
