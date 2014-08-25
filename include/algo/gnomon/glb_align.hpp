#ifndef ALGO_GNOMON___GLBALIGN__HPP
#define ALGO_GNOMON___GLBALIGN__HPP

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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <algo/gnomon/gnomon_model.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

using namespace std;

typedef pair<string,string> TCharAlign;

class CCigar {
public:
    CCigar(int qto = -1, int sto = -1) : m_qfrom(qto+1), m_qto(qto), m_sfrom(sto+1), m_sto(sto) {}
    struct SElement {
        SElement(int l, char t) : m_len(l), m_type(t) {}
        int m_len;
        char m_type;  // 'M' 'D' 'I'
    };
    void PushFront(const SElement& el) {
        if(el.m_type == 'M') {
            m_qfrom -= el.m_len;
            m_sfrom -= el.m_len;
        } else if(el.m_type == 'D')
            m_sfrom -= el.m_len;
        else
            m_qfrom -= el.m_len;
            
        if(m_elements.empty() || m_elements.front().m_type != el.m_type)
            m_elements.push_front(el);
        else
            m_elements.front().m_len += el.m_len;
    }
    string CigarString(int qstart, int qlen) const {
        string cigar;
        ITERATE(list<SElement>, i, m_elements)
            cigar += NStr::IntToString(i->m_len)+i->m_type;

        int missingstart = qstart+m_qfrom;
        if(missingstart > 0)
            cigar = NStr::IntToString(missingstart)+"S"+cigar;
        int missingend = qlen-1-m_qto-qstart;
        if(missingend > 0)
            cigar += NStr::IntToString(missingend)+"S";

        return cigar;
    }
    TSignedSeqRange QueryRange() const { return TSignedSeqRange(m_qfrom, m_qto); }
    TSignedSeqRange SubjectRange() const { return TSignedSeqRange(m_sfrom, m_sto); }
    TCharAlign ToAlign(const  char* query, const  char* subject) const {
        TCharAlign align;
        query += m_qfrom;
        subject += m_sfrom;
        ITERATE(list<SElement>, i, m_elements) {
            if(i->m_type == 'M') {
                align.first.insert(align.first.end(), query, query+i->m_len);
                query += i->m_len;
                align.second.insert(align.second.end(), subject, subject+i->m_len);
                subject += i->m_len;
            } else if(i->m_type == 'D') {
                align.first.insert(align.first.end(), i->m_len, '-');
                align.second.insert(align.second.end(), subject, subject+i->m_len);
                subject += i->m_len;
            } else {
                align.first.insert(align.first.end(), query, query+i->m_len);
                query += i->m_len;
                align.second.insert(align.second.end(), i->m_len, '-');
            }
        }

        return align;
    }
    int Matches(const  char* query, const  char* subject) const {
        int matches = 0;
        query += m_qfrom;
        subject += m_sfrom;
        ITERATE(list<SElement>, i, m_elements) {
            if(i->m_type == 'M') {
                for(int l = 0; l < i->m_len; ++l) {
                    if(*query == *subject)
                        ++matches;
                    ++query;
                    ++subject;
                }
            } else if(i->m_type == 'D') {
                subject += i->m_len;
            } else {
                query += i->m_len;
            }
        }

        return matches;
    }

private:
    list<SElement> m_elements;
    int m_qfrom, m_qto, m_sfrom, m_sto;
};

//Needleman-Wunsch
CCigar GlbAlign(const  char* query, int querylen, const  char* subject, int subjectlen, int gopen, int gapextend, const char delta[256][256]);

//Smith-Waterman
CCigar LclAlign(const  char* query, int querylen, const  char* subject, int subjectlen, int gopen, int gapextend, const char delta[256][256]);

//Smith-Waterman with optional NW ends
CCigar LclAlign(const  char* query, int querylen, const  char* subject, int subjectlen, int gopen, int gapextend, bool pinleft, bool pinright, const char delta[256][256]);

//reduced matrix Smith-Waterman
CCigar VariBandAlign(const  char* query, int querylen, const  char* subject, int subjectlen, int gopen, int gapextend, const char delta[256][256], const TSignedSeqRange* subject_limits);

struct SMatrix
{
	SMatrix(int match, int mismatch);  // matrix for DNA
    SMatrix(string name);  // matrix for proteins
	
	char matrix[256][256];
};


template<class T>
int EditDistance(const T &s1, const T & s2) {
	const int len1 = s1.size(), len2 = s2.size();
	vector<int> col(len2+1), prevCol(len2+1);
 
	for (int i = 0; i < (int)prevCol.size(); i++)
		prevCol[i] = i;
	for (int i = 0; i < len1; i++) {
		col[0] = i+1;
		for (int j = 0; j < len2; j++)
			col[j+1] = min( min( 1 + col[j], 1 + prevCol[1 + j]),
								prevCol[j] + (s1[i]==s2[j] ? 0 : 1) );
		col.swap(prevCol);
	}
	return prevCol[len2];
}

double Entropy(const string& seq);

END_SCOPE(gnomon)
END_SCOPE(ncbi)


#endif  // ALGO_GNOMON___GLBALIGN__HPP



