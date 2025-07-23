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
    CCigar(string& cigar_string, int qfrom, int sfrom);
    struct SElement {
        SElement(int l, char t) : m_len(l), m_type(t) {}
        int m_len;
        char m_type;  // 'M' 'D' 'I'
    };
    void PushFront(const SElement& el);
    void PushBack(const SElement& el);
    string CigarString(int qstart, int qlen) const; // qstart, qlen identify notaligned 5'/3' parts
    string DetailedCigarString(int qstart, int qlen, const  char* query, const  char* subject) const;
    TInDels GetInDels(int sstart, const  char* const query, const  char* subject) const;   // sstart is used for correct location
    TSignedSeqRange QueryRange() const { return TSignedSeqRange(m_qfrom, m_qto); }
    TSignedSeqRange SubjectRange() const { return TSignedSeqRange(m_sfrom, m_sto); }
    TCharAlign ToAlign(const  char* query, const  char* subject) const;
    int Matches(const  char* query, const  char* subject) const;
    int Distance(const  char* query, const  char* subject) const;
    int Score(const  char* query, const  char* subject, int gopen, int gapextend, const char delta[256][256]) const;
    const list<SElement>& Elements() { return m_elements; }

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
    SMatrix();                         // matrix for proteins blosum62
	
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

template<class T>
int MaxCommonInterval(const T& a, const T& b, int long_enough = numeric_limits<int>::max()) {
	vector<int> s(b.size()+1, 0);
	vector<int> sm(b.size()+1, 0);
	int len = 0;
	for(int i = 0; i < (int)a.size() && len < long_enough; ++i) {
		auto& ai = a[i];
		for(int j = 0; j < (int)b.size() && len < long_enough; ) {
			int l = 0;
			if(ai == b[j]) {
				l = sm[j]+1;
				len = max(len, l);
			}
			s[++j] = l;
		}
		s.swap(sm);
	}
	return len;
}

double Entropy(const string& seq);

END_SCOPE(gnomon)
END_SCOPE(ncbi)


#endif  // ALGO_GNOMON___GLBALIGN__HPP



