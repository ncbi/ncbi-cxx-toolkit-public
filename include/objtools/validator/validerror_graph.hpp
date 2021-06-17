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
 *`
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Privae classes and definition for the validator
 *   .......
 *
 */

#ifndef VALIDATOR___VALIDERROR_GRAPH__HPP
#define VALIDATOR___VALIDERROR_GRAPH__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objtools/validator/validator.hpp>
#include <objtools/validator/validerror_imp.hpp>
#include <objtools/validator/validerror_base.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_graph;
class CBioseq_Handle;
class CBioseq;
class CBioseq_set;

BEGIN_SCOPE(validator)

// ============================  Validate SeqGraph  ============================


class CValidError_graph : private CValidError_base
{
public:
    CValidError_graph(CValidError_imp& imp);
    virtual ~CValidError_graph(void);

    void ValidateSeqGraph(const CSeq_graph& graph);

    void ValidateSeqGraphContext(const CSeq_graph& graph, const CBioseq_set& set);
    void ValidateSeqGraphContext(const CSeq_graph& graph, const CBioseq& seq);
    void ValidateGraphsOnBioseq(const CBioseq& seq);

    static bool IsSupportedGraphType(const CSeq_graph& graph);

private:
    void x_ValidateMinValues(const CByte_graph& bg, const CSeq_graph& graph);
    void x_ValidateMaxValues(const CByte_graph& bg, const CSeq_graph& graph);
    bool x_ValidateGraphLocation (const CSeq_graph& graph);
    void x_ValidateGraphOrderOnBioseq (const CBioseq& seq, vector <CRef <CSeq_graph> > graph_list);
    void x_ValidateGraphValues(const CSeq_graph& graph, const CBioseq& seq,
        int& first_N, int& first_ACGT, size_t& num_bases, size_t& Ns_with_score, size_t& gaps_with_score,
        size_t& ACGTs_without_score, size_t& vals_below_min, size_t& vals_above_max);
    void x_ValidateGraphOnDeltaBioseq(const CBioseq& seq);

    SIZE_TYPE GetUngappedSeqLen(const CBioseq& seq);

    bool x_GetLitLength(const CDelta_seq& delta, TSeqPos& len);

};


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_GRAPH__HPP */
