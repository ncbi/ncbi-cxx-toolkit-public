#ifndef OBJMGR_UTIL___OBJUTILS_HPP
#define OBJMGR_UTIL___OBJUTILS_HPP

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
* Author:  Mati Shomrat
*
* File Description:
*   Flat-file generator shared utility functions
*/
#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDate;
class CBioseq;
class CScope;
class CSeq_feat;
class CBioseq_Handle;


enum ETildeStyle {
    eTilde_tilde,   // no-op
    eTilde_space,   // '~' -> ' ', except before /[ (]?\d/
    eTilde_newline, // '~' -> '\n' but "~~" -> "~"
    eTilde_comment,  // '~' -> '\n' always
    eTilde_note     // '~' -> ';\n' but "~~" -> "~", Except: after space or semi-colon, it just becomes "\n"
};
void ExpandTildes(string& s, ETildeStyle style);

// convert " to '
NCBI_XOBJEDIT_EXPORT
void ConvertQuotes(string& str);

NCBI_XOBJEDIT_EXPORT
string ConvertQuotes(const string& str);

NCBI_XOBJEDIT_EXPORT
void JoinString(string& to, 
                const string& prefix, 
                const string& str, 
                bool noRedundancy=true);

NCBI_XOBJEDIT_EXPORT
string JoinString(const list<string>& l, 
                  const string& delim, 
                  bool noRedundancy=true);

// Strips all spaces in string in following manner. If the function
// meet several spaces (spaces and tabs) in succession it replaces them
// with one space. Strips all spaces after '(' and before ')'
NCBI_XOBJEDIT_EXPORT
bool StripSpaces(string& str);
NCBI_XOBJEDIT_EXPORT
bool TrimSpacesAndJunkFromEnds(string& str, bool allow_ellipsis = false);
NCBI_XOBJEDIT_EXPORT
void TrimSpacesAndJunkFromEnds(string& result, const CTempString& str, bool allow_ellipsis = false);
NCBI_XOBJEDIT_EXPORT
void TrimSpaces(string& str, size_t indent = 0);
NCBI_XOBJEDIT_EXPORT
void CleanAndCompress (string& dest, const CTempString& instr);
NCBI_XOBJEDIT_EXPORT
string &CompressSpaces( string& str, const bool trim_beginning = true, const bool trim_end = true );
NCBI_XOBJEDIT_EXPORT
bool RemovePeriodFromEnd(string& str, bool keep_ellipsis = true);
NCBI_XOBJEDIT_EXPORT
void AddPeriod(string& str);

enum EAccValFlag
{
    eValidateAcc,
    eValidateAccDotVer
};
NCBI_XOBJEDIT_EXPORT
bool IsValidAccession(const string& accn, EAccValFlag flag = eValidateAcc);

// example: "10-SEP-2010"
enum EDateToString {
    eDateToString_regular = 1,
    eDateToString_cit_sub,
    eDateToString_patent
};
NCBI_XOBJEDIT_EXPORT
void DateToString(const CDate& date, string& str, EDateToString format_choice = eDateToString_regular );

struct NCBI_XOBJEDIT_EXPORT SDeltaSeqSummary
{
    string text;
    size_t num_segs;        // total number of segments
    size_t num_gaps;        // total number of segments representing gaps
    size_t residues;        // number of real residues in the sequence (not gaps)
    size_t num_faked_gaps;  // number of gaps where real length is not known,
                            // but where a length was guessed by spreading the
                            // total gap length out over all gaps evenly.

    SDeltaSeqSummary(void) :
        text(kEmptyStr),
        num_segs(0), num_gaps(0), residues(0), num_faked_gaps(0)
    {}
};

NCBI_XOBJEDIT_EXPORT
void GetDeltaSeqSummary(const CBioseq_Handle& seq, SDeltaSeqSummary& summary);

NCBI_XOBJEDIT_EXPORT
const string& GetTechString(int tech);


struct NCBI_XOBJEDIT_EXPORT SModelEvidance
{
    typedef std::pair<Int8, Int8> TSpanType;

    string        name;
    list<string>  assembly;
    string        method;
    bool          mrnaEv;
    bool          estEv;
    TGi           gi;
    TSpanType     span; // start, then end. 0-based

    SModelEvidance(void) :
        name(kEmptyStr), method(kEmptyStr), mrnaEv(false), estEv(false), gi(INVALID_GI), span(-1, -1)
    {}
};

NCBI_XOBJEDIT_EXPORT
bool GetModelEvidance(const CBioseq_Handle& bsh, SModelEvidance& me);

NCBI_XOBJEDIT_EXPORT
const char* GetAAName(unsigned char aa, bool is_ascii);

//////////////////////////////////////////////////////////////////////////////

enum EResolveOrder
{
    eResolve_NotFound,
    eResolve_RnaFirst,
    eResolve_ProtFirst
};

NCBI_XOBJEDIT_EXPORT
EResolveOrder GetResolveOrder(CScope& scope,
                              const CSeq_id_Handle& mrna,
                              const CSeq_id_Handle& prot,
                              CBioseq_Handle& mrna_bsh,
                              CBioseq_Handle& prot_bsh);


//////////////////////////////////////////////////////////////////////////////
// HTML utils and strings

//  ============================================================================
//  Link locations:
//  ============================================================================
extern NCBI_XOBJEDIT_EXPORT const char* strLinkBaseNuc;
extern NCBI_XOBJEDIT_EXPORT const char* strLinkBaseProt;

extern NCBI_XOBJEDIT_EXPORT const char* strLinkBaseEntrezViewer;

extern NCBI_XOBJEDIT_EXPORT const char* strLinkBaseTaxonomy;
extern NCBI_XOBJEDIT_EXPORT const char* strLinkBaseTransTable;
extern NCBI_XOBJEDIT_EXPORT const char* strLinkBasePubmed;
extern NCBI_XOBJEDIT_EXPORT const char* strLinkBaseExpasy;
extern NCBI_XOBJEDIT_EXPORT const char* strLinkBaseNucSearch;
extern NCBI_XOBJEDIT_EXPORT const char* strLinkBaseGenomePrj;
extern NCBI_XOBJEDIT_EXPORT const char* strLinkBaseLatLon;
extern NCBI_XOBJEDIT_EXPORT const char* strLinkBaseGeneOntology;
extern NCBI_XOBJEDIT_EXPORT const char* strLinkBaseGeneOntologyRef;
extern NCBI_XOBJEDIT_EXPORT const char* strLinkBaseUSPTO;
extern NCBI_XOBJEDIT_EXPORT const char* strLinkBaseUniProt;

extern NCBI_XOBJEDIT_EXPORT const char* strDocLink;

template <typename T>
void NcbiId(CNcbiOstream& os, const T& id, bool html = false)
{
    if (!html) {
        os << id;
    } else {
        os << "<a href=\"" << strLinkBaseNuc //strLinkBaseEntrezViewer
           << id << "\">" << id << "</a>";
    }
}

// Like ConvertQuotes, but skips characters inside HTML tags
// Returns true if it made any changes.
NCBI_XOBJEDIT_EXPORT
bool ConvertQuotesNotInHTMLTags( string &str );

// We use the word "try" in the name because this is NOT airtight security,
// but rather a net that catches the majority of cases.
NCBI_XOBJEDIT_EXPORT
void TryToSanitizeHtml( std::string &str );
NCBI_XOBJEDIT_EXPORT
void TryToSanitizeHtml(std::string &result, const CTempString& str);
NCBI_XOBJEDIT_EXPORT
void TryToSanitizeHtmlList(std::list<std::string> &strs);
NCBI_XOBJEDIT_EXPORT
bool CommentHasSuspiciousHtml( const string &str );



END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___UTILS_HPP */
