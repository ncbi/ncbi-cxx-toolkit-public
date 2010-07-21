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
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>

#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gtf_write_data.hpp>
#include <objmgr/util/seq_loc_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
string s_GtfDbtag( const CDbtag& dbtag )
//  ----------------------------------------------------------------------------
{
    string strGffTag;
    if ( dbtag.IsSetDb() ) {
        strGffTag += dbtag.GetDb();
        strGffTag += ":";
    }
    if ( dbtag.IsSetTag() ) {
        if ( dbtag.GetTag().IsId() ) {
            strGffTag += NStr::UIntToString( dbtag.GetTag().GetId() );
        }
        if ( dbtag.GetTag().IsStr() ) {
            strGffTag += dbtag.GetTag().GetStr();
        }
    }
    return strGffTag;
}
        
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
    m_strId = parent.Id();
    m_strSource = parent.Source();
    m_strType = parent.Type();
    m_strGeneId = parent.GeneId();
    m_strTranscriptId = parent.TranscriptId();

    m_uSeqStart = location.GetFrom();
    m_uSeqStop = location.GetTo();
    if ( parent.IsSetScore() ) {
        m_pdScore = new double( parent.Score() );
    }
    if ( parent.IsSetStrand() ) {
        m_peStrand = new ENa_strand( parent.Strand() );
    }

    m_Attributes.insert( parent.m_Attributes.begin(), parent.m_Attributes.end() );
    if ( 0 != uExonNumber ) {
        m_Attributes[ "exon_number" ] = NStr::UIntToString( uExonNumber );
    }
    return true;
};

//  ----------------------------------------------------------------------------
bool CGtfRecord::AssignFromAsn(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( ! CGff3WriteRecord::AssignFromAsn( feature ) ) {
        return false;
    }
    return true;
};

//  ----------------------------------------------------------------------------
string CGtfRecord::StrGeneId() const
//  ----------------------------------------------------------------------------
{
    return GeneId();
};

//  ----------------------------------------------------------------------------
string CGtfRecord::StrTranscriptId() const
//  ----------------------------------------------------------------------------
{
    return TranscriptId();
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

    if ( ! GeneId().empty() ) {
        strAttributes += "gene_id \"";
		strAttributes += StrGeneId();
		strAttributes += "\"";
        strAttributes += "; ";
    }

    if ( ! TranscriptId().empty() ) {
        strAttributes += "transcript_id \"";
		strAttributes += StrTranscriptId();
		strAttributes += "\"";
        strAttributes += "; ";
    }

    for ( it = attrs.begin(); it != attrs.end(); ++it ) {
        string strKey = it->first;
        if ( NStr::StartsWith( strKey, "gff_" ) ) {
            continue;
        }
        if ( strKey == "exon_number" ) {
            continue;
        }

        strAttributes += strKey;
        strAttributes += " ";
		
		bool quote = x_NeedsQuoting(it->second);
		if ( quote )
			strAttributes += '\"';		
		strAttributes += it->second;
		if ( quote )
			strAttributes += '\"';
		strAttributes += "; ";
    }
    
    it = attrs.find( "exon_number" );
    if ( it != attrs.end() ) {
        strAttributes += "exon_number \"";
        strAttributes += it->second;
        strAttributes += "\"; ";
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

    if ( ! GeneId().empty() ) {
        strAttributes += "gene_id \"";
		strAttributes += StrGeneId();
		strAttributes += "\"";
        strAttributes += "; ";
    }

    if ( ! TranscriptId().empty() ) {
        strAttributes += "transcript_id \"";
		strAttributes += StrTranscriptId();
		strAttributes += "\"";
        strAttributes += "; ";
    }

    it = attrs.find( "exon_number" );
    if ( it != attrs.end() ) {
        strAttributes += "exon_number \"";
        strAttributes += it->second;
        strAttributes += "\"; ";
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
bool CGtfRecord::x_AssignAttributesFromAsnCore(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        for ( /*NOOP*/; it != quals.end(); ++it ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "ID" ) {
                    continue;
                }
                if ( (*it)->GetQual() == "Parent" ) {
                    continue;
                }
                if ( (*it)->GetQual() == "gff_type" ) {
                    continue;
                }
                if ( (*it)->GetQual() == "transcript_id" ) {
                    m_strTranscriptId = (*it)->GetVal();
                    continue;
                }
                if ( (*it)->GetQual() == "gene_id" ) {
                    m_strGeneId = (*it)->GetVal();
                    continue;
                }
                m_Attributes[ (*it)->GetQual() ] = (*it)->GetVal();
            }
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfRecord::x_AssignAttributesFromAsnExtended(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( feature.CanGetDbxref() ) {
        const CSeq_feat::TDbxref& dbxrefs = feature.GetDbxref();
        if ( dbxrefs.size() > 0 ) {
            string value = s_GtfDbtag( *dbxrefs[ 0 ] );
            for ( size_t i=1; i < dbxrefs.size(); ++i ) {
                value += ";";
                value += s_GtfDbtag( *dbxrefs[ i ] );
            }
            m_Attributes[ "db_xref" ] = value;
        }
    }
    if ( feature.CanGetComment() ) {
        m_Attributes[ "note" ] = feature.GetComment();
    }
    if ( feature.CanGetPseudo()  &&  feature.GetPseudo() ) {
        m_Attributes[ "pseudo" ] = "";
    }
    if ( feature.CanGetPartial()  &&  feature.GetPartial() ) {
        m_Attributes[ "partial" ] = "";
    }

    switch ( feature.GetData().GetSubtype() ) {
    default:
        break;

    case CSeq_feat::TData::eSubtype_cdregion: {

            const CCdregion& cdr = feature.GetData().GetCdregion();

            if ( feature.IsSetProduct() ) {
                string strProduct = feature.GetProduct().GetId()->GetSeqIdString();
                m_Attributes[ "protein_id" ] = strProduct;
            }

            if ( feature.IsSetExcept_text() ) {
                if ( feature.GetExcept_text() == "ribosomal slippage" ) {
                    m_Attributes[ "ribosomal_slippage" ] = "";
                }
            }

            if ( feature.IsSetXref() ) {
                const vector< CRef< CSeqFeatXref > > xref = feature.GetXref();
                vector< CRef< CSeqFeatXref > >::const_iterator it = xref.begin();
                for ( ; it != xref.end(); ++it ) {
                    const CSeqFeatXref& ref = **it;
                    if ( ref.IsSetData() && ref.GetData().IsProt() && 
                        ref.GetData().GetProt().IsSetName() ) 
                    {
                        string strProduct = *( ref.GetData().GetProt().GetName().begin() );
                        m_Attributes[ "product" ] = strProduct; 
                        break;
                    }
                }
            }

            if ( cdr.IsSetCode() ) {
                string strCode = NStr::IntToString( cdr.GetCode().GetId() );
                m_Attributes[ "transl_table" ] = strCode;
            }
        }
        break;

    case CSeq_feat::TData::eSubtype_mRNA: {
            const CRNA_ref& rna = feature.GetData().GetRna();
            if ( rna.IsSetExt() && rna.GetExt().IsName() ) {
                string strProduct = rna.GetExt().GetName();
                m_Attributes[ "product" ] = strProduct;
            }
        }
        break;
    }
    return true;
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
    if ( ! IsSetPhase() ) {
        m_puPhase = new unsigned int( 0 );
    }
    if ( eStrand == eNa_strand_minus ) {
        unsigned int uTopSize = 0;
        list<CRef<CSeq_interval> >::const_iterator it = cdsLocs.begin();
        for ( ; it != cdsLocs.end(); ++it ) {
            if ( (*it)->CanGetFrom()  && (*it)->GetFrom() > m_uSeqStop ) {
                uTopSize += (*it)->GetLength();
            }
        }
        *m_puPhase = (3 - (uTopSize % 3) ) % 3;
    }
    else {
        unsigned int uBottomSize = 0;
        list<CRef<CSeq_interval> >::const_iterator it = cdsLocs.begin();
        for ( ; it != cdsLocs.end(); ++it ) {
            if ( (*it)->CanGetFrom()  &&  (*it)->GetTo() < m_uSeqStart ) {
                uBottomSize += (*it)->GetLength();
            }
        }
        *m_puPhase = (3 - (uBottomSize % 3) ) % 3;
    }
}

END_objects_SCOPE
END_NCBI_SCOPE
