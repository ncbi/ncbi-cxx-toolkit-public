#ifndef UTIL_CREADERS___ALIGNMENT_FILE__HPP
#define UTIL_CREADERS___ALIGNMENT_FILE__HPP

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
 * Authors:  Josh Cherry
 *
 * File Description:  C++ wrappers for alignment file reading
 *
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

/// A bunch of strings listing characters with various
/// meanings in an alignment file.  Analogous to
/// SSequenceInfo.
class CSequenceInfo {
public:
    const string& GetAlphabet(void) const {return m_Alphabet;};
    string& SetAlphabet(void) {return m_Alphabet;};
    void SetAlphabet(const string& value) {m_Alphabet = value;};
    const string& GetBeginningGap(void) const {return m_BeginningGap;};
    string& SetBeginningGap(void) {return m_BeginningGap;};
    void SetBeginningGap(const string& value) {m_BeginningGap = value;};
    const string& GetMiddleGap(void) const {return m_MiddleGap;};
    string& SetMiddleGap(void) {return m_MiddleGap;};
    void SetMiddleGap(const string& value) {m_MiddleGap = value;};
    const string& GetEndGap(void) const {return m_EndGap;};
    string& SetEndGap(void) {return m_EndGap;};
    void SetEndGap(const string& value) {m_EndGap = value;};
    /// Convenience function for setting beginning, middle, and
    /// end gap to the same thing
    void SetAllGap(const string& value) {
        m_BeginningGap = m_MiddleGap = m_EndGap = value;
    };
    const string& GetMissing(void) const {return m_Missing;};
    string& SetMissing(void) {return m_Missing;};
    void SetMissing(const string& value) {m_Missing = value;};
    const string& GetMatch(void) const {return m_Match;};
    string& SetMatch(void) {return m_Match;};
    void SetMatch(const string& value) {m_Match = value;};

private:
    string m_Alphabet;
    string m_BeginningGap;
    string m_MiddleGap;
    string m_EndGap;
    string m_Missing;
    string m_Match;
};


/// This class holds the results of reading an alignment file.
/// It is analogous to SAlignmentFile.
/// Seqs are upper-case strings representing the sequences, with
/// '-' for a gap.  Ids are ids read from file.  Organisms and
/// Deflines may not be set, depending on the file.
class CAlignment
{
public:
    const vector<string>& GetIds(void) const {return m_Ids;};
    vector<string>& SetIds(void) {return m_Ids;};
    const vector<string>& GetSeqs(void) const {return m_Seqs;};
    vector<string>& SetSeqs(void) {return m_Seqs;};
    const vector<string>& GetOrganisms(void) const {return m_Organisms;};
    vector<string>& SetOrganisms(void) {return m_Organisms;};
    const vector<string>& GetDeflines(void) const {return m_Deflines;};
    vector<string>& SetDeflines(void) {return m_Deflines;};
    
private:
    vector<string> m_Ids;
    vector<string> m_Seqs;
    vector<string> m_Organisms;
    vector<string> m_Deflines;
};


/// Methods for reading alignment files
class CAlignmentFile
{
public:
    static void Read(CNcbiIstream& is, const CSequenceInfo& info,
                     CAlignment& result);
    static void ReadClustalProtein(CNcbiIstream& is, CAlignment& result);
    static void ReadClustalNuc(CNcbiIstream& is, CAlignment& result);
};


END_NCBI_SCOPE

#endif // UTIL_CREADERS___ALIGNMENT_FILE__HPP

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/02/19 12:49:39  dicuccio
 * Roll back to version 1.1
 *
 * Revision 1.1  2004/02/09 16:02:36  jcherry
 * Initial versionnnn
 *
 * ===========================================================================
 */

