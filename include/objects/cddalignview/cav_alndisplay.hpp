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
* Authors:  Paul Thiessen
*
* File Description:
*      Classes to hold alignment display
*
* ===========================================================================
*/

#ifndef CAV_ALIGNMENT_DISPLAY__HPP
#define CAV_ALIGNMENT_DISPLAY__HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbistre.hpp>

#include <list>
#include <vector>

#include "objects/cddalignview/cddalignview.h"


BEGIN_NCBI_SCOPE

class Sequence;
class SequenceSet;
class AlignmentSet;

///// classes that go into the AlignmentDisplay /////

class TextRow
{
private:
    string chars;

public:
    TextRow(const string& str) : chars(str) { }         // initialize to string
    TextRow(int size) : chars(string(size, '-')) { }   // initialize to blank (all '-')

    int Length(void) const { return chars.size(); }
    void InsertGaps(int nGaps, int beforePos);
    void DeleteGaps(int nGaps, int startPos);
    char GetCharAt(int alnLoc) const { return chars[alnLoc]; }
    void SetCharAt(int alnLoc, char ch) { chars[alnLoc] = ch; }
    bool IsSqueezable(int alnLoc, int *nGaps, int *startPos, int maxGaps) const;
};

class IndexAlnLocToSeqLocRow
{
private:
    typedef vector < int > IntVec;
    IntVec seqLocs;

public:
    IndexAlnLocToSeqLocRow(const Sequence *seq, int length = 0);

    const Sequence *sequence;

    int Length(void) const { return seqLocs.size(); }
    void InsertGaps(int nGaps, int beforePos);
    int GetSeqLocAt(int alnLoc) const { return seqLocs[alnLoc]; }
    void SetSeqLocAt(int alnLoc, int seqLoc) { seqLocs[alnLoc] = seqLoc; }
    void ReIndex(const TextRow& textRow);
};


///// the actual AlignmentDisplay structure /////

class AlignmentDisplay
{
private:
    typedef vector < IndexAlnLocToSeqLocRow * > IndexAlnLocToSeqLocRows;
    IndexAlnLocToSeqLocRows indexAlnLocToSeqLocRows;

    typedef vector < TextRow * > TextRows;
    TextRows textRows;

    int firstAlnLoc, lastAlnLoc;

    void InsertGaps(int nGaps, int beforePos);
    void ShiftUnalignedLeft(void);
    void Squeeze(void);
    void SplitUnaligned(void);

    int status;

public:
    AlignmentDisplay(const SequenceSet *sequenceSet, const AlignmentSet *alignmentSet);
    ~AlignmentDisplay();

    // query functions
    int GetWidth(void) const { return textRows[0]->Length(); }
    int GetNRows(void) const { return textRows.size(); }
    char GetCharAt(int alnLoc, int row) const;
    int GetFirstAlignedLoc(void) const { return firstAlnLoc; }
    int GetLastAlignedLoc(void) const { return lastAlnLoc; }

    static const double SHOW_IDENTITY;
    const string& GetColumnColor(int alnLoc, double conservationThreshhold) const;

    // DumpText - plain text or HTML output controlled by 'options'
    int DumpText(CNcbiOstream& os, unsigned int options,
        int firstCol, int lastCol, int nColumns, double conservationThreshhold = 2.0,
        const char *titleHTML = NULL, int nFeatures = 0, const AlignmentFeature *alnFeatures = NULL) const;

    // DumpCondensed - plain text or HTML output with incompletely aligned columns not explicitly shown
    int DumpCondensed(CNcbiOstream& os, unsigned int options,
        int firstCol, int lastCol, int nColumns, double conservationThreshhold = 2.0,
        const char *titleHTML = NULL, int nFeatures = 0, const AlignmentFeature *alnFeatures = NULL) const;

    // FASTA
    int DumpFASTA(int firstCol, int lastCol, int nColumns, bool doLowercase, CNcbiOstream& outStream) const;

    int Status(void) const { return status; }
};


// character type utilitles
inline bool IsUnaligned(char ch) { return (ch >= 'a' && ch <= 'z'); }
inline bool IsAligned(char ch) { return (ch >= 'A' && ch <= 'Z'); }
inline bool IsGap(char ch) { return (ch == '-'); }

END_NCBI_SCOPE


#endif

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/03/19 19:05:31  thiessen
* move again
*
* Revision 1.1  2003/03/19 05:33:43  thiessen
* move to src/app/cddalignview
*
* Revision 1.14  2003/02/03 17:52:03  thiessen
* move CVS Log to end of file
*
* Revision 1.13  2003/01/21 18:01:07  thiessen
* add condensed alignment display
*
* Revision 1.12  2003/01/21 12:31:14  thiessen
* move includes into src dir
*
* Revision 1.11  2002/11/08 19:38:15  thiessen
* add option for lowercase unaligned in FASTA
*
* Revision 1.10  2002/02/08 19:53:59  thiessen
* add annotation to text/HTML displays
*
* Revision 1.9  2001/03/02 01:19:29  thiessen
* add FASTA output
*
* Revision 1.8  2001/02/15 19:23:06  thiessen
* add identity coloring
*
* Revision 1.7  2001/02/14 16:05:46  thiessen
* add block and conservation coloring to HTML display
*
* Revision 1.6  2001/01/29 23:55:03  thiessen
* add AlignmentDisplay verification
*
* Revision 1.5  2001/01/29 18:13:41  thiessen
* split into C-callable library + main
*
* Revision 1.4  2001/01/25 00:50:51  thiessen
* add command-line args; can read asn data from stdin
*
* Revision 1.3  2001/01/23 17:34:06  thiessen
* add HTML output
*
* Revision 1.2  2001/01/22 15:54:10  thiessen
* correctly set up ncbi namespacing
*
* Revision 1.1  2001/01/22 13:15:51  thiessen
* initial checkin
*
*/
