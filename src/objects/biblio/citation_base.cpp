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
#include <objects/biblio/citation_base.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Author.hpp>

#include <typeinfo>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

bool ICitationBase::GetLabel(string* label, TLabelFlags flags,
                             ELabelVersion version) const
{
    _ASSERT(label != NULL);
    if (version < eLabel_MinVersion  ||  version > eLabel_MaxVersion) {
        ERR_POST(Warning << "Unsupported citation label version " << version
                 << "; substituting default (" << eLabel_DefaultVersion << ')');
        version = eLabel_DefaultVersion;
    }
    switch (version) {
    case eLabel_V1:  return GetLabelV1(label, flags);
    case eLabel_V2:  return GetLabelV2(label, flags);
    default:         _TROUBLE;
    }
    return false;
}


string ICitationBase::FixPages(const string& raw_orig_pages)
{
    if (raw_orig_pages.empty()) {
        return kEmptyStr;
    }

    SIZE_TYPE hyphen_pos = NPOS, lhs_digit_pos = NPOS, lhs_letter_pos = NPOS,
              digit_pos = NPOS, letter_pos = NPOS;

    CTempString orig_pages = NStr::TruncateSpaces_Unsafe(raw_orig_pages);
    for (SIZE_TYPE pos = 0;  pos < orig_pages.size();  ++pos) {
        char c = orig_pages[pos];
        if (c >= '0'  &&  c <= '9') {
            if (digit_pos == NPOS) {
                digit_pos = pos;
            } else if (letter_pos > digit_pos  &&  letter_pos != NPOS) {
                return orig_pages; // invalid -- letters on either side
            }
        } else if ((c >= 'A' && c <= 'Z')  ||  (c >= 'a' && c <= 'z')) {
            if (letter_pos == NPOS) {
                letter_pos = pos;
            } else if (digit_pos > letter_pos  &&  digit_pos != NPOS) {
                return orig_pages; // invalid -- digits on either side
            }
        } else if (c == '-'  &&  digit_pos != NPOS  &&  hyphen_pos == NPOS) {
            hyphen_pos = pos;
            lhs_digit_pos = digit_pos;
            lhs_letter_pos = letter_pos;
            digit_pos = letter_pos = NPOS;
        } else {
            return orig_pages;
        }
    }

    CTempString lhs(orig_pages, 0, hyphen_pos);
    if (lhs == orig_pages.substr(hyphen_pos + 1)) {
        return lhs;
    }

    if (lhs_letter_pos > 0  &&  lhs_letter_pos != NPOS) {
        _ASSERT(lhs_digit_pos == 0);
        // Complex LHS, digits first; canonicalize 12a-c case (with a
        // single letter on each side), otherwise leave alone apart
        // from collapsing trivial (single-page) ranges.
        if (lhs_letter_pos == hyphen_pos - 1
            &&  letter_pos == hyphen_pos + 1
            &&  orig_pages.size() == letter_pos + 1) {
            int diff = orig_pages[letter_pos] - orig_pages[lhs_letter_pos];
            if (diff == 0) {
                return lhs;
            } else if (diff > 0) {
                string result(orig_pages, 0, letter_pos);
                result.append(orig_pages, 0, lhs_letter_pos);
                result += orig_pages[letter_pos];
                return result;
            }
        }
    } else if (letter_pos == NPOS  &&  digit_pos != NPOS) {
        // At this point, any letters on the LHS are known to precede
        // its digits; as such, if the RHS consists solely of digits
        // (checked just now), it may be subject to canonicalization.
        // (Should this reject page numbers starting with multiple letters?)
        CTempString lhs_digits(lhs, lhs_digit_pos), rhs(orig_pages, digit_pos);
        if (NStr::EndsWith(lhs, rhs)) {
            return lhs;
        } else if (lhs_digits.size() >= rhs.size()) {
            SIZE_TYPE lhs_tail_pos = lhs.size() - rhs.size();
            if (lhs.substr(lhs_tail_pos) < rhs) {
                string result(orig_pages, 0, hyphen_pos + 1);
                result.append(lhs, 0, lhs_tail_pos);
                result.append(rhs);
                return result;
            }
        } else if (lhs_letter_pos != NPOS
                   &&  rhs.size() > hyphen_pos - lhs_digit_pos) {
            // Handle A9-10 and the like.
            _ASSERT(lhs_letter_pos == 0);
            string result(orig_pages, 0, hyphen_pos + 1);
            result.append(lhs.substr(0, lhs_digit_pos));
            result.append(rhs);
            return result;
        }
    }

    return orig_pages;
}


string ICitationBase::GetParenthesizedYear(const CDate& date)
{
    if (date.IsStd()) {
        string year;
        date.GetDate(&year, "(%4Y)");
        return year;
    } else if (date.IsStr()  &&  HasText(date.GetStr())
               &&  date.GetStr() != "?") {
        return '(' + date.GetStr().substr(0, 4) + ')';
    } else {
        return kEmptyStr;
    }
}


void ICitationBase::NoteSup(string* label, const CImprint& ip)
{
    _ASSERT(label != NULL);

    const string* issue     = ip.CanGetIssue()     ? &ip.GetIssue()     : NULL;
    const string* part_sup  = ip.CanGetPart_sup()  ? &ip.GetPart_sup()  : NULL;
    const string* part_supi = ip.CanGetPart_supi() ? &ip.GetPart_supi() : NULL;

    if (HasText(part_sup)) {
        MaybeAddSpace(label);
        *label += *part_sup;
    }
    if (HasText(issue)  ||  HasText(part_supi)) {
        MaybeAddSpace(label);
        *label += '(';
        if (HasText(issue)) {
            *label += *issue;
        }
        if (HasText(part_supi)) {
            *label += ' ' + *part_supi;
        }
        *label += ')';
    }
}


bool ICitationBase::x_GetLabelV1(string*            label,
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
        authors->GetLabel(label, 0, eLabel_V1);
    }

    if (date) {
        MaybeAddSpace(label);
        *label += '(' + *date + ") ";
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
            *label += "(in) " + *title2 + " ";
        } else if (title1) {
            *label += *title1 + *title2 + " ";
        }
        else {
            *label += *title2 + " ";
        }
    }

    if (volume) {
        if (part_sup) {
            *label += *volume + *part_sup + ":";
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

    // If unique parameter true, then add unique tag to end of label
    // constructed from the first character of each whitespace separated
    // word in titleunique
    if (unique) {
        //string tag;
        // NB: add '|' even if tag is empty to maintain backward compatibility.
        *label += '|';
        if (titleunique  &&  !titleunique->empty()) {
#if 0
            CNcbiIstrstream is(titleunique->c_str(), titleunique->size());
            string word;
            int cnt = 0;
            while ( (is >> word) && (cnt++ < 40) ) {
                *label += word[0];
            }
#else
            size_t pos;
            CTempString temp = *titleunique;
            CTempString space(" \t\r\n");
            while ( (pos = temp.find_first_not_of(space)) != CTempString::npos)
            {
                *label += temp[pos];
                temp.assign(temp, pos, temp.size() - pos);
                pos = temp.find_first_of(space);
                if (pos != CTempString::npos)
                    temp.assign(temp, pos, temp.size() - pos);
                else
                    break;
            }
#endif
        }
    }

    return true;
}


END_objects_SCOPE
END_NCBI_SCOPE
