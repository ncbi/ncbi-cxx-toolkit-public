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
    auto fields = annot.GetDesc().Get();
    for (auto field: fields) {
        switch(field->Which()) {
        default:
            continue;
        case CAnnotdesc::e_Name:
            mName = field->GetName();
            continue;
        case CAnnotdesc::e_Title:
            mTitle = field->GetTitle();
            continue;
        case CAnnotdesc::e_User: {
                const auto& uo = field->GetUser();
                if (!uo.IsSetType()  ||  !uo.GetType().IsStr()  ||
                        uo.GetType().GetStr() != "Track Data") {
                    continue;
                }
                xImportKeyValuePairs(uo);
            }
            continue;
        }
    }
    // synchronize top leven name/title with what's in the key/value pairs:
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedTrackRecord::xImportKeyValuePairs(
    const CUser_object& uo)
//  ----------------------------------------------------------------------------
{
    const auto& fields = uo.GetData();
    for (auto field: fields) {
        if (!field->CanGetLabel() || ! field->GetLabel().IsStr()) {
            continue;
        }
        if (!field->CanGetData()) {
            continue;
        }
        auto key = field->GetLabel().GetStr();

        const auto& value = field->GetData();
        string valueStr;
        switch (value.Which()) {
        default:
            break;
        case CUser_field::C_Data::e_Str:
            valueStr = value.GetStr();
            break;
        case CUser_field::C_Data::e_Int:
            valueStr = NStr::IntToString(value.GetInt());
            break;
        case CUser_field::C_Data::e_Real:
            valueStr = NStr::DoubleToString(value.GetReal());
            break;
        }
        if (valueStr.empty()) {
            continue;
        }
        mKeyValuePairs[key] = valueStr;
    }
    return true;
}

//  ----------------------------------------------------------------------------
string
CBedTrackRecord::xGetKeyValue(
    const string& key) const
//  ----------------------------------------------------------------------------
{
    auto val = mKeyValuePairs.find(key);
    if (val == mKeyValuePairs.end()) {
        return "";
    }
    return val->second;
}

//  ----------------------------------------------------------------------------
bool CBedTrackRecord::Write(
    CNcbiOstream& ostr )
//  ----------------------------------------------------------------------------
{
    if (mName.empty()  &&  mTitle.empty()  && mKeyValuePairs.empty()) {
        //nothing there to write
        return true;
    }
    auto name = xGetKeyValue("name");
    if (name.empty()) {
        name = mName;
    }
    auto description = xGetKeyValue("description");
    if (description.empty()) {
        description = mTitle;
    }
    ostr << "track";
    if (!name.empty()) {
        ostr << " name=\"" << name << "\"";
    }
    if (!description.empty()) {
        ostr << " description=\"" << description << "\"";
    }
    for (auto pair: mKeyValuePairs) {
        auto key = pair.first;
        auto value = pair.second;

        if (key == "name") {
            continue;
        }
        if (key == "description") {
            continue;
        }
        string quotesym = "\"";
        if (NStr::StartsWith(value, "\"")  ||  string::npos == value.find(" ")) {
            quotesym = "";
        }
        ostr << " " << key << "=" << quotesym << value << quotesym;
    }
    ostr << "\n";
    return true;
}


//  ----------------------------------------------------------------------------
bool
CBedTrackRecord::UseScore() const
//  ----------------------------------------------------------------------------
{
    auto useScore = xGetKeyValue("useScore");
    if (useScore.empty()) {
        return false;
    }
    return (useScore == "1");
}


END_NCBI_SCOPE
