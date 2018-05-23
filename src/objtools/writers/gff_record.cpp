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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objtools/writers/gff_record.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGffRecord::CGffRecord():
//  ----------------------------------------------------------------------------
    m_strSeqId( "<FIX ME>" ),
    m_strSource( "<FIX ME>" ),
    m_strType( "<FIX ME>" ),
    m_strStart( "<FIX ME>" ),
    m_strEnd( "<FIX ME>" ),
    m_strScore( "<FIX ME>" ),
    m_strStrand( "<FIX ME>" ),
    m_strPhase( "<FIX ME>" ),
    m_strAttributes( "<FIX ME>" )
{
}

//  ----------------------------------------------------------------------------
bool CGffRecord::AssignSeqId(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    m_strSeqId = "<unknown>";

    if ( feature.CanGetLocation() ) {
        const CSeq_loc& location = feature.GetLocation();
        const CSeq_id* pId = location.GetId();
        switch ( pId->Which() ) {
            
            case CSeq_id::e_Local:
                if ( pId->GetLocal().IsId() ) {
                    m_strSeqId = NStr::UIntToString( pId->GetLocal().GetId() );
                }
                else {
                    m_strSeqId = pId->GetLocal().GetStr();
                }
                break;

            case CSeq_id::e_Gi:
                m_strSeqId = NStr::IntToString( pId->GetGi() );
                break;

            case CSeq_id::e_Other:
                if ( pId->GetOther().CanGetAccession() ) {
                    m_strSeqId = pId->GetOther().GetAccession();
                    if ( pId->GetOther().CanGetVersion() ) {
                        m_strSeqId += ".";
                        m_strSeqId += NStr::UIntToString( 
                            pId->GetOther().GetVersion() ); 
                    }
                }
                break;

            default:
                break;
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGffRecord::AssignType(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    m_strType = "region";

    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "standard_name" ) {
                    m_strType = (*it)->GetVal();
                    return true;
                }
            }
            ++it;
        }
    }

    if ( ! feature.CanGetData() ) {
        return true;
    }

    switch ( feature.GetData().GetSubtype() ) {
    default:
        m_strType = feature.GetData().GetKey();
        break;

    case CSeq_feat::TData::eSubtype_gene:
        m_strType = "gene";
        break;

    case CSeq_feat::TData::eSubtype_cdregion:
        m_strType = "CDS";
        break;

    case CSeq_feat::TData::eSubtype_mRNA:
        m_strType = "mRNA";
        break;

    case CSeq_feat::TData::eSubtype_scRNA:
        m_strType = "scRNA";
        break;

    case CSeq_feat::TData::eSubtype_exon:
        m_strType = "exon";
        break;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffRecord::AssignStart(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( feature.CanGetLocation() ) {
        const CSeq_loc& location = feature.GetLocation();
        unsigned int uStart = location.GetStart( eExtreme_Positional ) + 1;
        m_strStart = NStr::UIntToString( uStart );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffRecord::AssignStop(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( feature.CanGetLocation() ) {
        const CSeq_loc& location = feature.GetLocation();
        unsigned int uEnd = location.GetStop( eExtreme_Positional ) + 1;
        m_strEnd = NStr::UIntToString( uEnd );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffRecord::AssignSource(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    m_strSource = ".";

    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "source" ) {
                    m_strSource = (*it)->GetVal();
                    return true;
                }
            }
            ++it;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffRecord::AssignScore(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    m_strScore = ".";

    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "score" ) {
                    m_strScore = (*it)->GetVal();
                    return true;
                }
            }
            ++it;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffRecord::AssignStrand(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    m_strStrand = ".";
    if ( feature.CanGetLocation() ) {
        const CSeq_loc& location = feature.GetLocation();
        ENa_strand strand = location.GetStrand();
        switch( strand ) {
        default:
            break;
        case eNa_strand_plus:
            m_strStrand = "+";
            break;
        case eNa_strand_minus:
            m_strStrand = "-";
            break;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffRecord::AssignPhase(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    m_strPhase = ".";

    if ( ! feature.CanGetData() ) {
        return true;
    }
    const CSeq_feat::TData& data = feature.GetData();
    if ( data.GetSubtype() != CSeq_feat::TData::eSubtype_cdregion ) {
        return true;
    }

    const CCdregion& cdr = data.GetCdregion();
    CCdregion::TFrame frame = cdr.GetFrame();
    switch ( frame ) {
    default:
        break;
    case CCdregion::eFrame_one:
        m_strPhase = "0";
        break;
    case CCdregion::eFrame_two:
        m_strPhase = "1";
        break;
    case CCdregion::eFrame_three:
        m_strPhase = "2";
        break;
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGffRecord::AssignAttributesCore(
    const CSeq_annot& annot,
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    m_strAttributes = "";

    // If feature ids are present then they are likely used to show parent/child
    // relationships, via corresponding xrefs. Thus, any feature ids override
    // gb ID tags (feature ids and ID tags should agree in the first place, but
    // if not, feature ids must trump ID tags).
    //
    bool bIdAssigned = false;

    if ( feature.CanGetId() ) {
        const CSeq_feat::TId& id = feature.GetId();
        string value = CGffRecord::FeatIdString( id );
        AddAttribute( "ID", value );
        bIdAssigned = true;
    }

    if ( feature.CanGetXref() ) {
        const CSeq_feat::TXref& xref = feature.GetXref();
        string value;
        for ( size_t i=0; i < xref.size(); ++i ) {
//            const CSeqFeatXref& ref = *xref[i];
            if ( xref[i]->CanGetId() && xref[i]->CanGetData() ) {
                const CSeqFeatXref::TId& id = xref[i]->GetId();
                CSeq_feat::TData::ESubtype other_type = GetSubtypeOf( annot, id );
                if ( ! IsParentOf( other_type, feature.GetData().GetSubtype() ) ) {
                    continue;
                }
                if ( ! value.empty() ) {
                    value += ",";
                }
                value += CGffRecord::FeatIdString( id );
            }
        }
        if ( ! value.empty() ) {
            AddAttribute( "Parent", value );
        }
    }

    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "ID" ) {
                    if ( ! bIdAssigned ) {
                        AddAttribute( "ID", (*it)->GetVal() );
                    }
                }
                if ( (*it)->GetQual() == "Name" ) {
                    AddAttribute( "Name", (*it)->GetVal() );
                }
                if ( (*it)->GetQual() == "Var_type" ) {
                    AddAttribute( "Var_type", (*it)->GetVal() );
                }
            }
            ++it;
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGffRecord::AssignAttributesExtended(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( feature.CanGetDbxref() ) {
        const CSeq_feat::TDbxref& dbxrefs = feature.GetDbxref();
        if ( dbxrefs.size() > 0 ) {
            string value;
            dbxrefs[0]->GetLabel( &value );
            for ( size_t i=1; i < dbxrefs.size(); ++i ) {
                string label;
                dbxrefs[i]->GetLabel( &label );
                value += ",";
                value += label;
            }
            AddAttribute( "Dbxref", value );
        }
    }
    if ( feature.CanGetComment() ) {
        AddAttribute( "comment", feature.GetComment() );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffRecord::SetRecord(
    const CSeq_annot& annot,
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( ! AssignType( feature ) ) {
        return false;
    }
    if ( ! AssignSeqId( feature ) ) {
        return false;
    }
    if ( ! AssignSource( feature ) ) {
        return false;
    }
    if ( ! AssignStart( feature ) ) {
        return false;
    }
    if ( ! AssignStop( feature ) ) {
        return false;
    }
    if ( ! AssignScore( feature ) ) {
        return false;
    }
    if ( ! AssignStrand( feature ) ) {
        return false;
    }
    if ( ! AssignPhase( feature ) ) {
        return false;
    }
    if ( ! AssignAttributesCore( annot, feature ) ) {
        return false;
    }
    if ( ! AssignAttributesExtended( feature ) ) {
        return false;
    }

    return true;
}

//  ----------------------------------------------------------------------------
void CGffRecord::DumpRecord(
    CNcbiOstream& out )
//  ----------------------------------------------------------------------------
{
    out << m_strSeqId + '\t';
    out << m_strSource << '\t';
    out << m_strType << '\t';
    out << m_strStart << '\t';
    out << m_strEnd << '\t';
    out << m_strScore << '\t';
    out << m_strStrand << '\t';
    out << m_strPhase << '\t';
    out << m_strAttributes << '\n';
}

//  ----------------------------------------------------------------------------
void CGffRecord::AddAttribute(
    const string& key,
    const string& value )
//  ----------------------------------------------------------------------------
{
    if ( ! m_strAttributes.empty() ) {
        m_strAttributes += ";";
    }
    m_strAttributes += key;
    m_strAttributes += "=\"";
    m_strAttributes += value;
    m_strAttributes += "\"";
}
    
//  ----------------------------------------------------------------------------
string CGffRecord::FeatIdString(
    const CFeat_id& id )
//  ----------------------------------------------------------------------------
{
    switch ( id.Which() ) {
    default:
        break;

    case CFeat_id::e_Local: {
        const CFeat_id::TLocal& local = id.GetLocal();
        if ( local.IsId() ) {
            return NStr::IntToString( local.GetId() );
        }
        if ( local.IsStr() ) {
            return local.GetStr();
        }
        break;
        }
    }
    return "FEATID";
}

//  ----------------------------------------------------------------------------
CSeq_feat::TData::ESubtype CGffRecord::GetSubtypeOf(
    const CSeq_annot& annot,
    const CFeat_id& id )
//  ----------------------------------------------------------------------------
{
    const list< CRef< CSeq_feat > >& table = annot.GetData().GetFtable();
    list< CRef< CSeq_feat > >::const_iterator it = table.begin();
    while ( it != table.end() ) {
        if ( (*it)->CanGetId() && (*it)->CanGetData() ) {
            if ( id.Equals( (*it)->GetId() ) ) {
                return (*it)->GetData().GetSubtype();
            }
        }
        ++it;
    }
    return CSeq_feat::TData::eSubtype_bad;
}

//  ----------------------------------------------------------------------------
bool CGffRecord::IsParentOf(
    CSeq_feat::TData::ESubtype maybe_parent,
    CSeq_feat::TData::ESubtype maybe_child )
//  ----------------------------------------------------------------------------
{
    switch ( maybe_parent ) {
    default:
        return false;

    case CSeq_feat::TData::eSubtype_10_signal:
    case CSeq_feat::TData::eSubtype_35_signal:
    case CSeq_feat::TData::eSubtype_3UTR:
    case CSeq_feat::TData::eSubtype_5UTR:
        return false;

    case CSeq_feat::TData::eSubtype_operon:
        switch ( maybe_child ) {

        case CSeq_feat::TData::eSubtype_gene:
        case CSeq_feat::TData::eSubtype_promoter:
            return true;

        default:
            return IsParentOf( CSeq_feat::TData::eSubtype_gene, maybe_child ) ||
                IsParentOf( CSeq_feat::TData::eSubtype_promoter, maybe_child );
        }

    case CSeq_feat::TData::eSubtype_gene:
        switch ( maybe_child ) {

        case CSeq_feat::TData::eSubtype_intron:
        case CSeq_feat::TData::eSubtype_mRNA:
            return true;

        default:
            return IsParentOf( CSeq_feat::TData::eSubtype_intron, maybe_child ) ||
                IsParentOf( CSeq_feat::TData::eSubtype_mRNA, maybe_child );
        }

    case CSeq_feat::TData::eSubtype_cdregion:
        switch ( maybe_child ) {

        case CSeq_feat::TData::eSubtype_exon:
            return true;

        default:
            return IsParentOf( CSeq_feat::TData::eSubtype_exon, maybe_child );
        }
    }

    return false;
}

END_NCBI_SCOPE
