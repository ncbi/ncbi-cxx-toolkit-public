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
#include "validatorp.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


CValidError_graph::CValidError_graph(CValidError_imp& imp) :
    CValidError_base(imp), m_NumMisplaced(0)
{
}


CValidError_graph::~CValidError_graph(void)
{
}


void CValidError_graph::ValidateSeqGraph(const CSeq_graph& graph)
{
    if ( x_IsMisplaced(graph) ) {
        ++m_NumMisplaced;
    }
}


bool s_FindGraph(const CSeq_graph& graph, CBioseq_Handle& bsh)
{
    if ( !bsh ) {
        return false;
    }

    for ( CGraph_CI it(bsh, 0, 0); it; ++it ) {
        if ( &graph == &(it->GetOriginalGraph()) ) {
            return true;
        }
    }

    return false;
}


bool CValidError_graph::x_IsMisplaced(const CSeq_graph& graph)
{
    if ( !graph.CanGetLoc() ) {
        return false;
    }

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(graph.GetLoc());
    if ( s_FindGraph(graph, bsh) ) {
        return false;
    }

    // need to test on the master bioseq for segmented?

    return true;
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2004/05/21 21:42:56  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2004/04/23 16:27:21  shomrat
* Stop using CBioseq_Handle deprecated interface
*
* Revision 1.4  2003/12/17 19:16:57  shomrat
* Implemented test for graph packaging test
*
* Revision 1.3  2003/03/31 14:40:05  shomrat
* $id: -> $id$
*
* Revision 1.2  2002/12/24 16:54:13  shomrat
* Changes to include directives
*
* Revision 1.1  2002/12/23 20:16:39  shomrat
* Initial submission after splitting former implementation
*
*
* ===========================================================================
*/
