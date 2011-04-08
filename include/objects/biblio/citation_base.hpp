#ifndef OBJECTS_BIBLIO___CITATION_BASE__HPP
#define OBJECTS_BIBLIO___CITATION_BASE__HPP

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
 * Authors:  Aaron Ucko, Cliff Clausen
 *
 */

/// @file citation_base.hpp
/// "Mix-in" interfaces to implement GetLabel for CCit_* et al.

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

#ifndef BEGIN_objects_SCOPE
#  define BEGIN_objects_SCOPE BEGIN_SCOPE(objects)
#  define END_objects_SCOPE END_SCOPE(objects)
#endif
BEGIN_objects_SCOPE // namespace ncbi::objects::

class CAuth_list;
class CDate;
class CImprint;
class CTitle;
class CCit_book;
class CCit_jour;

/// Basic citation GetLabel interface, suitable both for actual
/// citation objects such as CCit_* and containers such as CPub.
class NCBI_BIBLIO_EXPORT IAbstractCitation
{
public:
    virtual ~IAbstractCitation() { }

    /// Flags for use by GetLabel methods.
    enum ELabelFlags {
        fLabel_Unique       = 1 <<  0, ///< Append a unique tag [V1]
        fLabel_FlatNCBI     = 1 <<  1, ///< For GenBank or GenPept [V2]
        fLabel_FlatEMBL     = 1 <<  2, ///< For EMBL or EMBLPept [V2]
        fLabel_ISO_JTA      = 1 <<  3, ///< Only ISO jnl. title abbrevs. OK [V2]
        fLabel_NoBadCitGen  = 1 <<  4, ///< Ignore "bad" Cit-gen data [V2]
        fLabel_NoUnpubAffil = 1 <<  5, ///< No affil on unpublished Cit-gen [V2]
        fLabel_Consortia    = 1 << 30  ///< Consortia, not authors [internal]
    };
    typedef int TLabelFlags; ///< binary OR of ELabelFlags

    enum ELabelVersion {
        /// Traditional GetLabel semantics, modeled on the C Toolkit's
        /// PubLabelUnique.  Version 1 labels typically indicate item
        /// authorship, and optionally feature abbreviated item titles.
        eLabel_V1 = 1,
        /// New implementation, in line with GenBank/GenPept REFERENCE
        /// JOURNAL fields and the like.  One difference (among many!)
        /// between version 1 and 2 labels is that the latter generally
        /// leave off item-specific author and title information, which
        /// would appear in neighboring flat-file fields.
        eLabel_V2 = 2,
        eLabel_MinVersion     = eLabel_V1, ///< Minimum supported version
        eLabel_DefaultVersion = eLabel_V1, ///< Current default version
        eLabel_MaxVersion     = eLabel_V2  ///< Maximum supported version
    };

    /// Append a label to the specified string per the specified flags.
    virtual bool GetLabel(string* label, TLabelFlags flags = 0,
                          ELabelVersion version = eLabel_DefaultVersion)
        const = 0;
};


/// GetLabel interface for actual citation objects, as opposed to mere
/// containers such as CPub.
class NCBI_BIBLIO_EXPORT ICitationBase : public IAbstractCitation
{
public:
    bool GetLabel(string* label, TLabelFlags flags = 0,
                  ELabelVersion version = eLabel_DefaultVersion) const;
    // Historic variant
    bool GetLabel(string* label, bool unique) const
        { return GetLabel(label, unique ? fLabel_Unique : 0); }

    // Static utilities of interest to multiple implementations:

    /// Canonicalize a range of page numbers, expanding Medline-style
    /// 125-35 -> 125-135, F124-34 -> F124-F134, and 12a-c -> 12a-12c, and
    /// returning a single number (without a dash) for a single page.
    /// Return orig_pages as is, modulo whitespace trimming, if unable to
    /// parse as an ascending range in one of the above formats.
    /// (In particular, do not attempt to parse Roman numerals.)
    static string FixPages(const string& orig_pages);

    static string GetParenthesizedYear(const CDate& date);

    static bool HasText(const string& s)
        { return s.find_first_not_of(" \t\n\r") != NPOS; }
    static bool HasText(const string* s) { return s != NULL && HasText(*s); }

    static void MaybeAddSpace(string* label);

    static void NoteSup(string* label, const CImprint& imp);

    static bool SWNC(const string& str, const string& pfx)
        { return NStr::StartsWith(str, pfx, NStr::eNocase); }

protected:
    virtual bool GetLabelV1(string* label, TLabelFlags flags) const = 0;
    virtual bool GetLabelV2(string* label, TLabelFlags flags) const = 0;

    static bool x_GetLabelV1(string*            label,
                             bool               unique,
                             const CAuth_list*  authors,
                             const CImprint*    imprint,
                             const CTitle*      title,
                             const CCit_book*   book,
                             const CCit_jour*   journal,
                             const string*      title1 = 0,
                             const string*      title2 = 0,
                             const string*      titleunique = 0,
                             const string*      date = 0,
                             const string*      volume = 0,
                             const string*      issue = 0,
                             const string*      pages = 0,
                             bool               unpublished = false);
};


inline
void ICitationBase::MaybeAddSpace(string* label)
{
    _ASSERT(label != NULL);
    if ( !label->empty()  &&  !NStr::EndsWith(*label, ' ') ) {
        *label += ' ';
    }
}

END_objects_SCOPE

END_NCBI_SCOPE

#endif  /* OBJECTS_BIBLIO___CITATION_BASE__HPP */
