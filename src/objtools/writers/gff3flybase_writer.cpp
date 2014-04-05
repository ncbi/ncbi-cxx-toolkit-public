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
 * Authors:
 *
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objmgr/util/sequence.hpp> 
#include <objtools/alnmgr/alnmap.hpp>
 
#include <objtools/writers/gff3flybase_record.hpp>
#include <objtools/writers/gff3flybase_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define INSERTION(sf, tf) ( ((sf) &  CAlnMap::fSeq) && !((tf) &  CAlnMap::fSeq) )
#define DELETION(sf, tf) ( !((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )
#define MATCH(sf, tf) ( ((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )

//  ----------------------------------------------------------------------------
static CConstRef<CSeq_id> s_GetSourceId(
    const CSeq_id& id, CScope& scope )
//  ----------------------------------------------------------------------------
{
    try {
        return sequence::GetId(id, scope, sequence::eGetId_Best).GetSeqId();
    }
    catch (CException&) {
    }
    return CConstRef<CSeq_id>(&id);
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::WriteHeader()
//  ----------------------------------------------------------------------------
{
    if (!m_bHeaderWritten) {
        m_Os << "##gff-version 3" << '\n';
        m_Os << "#!gff-spec-version 1.20" << '\n';
        m_Os << "##!gff-variant flybase" << '\n';
        m_Os << "# This variant of GFF3 interprets ambiguities in the" << '\n';
        m_Os << "# GFF3 specifications in accordance with the views of Flybase." << '\n';
        m_Os << "# This impacts the feature tag set, and meaning of the phase." << '\n';
        m_Os << "#!processor NCBI annotwriter" << '\n';
        m_bHeaderWritten = true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::x_WriteAlignDisc(
    const CSeq_align& align)
//  ----------------------------------------------------------------------------
{
    CGff3Writer::xWriteAlignDisc(align);
    m_Os << "###" << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::x_WriteAlignDenseg(
    const CSeq_align& align)
//  ----------------------------------------------------------------------------
{
    return true;
}

END_NCBI_SCOPE
