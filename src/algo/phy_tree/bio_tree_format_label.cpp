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
 * Authors: Vladislav Evgeniev, Vladimir Tereshkov
 *
 * File Description: Generates node labels as a mix of dictionary ids and (optional) text in between
 *
 */

#include <ncbi_pch.hpp>
#include <algo/phy_tree/bio_tree_format_label.hpp>

using namespace std;

BEGIN_NCBI_SCOPE

CBioTreeFormatLabel::CBioTreeFormatLabel(const CBioTreeFeatureDictionary& dict, const string &format)
{
    string retval = format;

    // Tokenize the label where the result is a mix of dictionary ids
    // and (optional) text inbetween
    size_t srt_var = string::npos;
    size_t end_var = string::npos;
    string::size_type last_pos = 0;

    do {
        srt_var = retval.find("$(", last_pos);

        if (srt_var != string::npos) {
            end_var = retval.find(")", srt_var);

            if (end_var != string::npos) {
                // If another $( occurs before this one ends, ignore the current one.
                size_t embedded_key = retval.find("$(", srt_var + 1);
                if (embedded_key != string::npos && embedded_key < end_var) {
                    last_pos = embedded_key;
                    continue;
                }

                // found a key. If its in the dictionary, add the BioTreeFeatureId
                // otherwise just leave it blank.
                string key = retval.substr(srt_var + 2, (end_var - srt_var) - 2);
                key = NStr::TruncateSpaces(key);
                TBioTreeFeatureId id = dict.GetId(key);

                LabelElt elt;
                elt.m_Value = retval.substr(last_pos, (srt_var - last_pos));
                elt.m_ID = id;
                m_LabelElements.push_back(elt);

                last_pos = end_var + 1;
            }
        }
    } while (srt_var != string::npos && end_var != string::npos);

    // Add any text on the end
    if (last_pos < retval.length()) {
        LabelElt elt;
        elt.m_Value = retval.substr(last_pos, retval.length() - last_pos);
        elt.m_ID = -1;
        m_LabelElements.push_back(elt);
    }
}

string CBioTreeFormatLabel::FormatLabel(const CBioTreeFeatureList& features) const
{
    string  retval;

    ITERATE(vector<LabelElt>, it, m_LabelElements) {
        retval += (*it).m_Value;
        if ((*it).m_ID != TBioTreeFeatureId(-1))
            retval += features.GetFeatureValue((*it).m_ID);
    }

    return retval;
}

END_NCBI_SCOPE
