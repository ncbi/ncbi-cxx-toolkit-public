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
#include <algo/gnomon/gnomon_exception.hpp>
#include "gnomon_seq.hpp"
#include "parse.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

struct SGnomonEngineImplData {
    SGnomonEngineImplData(const string& modeldatafilename, const string& seqname, const CResidueVec& sequence, TSignedSeqRange range) :
        m_modeldatafilename(modeldatafilename), m_seqname(seqname), m_seq(sequence), m_range(range), m_gccontent(0)
{}

    string            m_modeldatafilename;
    string            m_seqname;
    CResidueVec       m_seq;
    CDoubleStrandSeq  m_ds;
    TSignedSeqRange   m_range;
    int               m_gccontent;

    auto_ptr<CTerminal>        m_acceptor;
    auto_ptr<CTerminal>        m_donor;
    auto_ptr<CTerminal>        m_start;
    auto_ptr<CTerminal>        m_stop;
    auto_ptr<CCodingRegion>    m_cdr;
    auto_ptr<CNonCodingRegion> m_ncdr;
    auto_ptr<CNonCodingRegion> m_intrg;

    auto_ptr<CSeqScores>       m_ss;
    auto_ptr<CParse>           m_parse;
};

END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // ALGO_GNOMON___GNOMON_ENGINE__HPP
