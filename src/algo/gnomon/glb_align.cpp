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

CCigar GlbAlign(const  char* a, int na, const  char*  b, int nb, int rho, int sigma, const char delta[256][256]) {
    //	rho - new gap penalty (one base gap rho+sigma)
    // sigma - extension penalty
    
	int* s = new int[nb+1];                                 // best scores in current a-raw
	int* sm = new int[nb+1];                                // best scores in previous a-raw
	int* gapb = new int[nb+1];                              // best score with b-gap
    char* mtrx = new  char[(na+1)*(nb+1)];                  // backtracking info (Astart/Bstart gap start, Agap/Bgap best score has gap and should be backtracked to Asrt/Bsart)

    int rs = rho+sigma;
    int bignegative = numeric_limits<int>::min()/2;

	sm[0] = 0;
	sm[1] = -rs;                  // scores for --------------   (the best scores for i == -1)
	for(int i = 2; i <= nb; ++i)  //            BBBBBBBBBBBBBB
        sm[i] = sm[i-1]-sigma;    
	s[0] = -rs;                   // score for A   (the best score for j == -1 and i == 0)
                                  //           -
	for(int i = 0; i <= nb; ++i) 
        gapb[i] = bignegative;
	
	enum{Agap = 1, Bgap = 2, Astart = 4, Bstart = 8};
	
    mtrx[0] = 0;
    for(int i = 1; i <= nb; ++i) {  // ---------------
        mtrx[i] = Agap;             // BBBBBBBBBBBBBBB
    }
    mtrx[1] |= Astart;
	
    char* m = mtrx+nb;
	for(int i = 0; i < na; ++i) {
		*(++m) = Bstart|Bgap;       //AAAAAAAAAAAAAAA
                                    //---------------
        int gapa = bignegative;
		int ai = a[i];
        const char* matrix = delta[ai];
		int* sp = s;
		for(int j = 0; j < nb; ) {
			*(++m) = 0;
			int ss = sm[j]+matrix[(int)b[j]];

			gapa -= sigma;
			if(*sp-rs > gapa) {    // for j == 0 this will open   AAAAAAAAAAA-  which could be used if mismatch is very expensive
				gapa = *sp-rs;     //                             -----------B
				*m |= Astart;
			}
			
			int& gapbj = gapb[++j];
			gapbj -= sigma;
			if(sm[j]-rs > gapbj) {  // for i == 0 this will open  BBBBBBBBBBB- which could be used if mismatch is very expensive
				gapbj = sm[j]-rs;   //                            -----------A
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
		*s = *sm-sigma; 
	}
	
	int ia = na-1;
	int ib = nb-1;
	m = mtrx+(na+1)*(nb+1)-1;
    CCigar track(ia, ib);
    while(ia >= 0 || ib >= 0) {
        if(*m&Agap) {
            int len = 1;
            while(!(*m&Astart)) {
                ++len;
                --m;
            }
            --m;
            ib -= len;
            track.PushFront(CCigar::SElement(len,'D'));
        } else if(*m&Bgap) {
            int len = 1;
            while(!(*m&Bstart)) {
                ++len;
                m -= nb+1;
            }
            m -= nb+1;
            ia -= len;
            track.PushFront(CCigar::SElement(len,'I'));
        } else {
            track.PushFront(CCigar::SElement(1,'M'));
            --ia;
            --ib;
            m -= nb+2;
        }
    }
	
	delete[] s;
	delete[] sm;
	delete[] gapb;
	delete[] mtrx;

	return track;
}


CCigar LclAlign(const  char* a, int na, const  char*  b, int nb, int rho, int sigma, const char delta[256][256]) {
    //	rho - new gap penalty (one base gap rho+sigma)
    // sigma - extension penalty

	int* s = new int[nb+1];                                 // best scores in current a-raw
	int* sm = new int[nb+1];                                // best scores in previous a-raw
	int* gapb = new int[nb+1];                              // best score with b-gap
    char* mtrx = new  char[(na+1)*(nb+1)];                  // backtracking info (Astart/Bstart gap start, Agap/Bgap best score has gap and should be backtracked to Asrt/Bsart; Zero stop bactracking)

    int rs = rho+sigma;

    enum{Agap = 1, Bgap = 2, Astart = 4, Bstart = 8, Zero = 16};
    
    for(int i = 0; i <= nb; ++i) {
        sm[i] = 0;
        mtrx[i] = Zero;
        gapb[i] = 0;
    }
    s[0] = 0;

    int max_score = 0;
    char* max_ptr = mtrx;
    char* m = mtrx+nb;
	
    for(int i = 0; i < na; ++i) {
		*(++m) = Zero;
		int gapa = 0;
		int ai = a[i];
        const char* matrix = delta[ai];
		int* sp = s;
		for(int j = 0; j < nb; ) {
			*(++m) = 0;
			int ss = sm[j]+matrix[(int)b[j]];

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
                    if(ss > max_score) {
                        max_score = ss;
                        max_ptr = m;
                    }
				} else {
					*(++sp) = gapa;
					*m |= Agap;
				}
			} else {
				if(ss > gapbj) {
					*(++sp) = ss;
                    if(ss > max_score) {
                        max_score = ss;
                        max_ptr = m;
                    }
				} else {
					*(++sp) = gapbj;
					*m |= Bgap;
				}
			}
            if(*sp <= 0) {
                *sp = 0;
                *m |= Zero;  
            }
		}
		swap(sm,s);
	}

    int ia = (max_ptr-mtrx)/(nb+1)-1;
    int ib = (max_ptr-mtrx)%(nb+1)-1;
    CCigar track(ia, ib);
    m = max_ptr;
    while((ia >= 0 || ib >= 0) && !(*m&Zero)) {
        if(*m&Agap) {
            int len = 1;
            while(!(*m&Astart)) {
                ++len;
                --m;
            }
            --m;
            ib -= len;
            track.PushFront(CCigar::SElement(len,'D'));
        } else if(*m&Bgap) {
            int len = 1;
            while(!(*m&Bstart)) {
                ++len;
                m -= nb+1;
            }
            m -= nb+1;
            ia -= len;
            track.PushFront(CCigar::SElement(len,'I'));
        } else {
            track.PushFront(CCigar::SElement(1,'M'));
            --ia;
            --ib;
            m -= nb+2;
        }
    }

    delete[] s;
    delete[] sm;
    delete[] gapb;
    delete[] mtrx;

    return track;
}


CCigar LclAlign(const  char* a, int na, const  char*  b, int nb, int rho, int sigma, bool pinleft, bool pinright, const char delta[256][256]) {
    //	rho - new gap penalty (one base gap rho+sigma)
    // sigma - extension penalty

	int* s = new int[nb+1];                                 // best scores in current a-raw
	int* sm = new int[nb+1];                                // best scores in previous a-raw
	int* gapb = new int[nb+1];                              // best score with b-gap
    char* mtrx = new  char[(na+1)*(nb+1)];                  // backtracking info (Astart/Bstart gap start, Agap/Bgap best score has gap and should be backtracked to Asrt/Bsart; Zero stop bactracking)

    int rs = rho+sigma;
    int bignegative = numeric_limits<int>::min()/2;

    enum{Agap = 1, Bgap = 2, Astart = 4, Bstart = 8, Zero = 16};
    
    sm[0] = 0;
    mtrx[0] = 0;
    gapb[0] = bignegative;  // not used
    if(pinleft) {
        if(nb > 0) {
            sm[1] = -rs;
            mtrx[1] = Astart|Agap;
            gapb[1] = bignegative;
            for(int i = 2; i <= nb; ++i) {
                sm[i] = sm[i-1]-sigma;
                mtrx[i] = Agap;
                gapb[i] = bignegative;
            }
        }
        s[0] = -rs; 
    } else {
        for(int i = 1; i <= nb; ++i) {
            sm[i] = 0;
            mtrx[i] = Zero;
            gapb[i] = bignegative;
        }
        s[0] = 0;
    }

    int max_score = 0;
    char* max_ptr = mtrx;
	
    char* m = mtrx+nb;
	for(int i = 0; i < na; ++i) {
		*(++m) = pinleft ? Bstart|Bgap : Zero;
		int gapa = bignegative;
		int ai = a[i];
		
		const char* matrix = delta[ai];
		int* sp = s;
		for(int j = 0; j < nb; ) {
			*(++m) = 0;
			int ss = sm[j]+matrix[(int)b[j]];

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
                    if(ss > max_score) {
                        max_score = ss;
                        max_ptr = m;
                    }
				} else {
					*(++sp) = gapa;
					*m |= Agap;
				}
			} else {
				if(ss > gapbj) {
					*(++sp) = ss;
                    if(ss > max_score) {
                        max_score = ss;
                        max_ptr = m;
                    }
				} else {
					*(++sp) = gapbj;
					*m |= Bgap;
				}
			}
            if(*sp <= 0 && !pinleft) {
                *sp = 0;
                *m |= Zero;  
            }
		}
		swap(sm,s);
        if(pinleft)
            *s = *sm-sigma; 
	}

    int maxa, maxb;
    if(pinright) {
        maxa = na-1;
        maxb = nb-1;
        max_score = sm[nb];
    } else {
        maxa = (max_ptr-mtrx)/(nb+1)-1;
        maxb = (max_ptr-mtrx)%(nb+1)-1;
        m = max_ptr;
    }
    int ia = maxa;
    int ib = maxb;
    CCigar track(ia, ib);
    while((ia >= 0 || ib >= 0) && !(*m&Zero)) {
        if(*m&Agap) {
            int len = 1;
            while(!(*m&Astart)) {
                ++len;
                --m;
            }
            --m;
            ib -= len;
            track.PushFront(CCigar::SElement(len,'D'));
        } else if(*m&Bgap) {
            int len = 1;
            while(!(*m&Bstart)) {
                ++len;
                m -= nb+1;
            }
            m -= nb+1;
            ia -= len;
            track.PushFront(CCigar::SElement(len,'I'));
        } else {
            track.PushFront(CCigar::SElement(1,'M'));
            --ia;
            --ib;
            m -= nb+2;
        }
    }

    delete[] s;
    delete[] sm;
    delete[] gapb;
    delete[] mtrx;

    return track;
}

CCigar VariBandAlign(const  char* a, int na, const  char*  b, int nb, int rho, int sigma, const char delta[256][256], const TSignedSeqRange* blimits) {
    //	rho - new gap penalty (one base gap rho+sigma)
    // sigma - extension penalty

	int* s = new int[nb+1];                                 // best scores in current a-raw
	int* sm = new int[nb+1];                                // best scores in previous a-raw
	int* gapb = new int[nb+1];                              // best score with b-gap
    char* mtrx = new  char[(na+1)*(nb+1)];                  // backtracking info (Astart/Bstart gap start, Agap/Bgap best score has gap and should be backtracked to Asrt/Bsart; Zero stop bactracking)

    int rs = rho+sigma;

    enum{Agap = 1, Bgap = 2, Astart = 4, Bstart = 8, Zero = 16};

    for(int i = 0; i <= nb; ++i) {
        s[i] = 0;
        sm[i] = 0;
        gapb[i] = 0;
        mtrx[i] = Zero;
    }

    int max_score = 0;
    char* max_ptr = mtrx;
    char* m = mtrx+nb;
	
    const TSignedSeqRange* last = blimits+na;
    while(true) {
		int ai = *a++;
		const char* matrix = delta[ai];

        int bleft = blimits->GetFrom();
        int bright = blimits->GetTo();
        m += bleft;
        *(++m) = Zero;
		int gapa = 0;
        int* sp = s+bleft;
        *sp = 0;
		for(int j = bleft; j <= bright; ) {
			*(++m) = 0;
			int ss = sm[j]+matrix[(int)b[j]];

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
                    if(ss > max_score) {
                        max_score = ss;
                        max_ptr = m;
                    }
				} else {
					*(++sp) = gapa;
					*m |= Agap;
				}
			} else {
				if(ss > gapbj) {
					*(++sp) = ss;
                    if(ss > max_score) {
                        max_score = ss;
                        max_ptr = m;
                    }
				} else {
					*(++sp) = gapbj;
					*m |= Bgap;
				}
			}
            if(*sp <= 0) {
                *sp = 0;
                *m |= Zero;  
            }
		}
        if(++blimits == last)
            break;

		swap(sm,s);

        int next = blimits->GetTo();
        for( ; bright < next-1; ++bright)
            *(++m) = Zero;
        
        m += nb-bright-1;
	}
  
    int ia = (max_ptr-mtrx)/(nb+1)-1;
    int ib = (max_ptr-mtrx)%(nb+1)-1;
    CCigar track(ia, ib);
    m = max_ptr;
    while((ia >= 0 || ib >= 0) && !(*m&Zero)) {
        if(*m&Agap) {
            int len = 1;
            while(!(*m&Astart)) {
                ++len;
                --m;
            }
            --m;
            ib -= len;
            track.PushFront(CCigar::SElement(len,'D'));
        } else if(*m&Bgap) {
            int len = 1;
            while(!(*m&Bstart)) {
                ++len;
                m -= nb+1;
            }
            m -= nb+1;
            ia -= len;
            track.PushFront(CCigar::SElement(len,'I'));
        } else {
            track.PushFront(CCigar::SElement(1,'M'));
            --ia;
            --ib;
            m -= nb+2;
        }
    }

    delete[] s;
    delete[] sm;
    delete[] gapb;
    delete[] mtrx;

    return track;
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

double Entropy(const string& seq) {
    int length = seq.size();
    double tA = 1.e-8;
    double tC = 1.e-8;
    double tG = 1.e-8;
    double tT = 1.e-8;
    ITERATE(string, i, seq) {
        switch(*i) {
        case 'A': tA += 1; break;
        case 'C': tC += 1; break;
        case 'G': tG += 1; break;
        case 'T': tT += 1; break;
        default: break;
        }
    }
    double entropy = -(tA*log(tA/length)+tC*log(tC/length)+tG*log(tG/length)+tT*log(tT/length))/(length*log(4));
    
    return entropy;
}

	

END_SCOPE(gnomon)
END_SCOPE(ncbi)



