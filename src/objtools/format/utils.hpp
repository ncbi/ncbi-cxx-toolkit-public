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
    eTilde_comment  // '~' -> '\n' always
};
string ExpandTildes(const string& s, ETildeStyle style);

// convert " to '
void ConvertQuotes(string& str);
string ConvertQuotes(const string& str);

void JoinNoRedund(string& to, const string& prefix, const string& str);
string JoinNoRedund(const list<string>& l, const string& delim);

// Strips all spaces in string in following manner. If the function
// meet several spaces (spaces and tabs) in succession it replaces them
// with one space. Strips all spaces after '(' and before ')'
void StripSpaces(string& str);
void TrimSpacesAndJunkFromEnds(string& str, bool allow_ellipsis = false);
void TrimSpaces(string& str, int indent = 0);
bool RemovePeriodFromEnd(string& str, bool keep_ellipsis = true);
void AddPeriod(string& str);


enum EAccValFlag
{
    eValidateAcc,
    eValidateAccDotVer
};

bool IsValidAccession(const string& accn, EAccValFlag flag = eValidateAcc);
void DateToString(const CDate& date, string& str, bool is_cit_sub = false);

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
    string name;
    string method;
    bool mrnaEv;
    bool estEv;

    SModelEvidance(void) :
        name(kEmptyStr), method(kEmptyStr), mrnaEv(false), estEv(false)
    {}
};

bool GetModelEvidance(const CBioseq_Handle& bsh, SModelEvidance& me);


const char* GetAAName(unsigned char aa, bool is_ascii);


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.15  2005/01/31 16:31:59  shomrat
* Added validation of accession.version
*
* Revision 1.14  2004/11/15 20:05:06  shomrat
* ValidateAccession -> IsValidAccession
*
* Revision 1.13  2004/10/18 18:52:26  shomrat
* Removed FindBestId
*
* Revision 1.12  2004/10/05 15:48:52  shomrat
* + FindBestId
*
* Revision 1.11  2004/08/30 18:21:59  shomrat
* + TrimSpaces
*
* Revision 1.10  2004/08/30 13:37:53  shomrat
* + TrimSpacesAndJunkFromEnds
*
* Revision 1.9  2004/08/23 15:50:38  shomrat
* + new tilde format
*
* Revision 1.8  2004/08/19 16:32:36  shomrat
* + AddPeriod, ConvertQuotes and IsBlankString
*
* Revision 1.7  2004/05/07 15:23:11  shomrat
* + RemovePeriodFromEnd
*
* Revision 1.6  2004/04/22 15:54:37  shomrat
* Use CBioseq_Handle instead of CBioseq
*
* Revision 1.5  2004/04/07 14:29:19  shomrat
* + GetAAName
*
* Revision 1.4  2004/03/25 20:47:59  shomrat
* Add class forwarding
*
* Revision 1.3  2004/03/18 15:34:54  shomrat
* Meaningful argument names
*
* Revision 1.2  2004/02/11 16:57:17  shomrat
* added JoinNoRedund functions
*
* Revision 1.1  2003/12/17 20:25:10  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT___UTILS_HPP */
