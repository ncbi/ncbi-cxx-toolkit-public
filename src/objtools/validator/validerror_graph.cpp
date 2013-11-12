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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   validation of seq_graph
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objmgr/graph_ci.hpp>
#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/utilities.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


CValidError_graph::CValidError_graph(CValidError_imp& imp) :
    CValidError_base(imp)
{
}


CValidError_graph::~CValidError_graph(void)
{
}


void CValidError_graph::ValidateSeqGraph(CSeq_entry_Handle seh)
{
    if (seh.IsSeq()) {
        ValidateSeqGraph (seh.GetCompleteSeq_entry()->GetSeq());
    } else if (seh.IsSet()) {
        ValidateSeqGraph (seh.GetCompleteSeq_entry()->GetSet());
    }
}


void CValidError_graph::ValidateSeqGraph(const CBioseq& seq)
{
    FOR_EACH_ANNOT_ON_BIOSEQ (it, seq) {
        if ((*it)->IsGraph()) {
            FOR_EACH_GRAPH_ON_ANNOT (graph, **it) {
                if (!(*graph)->IsSetLoc()) {
                    m_Imp.IncrementMisplacedGraphCount(); 
                } else {
                    CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, (*graph)->GetLoc());
                    if (m_Scope->GetBioseqHandle(seq) != bsh) {
                        m_Imp.IncrementMisplacedGraphCount(); 
                    }
                }
                ValidateSeqGraph (**graph);
            }
        }
    }
}


void CValidError_graph::ValidateSeqGraph (const CBioseq_set& set)
{
    FOR_EACH_ANNOT_ON_SEQSET (it, set) {
        if ((*it)->IsGraph()) {
            FOR_EACH_GRAPH_ON_ANNOT (graph, **it) {
                m_Imp.IncrementMisplacedGraphCount(); 
                ValidateSeqGraph (**graph);
            }
        }
    }
    FOR_EACH_SEQENTRY_ON_SEQSET (seq_it, set) {
        if ((*seq_it)->IsSeq()) {
            ValidateSeqGraph ((*seq_it)->GetSeq());
        } else if ((*seq_it)->IsSet()) {
            ValidateSeqGraph ((*seq_it)->GetSet());
        }
    }
}


void CValidError_graph::ValidateSeqGraph(const CSeq_graph& graph)
{
}


void CValidError_graph::ValidateSeqGraphContext(const CSeq_graph& graph, const CBioseq_set& set)
{
    m_Imp.IncrementMisplacedGraphCount(); 
}


void CValidError_graph::ValidateSeqGraphContext(const CSeq_graph& graph, const CBioseq& seq)
{
    if (!graph.IsSetLoc()) {
        m_Imp.IncrementMisplacedGraphCount(); 
    } else {
        CBioseq_Handle bsh = BioseqHandleFromLocation(m_Scope, graph.GetLoc());
        if (m_Scope->GetBioseqHandle(seq) != bsh) {
            m_Imp.IncrementMisplacedGraphCount(); 
        }
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
