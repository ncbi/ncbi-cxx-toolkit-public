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
 * Author:  Christiam Camacho
 *
 * File Description:
 *   CSegMasker class implementation.
 *
 */

#include <ncbi_pch.hpp>
#include <algo/blast/core/blast_seg.h>
#include <algo/blast/core/blast_filter.h>
#include "seg.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CSegMasker::CSegMasker(int window /* = kSegWindow */,
                       double locut /* = kSegLocut */,
                       double hicut /* = kSegHicut */)
: m_SegParameters(SegParametersNewAa())
{
    if ( !m_SegParameters ) {
        throw runtime_error("Failed to allocate SegParameters structure");
    }
    m_SegParameters->window = window;
    m_SegParameters->locut = locut;
    m_SegParameters->hicut = hicut;
}

CSegMasker::~CSegMasker()
{
    SegParametersFree(m_SegParameters);
}

//------------------------------------------------------------------------------
CSegMasker::TMaskList*
CSegMasker::operator()(const CSeqVector & data)
{
    if ( !data.IsProtein() ) {
        throw logic_error("SEG can only filter protein sequences");
    }

    if (data.GetCoding() != CSeq_data::e_Ncbistdaa ) {
        throw logic_error("SEG expects protein sequences in ncbistdaa format");
    }

    string sequence;
    BlastSeqLoc* seq_locs = NULL;
    data.GetSeqData(data.begin(), data.end(), sequence);

    Int2 status = SeqBufferSeg((Uint1*)(sequence.c_str()),
                               static_cast<Int4>(sequence.size()), 0,
                               m_SegParameters, &seq_locs);
    sequence.clear();
    if (status != 0) {
        seq_locs = BlastSeqLocFree(seq_locs);
        throw runtime_error("SEG internal error " + NStr::IntToString(status));
    }

    auto_ptr<TMaskList> retval(new TMaskList);
    for (BlastSeqLoc* itr = seq_locs; itr; itr = itr->next) {
        retval->push_back
            (TMaskList::value_type(itr->ssr->left, itr->ssr->right));
    }

    seq_locs = BlastSeqLocFree(seq_locs);

    return retval.release();
}


END_NCBI_SCOPE
