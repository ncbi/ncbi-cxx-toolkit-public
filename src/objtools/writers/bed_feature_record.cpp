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
 * File Description:  Write bed file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seq/Seq_annot.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objmgr/mapped_feat.hpp>

#include <objtools/writers/bed_feature_record.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define BUMP( x, y ) if ( x < (y) ) x = (y)

//  ----------------------------------------------------------------------------
CBedFeatureRecord::CBedFeatureRecord():
//  ----------------------------------------------------------------------------
    m_uColumnCount( 0 ),
    m_strChrom( "." ),
    m_strChromStart( "." ),
    m_strChromEnd( "." ),
    m_strName( "." ),
    m_strScore( "." ),
    m_strStrand( "." ),
    m_strThickStart( "." ), 
    m_strThickEnd( "." ), 
    m_strItemRgb( "." ), 
    m_strBlockCount( "." ), 
    m_strBlockSizes( "." ), 
    m_strBlockStarts( "." ) 
{
}

//  ----------------------------------------------------------------------------
CBedFeatureRecord::~CBedFeatureRecord()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
string CBedFeatureRecord::Chrom() const
//  ----------------------------------------------------------------------------
 { 
    return m_strChrom; 
}

//  ----------------------------------------------------------------------------
string CBedFeatureRecord::ChromStart() const
//  ----------------------------------------------------------------------------
{ 
    return m_strChromStart; 
}

//  ----------------------------------------------------------------------------
string CBedFeatureRecord::ChromEnd() const 
//  ----------------------------------------------------------------------------
{ 
    return m_strChromEnd; 
}

//  ----------------------------------------------------------------------------
string CBedFeatureRecord::Name() const 
//  ----------------------------------------------------------------------------
{ 
    return m_strName; 
}
        
//  ----------------------------------------------------------------------------
string CBedFeatureRecord::Score() const 
//  ----------------------------------------------------------------------------
{ 
    return m_strScore; 
}
     
//  ----------------------------------------------------------------------------
string CBedFeatureRecord::Strand() const 
//  ----------------------------------------------------------------------------
{ 
    return m_strStrand; 
}
   
//  ----------------------------------------------------------------------------
string CBedFeatureRecord::ThickStart() const 
//  ----------------------------------------------------------------------------
{ 
    return m_strThickStart; 
}   

//  ----------------------------------------------------------------------------
string CBedFeatureRecord::ThickEnd() const 
//  ----------------------------------------------------------------------------
{ 
    return m_strThickEnd; 
}
   
//  ----------------------------------------------------------------------------
string CBedFeatureRecord::ItemRgb() const  
//  ----------------------------------------------------------------------------
{ 
    return m_strItemRgb; 
}
  
//  ----------------------------------------------------------------------------
string CBedFeatureRecord::BlockCount() const   
//  ----------------------------------------------------------------------------
{ 
    return m_strBlockCount; 
}
 
//  ----------------------------------------------------------------------------
string CBedFeatureRecord::BlockSizes() const
//  ----------------------------------------------------------------------------
 { 
    return m_strBlockSizes; 
}
   
//  ----------------------------------------------------------------------------
string CBedFeatureRecord::BlockStarts() const   
//  ----------------------------------------------------------------------------
{ 
    return m_strBlockStarts; 
}

//  ----------------------------------------------------------------------------
bool CBedFeatureRecord::AssignDisplayData(
    CMappedFeat mf,
    bool bUseScore )
//  ----------------------------------------------------------------------------
{
    if ( ! mf.GetData().IsUser() ) { 
        return false;
    }
    const CUser_object& uo = mf.GetData().GetUser();
    if ( ! uo.IsSetType() || ! uo.GetType().IsStr() || 
        uo.GetType().GetStr() != "Display Data" ) {
        return false;
    }
    const vector< CRef< CUser_field > >& fields = uo.GetData();
    vector< CRef< CUser_field > >::const_iterator it = fields.begin();
    for ( ; it != fields.end(); ++it ) {
        if ( ! (*it)->CanGetLabel() || ! (*it)->GetLabel().IsStr() ) {
            continue;
        }
        string strLabel = (*it)->GetLabel().GetStr();
        if ( strLabel == "name" ) {
            BUMP( m_uColumnCount, 4 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsStr() ) {
                m_strName = (*it)->GetData().GetStr();
            }
            continue;
        }
        if ( strLabel == "score" && ! bUseScore ) {
            BUMP( m_uColumnCount, 5 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                m_strScore = NStr::UIntToString( (*it)->GetData().GetInt() );
            }
            continue;
        }
        if ( strLabel == "greylevel" && bUseScore ) {
            BUMP( m_uColumnCount, 5 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                m_strScore = NStr::UIntToString( (*it)->GetData().GetInt() );
            }
            continue;
        }
        if ( strLabel == "thickStart" ) {
            BUMP( m_uColumnCount, 7 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                m_strThickStart = NStr::UIntToString( (*it)->GetData().GetInt() + 1 );
            }
            continue;
        }
        if ( strLabel == "thickEnd" ) {
            BUMP( m_uColumnCount, 8 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                m_strThickEnd = NStr::UIntToString( (*it)->GetData().GetInt() + 1 );
            }
            continue;
        }
        if ( strLabel == "itemRGB" ) {
            BUMP( m_uColumnCount, 9 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                m_strItemRgb = NStr::UIntToString( (*it)->GetData().GetInt() );
                continue;
            }
            if ( (*it)->IsSetData() && (*it)->GetData().IsStr() ) {
                m_strItemRgb = (*it)->GetData().GetStr();
                continue;
            }
            continue;
        }
        if ( strLabel == "blockCount" ) {
            BUMP( m_uColumnCount, 10 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                m_strBlockCount = NStr::UIntToString( (*it)->GetData().GetInt() );
            }
            continue;
        }
        if ( strLabel == "blockSizes" ) {
            BUMP( m_uColumnCount, 11 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsStr() ) {
                m_strBlockSizes = (*it)->GetData().GetStr();
            }
            continue;
        }
        if ( strLabel == "blockStarts" ) {
            BUMP( m_uColumnCount, 12 );
            if ( (*it)->IsSetData() && (*it)->GetData().IsStr() ) {
                m_strBlockStarts = (*it)->GetData().GetStr();
            }
            continue;
        }
    }
    return true;

}

//  ----------------------------------------------------------------------------
bool CBedFeatureRecord::AssignLocation(
    const CSeq_interval& interval )
//  ----------------------------------------------------------------------------
{
    if ( interval.CanGetId() ) {
        m_strChrom = interval.GetId().GetSeqIdString();
    }
    if ( interval.IsSetFrom() ) {
        m_strChromStart = NStr::UIntToString( interval.GetFrom() + 1 );
    }
    if ( interval.IsSetTo() ) {
        m_strChromEnd = NStr::UIntToString( interval.GetTo() + 1 );
    }
    BUMP( m_uColumnCount, 3 );
    if ( interval.IsSetStrand() ) {
        switch ( interval.GetStrand() ) {
        default:
            break;
        case eNa_strand_plus:
            m_strStrand = "+";
            break;
        case eNa_strand_minus:
            m_strStrand = "-";
            break;
        }
        BUMP( m_uColumnCount, 6 );
    }
    return true;
}

END_NCBI_SCOPE
