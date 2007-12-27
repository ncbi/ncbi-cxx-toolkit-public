#ifndef ALGO_GNOMON___GNOMON_ENGINE__HPP
#define ALGO_GNOMON___GNOMON_ENGINE__HPP

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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include "gnomon_seq.hpp"

class CTerminal;
class CCodingRegion;
class CNonCodingRegion;
class CSeqScores;
class CParse;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

struct CGnomonEngine::SGnomonEngineImplData {
    SGnomonEngineImplData(const CResidueVec& sequence, TSignedSeqRange range);
    ~SGnomonEngineImplData();

    CResidueVec       m_seq;
    CDoubleStrandSeq  m_ds;
    TSignedSeqRange   m_range;
    int               m_gccontent;

    const CTerminal*        m_acceptor;
    const CTerminal*        m_donor;
    const CTerminal*        m_start;
    const CTerminal*        m_stop;
    const CCodingRegion*    m_cdr;
    const CNonCodingRegion* m_ncdr;
    const CNonCodingRegion* m_intrg;

    auto_ptr<CSeqScores>       m_ss;
    auto_ptr<CParse>           m_parse;
};

END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // ALGO_GNOMON___GNOMON_ENGINE__HPP
