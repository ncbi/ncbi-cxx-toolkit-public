#ifndef GPIPE_APP_BGPIPE_ALIGN_UTIL_HPP
#define GPIPE_APP_BGPIPE_ALIGN_UTIL_HPP
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
 * Authors:  Vyacheslav Chetvernin
 *
 * File Description:
 */
#include <corelib/ncbistl.hpp>
#include <algo/sequence/gene_model.hpp>

#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_align;
    class CSpliced_exon_chunk;
END_SCOPE(objects)
USING_SCOPE(objects);

class NCBI_XALGOSEQ_EXPORT CInternalStopFinder {
    CScope& scope;
    CFeatureGenerator generator;
public:
    CInternalStopFinder(CScope& scope);
    set<TSeqPos> FindStops(const CSeq_align& align);

    bool HasInternalStops(const CSeq_align& align);

    pair<set<TSeqPos>, set<TSeqPos> > FindStartsStops(const CSeq_align& align, int padding=0);

    //in presence of frameshifts start and stops not always are triplets
    //ranges are biological, i.e. inverted if minus strand or cross-origin  
    pair<set<TSeqRange>, set<TSeqRange> > FindStartStopRanges(const CSeq_align& align, int padding=0);

private:
    string GetCDSNucleotideSequence(const CSeq_align& align);
};

// pair(genomic, product)
pair<int, int> NCBI_XALGOSEQ_EXPORT ChunkSize(const CSpliced_exon_chunk& chunk);

END_NCBI_SCOPE

#endif // GPIPE_APP_BGPIPE_ALIGN_UTIL_HPP
