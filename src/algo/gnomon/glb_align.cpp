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

#include <ncbi_pch.hpp>
#include <algo/gnomon/gnomon_model.hpp>
#include <algo/gnomon/glb_align.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

using namespace std;

TCharAlign GlbAlign(const CResidueVec& a, const CResidueVec& b, int rho, int sigma, const char delta[256][256], int end_gap_releif) {
    //	rho - new gap penalty (one base gap rho+sigma), for end gaps rho-end_gap_releif
    // sigma - extension penalty

	int na = a.size();
	int nb = b.size();
    
    if(numeric_limits<int>::max()/(na+1) <= nb+1) throw bad_alloc();
    
	int* s = new(nothrow) int[nb+1];
	int* sm = new(nothrow) int[nb+1];
	int* gapb = new(nothrow) int[nb+1];
	unsigned char* mtrx = new(nothrow) unsigned char[(na+1)*(nb+1)];
	if(!s || ! sm || !gapb || !gapb || !mtrx) {
		delete[] s;
		delete[] sm;
		delete[] gapb;
		delete[] mtrx;
		throw bad_alloc();
	}

    int rs = rho+sigma;

	sm[0] = 0;
	sm[1] = -rs+end_gap_releif;
	for(int i = 2; i <= nb; ++i) 
        sm[i] = sm[i-1]-sigma; 
	s[0] = -rho-sigma+end_gap_releif;

	for(int i = 0; i <= nb; ++i) 
        gapb[i] = sm[i];
	
	enum{Agap = 1, Bgap = 2, Astart = 4, Bstart = 8};
	
	mtrx[0] = 0;
	for(int i = 1; i <= nb; ++i) {
        mtrx[i] = Agap;
    }
	mtrx[1] |= Astart;
	
	unsigned char* m = mtrx+nb;
    int gapa = 0, ss = 0;
	for(int i = 0; i < na; ++i) {
		*(++m) = Bstart|Bgap;
		gapa = s[0];
		int ai = a[i];
		
		const char* matrix = delta[ai];
		int* sp = &s[0];
		for(int j = 0; j < nb; ) {
			*(++m) = 0;
			ss = sm[j]+matrix[b[j]];

			gapa -= sigma;
			if(*sp-rs > gapa) {
				gapa = *sp-rs;
				*m |= Astart;
			}
			
			int& gapbj = gapb[++j];
			gapbj -= sigma;
			if(sm[j]-rs > gapbj) {
				gapbj = sm[j]-rs;
				*m |= Bstart;
			}
			 
			if(gapa > gapbj) {
				if(ss > gapa) {
					*(++sp) = ss;
				} else {
					*(++sp) = gapa;
					*m |= Agap;
				}
			} else {
				if(ss > gapbj) {
					*(++sp) = ss;
				} else {
					*(++sp) = gapbj;
					*m |= Bgap;
				}
			}
		}
		swap(sm,s);
		s[0] = sm[0]-sigma; 
	}
			
	*m &= ~(Agap|Bgap);
    int& gapbj = gapb[nb];
    if(gapa > gapbj) {
		if(ss <= gapa+end_gap_releif) {
			*m |= Agap;
		}
	} else {
		if(ss <= gapbj+end_gap_releif) {
			*m |= Bgap;
		}
	}
	
	TCharAlign track;
	
	int ia = na-1;
	int ib = nb-1;
	m = mtrx+(na+1)*(nb+1)-1;
	try {
		while(ia >= 0 || ib >= 0) {
			if(*m&Agap) {
				bool enough = false;
				while(!enough) {
					track.first.push_back('-');
					track.second.push_back(b[ib--]);
					enough = *m&Astart;
					--m;
				}
			} else if(*m&Bgap) {
				bool enough = false;
				while(!enough) {
					track.first.push_back(a[ia--]);
					track.second.push_back('-');
					enough = *m&Bstart;
					m -= nb+1;
				}
			} else {
				track.first.push_back(a[ia--]);
				track.second.push_back(b[ib--]);
				m -= nb+2;
			}
		}
	}
	catch(bad_alloc) {
		delete[] s;
		delete[] sm;
		delete[] gapb;
		delete[] mtrx;
		throw;
	}
	reverse(track.first.begin(),track.first.end());
	reverse(track.second.begin(),track.second.end());
	
	delete[] s;
	delete[] sm;
	delete[] gapb;
	delete[] mtrx;

	return track;
}

int GetGlbAlignScore(const TCharAlign& align, int gopen, int gap, const char delta[256][256], int end_gap_releif) {
    const CResidueVec& seq1 = align.first;
    const CResidueVec& seq2 = align.second;
    unsigned int len = seq1.size();
	int score = 0;
	for(unsigned int i = 0; i < len; ++i) {
		if(seq1[i] == '-') {
			if(i == 0 || seq1[i-1] != '-') score -= gopen+gap;
			else score -= gap;
		} else if(seq2[i] == '-') {
			if(i == 0 || seq2[i-1] != '-') score -= gopen+gap;
			else score -= gap;
		} else {
			score += delta[seq1[i]][seq2[i]];
		}
	}
    if(seq1[0] == '-' || seq2[0] == '-') 
        score -= end_gap_releif;
    if(seq1[len-1] == '-' || seq2[len-1] == '-') 
        score -= end_gap_releif;
	return score;
}

SMatrix::SMatrix(int match, int mismatch) { // matrix for DNA

    for(int i = 0; i < 256;  ++i) {
        int c = toupper(i);
        for(int j = 0; j < 256;  ++j) {
            if(c != 'N' && c == toupper(j)) matrix[i][j] = match;
            else matrix[i][j] = -mismatch;
        }
    }
}
	
SMatrix::SMatrix(string name) { // matrix for proteins   

    for(int i = 0; i < 256;  ++i) {
        for(int j = 0; j < 256;  ++j) {
            matrix[i][j] = 0;
        }
    }
		
    ifstream from((name).c_str());
    if(!from) throw runtime_error("Matrix "+name+" is not found");
		
    string line;
    while(getline(from,line) && line[0] == '#');
		
    istringstream istr(line);
		
    string letters;
    char c;
    while(istr >> c) letters.push_back(c);
		
    while(getline(from,line)) {
        istringstream istr(line);
        istr >> c;
			
        int score;
        for(int i = 0; istr >> score; ++i) {
            char d = letters[i];
            matrix[(int)c][(int)d] = score;
            matrix[(int)tolower(c)][(int)tolower(d)] = score;
            matrix[(int)c][(int)tolower(d)] = score;
            matrix[(int)tolower(c)][(int)d] = score;
        }
    }
}
	

END_SCOPE(gnomon)
END_SCOPE(ncbi)



