 /*$Id$
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
 * Author:  Clifford Clausen, Aleksey Grichenko
 *          (moved from CPub class)
 *
 * File Description:
 *   utility functions for GetLabel()
 *
 */  

#include <ncbi_pch.hpp>
#include <objects/biblio/label_util.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Author.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


void GetLabelContent(string*            label,
                     bool               unique,
                     const CAuth_list*  authors,
                     const CImprint*    imprint,
                     const CTitle*      title,
                     const CCit_book*   book,
                     const CCit_jour*   /* journal */,
                     const string*      title1,
                     const string*      title2,
                     const string*      titleunique,
                     const string*      date,
                     const string*      volume,
                     const string*      issue,
                     const string*      pages,
                     bool               unpublished)
{
    const string* part_sup = 0;
    const string* part_supi = 0;
    string subst_date;
    if (imprint) {
        if ( !date ) {
            imprint->GetDate().GetDate(&subst_date);
            date = &subst_date;
        }
        volume = !volume && imprint->IsSetVolume() ?
            &imprint->GetVolume() : volume;
        issue = !issue && imprint->IsSetIssue() ? &imprint->GetIssue() : issue;
        pages = !pages && imprint->IsSetPages() ? &imprint->GetPages() : pages;
        part_sup = imprint->IsSetPart_sup() ? &imprint->GetPart_sup() : 0;
        part_supi = imprint->IsSetPart_supi() ? &imprint->GetPart_supi() : 0;
    }

    if (authors) {
        switch (authors->GetNames().Which()) {
        case CAuth_list::C_Names::e_Std:
            if (authors->GetNames().GetStd().size() > 0) {
                const CPerson_id& id = 
                    authors->GetNames().GetStd().front()->GetName();
                    id.GetLabel(label);
            }
            break;
        case CAuth_list::C_Names::e_Ml:
            if (authors->GetNames().GetMl().size() > 0) {
                *label += authors->GetNames().GetMl().front();
            }
            break;
        case CAuth_list::C_Names::e_Str:
            if (authors->GetNames().GetStr().size() > 0) {
                *label += authors->GetNames().GetStr().front();
            }
            break;
        default:
            break;
        }
    }

    string::size_type z = label->size();
    if (date) {
        if (z == 0 || label->substr(z-1, 1).compare(" ") == 0) {
          *label += "(";
        }
        else {
          *label += " (";
        }
        *label += *date + ") ";
    }

    if (title && !titleunique) {
        try {
            titleunique = &title->GetTitle();
        } catch (exception&) {}
    }

    if (title && !title2) {
        try {
            title2 = &title->GetTitle();
        } catch (exception&) {}
    }

    if (title2) {
        if (book) {
            *label += "(in) " + *title2;
        }
        else if (title1) {
            *label += *title1 + *title2 + " ";
        }
        else {
            *label += *title2 + " ";
        }
    }

    if (volume) {
        if (part_sup) {
            *label += *volume + *part_sup;
        }
        else {
            *label += *volume + ":";
        }
    }

    if (issue) {
        if (part_supi) {
            *label += "(" + *issue + *part_supi + ")";
        }
        else {
            *label += "(" + *issue + ")";
        }
    }

    if (pages) {
        *label += *pages;
    }

    if (unpublished) {
        *label += "Unpublished";
    }

    // If unique paramter true, then add unique tag to end of label
    // constructed from the first character of each whitespace separated
    // word in titleunique
    if (unique) {
        string tag;
        if (titleunique  &&  !titleunique->empty()) {
            CNcbiIstrstream is(titleunique->c_str(), titleunique->size());
            string word;
            int cnt = 0;
            while ( (is >> word) && (cnt++ < 40) ) {
                tag += word[0];
            }
        }
        // NB: add '|' even if tag is empty to maintain backward compatibility.
        *label += "|" + tag;
    }
}


END_objects_SCOPE
END_NCBI_SCOPE

/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.7  2005/02/02 19:49:54  grichenk
 * Fixed more warnings
 *
 * Revision 1.6  2004/10/22 17:54:49  shomrat
 * Bug fix
 *
 * Revision 1.5  2004/10/22 14:16:27  shomrat
 * fixed generation of unique label
 *
 * Revision 1.4  2004/09/29 14:05:34  shomrat
 * Fixes to match GetLabelContent to C version.
 *
 * Revision 1.3  2004/08/18 13:36:35  shomrat
 * Fixed GetLabelContent to match C version
 *
 * Revision 1.2  2004/05/19 17:18:17  gorelenk
 * Added include of PCH - ncbi_pch.hpp
 *
 * Revision 1.1  2004/02/24 15:52:25  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */
