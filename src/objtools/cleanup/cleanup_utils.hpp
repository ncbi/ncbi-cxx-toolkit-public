#ifndef OBJECTS_GENERAL___CLEANUP_UTILS__HPP
#define OBJECTS_GENERAL___CLEANUP_UTILS__HPP

/* $Id$
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
 * Author:  Mati Shomrat
 *
 * File Description:
 *   General utilities for data cleanup.
 *
 * ===========================================================================
 */
#include <corelib/ncbistd.hpp>
#include <algorithm>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>

#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CCit_sub;
class CPubdesc;
class CAuthor;
class CAuth_list;

/// Cleaning functions may return true if they have changed something.
/// false if no change was needed.

/// convert double quotes to single quotes
inline 
bool ConvertDoubleQuotes(string& str)
{
    bool changed = false;
    if (!str.empty()) {
        NON_CONST_ITERATE(string, c_it, str) {
            if (*c_it == '\"') {
                *c_it = '\'';
                changed = true;
            }
        }
    }
    return changed;
}


/// truncate spaces and other trailing characters.
bool CleanString(string& str, bool rm_trailing_period = false);

bool CleanVisString( string &str );

bool CleanVisStringJunk( string &str );

bool CleanStringList(list< string >& string_list);

/// remove a trailing period, 
bool RemoveTrailingPeriod(string& str);

/// remove trailing white space, commas, tildes and semicolons
bool RemoveTrailingJunk(string& str);

/// remove white space between pairs of tildes.
/// "~ ~  ~ a~" -> "~~~ a~"
bool  RemoveSpacesBetweenTildes(string& str);

/// remove duplicate internal semicolons.
/// "a;;b" will become "a;b".
void TrimInternalSemicolons (string& str);

/// Change double to single quotes
bool CleanDoubleQuote(string& str);

bool OnlyPunctuation (string str);

/// remove all spaces from a string
bool RemoveSpaces(string& str);

/// convert medline names to standard names
CRef<CAuthor> ConvertMltoSTD(const string& token);

/// clean a container of strings, remove blanks and repeats.
template<typename C>
bool CleanStringContainer(C& str_cont, bool rm_trailing_junk = false)
{
    bool changed = false;
    typename C::iterator it = str_cont.begin();
    while (it != str_cont.end()) {
        if (CleanString(*it, rm_trailing_junk)) {
            changed = true;
        }
        if (NStr::IsBlank(*it)) {
            it = str_cont.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    return changed;
}

template<typename C>
bool CleanVisStringContainer(C& str_cont)
{
    bool changed = false;
    typename C::iterator it = str_cont.begin();
    while (it != str_cont.end()) {
        if (CleanVisString(*it)) {
            changed = true;
        }
        if (NStr::IsBlank(*it)) {
            it = str_cont.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    return changed;
}

template<typename C>
bool CleanVisStringContainerJunk(C& str_cont)
{
    bool changed = false;
    typename C::iterator it = str_cont.begin();
    while (it != str_cont.end()) {
        if (CleanVisStringJunk(*it)) {
            changed = true;
        }
        if (NStr::IsBlank(*it)) {
            it = str_cont.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    return changed;
}

// convert a container of authors, medline names to standard names
bool ConvertAuthorContainerMlToStd( CAuth_list& authors );

struct SCaseInsensitiveStrComp {
    SCaseInsensitiveStrComp(const string& str) : m_Target(str) { }
    bool operator()(string& s) {
        return NStr::EqualNocase(s, m_Target);
    }
    string m_Target;
};

template<typename Cont>
bool RemoveDupsNoSort(Cont& l, bool case_insensitive = false)
{
    bool changed = false;
    typedef typename Cont::iterator iterator;
    iterator l_it = l.begin();
    while (l_it != l.end()) {
        
        iterator dup_it = case_insensitive ?
            find_if( l.begin(), l_it, SCaseInsensitiveStrComp(*l_it) ) :
            find( l.begin(), l_it, *l_it );
        if (dup_it != l_it) {
            l_it = l.erase(l_it);
            changed = true;
        } else {
            ++l_it;            
        }
    }
    return changed;
}


#define TRUNCATE_SPACES(o, x) \
    if ((o).IsSet##x()) { \
        size_t n = (o).Get##x().size(); \
        NStr::TruncateSpacesInPlace((o).Set##x()); \
        if (n != (o).Get##x().size() ) { \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
        if (NStr::IsBlank((o).Get##x())) { \
            (o).Reset##x(); \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
    }

#define TRUNCATE_CHOICE_SPACES(o, x) \
    { \
        size_t n = (o).Get##x().size(); \
        NStr::TruncateSpacesInPlace((o).Set##x()); \
        if (n != (o).Get##x().size() ) { \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
        if (NStr::IsBlank((o).Get##x())) { \
            (o).Reset(); \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
    }

#define CONVERT_QUOTES(x) \
    if (IsSet##x()) { \
        if (ConvertDoubleQuotes(Set##x()) ) { \
            ChangeMade(CCleanupChange::eCleanDoubleQuotes); \
        } \
    }

#define COMPRESS_STRING_MEMBER(o, x) \
    if ((o).IsSet##x()) { \
        const size_t old_len = (o).Set##x().length(); \
        CompressSpaces((o).Set##x()); \
        if( old_len != (o).Set##x().length() ) { \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
        if (NStr::IsBlank((o).Get##x())) { \
            (o).Reset##x(); \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
    }

#define CLEAN_STRING_MEMBER(o, x) \
    if ((o).IsSet##x()) { \
        if (CleanVisString((o).Set##x()) ) { \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
        if (NStr::IsBlank((o).Get##x())) { \
            (o).Reset##x(); \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
    }

// The "do-while" forces the need for a semicolon
#define CLEAN_AND_COMPRESS_STRING_MEMBER(o, x) \
    do { COMPRESS_STRING_MEMBER(o, x); CLEAN_STRING_MEMBER(o, x); } while(false)

#define CLEAN_STRING_MEMBER_JUNK(o, x) \
if ((o).IsSet##x()) { \
    if (CleanVisStringJunk((o).Set##x()) ) { \
        ChangeMade(CCleanupChange::eTrimSpaces); \
    } \
    if (NStr::IsBlank((o).Get##x())) { \
        (o).Reset##x(); \
        ChangeMade(CCleanupChange::eTrimSpaces); \
    } \
}

// The "do-while" forces the need for a semicolon
#define CLEAN_AND_COMPRESS_STRING_MEMBER_JUNK(o, x) \
    do { COMPRESS_STRING_MEMBER(o, x); CLEAN_STRING_MEMBER_JUNK(o, x); } while(false)

#define EXTENDED_CLEAN_STRING_MEMBER(o, x) \
if ((o).IsSet##x()) { \
    CleanVisString((o).Set##x()); \
        if (NStr::IsBlank((o).Get##x())) { \
            (o).Reset##x(); \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
}

#define CLEAN_STRING_CHOICE(o, x) \
        if (CleanVisString((o).Set##x()) ) { \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
        if (NStr::IsBlank((o).Get##x())) { \
            (o).Reset(); \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        }

#define CLEAN_STRING_LIST(o, x) \
    if ((o).IsSet##x()) { \
        if (CleanVisStringContainer((o).Set##x()) ) { \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
        if ((o).Get##x().empty()) { \
            (o).Reset##x(); \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
    }

#define CLEAN_STRING_LIST_JUNK(o, x) \
if ((o).IsSet##x()) { \
    if (CleanVisStringContainerJunk((o).Set##x()) ) { \
        ChangeMade(CCleanupChange::eTrimSpaces); \
    } \
    if ((o).Get##x().empty()) { \
        (o).Reset##x(); \
        ChangeMade(CCleanupChange::eTrimSpaces); \
    } \
}

#define EXTENDED_CLEAN_STRING_LIST(o, x) \
    if ((o).IsSet##x()) { \
        CleanVisStringList((o).Set##x()); \
        if ((o).Get##x().empty()) { \
            (o).Reset##x(); \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
    }

/// clean a string member 'x' of an internal object 'o'
#define CLEAN_INTERNAL_STRING(o, x) \
    if (o.IsSet##x()) { \
        if ( CleanVisString(o.Set##x()) ) { \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
        if (NStr::IsBlank(o.Get##x())) { \
            o.Reset##x(); \
            ChangeMade(CCleanupChange::eTrimSpaces); \
        } \
    }

#define MERGEABLE_STRING_VALUE(o1, o2, x) \
    if ((o1).CanGet##x() && (o2).CanGet##x() \
        && !NStr::IsBlank ((o1).Get##x()) \
        && !NStr::IsBlank ((o2).Get##x()) \
        && !NStr::Equal ((o1).Get##x(), (o2).Get##x())) { \
        return false; \
    }    

#define MERGEABLE_STRING_LIST(o1, o2, x) \
    if ((o1).CanGet##x() && (o2).CanGet##x() \
        && (o1).Get##x().size() > 0 \
        && (o2).Get##x().size() > 0) { \
        list <string>::const_iterator it1 = (o1).Get##x().begin(); \
        list <string>::const_iterator it2 = (o2).Get##x().begin(); \
        while (it1 != (o1).Get##x().end() && it2 != (o2).Get##x().end()) { \
            if (!NStr::Equal ((*it1), (*it2))) { \
                return false; \
            } \
            ++it1; ++it2; \
        } \
        if (it1 != (o1).Get##x().end() || it2 != (o2).Get##x().end()) { \
            return false; \
        } \
    } 
    
#define MERGEABLE_INT_VALUE(o1, o2, x) \
    (!(o1).CanGet##x() || !(o2).CanGet##x() \
     || (o1).Get##x() == 0 \
     || (o2).Get##x() == 0 \
     || (o1).Get##x() == (o2).Get##x())

#define MATCH_STRING_VALUE(o1, o2, x) \
    ((!(o1).CanGet##x() && !(o2).CanGet##x()) \
     || ((o1).CanGet##x() && (o2).CanGet##x() \
         && NStr::Equal((o1).Get##x(), (o2).Get##x())))

#define MATCH_STRING_LIST(o1, o2, x) \
    if (((o1).CanGet##x() && (o1).Get##x().size() > 0 && !(o2).CanGet##x()) \
        || (!(o1).CanGet##x() && (o2).CanGet##x() && (o2).Get##x().size() > 0)) { \
        return false; \
    } else if ((o1).CanGet##x() && (o2).CanGet##x()) { \
        list <string>::const_iterator it1 = (o1).Get##x().begin(); \
        list <string>::const_iterator it2 = (o2).Get##x().begin(); \
        while (it1 != (o1).Get##x().end() && it2 != (o2).Get##x().end()) { \
            if (!NStr::Equal ((*it1), (*it2))) { \
                return false; \
            } \
            ++it1; ++it2; \
        } \
        if (it1 != (o1).Get##x().end() || it2 != (o2).Get##x().end()) { \
            return false; \
        } \
    } 
    
#define MATCH_INT_VALUE(o1, o2, x) \
    ((!(o1).CanGet##x() && !(o2).CanGet##x()) \
     || ((o1).CanGet##x() && (o2).CanGet##x() \
         && (o1).Get##x() == (o2).Get##x()) \
     || ((o1).CanGet##x() && (o1).Get##x() == 0 \
         && !(o2).CanGet##x()) \
     || ((o2).CanGet##x() && (o2).Get##x() == 0 \
         && !(o1).CanGet##x()))         

#define MATCH_BOOL_VALUE(o1, o2, x) \
    if (!(o1).IsSet##x()) { \
        if ((o2).IsSet##x()) { \
            return false; \
        } \
    } else if (!(o2).IsSet##x()) { \
        return false; \
    } else if (((o1).Get##x() && !(o2).Get##x()) \
               || (!(o2).Get##x() && (o2).Get##x())) { \
        return false; \
    }



class CLexToken 
{
public:
    CLexToken(unsigned int token_type) { m_TokenType = token_type; m_HasError = false; }
    virtual ~CLexToken() {}
    unsigned int GetTokenType() { return m_TokenType; }
    bool HasError () { return m_HasError; }
    
    virtual unsigned int GetInt() { return 0; }
    virtual string GetString() { return ""; }
    
    virtual CRef<CSeq_loc> GetLocation(CSeq_id *id, CScope* scope) { return CRef<CSeq_loc>(NULL); }
    enum E_TokenType {
        e_Int = 0,
        e_String,
        e_ParenPair,
        e_Join,
        e_Order,
        e_Complement,
        e_DotDot,
        e_LeftPartial,
        e_RightPartial,
        e_Comma,
        e_Accession
    };
    
protected:
    unsigned int m_TokenType;
    bool m_HasError;
};

typedef vector<CLexToken *>  TLexTokenArray;

class CLexTokenString : public CLexToken
{
public:
    CLexTokenString (string token_data);
    virtual ~CLexTokenString();
    virtual string GetString() { return m_TokenData; };
private:
    string m_TokenData;
};

class CLexTokenInt : public CLexToken
{
public:
    CLexTokenInt (unsigned int token_data);
    virtual ~CLexTokenInt ();
    virtual unsigned int GetInt() { return m_TokenData; };
private:
    unsigned int m_TokenData;
};

class CLexTokenAccession : public CLexToken {
public:
    CLexTokenAccession (const string &token_data);
    virtual ~CLexTokenAccession();
    virtual string GetString(void) { return m_TokenData; };
private:
    string m_TokenData;
};

class CLexTokenParenPair : public CLexToken
{
public:
    CLexTokenParenPair (unsigned int token_type, string between_text);
    virtual ~CLexTokenParenPair();
    
    virtual CRef<CSeq_loc> GetLocation(CSeq_id *id, CScope* scope);
    
    static CRef<CSeq_loc> ReadLocFromTokenList (TLexTokenArray token_list, CSeq_id *id, CScope* scope);
    
private:
    TLexTokenArray m_TokenList;
};


// for converting strings to locations
CRef<CSeq_loc> ReadLocFromText(string text, const CSeq_id *id, CScope *scope);

// for finding the correct amino acid letter given an abbreviation
char ValidAminoAcid (string abbrev);

// for matching equivalent cit-sub publications
bool CitSubsMatch(const CCit_sub& sub1, const CCit_sub& sub2);

// for sorting and uniquing dbtags
bool s_DbtagCompare (const CRef<CDbtag>& dbt1, const CRef<CDbtag>& dbt2);
bool s_DbtagEqual (const CRef<CDbtag>& dbt1, const CRef<CDbtag>& dbt2);

// Is this range sorted according to the given comparison?
// needed to see if sorts in cleanup do anything.
// Maybe should be at a higher level of the source tree?

template <class Iter, class Comp>
bool is_sorted(Iter first, Iter last,
               Comp comp)
{
    if (first == last)
        return true;
    
    Iter next = first;
    for (++next; next != last; first = next, ++next) {
        if (comp(*next, *first))
            return false;
    }
    
    return true;
}


template <class Iter>
bool is_sorted(Iter first, Iter last)
{
    if (first == last)
        return true;
    
    Iter next = first;
    for (++next; next != last; first = next, ++next) {
        if (*next < *first)
            return false;
    }
    
    return true;
}

CRef<CSeq_loc> MakeFullLengthLocation(const CSeq_loc& loc, CScope* scope);

bool IsFeatureFullLength(const CSeq_feat& cf, CScope* scope);

CBioSource::EGenome GenomeByOrganelle(string& organelle, bool strip, NStr::ECase use_case = NStr::eNocase);

bool IsmRNA(CBioseq_Handle bsh);
bool IsmRNA(CBioseq_set_Handle bsh);
const CBioSource* GetAssociatedBioSource(CBioseq_set_Handle bh);
const CBioSource* GetAssociatedBioSource(CBioseq_Handle bh);
bool IsArtificialSyntheticConstruct (const CBioSource *bsrc);
bool IsArtificialSyntheticConstruct (CBioseq_Handle bsh);
bool IsArtificialSyntheticConstruct (CBioseq_set_Handle bsh);





END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJECTS_GENERAL___CLEANUP_UTILS__HPP
