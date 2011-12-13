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
 * File Description:  BED file track line data
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seq/Seq_annot.hpp>

#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>

#include <objtools/writers/bed_track_record.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CBedTrackRecord::CBedTrackRecord()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
CBedTrackRecord::~CBedTrackRecord()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
bool CBedTrackRecord::UseScore() const
//  ----------------------------------------------------------------------------
{ 
    return ( m_strUseScore == "1" ); 
}

//  ----------------------------------------------------------------------------
string CBedTrackRecord::Name() const 
//  ----------------------------------------------------------------------------
{ 
    return m_strName; 
}

//  ----------------------------------------------------------------------------
string CBedTrackRecord::Title() const 
//  ----------------------------------------------------------------------------
{ 
    return m_strTitle; 
}

//  ----------------------------------------------------------------------------
string CBedTrackRecord::Color() const 
//  ----------------------------------------------------------------------------
{ 
    return m_strColor; 
}

//  ----------------------------------------------------------------------------
string CBedTrackRecord::Visibility() const 
//  ----------------------------------------------------------------------------
{ 
    return m_strVisibility; 
}

//  ----------------------------------------------------------------------------
bool CBedTrackRecord::ItemRgb() const
//  ----------------------------------------------------------------------------
{        
    return ( m_strItemRgb == "on" ); 
}

//  ----------------------------------------------------------------------------
bool CBedTrackRecord::Assign(
    const CSeq_annot& annot ) 
//  ----------------------------------------------------------------------------
{
    if ( ! annot.IsSetDesc() ) {
        return true; // results in a dummy track line without any directives
    }
    list< CRef< CAnnotdesc > > fields = annot.GetDesc().Get();
    list< CRef< CAnnotdesc > >::const_iterator it = fields.begin();
    for ( ; it != fields.end(); ++it ) {
        if ( (*it)->IsTitle() ) {
            m_strTitle = (*it)->GetTitle();
            continue;
        }
        if ( (*it)->IsName() ) {
            m_strName = (*it)->GetName();
            continue;
        } 
        if ( (*it)->IsUser() ) {
            const CUser_object& uo = (*it)->GetUser();
            if ( ! uo.IsSetType() || ! uo.GetType().IsStr() || 
                uo.GetType().GetStr() != "Track Data" ) {
                continue;
            }
            const vector< CRef< CUser_field > >& fields = uo.GetData();
            vector< CRef< CUser_field > >::const_iterator it = fields.begin();
            for ( ; it != fields.end(); ++it ) {
                if ( ! (*it)->CanGetLabel() || ! (*it)->GetLabel().IsStr() ) {
                    continue;
                }
                string strLabel = (*it)->GetLabel().GetStr();
                if ( strLabel == "useScore" ) {
                    if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                        m_strUseScore = NStr::UIntToString( 
                            (*it)->GetData().GetInt() );
                    }
                    continue;
                }
                if ( strLabel == "color" ) {
                    if ( (*it)->IsSetData() && (*it)->GetData().IsStr() ) {
                        m_strColor = (*it)->GetData().GetStr();
                    }
                    continue;
                }   
                if ( strLabel == "visibility" ) {
                    if ( (*it)->IsSetData() && (*it)->GetData().IsInt() ) {
                        m_strVisibility = NStr::UIntToString( 
                            (*it)->GetData().GetInt() );
                    }
                    continue;
                }
                if ( strLabel == "itemRGB" ) {
                    if ( (*it)->IsSetData() && (*it)->GetData().IsStr() ) {
                        m_strColor = (*it)->GetData().GetStr();
                    }
                    continue;
                }   
            }
        }
    }
    return true;
}

END_NCBI_SCOPE
