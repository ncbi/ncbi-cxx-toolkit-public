#ifndef ALGO_BLAST_API___SEEDTOP__HPP
#define ALGO_BLAST_API___SEEDTOP__HPP

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
 * Authors:  Ning Ma
 *
 */

/// @file seedtop.hpp
/// Declares the CSeedTop class.


/** @addtogroup AlgoBlast
 *
 * @{
 */

#include <corelib/ncbistd.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/api/blast_seqinfosrc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

struct SPatternUnit {
    string allowed_letters;
    string disallowed_letters;
    size_t at_least;
    size_t at_most;
    bool is_x;
    SPatternUnit(const string unit) {
        size_t tail_start = 0;
        bool parse_failed = false;
        is_x = false;
        switch(unit[0]) {
            case '[':
                tail_start = unit.find(']');
                if (tail_start == string::npos){
                    parse_failed = true;
                    break;
                }
                tail_start++;
                allowed_letters = string(unit, 1, tail_start - 2);
                break;
            case '{':
                tail_start = unit.find('}');
                if (tail_start == string::npos){
                    parse_failed = true;
                    break;
                }
                tail_start++;
                disallowed_letters = string(unit, 1, tail_start - 2);
                break;
            case 'X':
                tail_start = 1;
                is_x = true;
                break;
            default:
                if (unit[0] > 'Z' || unit[0] < 'A'){
                    parse_failed = true;
                    break;
                }
                tail_start = 1;
                allowed_letters = string(unit, 0, 1);
                break;
        }

        if (parse_failed) {
            NCBI_THROW(CBlastException, eInvalidArgument, "Can not parse pattern file");
        }

        // parse the (x,y) part
        if (tail_start >= unit.size()) {
            at_least = 1;
            at_most = 2;
        } else {
            if (unit[tail_start] != '(' || unit[unit.size()-1] != ')') {
                NCBI_THROW(CBlastException, eInvalidArgument, "Can not parse pattern file");
            }
            try {
                string rep(unit, tail_start + 1, unit.size()-2-tail_start);
                size_t pos_comma = rep.find(',');
                if (pos_comma == rep.npos) {
                    at_least = NStr::StringToUInt(rep);
                    at_most = at_least + 1;
                } else if (pos_comma == rep.size() -1) {
                    at_least = NStr::StringToUInt(string(rep, 0, pos_comma));
                    at_most = rep.npos;
                } else {
                    at_least = NStr::StringToUInt(string(rep, 0, pos_comma));
                    at_most = NStr::StringToUInt(string(rep, 
                             pos_comma + 1, rep.size()-1-pos_comma)) + 1;
                }
            } catch (...) {
                NCBI_THROW(CBlastException, eInvalidArgument, "Can not parse pattern file");
            }
        }
    }
    bool test(Uint1 letter) {
        if (allowed_letters != "") {
            return (allowed_letters.find(letter) != allowed_letters.npos);
        } else {
            return (disallowed_letters.find(letter) == disallowed_letters.npos);
        }
    }
};

class NCBI_XBLAST_EXPORT CSeedTop : public CObject {
public:
    // the return type for seedtop search
    // a vector of results (matches) as seq_loc on each subject
    // the results will be sorted first by subject oid (if multiple subject
    // sequences or database is supplied during construction), then by the first
    // posotion of the match
    typedef vector < CConstRef <CSeq_loc> > TSeedTopResults;

    // constructor 
    CSeedTop(const string & pattern);       // seedtop pattern

    // search a database or a set of subject sequences
    TSeedTopResults Run(CRef<CLocalDbAdapter> db);  

    // search a bioseq
    TSeedTopResults Run(CBioseq_Handle & b_hdl);
    
private:
    const static EBlastProgramType m_Program = eBlastTypePhiBlastp;
    string m_Pattern; 
    CLookupTableWrap m_Lookup;
    CBlastScoreBlk m_ScoreBlk;
    vector< struct SPatternUnit > m_Units;

    void x_ParsePattern();
    void x_MakeLookupTable();
    void x_MakeScoreBlk();
    // parsing the result into a list of ranges
    void x_GetPatternRanges(vector<int> &pos,
                            Uint4 off, 
                            Uint1 *seq, 
                            Uint4 len, 
                            vector<vector<int> > &ranges);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


#endif  /* ALGO_BLAST_API___SEEDTOP__HPP */
