#ifndef OBJTOOLS_FORMAT___UTILS_HPP
#define OBJTOOLS_FORMAT___UTILS_HPP

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
void ConvertQuotes(string& str);
string ConvertQuotes(const string& str);

void JoinString(string& to, 
                const string& prefix, 
                const string& str, 
                bool noRedundancy=true);
string JoinString(const list<string>& l, 
                  const string& delim, 
                  bool noRedundancy=true);

// Strips all spaces in string in following manner. If the function
// meet several spaces (spaces and tabs) in succession it replaces them
// with one space. Strips all spaces after '(' and before ')'
void StripSpaces(string& str);
bool TrimSpacesAndJunkFromEnds(string& str, bool allow_ellipsis = false);
void TrimSpaces(string& str, int indent = 0);
string &CompressSpaces( string& str, const bool trim_beginning = true, const bool trim_end = true );
bool RemovePeriodFromEnd(string& str, bool keep_ellipsis = true);
void AddPeriod(string& str);

enum EAccValFlag
{
    eValidateAcc,
    eValidateAccDotVer
};

bool IsValidAccession(const string& accn, EAccValFlag flag = eValidateAcc);
// example: "10-SEP-2010"
enum EDateToString {
    eDateToString_regular = 1,
    eDateToString_cit_sub,
    eDateToString_patent
};
void DateToString(const CDate& date, string& str, EDateToString format_choice = eDateToString_regular );

struct SDeltaSeqSummary
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

void GetDeltaSeqSummary(const CBioseq_Handle& seq, SDeltaSeqSummary& summary);

const string& GetTechString(int tech);


struct SModelEvidance
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
        name(kEmptyStr), method(kEmptyStr), mrnaEv(false), estEv(false), gi(TGi(-1)), span(-1, -1)
    {}
};

bool GetModelEvidance(const CBioseq_Handle& bsh, SModelEvidance& me);


const char* GetAAName(unsigned char aa, bool is_ascii);

//////////////////////////////////////////////////////////////////////////////

enum EResolveOrder
{
    eResolve_NotFound,
    eResolve_RnaFirst,
    eResolve_ProtFirst
};


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
extern const string strLinkBaseNuc;
extern const string strLinkBaseProt;

extern const string strLinkBaseEntrezViewer;

extern const string strLinkBaseTaxonomy;
extern const string strLinkBaseTransTable;
extern const string strLinkBasePubmed;
extern const string strLinkBaseExpasy;
extern const string strLinkBaseNucSearch;
extern const string strLinkBaseGenomePrj;
extern const string strLinkBaseLatLon;
extern const string strLinkBaseGeneOntology;
extern const string strLinkBaseGeneOntologyRef;
extern const string strLinkBaseUSPTO;

extern const string strDocLink;

template <typename T>
void NcbiId(CNcbiOstream& os, const T& id, bool html = false)
{
    if (!html) {
        os << id;
    } else {
        os << "<a href=\"" << strLinkBaseEntrezViewer
           << id << "\">" << id << "</a>";
    }
}

// Like ConvertQuotes, but skips characters inside HTML tags
// Returns true if it made any changes.
bool ConvertQuotesNotInHTMLTags( string &str );

// We use the word "try" in the name because this is NOT airtight security,
// but rather a net that catches the majority of cases.
void TryToSanitizeHtml( std::string &str );
void TryToSanitizeHtmlList( std::list<std::string> &strs );

bool CommentHasSuspiciousHtml( const string &str );

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___UTILS_HPP */
