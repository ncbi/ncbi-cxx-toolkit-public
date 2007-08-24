/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Boris Kiryutin
*
* =========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <algo/align/prosplign/prosplign_exception.hpp>
#include <algo/align/prosplign/prosplign.hpp>

#include "Info.hpp"
#include "Ali.hpp"
#include "nucprot.hpp"
#include "NSeq.hpp"
#include "PSeq.hpp"

BEGIN_NCBI_SCOPE

CProSplignOutputOptionsExt::CProSplignOutputOptionsExt(const CProSplignOutputOptions& options) : CProSplignOutputOptions(options)
{
    drop = GetTotalPositives() - GetFlankPositives();
    splice_cost = GetFlankPositives()?((100 - GetFlankPositives())*GetMinFlankingExonLen())/GetFlankPositives():0;
}

BEGIN_SCOPE(prosplign)

bool CInfo::full = true;
bool CInfo::eat_gaps = true;

CInfo::CInfo(const CAli& ali, CProSplignOutputOptionsExt output_options) :
    m_Ali(ali), nseq(*ali.cnseq), m_options(output_options)
{
    full = m_options.IsPassThrough();
    eat_gaps = m_options.GetEatGaps();
}

CInfo::~CInfo(void)
{
}

void CInfo::Cut(void)
{
    //init
    if(full) {
        string::size_type n1 = outp.find_first_not_of("-");
        beg_del = (int)outn.find_first_not_of("-");
        string::size_type n2 = outp.find_last_not_of("-");
        end_del = (int)(outn.size() - 1 - outn.find_last_not_of("-"));
        m_AliPiece.push_back(CNPiece(n1, n2+1, 0, 0));
        return;
    }
    string::size_type n = match.find_first_not_of(" ");
    if(n == string::npos) return; //no matches
    bool ism = true;
    bool isintr = false;
    string::size_type beg = n;
    int efflen = 0;
//    if(match[n] == '+' && pcoor[beg] == 1 && toupper(pseq[0]) == 'M') efflen += CNPiece::start_bonus;
//    if(outr[n+1] == 'M' && pcoor[beg] == 1) efflen += CNPiece::start_bonus; THE LAST VERSION
    for(;n<match.size(); ++n) {
        if(match[n] == '+' || match[n] == '.') {
            if(!ism) {
                ism = true;
                m_AliPiece.push_back(CNPiece(beg, n, 0, efflen));
                beg = n;
                efflen = 0;
            } 
        } else {
            if(ism) {
                //if(outp[n] != '.') {
                    ism = false;
                    /*
                    if(pcoor[n] == pcoor.back()) {
                      char sto[] = "---";
                      int seqn = ncoor[n-1];
                      if(seqn < nseq.size()) sto[0] = nseq.Upper(seqn);
                      seqn++;
                      if(seqn < nseq.size()) sto[1] = NucToChar(nseq[seqn]);       
                      seqn++;
                      if(seqn < nseq.size()) sto[2] = NucToChar(nseq[seqn]);
                      if(sto == string("TAG") || sto == string("TGA") || sto == string("TAA")) efflen += CNPiece::stop_bonus;
                    }
                    */
                    m_AliPiece.push_back(CNPiece(beg, n, efflen, efflen));
                    beg = n;
                    efflen = 0;
                //}
            }
        }
        if(outp[n] != '.') {//inside exon
            efflen ++;  
            isintr = false;
        }
        else {//intron
            if(!isintr) {
                efflen += m_options.splice_cost;
                isintr = true;
            }
        }
    }
    if(ism) {//endpoint is a match
        m_AliPiece.push_back(CNPiece(beg, n, efflen, efflen));
    }
    //join pieces
    list<CNPiece>::iterator itb, ite, itc;
    list<CNPiece>::size_type pnum = m_AliPiece.size() + 1;
    while(pnum > m_AliPiece.size()) {
        pnum = m_AliPiece.size();
        //forward
        for(itb = m_AliPiece.begin(); ; ) {
            ite = itb;
            int slen = 0, spos = 0;
            itc = itb;
            ++itc;
            while(itc != m_AliPiece.end()) {
                if(m_options.Bad(itc)) break;//really bad
                slen += itc->efflen;
                spos += itc->posit;
                if(m_options.Dropof(slen, spos, itb)) break;
                ++itc;//points to  'good'
                if(m_options.Perc(itc, slen, spos, itb)) {
                    if(m_options.BackCheck(itb, itc)) ite = itc;//join!
                }
                slen += itc->efflen;
                spos += itc->posit;
                ++itc;//points to  ' bad' or end
            }
            if(ite != itb) {
                m_options.Join(itb, ite);
                m_AliPiece.erase(itb, ite);
                itb = ite;
            }
            ++itb;
            if(itb == m_AliPiece.end()) break; 
            ++itb;
        }
        //backward
        itb = m_AliPiece.end();
        --itb;
        while(itb != m_AliPiece.begin()) {
            ite = itb;
            int slen = 0, spos = 0;
            itc = itb;
            while(itc != m_AliPiece.begin()) {
                --itc;//points to  ' bad'
                if(m_options.Bad(itc)) break;//really bad
                slen += itc->efflen;
                spos += itc->posit;
                if(m_options.Dropof(slen, spos, itb)) break;
                --itc;//points to  'good'
                if(m_options.Perc(itc, slen, spos, itb)) {
                    if(m_options.ForwCheck(itc, itb)) ite = itc;//join!
                }
                slen += itc->efflen;
                spos += itc->posit;
            }
            if(ite != itb) {
                m_options.Join(ite, itb);
                m_AliPiece.erase(ite, itb);
            }
            if(itb == m_AliPiece.begin()) break;
            --itb;
            --itb;
        }
    }
	//throw out bad pieces
    for(list<CNPiece>::iterator it = m_AliPiece.begin(); it != m_AliPiece.end(); ) {
        if(it->posit == 0) it = m_AliPiece.erase(it);
        else if (it->efflen < m_options.GetMinGoodLen()) it = m_AliPiece.erase(it);
        else ++it;
    }
}

bool CInfo::ReportStart(void) const
{
    return ProtBegOnAli() && m_Ali.cpseq->HasStart() && m_Ali.HasStartOnNuc();
}

bool CInfo::ReportStop(void) const
{
    if(m_AliPiece.empty()) return false;
    int pos = m_AliPiece.back().end - 1;
    int npos = NextNucNullBased(pos);
    if(!nseq.ValidPos(npos+2)) return false;
    if(2 != m_Ali.FindFrame(pos))  return false;
    string sto("---");
    sto[0] = nseq.Upper(npos);
    sto[1] = nseq.Upper(npos+1);
    sto[2] = nseq.Upper(npos+2);
    if(sto == string("TAG") || sto == string("TGA") || sto == string("TAA")) return true;
    return false;
}

// void CInfo::Out(ostream& out)
// {
//     int pcnt = 0; // piece count
//     string sto;
//     bool has_framish = false;
//     bool has_start = false;
//     bool stop_inside = false;

//     for(list<CNPiece>::iterator it = m_AliPiece.begin(); it != m_AliPiece.end(); ++it) {
//         ++pcnt;
//         CProtPiece pp(m_Ali, *it, *this, pcnt);
//         if(pcnt == 1) {
//             if(pp.GetFrame() == 0) {
//                string& sta = pp.m_Exons.front().m_B;//start->sta
//                if('M' == SEQUTIL::nuc2a(CharToNuc(sta[0]), CharToNuc(sta[1]), CharToNuc(sta[2]))) has_start = true;
//             }
//         }
//         sto = pp.m_Exons.back().m_A;//remember stop
//         if(pp.HasFr()) has_framish = true;
//         if(pp.StopInside()) stop_inside = true;
//         pp.Out(out);
//     }

//     if(pcnt > 0) {
//         out<<"start ";
// //      now we print '+' for true start only
//         if(ReportStart()) out<<"+";
// //        if(has_start) out<<"+";
//         else out<<"-";
//         out<<" stop ";
//         //exons are from the last piece here
//         if( ( 2 == m_Ali.FindFrame(m_AliPiece.back().end - 1) ) && (sto == string("TAG") || sto == string("TGA") || sto == string("TAA")) ) out<<"+";
//         else out<<"-";
//         out<<" frameshifts ";
//         if(has_framish) out<<"+";
//         else out<<"-";
//         out<<" stop_inside_exon ";
//         if(stop_inside) out<<"+";
//         else out<<"-";
//         out<<" number_of_pieces "<<pcnt<<endl;
//     }
// }

bool CInfo::ProtBegOnAli(void) const
{
        if(!m_AliPiece.empty() && 
           m_AliPiece.front().beg == ProtBegOnAliCoord()) return true;
        return false;
}

// void CInfo::StatOut(ostream& stat_out) const
// {
//     //STOPPED HERE
// }

int CInfo::GetAliLen(void) const
{
    if(m_AliPiece.empty()) return 0;
    int  count = 0;
    for(list<CNPiece>::const_iterator it = m_AliPiece.begin(); it != m_AliPiece.end(); ++it) {
        for(int i = it->beg; i<it->end; ++i) {
            if(m_Gpseq[i] != 'i') ++count;
        }
    }
    return count;
}

//returns number of positives*100% (after postprocessing!) divided by 3*prot_length
double CInfo::GetTotalPosPerc(void) const
{
    int  posit = 0;
    for(list<CNPiece>::const_iterator it = m_AliPiece.begin(); it != m_AliPiece.end(); ++it) {
        for(int i = it->beg; i<it->end; ++i) {
            if(match[i] == '.' || match[i] == '+') ++posit;
        }
    }
    return posit*100/(double)(3*m_Ali.cpseq->seq.size());
}

int CInfo::GetIdentNum(void) const
{
    if(m_AliPiece.empty()) return 0;
    int  count = 0;
    for(list<CNPiece>::const_iterator it = m_AliPiece.begin(); it != m_AliPiece.end(); ++it) {
        for(int i = it->beg; i<it->end; ++i) {
            if(match[i] == '+') ++count;
        }
    }
    return count;
}

int CInfo::GetPositNum(void) const
{
    if(m_AliPiece.empty()) return 0;
    int  count = 0;
    for(list<CNPiece>::const_iterator it = m_AliPiece.begin(); it != m_AliPiece.end(); ++it) {
        for(int i = it->beg; i<it->end; ++i) {
            if(match[i] == '.' || match[i] == '+') ++count;
        }
    }
    return count;
}

int CInfo::GetNegatNum(void) const
{
    if(m_AliPiece.empty()) return 0;
    int  count = 0;
    for(list<CNPiece>::const_iterator it = m_AliPiece.begin(); it != m_AliPiece.end(); ++it) {
        for(int i = it->beg; i<it->end; ++i) {
            switch(m_Gpseq[i]) {
                case 'u':
                case 'l':
                    if((match[i] != '.') && (match[i] != '+') && m_Gnseq[i]) ++count;
                    break;
                default:
                    break;
            }
        }
    }
    return count;
}

int CInfo::GetNucGapLen(void) const
{
    if(m_AliPiece.empty()) return 0;
    int  count = 0;
    for(list<CNPiece>::const_iterator it = m_AliPiece.begin(); it != m_AliPiece.end(); ++it) {
        for(int i = it->beg; i<it->end; ++i) {
            if(!m_Gnseq[i]) ++count;
        }
    }
    return count;
}

int CInfo::GetProtGapLen(void) const
{
    if(m_AliPiece.empty()) return 0;
    int  count = 0;
    for(list<CNPiece>::const_iterator it = m_AliPiece.begin(); it != m_AliPiece.end(); ++it) {
        for(int i = it->beg; i<it->end; ++i) {
            if(m_Gnseq[i]) {
                switch(m_Gpseq[i]) {
                    case 'g':
                    case 't':
                    case 'T':
                        ++count;
                        break;
                    default:
                        break;
                }
            }
        }
    }
    return count;
}


int CInfo::ProtBegOnAliCoord(void) const
{
    int res = 0;
    vector<CAliPiece>::const_iterator it = m_Ali.m_ps.begin();
    for(;it != m_Ali.m_ps.end(); ++it) {
      if( it->m_type == eHP || it->m_type == eSP ) res += it->m_len;
      else return res;
    }
    return 0;
}

int CInfo::ProtEndOnAliCoord(void) const
{
    int res = GetFullAliLen();
    for(vector<CAliPiece>::size_type i = m_Ali.m_ps.size()-1; i >= 0; --i) {
      if( m_Ali.m_ps[i].m_type == eHP || m_Ali.m_ps[i].m_type  == eSP ) res -= m_Ali.m_ps[i].m_len;
      else return res;
    }
    return res;
}

//CNPiece implementation

CNPiece::CNPiece(string::size_type obeg, string::size_type oend, int oposit, int oefflen)
    {
        beg = (int)obeg;
        end = (int)oend;
        posit = oposit;
        efflen = oefflen;
    }
END_SCOPE(prosplign)

    bool CProSplignOutputOptionsExt::Bad(list<prosplign::CNPiece>::iterator it)
{
    if(it->efflen > GetMaxBadLen()) return true;
    return false;
}

bool CProSplignOutputOptionsExt::Dropof(int efflen, int posit, list<prosplign::CNPiece>::iterator it)
{
    if((GetTotalPositives()-drop)*(efflen+it->efflen) > 100*(posit+it->posit)) return true;
    return false;
}

bool CProSplignOutputOptionsExt::Perc(list<prosplign::CNPiece>::iterator add, int efflen, int posit, list<prosplign::CNPiece>::iterator cur)
{
    if(Dropof(efflen, posit, add)) return false;
    if(GetTotalPositives()*(efflen+cur->efflen+add->efflen) > 100*(posit+cur->posit+add->posit)) return false;
    return true;
}

void CProSplignOutputOptionsExt::Join(list<prosplign::CNPiece>::iterator it, list<prosplign::CNPiece>::iterator last)
{
    int posit = last->posit;
    int efflen = last->efflen;
    for(list<prosplign::CNPiece>::iterator it1 = it; it1 != last; ++it1) {
        posit += it1->posit;
        efflen += it1->efflen;
    }
    last->posit = posit;
    last->efflen = efflen;
    last->beg = it->beg;
}

bool CProSplignOutputOptionsExt::ForwCheck(list<prosplign::CNPiece>::iterator it1, list<prosplign::CNPiece>::iterator it2)
{
    int efflen = it1->efflen;
    int pos = it1->posit;
    for(;it1!= it2;){
        it1++;
        if(Dropof(efflen, pos, it1)) return false;
        efflen += it1->efflen;
        pos += it1->posit;
        it1++;
        efflen += it1->efflen;
        pos += it1->posit;
    }
    return true;
}

bool CProSplignOutputOptionsExt::BackCheck(list<prosplign::CNPiece>::iterator it1, list<prosplign::CNPiece>::iterator it2)
{
    int efflen = it2->efflen;
    int pos = it2->posit;
    for(;it1!= it2;){
        it2--;
        if(Dropof(efflen, pos, it2)) return false;
        efflen += it2->efflen;
        pos += it2->posit;
        it2--;
        efflen += it2->efflen;
        pos += it2->posit;
    }
    return true;
}


BEGIN_SCOPE(prosplign)

bool CInfo::IsFV(int n)
{
    int i, len = 0;
    for(i=n;i>=0;--i) {
        if(outn[i] == '-') len++;
        else if(outp[i] != '.') break;
    }
    for(i=n+1;i<(int)outn.size();++i) {
        if(outn[i] == '-') len++;
        else if(outp[i] != '.') break;
    }
    if(len%3) return true;
    return false;
}


bool CInfo::IsFH(int n)
{
    int i, len = 0;
    for(i=n;i>=0;--i) {
        if(outp[i] == '-') len++;
        else if(outp[i] != '.') break;
    }
    for(i=n+1;i<(int)outn.size();++i) {
        if(outp[i] == '-') len++;
        else if(outp[i] != '.') break;
    }
    if(len%3) return true;
    return false;
}

//Interface for info output

CExonPiece::CExonPiece(void)
{
}

CExonPiece::~CExonPiece(void)
{
}

// void CExonIns::Out(ostream& out) 
// {
//     out<<"\tI("<<m_From<<"-"<<m_To<<")";
// /*  old version
//   out<<"\tI"<<GetLen();
// */
// }

// void CExonMM::Out(ostream& out) 
// {
//     out<<"\t"<<m_From<<"\t"<<m_To;
// }


CExonStruct::CExonStruct(void)
{
    m_A = m_B = "--";
}

CExonStruct::~CExonStruct(void)
{
    //for(vector<CExonPiece *>::iterator it = m_pc.begin(); it != m_pc.end(); ++it) delete *it;
}

// void CExonStruct::StructOut(ostream& out)
// {
//     for(vector<CExonPiece *>::iterator it = m_pc.begin(); it != m_pc.end(); ++it) {
//         (*it)->Out(out);
//     }
// }


void CProtPiece::Clear(void)
{
    vector<CExonStruct>::iterator et;
    vector<CExonPiece *>::iterator it;
    for(et = m_Exons.begin(); et != m_Exons.end(); ++et) {
        for(it = et->m_pc.begin(); it != et->m_pc.end(); ++it) {
            delete *it;
        }
    }
}

void CProtPiece::EatGaps(void)
{
    vector<CExonStruct>::iterator et, et1;
    vector<CExonPiece *>::iterator it;
    vector<CExonStruct> tmp;
    tmp.resize(m_Exons.size());
    for(et = m_Exons.begin(), et1 = tmp.begin(); et != m_Exons.end(); ++et, ++et1) {
        for(it = et->m_pc.begin(); it != et->m_pc.end(); ++it) {            
            if(dynamic_cast<CExonDel *> (*it)) {
                if(IsFr(et, it)) {
                    CExonDel *del = new CExonDel(*dynamic_cast<CExonDel *> (*it));                    
                    et1->m_pc.push_back(del);
                }
                //of not frameshift just ignore
            } else {
                CExonNonDel *ndel = dynamic_cast<CExonNonDel *> (*it);
                if(!ndel) throw runtime_error("exon piece of unknown type");
                if(IsFr(et, it)) {
                    CExonIns *fr = dynamic_cast<CExonIns *>(*it);
                    if(!fr) throw runtime_error("frameshift of unknown type"); 
                    et1->m_pc.push_back(new CExonIns(*fr));
                } else {
                    CExonMM *cur = NULL;
                    if(!et1->m_pc.empty()) cur = dynamic_cast<CExonMM *>(et1->m_pc.back());
                    if(!cur) {//new
                        cur = new CExonMM();
                        et1->m_pc.push_back(cur);
                        cur->m_From = ndel->m_From;
                    }
                    cur->m_To = ndel->m_To;
                    cur->m_Len = cur->m_To - cur->m_From + 1;
                }
            }
        }
    }

    Clear();
    for(et = m_Exons.begin(), et1 = tmp.begin(); et != m_Exons.end(); ++et, ++et1) {
        et->m_pc.swap(et1->m_pc);
    }
}

bool CProtPiece::HasFr(void)
{
    vector<CExonStruct>::iterator et;
    vector<CExonPiece *>::iterator it;
    for(et = m_Exons.begin(); et != m_Exons.end(); ++et) {
        for(it = et->m_pc.begin(); it != et->m_pc.end(); ++it) {
            if(IsFr(et, it)) return true;
        }
    }
    return false;
}

bool CProtPiece::IsFr(vector<CExonStruct>::iterator et, vector<CExonPiece *>::iterator it)
{
    vector<CExonStruct>::iterator etf = et;
    vector<CExonPiece *>::iterator itf = it;
    if(dynamic_cast<CExonIns *>(*it)) {
         int len = 0;
         bool good = false;
        //len to left
        do {
            if(dynamic_cast<CExonIns *>(*it)) len += (*it)->GetLen();
            else break;
            if(it == et->m_pc.begin()) {
                if(et != m_Exons.begin()) {
                    --et;
                    it = et->m_pc.end();
                } else break;
            }
            --it;
        } while(true);
        //is there a match on left?
        good = false;
        do {
            if(dynamic_cast<CExonMM *> (*it)) {
                good = true;
                break;
            }
            if(it == et->m_pc.begin()) {
                if(et != m_Exons.begin()) {
                    --et;
                    it = et->m_pc.end();
                } else break;
            }
            --it;
        } while(true);
        if(!good) return false; //no match on left -> not frameshift
//forward check
        do {
            itf++;
            if(itf == etf->m_pc.end()) {
                etf++;
                if(etf == m_Exons.end()) break;
                itf = etf->m_pc.begin();
            }
            if(dynamic_cast<CExonIns *> (*itf)) len += (*itf)->GetLen();
            else break;
        } while(true);
//search for a match
        if(etf == m_Exons.end()) return false; //no match on right -> not frameshift
        good = false;
        do {
            if(dynamic_cast<CExonMM *> (*itf)) {
                good = true;
                break;
            }
            itf++;
            if(itf == etf->m_pc.end()) {
                etf++;
                if(etf == m_Exons.end()) break;
                itf = etf->m_pc.begin();
            }
        } while(true);

        if(!good) return false; //no match on right -> not frameshift



        //there are matches at the right end at the left, check for length
        if(len%3) return true;
        return false;
    }




    else if(dynamic_cast<CExonDel *> (*it)) {
         int len = 0;
         bool good = false;
        //len to left
        do {
            if(dynamic_cast<CExonDel *> (*it)) len += (*it)->GetLen();
            else break;
            if(it == et->m_pc.begin()) {
                if(et != m_Exons.begin()) {
                    --et;
                    it = et->m_pc.end();
                } else break;
            }
            --it;
        } while(true);
        //is there a match on left?
        good = false;
        do {
            if(dynamic_cast<CExonMM *> (*it)) {
                good = true;
                break;
            }
            if(it == et->m_pc.begin()) {
                if(et != m_Exons.begin()) {
                    --et;
                    it = et->m_pc.end();
                } else break;
            }
            --it;
        } while(true);
        if(!good) return false; //no match on left -> not frameshift
//forward check
        do {
            itf++;
            if(itf == etf->m_pc.end()) {
                etf++;
                if(etf == m_Exons.end()) break;
                itf = etf->m_pc.begin();
            }
            if(dynamic_cast<CExonDel *> (*itf)) len += (*itf)->GetLen();
            else break;
        } while(true);
//search for a match
        if(etf == m_Exons.end()) return false; //no match on right -> not frameshift
        good = false;
        do {
            if(dynamic_cast<CExonMM *> (*itf)) {
                good = true;
                break;
            }
            itf++;
            if(itf == etf->m_pc.end()) {
                etf++;
                if(etf == m_Exons.end()) break;
                itf = etf->m_pc.begin();
            }
        } while(true);

        if(!good) return false; //no match on right -> not frameshift



        //there are matches at the right end at the left, check for length
        if(len%3) return true;
        return false;
    }




    return false;//not a gap
}

        
CProtPiece::CProtPiece(const CAli& cali, CNPiece& np, CInfo& info, int num) : m_Info(info)
{
        m_Num = num;
        vector <CExonStruct>& exons = m_Exons;
        CNPiece *it = &np;

        CExonStruct exon;
        int shift;//shift from pit beginning
        int alipos = it->beg;//position on alignment
        vector<CAliPiece>::const_iterator pit;
        cali.FindPos(pit, shift, it->beg);
        exon.m_AliBeg = it->beg;
        if(pit->m_type == eSP) runtime_error("Wrong piece asignment in CProtPiece");
        do {
          switch (pit->m_type) {
            case eMP:
                {
                CExonMM *mm = new CExonMM;
                exon.m_pc.push_back(mm);
                mm->m_From = info.ncoor[alipos];  
                int topos = alipos + pit->m_len - shift - 1;
                if(topos >= it->end) topos = it->end -1;
                mm->m_To = info.ncoor[topos];
                mm->m_Len = mm->m_To - mm->m_From + 1;
                break;
                }
            case eVP:
                {
                CExonDel *del = new CExonDel;
                exon.m_pc.push_back(del);
                del->m_Len = pit->m_len - shift;
                break;
                }
            case eHP:
                {
                CExonIns *ins = new CExonIns;
                exon.m_pc.push_back(ins);
                ins->m_From = info.ncoor[alipos];  
                int topos = alipos + pit->m_len - shift - 1;
                if(topos >= it->end) topos = it->end -1;
                ins->m_To = info.ncoor[topos];
                ins->m_Len = ins->m_To - ins->m_From + 1;
                break;
                }
            case eSP:
                {
                exon.m_AliEnd = alipos;
                if(exon.m_AliEnd != exon.m_AliBeg) {//ignore exon of 0 length 
                    //WARNING deletion shifts to the beg of next exon (if exists), it makes info slightly different from the alignment
                    exons.push_back(exon);                
                    exon.m_pc.clear();
                }
                exon.m_AliBeg = alipos + pit->m_len - shift;
                break;
                }
          }
          alipos += pit->m_len - shift;
          shift = 0;
          if(alipos >= it->end) break;
          else ++pit;
        } while(true);
        //last exon 
        if(pit->m_type != eSP) {
            exon.m_AliEnd= it->end;
            exons.push_back(exon);
            exon.m_pc.clear(); 
        }
        //remove regular gaps
       if(info.eat_gaps) EatGaps();

       //add start/stop/splice sites
        vector<CExonStruct>::iterator et;
        et = exons.begin();
        et->m_B = "---";
        /*  three before
        int npos;//null based
        npos = info.PrevNucNullBased(et->m_AliBeg);
        npos++;
        if(npos<info.) et->m_B[2] = cali.cnseq->Upper(npos);
        --npos;
        if(npos>-1) et->m_B[1] = cali.cnseq->Upper(npos);
        --npos;
        if(npos>-1) et->m_B[0] = cali.cnseq->Upper(npos);
        */
        // first three on alignment (including gaps
        int apos = et->m_AliBeg;
        et->m_B[0] = info.outn[apos++];
        if(apos < et->m_AliEnd) et->m_B[1] = info.outn[apos++];
        if(apos < et->m_AliEnd) et->m_B[2] = info.outn[apos];
        /**/
        int npos;
        /*
        npos = info.NextNucNullBased(et->m_AliBeg-1);  //first three
        et->m_B[0] = cali.cnseq->Upper(npos);
        npos++;
        et->m_B[1] = cali.cnseq->Upper(npos);
        npos++;
        et->m_B[2] = cali.cnseq->Upper(npos);
        */
        do {
            npos = info.NextNucNullBased(et->m_AliEnd -1);
            if(npos != info.ncoor.back()) {
                et->m_A[0] = cali.cnseq->Upper(npos);
                ++npos;
                if(npos != info.ncoor.back())  et->m_A[1] = cali.cnseq->Upper(npos);
            }
            ++et;
            if(et == exons.end()) break;
            npos = info.PrevNucNullBased(et->m_AliBeg);
            if(npos>-1) et->m_B[1] = cali.cnseq->Upper(npos);
            --npos;
            if(npos>-1) et->m_B[0] = cali.cnseq->Upper(npos);
        } while(true);
        //end of the last one
        npos = info.NextNucNullBased(exons.back().m_AliEnd -1);
        exons.back().m_A = "---";
        if(cali.cnseq->ValidPos(npos)) exons.back().m_A[0] = cali.cnseq->Upper(npos++);
        if(cali.cnseq->ValidPos(npos)) exons.back().m_A[1] = cali.cnseq->Upper(npos++);
        if(cali.cnseq->ValidPos(npos)) exons.back().m_A[2] = cali.cnseq->Upper(npos);
}

// void CProtPiece::Out(ostream& out)
// {

//         //out!!
//     vector<CExonStruct>::iterator et;
//         int len = 0, pos = 0, match = 0;
//         for(et = m_Exons.begin(); ;) {
//             out<<m_Num<<"\t"<<et->m_B;
//             et->StructOut(out);
//             out<<"\t"<<et->m_A;
//             //strand
//            if(strand) out<<"\t"<<"+";
//            else out<<"\t"<<"-";
//             //positives for exon
//             int epos = 0;
//             int ematch = 0;
//             for(int i=et->m_AliBeg; i < et->m_AliEnd; ++i) {
//                 if(m_Info.match[i] == '.' || m_Info.match[i] == '+') epos++;
//                 if(m_Info.match[i] == '+') ematch++;
//             }
//             match += ematch;
//             pos += epos;
//             len += et->m_AliEnd - et->m_AliBeg;
//             out<<"\t"<<"id:"<<"\t"<<(100*ematch)/(et->m_AliEnd - et->m_AliBeg)<<"%";
//             out<<"\t"<<"pos:"<<"\t"<<(100*epos)/(et->m_AliEnd - et->m_AliBeg)<<"%";
//             if(m_Info.StopInside(et->m_AliBeg, et->m_AliEnd)) out<<"\t*";
//             //last one?
//             ++et;
//             if(et == m_Exons.end()) {
//                 out<<"\ttotal:";
//                 out<<"\t"<<"id:"<<"\t"<<(100*match)/len<<"%";
//                 out<<"\t"<<"pos:"<<"\t"<<(100*pos)/len<<"%";
//                 if(!m_Info.full) {
//                     //what frame
//                     out<<"\tframe: \t"<<GetFrame()<<"\n";
//                 }
//                 else out<<"\n";
//                 break;
//             } else {
//                 out<<endl;
//             }
//         }

// }

int CProtPiece::GetFrame(void)//frame at the beginning of the first exon. exon must be at least length three
{
    if(m_Info.full) return 0;//full always starts at protein beginning

                    int frame;
                    int apos = m_Exons.front().m_AliBeg;
                    if((m_Exons.front().m_AliEnd - m_Exons.front().m_AliBeg < 4) || m_Info.outn[apos] == '-' || m_Info.outn[apos+1] == '-' ||
                       m_Info.outp[apos] == '-' || m_Info.outp[apos+1] == '-') {
                           throw runtime_error("prot piece finding error");
                    }
                    if(isspace(m_Info.outr[apos]) && isupper(m_Info.outr[apos+1])) frame = 0;
                    //else if(isspace(m_Info.outr[apos+1]) && isupper(m_Info.outr[apos])) frame = 1;//should not happen
                    else if(islower(m_Info.outr[apos])) {
                        if(isspace(m_Info.outr[apos+1]) || (islower(m_Info.outr[apos+1]) &&  m_Info.outr[apos] != m_Info.outr[apos+1])) frame = 2;
                        else if(m_Info.outr[apos] == m_Info.outr[apos+1]) frame = 1;//true if exon length > 3
                        else throw runtime_error("prot piece finding error");
                    }
                    else if(isspace(m_Info.outr[apos+1]) && isspace(m_Info.outr[apos])) frame = 2;//should not happen
                    else throw runtime_error("prot piece finding error");
     return frame;
}

bool CProtPiece::StopInside(void)
{
    for(vector<CExonStruct>::iterator et = m_Exons.begin(); et != m_Exons.end(); ++et) {
        if(m_Info.StopInside(et->m_AliBeg, et->m_AliEnd)) return true;
    }
    return false;
}

END_SCOPE(prosplign)
END_NCBI_SCOPE
