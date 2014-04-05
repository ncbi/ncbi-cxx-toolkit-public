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
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>

#include <objtools/writers/bed_track_record.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
bool CBedTrackRecord::Assign(
    const CSeq_annot& annot)
//
//  The only way there could be BED specific track data to a Seq-annot is that
//  the Seq-annot started out its like as BED data, then got converted to a
//  Genbank Seq-annot with the track line information preserved.
//  If that's the case then the preserved information can be found in a user
//  object "Track Data" among the Seq-annot's descriptors.
//  ----------------------------------------------------------------------------
{
    if (!annot.IsSetDesc()) {
        return true; // results in a dummy track line without any directives
    }
    list< CRef< CAnnotdesc > > fields = annot.GetDesc().Get();
    list< CRef< CAnnotdesc > >::const_iterator it = fields.begin();
    for ( ; it != fields.end(); ++it) {
        if ((*it)->IsTitle()) {
            m_strTitle = (*it)->GetTitle();
            continue;
        }
        if ((*it)->IsName()) {
            m_strName = (*it)->GetName();
            continue;
        }
        if ( (*it)->IsUser() ) {
            const CUser_object& uo = (*it)->GetUser();
            if (!uo.IsSetType() || !uo.GetType().IsStr() || 
                    uo.GetType().GetStr() == "Track Data") {
                if (!xImportTrackData(uo)) {
                    return false;
                }
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedTrackRecord::xImportTrackData(
    const CUser_object& uo)
//  ----------------------------------------------------------------------------
{
    const vector< CRef< CUser_field > >& fields = uo.GetData();
    vector<CRef< CUser_field> >::const_iterator it = fields.begin();
    for (; it != fields.end(); ++it) {
        if (!(*it)->CanGetLabel() || ! (*it)->GetLabel().IsStr()) {
            continue;
        }
        string strLabel = (*it)->GetLabel().GetStr();
        if (strLabel == "useScore") {
            if ((*it)->IsSetData() && (*it)->GetData().IsInt()) {
                m_strUseScore = NStr::UIntToString( 
                    (*it)->GetData().GetInt() );
            }
            continue;
        }
        if (strLabel == "color") {
            if ((*it)->IsSetData() && (*it)->GetData().IsStr()) {
                m_strColor = (*it)->GetData().GetStr();
            }
            continue;
        }   
        if (strLabel == "visibility") {
            if ((*it)->IsSetData() && (*it)->GetData().IsInt()) {
                m_strVisibility = NStr::UIntToString( 
                    (*it)->GetData().GetInt());
            }
            continue;
        }
        if (strLabel == "itemRGB") {
            if ((*it)->IsSetData() && (*it)->GetData().IsStr()) {
                m_strColor = (*it)->GetData().GetStr();
            }
            continue;
        } 
    }  
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedTrackRecord::Write(
    CNcbiOstream& ostr )
//  ----------------------------------------------------------------------------
{
    ostr << "track";
    if (!Name().empty()) {
        ostr << " name=\"" << Name() << "\"";
    }
    if (!Title().empty()) {
        ostr << " title=\"" << Title() << "\"";
    }
    if (UseScore()) {
        ostr << " useScore=1";
    }
    if (ItemRgb()) {
        ostr << " itemRgb=\"on\"";
    }
    if (!Color().empty()) {
        ostr << " color=\"" << Color() << "\"";
    }
    if (!Visibility().empty()) {
        ostr << " visibility=" << Visibility();
    }
    ostr << '\n';
    return true;
}


END_NCBI_SCOPE
