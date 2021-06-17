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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   GFF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>

//#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/writers/write_util.hpp>
#include <objtools/writers/gff2_write_data.hpp>
#include <objtools/writers/gtf_write_data.hpp>
#include <objmgr/util/seq_loc_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
bool CGtfRecord::MakeChildRecord(
    const CGtfRecord& parent,
    const CSeq_interval& location,
    unsigned int uExonNumber )
//  ----------------------------------------------------------------------------
{
    if ( ! location.CanGetFrom() || ! location.CanGetTo() ) {
        return false;
    }
    mSeqId = parent.mSeqId;
    mMethod = parent.mMethod;
    mType = parent.mType;
    m_strGeneId = parent.GeneId();
    m_strTranscriptId = parent.TranscriptId();

    CGffBaseRecord::SetLocation(
        location.GetFrom(), location.GetTo(),
        (location.IsSetStrand() ? location.GetStrand() : eNa_strand_plus));
    mScore = parent.mScore;

    mAttributes.insert( parent.mAttributes.begin(), parent.mAttributes.end() );
    if ( 0 != uExonNumber ) {
        SetAttribute("exon_number", NStr::UIntToString(uExonNumber));
    }
    return true;
};

//  ----------------------------------------------------------------------------
string CGtfRecord::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
    strAttributes.reserve(256);
    CGtfRecord::TAttributes attrs;
    attrs.insert( Attributes().begin(), Attributes().end() );
    CGtfRecord::TAttrIt it;

    strAttributes += x_AttributeToString( "gene_id", GeneId() );
    strAttributes += x_AttributeToString( "transcript_id", TranscriptId() );

    for ( it = attrs.begin(); it != attrs.end(); ++it ) {
        string strKey = it->first;
        if ( NStr::StartsWith( strKey, "gff_" ) ) {
            continue;
        }
        if ( strKey == "exon_number" ) {
            continue;
        }
        if (strKey == "db_xref") {
            for (const auto& xref: it->second) {
                strAttributes += x_AttributeToString(strKey, xref);
            }
            continue;
        }
        for (const auto& value: it->second) {
            strAttributes += x_AttributeToString(strKey, value);
        }
    }

    if ( ! m_bNoExonNumbers ) {
        it = attrs.find( "exon_number" );
        if ( it != attrs.end() ) {
            strAttributes += x_AttributeToString( "exon_number", it->second.front() );
        }
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
string CGtfRecord::StrStructibutes() const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
    strAttributes.reserve(256);
    CGtfRecord::TAttributes attrs;
    attrs.insert( Attributes().begin(), Attributes().end() );
    CGtfRecord::TAttrIt it;

    strAttributes += x_AttributeToString( "gene_id", GeneId() );
    if ( StrType() != "gene" ) {
        strAttributes += x_AttributeToString( "transcript_id", TranscriptId() );
    }

    it = attrs.find( "exon_number" );
    if ( it != attrs.end() ) {
        strAttributes += x_AttributeToString( "exon_number", it->second.front() );
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
void CGtfRecord::SetCdsPhase(
    const list<CRef<CSeq_interval> > & cdsLocs,
    ENa_strand eStrand )
//  ----------------------------------------------------------------------------
{
    if ( cdsLocs.empty() ) {
        return;
    }
    mPhase = "0";
    if ( eStrand == eNa_strand_minus ) {
        unsigned int uTopSize = 0;
        list<CRef<CSeq_interval> >::const_iterator it = cdsLocs.begin();
        for ( ; it != cdsLocs.end(); ++it ) {
            if ( (*it)->CanGetFrom()  && (*it)->GetFrom() > mSeqStop ) {
                uTopSize += (*it)->GetLength();
            }
        }
        mPhase = NStr::NumericToString((3 - (uTopSize % 3) ) % 3);
    }
    else {
        unsigned int uBottomSize = 0;
        list<CRef<CSeq_interval> >::const_iterator it = cdsLocs.begin();
        for ( ; it != cdsLocs.end(); ++it ) {
            if ( (*it)->CanGetFrom()  &&  (*it)->GetTo() < mSeqStart ) {
                uBottomSize += (*it)->GetLength();
            }
        }
        mPhase = NStr::NumericToString((3 - (uBottomSize % 3) ) % 3);
    }
}

//  ============================================================================
string CGtfRecord::x_AttributeToString(
    const string& strKey,
    const string& strValue )
//  ============================================================================
{
    string str( strKey );
    str += " \"";
    str += strValue;
    str += "\"; ";
    return str;
}

END_objects_SCOPE
END_NCBI_SCOPE
