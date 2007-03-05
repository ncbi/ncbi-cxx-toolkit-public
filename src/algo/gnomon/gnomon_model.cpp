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
#include "gnomon_seq.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

int CAlignVec::FShiftedLen(TSignedSeqPos a, TSignedSeqPos b) const
{
        unsigned int i;
        for(i = 0; i < size() && (*this)[i].GetTo() < a; ++i);
        int len = (*this)[i].GetTo()-a+1;
        for(++i; i < size() && (*this)[i].GetFrom() <= b; ++i)
        {
            len += (*this)[i].GetTo()-(*this)[i].GetFrom()+1;
        }
        len -= (*this)[i-1].GetTo()-b;
        
        for(i = 0; i < m_fshifts.size(); ++i)
        {
            if(m_fshifts[i].IsDeletion()) {
                if(m_fshifts[i].Loc() >= a && m_fshifts[i].Loc() <= b+1) len += m_fshifts[i].Len();
            } else {
                if(m_fshifts[i].Loc() > a && m_fshifts[i].Loc() < b) len -= m_fshifts[i].Len();
            }
        }
        
        return len;
}

template <typename Vec>
class vec_traits {
public:
    typedef typename Vec::value_type value_type;
    static value_type _fromACGT(TResidue x)
    { return x; }
};
template<>
class vec_traits<CEResidueVec> {
public:
    typedef CEResidueVec::value_type value_type;
    static value_type _fromACGT(TResidue x)
    { return fromACGT(x); }
};

template <class Vec>
void CAlignVec::GetSequence(const Vec& seq, Vec& mrna, TIVec* mrnamap, bool cdsonly) const
{
    TSignedSeqRange lim = (cdsonly ? RealCdsLimits() : Limits());
    int len = (cdsonly ? CdsLen() : AlignLen());
    mrna.clear();

    mrna.reserve(len);
    if(mrnamap != 0)
    {
        mrnamap->clear();
        mrnamap->reserve(len);
    }
    
    TFrameShifts::const_iterator fsi = FrameShifts().begin();
    ITERATE(CAlignVec, i, *this)
    {
        TSignedSeqPos start = i->GetFrom();
        TSignedSeqPos stop = i->GetTo();
        
        if(stop < lim.GetFrom()) continue;
        if(start > lim.GetTo()) break;
        
        start = max(start,lim.GetFrom());
        stop = min(stop,lim.GetTo());
        while(fsi != FrameShifts().end() && fsi->Loc() < start) ++fsi;    // skipping possible insertions before right exon boundary
        for(TSignedSeqPos k = start; k <= stop; ++k) {
            if (fsi != FrameShifts().end() && fsi->IsInsertion() && k == fsi->Loc()) {
                k += fsi->Len()-1;
                ++fsi;
            } else if (fsi != FrameShifts().end() && fsi->IsDeletion() && k == fsi->Loc()) {
                for(int j = 0; j < fsi->Len(); ++j) {
                    mrna.push_back(vec_traits<Vec>::_fromACGT(fsi->DeletedValue()[j]));
                    if(mrnamap != 0) mrnamap->push_back(-1);
                }

                --k;          // this position will be reconsidered next time (it still could be an insertion)
                ++fsi;
            } else {
                mrna.push_back(seq[k]);
                if(mrnamap != 0) mrnamap->push_back(k);
            }
        }
        if(fsi != FrameShifts().end() && fsi->IsDeletion() && stop+1 == fsi->Loc())   // deletion at the right exon boundary
        {
            for(int j = 0; j < fsi->Len(); ++j)
            {
                mrna.push_back(vec_traits<Vec>::_fromACGT(fsi->DeletedValue()[j]));
                if(mrnamap != 0) mrnamap->push_back(-1);
            }
        }
    }
    
    if(Strand() == eMinus) {
        Complement(mrna.begin(), mrna.end());
        if(mrnamap != 0) reverse(mrnamap->begin(),mrnamap->end());
    }
}

template
void CAlignVec::GetSequence<CEResidueVec>(const CEResidueVec& seq, CEResidueVec& mrna, TIVec* mrnamap, bool cdsonly) const;
template
void CAlignVec::GetSequence<CResidueVec>(const CResidueVec& seq, CResidueVec& mrna, TIVec* mrnamap, bool cdsonly) const;


TSignedSeqRange CAlignVec::RealCdsLimits() const
{
    TSignedSeqRange cds_lim = CdsLimits();
    
    for(int i = 0; i < (int)size(); ++i) {
        if (Include((*this)[i].Limits(),cds_lim.GetFrom())) {
            if((*this)[i].GetFrom()+3 <= cds_lim.GetFrom()) {
                cds_lim.SetFrom(cds_lim.GetFrom() - 3);
            }
            else{
                int remain = 3-(cds_lim.GetFrom()-(*this)[i].GetFrom());
                while(remain > 0 && i > 0) {
                    int exon_len = (*this)[i-1].GetTo()-(*this)[i-1].GetFrom()+1;
                    if(exon_len >= remain) {
                        cds_lim.SetFrom( (*this)[i-1].GetTo()-remain+1 );
                        remain = 0;
                    }
                    else{
                        remain -= exon_len;
                        --i;
                    }
                }
                break;
            }
        }
    }
    
    for(int i = (int)size()-1; i >= 0; --i) {
        if(Include((*this)[i].Limits(),cds_lim.GetTo())) {
            if((*this)[i].GetTo() >= cds_lim.GetTo()+3) {
                cds_lim.SetTo(cds_lim.GetTo() + 3);
            }
            else {
                int remain = 3-((*this)[i].GetTo()-cds_lim.GetTo());
                while(remain > 0 && i < (int)size()-1) {
                    int exon_len = (*this)[i+1].GetTo() -(*this)[i+1].GetFrom() + 1;
                    if(exon_len >= remain) {
                        cds_lim.SetTo( (*this)[i+1].GetFrom()+remain-1 );
                        remain = 0;
                    }
                    else {
                        remain -= exon_len;
                        ++i;
                    }
                }
                break;
            }
        }
    }
    return cds_lim;
}

int CAlignVec::MutualExtension(const CAlignVec& a) const
{
    //    if(Strand() != a.Strand()) return 0;
    const TSignedSeqRange limits = Limits();
    const TSignedSeqRange alimits = a.Limits();

    const int intersection = (limits & alimits).GetLength();
    if (intersection==0 || intersection==limits.GetLength() || intersection==alimits.GetLength()) return 0;
    
    return isCompatible(a);
}

bool CAlignVec::Similar(const CAlignVec& a, int tolerance) const
{
    if(Strand() != a.Strand() || !Limits().IntersectingWith(a.Limits())) return false;
        
    TSignedSeqPos mutual_min = max(Limits().GetFrom(),a.Limits().GetFrom());
    TSignedSeqPos mutual_max = min(Limits().GetTo(),a.Limits().GetTo());
        
    int imin = 0;
    while(imin < (int)size() && (*this)[imin].GetTo() < mutual_min) ++imin;
    if(imin == (int)size()) return false;
    
    int imax = (int)size()-1;
    while(imax >=0 && (*this)[imax].GetFrom() > mutual_max) --imax;
    if(imax < 0) return false;
    
    int jmin = 0;
    while(jmin < (int)a.size() && a[jmin].GetTo() < mutual_min) ++jmin;
    if(jmin == (int)a.size()) return false;
    int jmax = (int)a.size()-1;
    while(jmax >=0 && a[jmax].GetFrom() > mutual_max) --jmax;
    if(jmax < 0) return false;
    
    if(imax-imin != jmax-jmin) return false;
    
    for( ; imin <= imax; ++imin, ++jmin)
    {
        if(abs(int(max(mutual_min,(*this)[imin].GetFrom())-max(mutual_min,a[jmin].GetFrom()))) > tolerance) return false;
        if(abs(int(min(mutual_max,(*this)[imin].GetTo())-min(mutual_max,a[jmin].GetTo()))) > tolerance) return false;
    }
    
    return true;
}

int CAlignVec::isCompatible(const CAlignVec& a) const
{
    const CAlignVec& b = *this;  // shortcut to this alignment

    //    if(b.Strand() != a.Strand()) return 0;
    TSignedSeqRange intersect(a.Limits() & b.Limits());
    if(intersect.Empty()) return 0;
    
    int anum = a.size()-1;   // exon containing left point or first exon on the left 
    for(; intersect.GetFrom() < a[anum].GetFrom() ; --anum);
    bool aexon = intersect.GetFrom() <= a[anum].GetTo();
    if(!aexon && intersect.GetTo() < a[anum+1].GetFrom()) return 0;    // b is in intron
    
    int bnum = b.size()-1;   // exon containing left point or first exon on the left 
    for(; intersect.GetFrom() < b[bnum].GetFrom(); --bnum);
    bool bexon = intersect.GetFrom() <= b[bnum].GetTo();
    if(!bexon && intersect.GetTo() < b[bnum+1].GetFrom()) return 0;    // a is in intron
    
    TSignedSeqPos left = intersect.GetFrom();
    int commonspl = 0;
    int ashift = 0;
    int bshift = 0;
    bool count = false;
    while(left <= intersect.GetTo())
    {
        TSignedSeqPos aright = aexon ? a[anum].GetTo() : a[anum+1].GetFrom()-1;
        TSignedSeqPos bright = bexon ? b[bnum].GetTo() : b[bnum+1].GetFrom()-1;
        TSignedSeqPos right = min(aright,bright);
        
        if(!aexon && bexon)
        {
            if(a[anum].m_ssplice && a[anum+1].m_fsplice) return 0;        // intron has both splices
            if(left == a[anum].GetTo()+1 && a[anum].m_ssplice) return 0;   // intron has left splice and == left
            if(right == aright && a[anum+1].m_fsplice) return 0;          // intron has right splice and == right
        }
        if(aexon && !bexon)
        {
            if(b[bnum].m_ssplice && b[bnum+1].m_fsplice) return 0;
            if(left == b[bnum].GetTo()+1 && b[bnum].m_ssplice) return 0;
            if(right == bright && b[bnum+1].m_fsplice) return 0; 
        }
        
        if(aexon && bexon) 
        {
            count = true;
        }
        if(aexon && count) ashift += a.FShiftedLen(left, right);
        if(bexon && count) bshift += b.FShiftedLen(left, right);
        if(aexon && bexon && ashift%3 != bshift%3) return 0;      // extension makes a frameshift 
        
        if(aexon && aright == right && a[anum].m_ssplice &&
           bexon && bright == right && b[bnum].m_ssplice) ++commonspl;
        if(!aexon && aright == right && a[anum+1].m_fsplice &&
           !bexon && bright == right && b[bnum+1].m_fsplice) ++commonspl;
        
        left = right+1;
        
        if(right == aright)
        {
            aexon = !aexon;
            if(aexon) ++anum;
        }
        
        if(right == bright)
        {
            bexon = !bexon;
            if(bexon) ++bnum;
        }
    }
    
    if(count) return commonspl+1;
    else return 0;                  // theoretical case when there are no common points (mutually exclusive holes)
}

void CAlignVec::Insert(const CAlignExon& p)
{
    m_limits += p.Limits();
    push_back(p);
}

void CAlignVec::Init()
{
    clear();
    m_limits = TSignedSeqRange::GetEmpty();
    m_cds_limits = TSignedSeqRange::GetEmpty();
    m_max_cds_limits = TSignedSeqRange::GetEmpty();
    SetOpenCds(false);
    SetPStop(false);
    m_fshifts.clear();
}

void CAlignExon::Extend(const CAlignExon& e)
{
    m_limits.CombineWith(e.m_limits);
    m_fsplice = m_fsplice || e.m_fsplice;
    m_ssplice = m_ssplice || e.m_ssplice;
}

void CAlignVec::Extend(const CAlignVec& a)
{
    CAlignVec tmp(*this);
    Init();
    m_cds_limits = tmp.m_cds_limits.CombinationWith(a.m_cds_limits);
    m_max_cds_limits = tmp.m_max_cds_limits.CombinationWith(a.m_max_cds_limits);
    
    int i;
    for(i = 0; a[i].GetTo() < tmp.front().GetFrom(); ++i) push_back(a[i]);
    if(a[i].GetFrom() <= tmp.front().GetFrom())
        tmp.front().Extend( a[i] );

    for(i = 0; i < (int)tmp.size(); ++i) push_back(tmp[i]);
    
    if(a.Limits().GetTo() > back().GetTo())
    {
        for(i = 0; a[i].GetTo() < back().GetFrom(); ++i);
        if(a[i].GetTo() >= back().GetTo())
            back().Extend( a[i] );
        for(++i; i < (int)a.size(); ++i) push_back(a[i]);
    }
    
    RecalculateLimits();
    
    m_fshifts.swap(tmp.m_fshifts);
    m_fshifts.insert(m_fshifts.end(),a.m_fshifts.begin(),a.m_fshifts.end());
    if(!m_fshifts.empty())
    {
        sort(m_fshifts.begin(),m_fshifts.end());
        int n = unique(m_fshifts.begin(),m_fshifts.end())-m_fshifts.begin();
        m_fshifts.resize(n);
    }
}

int CAlignVec::AlignLen() const
{
    int len = 0;
    for(int i = 0; i < (int)size(); ++i) len += (*this)[i].GetTo()-(*this)[i].GetFrom()+1;
    for(int i = 0; i < (int)m_fshifts.size(); ++i)
    {
        len += (m_fshifts[i].IsDeletion() ? m_fshifts[i].Len() : -m_fshifts[i].Len());
    }
    return len;
}

int CAlignVec::CdsLen() const
{
    int len = 0;
    if(CdsLimits().GetFrom() < 0) return len;
    
    TSignedSeqRange cds_lim = RealCdsLimits();
    
    for(int i = 0; i < (int)size(); ++i) 
    {
        if((*this)[i].GetTo() < cds_lim.GetFrom()) continue;
        if((*this)[i].GetFrom() > cds_lim.GetTo()) break;
        
        len += min(cds_lim.GetTo(),(*this)[i].GetTo())-max(cds_lim.GetFrom(),(*this)[i].GetFrom())+1;
    }
    for(int i = 0; i < (int)m_fshifts.size(); ++i)
    {
        if(m_fshifts[i].Loc() > cds_lim.GetFrom() && m_fshifts[i].Loc() < cds_lim.GetTo())
        {
            len += (m_fshifts[i].IsDeletion() ? m_fshifts[i].Len() : -m_fshifts[i].Len());
        }
    }
    
    return len;
}

int CAlignVec::MaxCdsLen() const
{
    int len = 0;
    if(MaxCdsLimits().Empty()) return len;
    
    TSignedSeqRange cds_lim = MaxCdsLimits();
    
    for(int i = 0; i < (int)size(); ++i) 
    {
        if((*this)[i].GetTo() < cds_lim.GetFrom()) continue;
        if((*this)[i].GetFrom() > cds_lim.GetTo()) break;
        
        len += min(cds_lim.GetTo(),(*this)[i].GetTo())-max(cds_lim.GetFrom(),(*this)[i].GetFrom())+1;
    }
    for(int i = 0; i < (int)m_fshifts.size(); ++i)
    {
        if(m_fshifts[i].Loc() > cds_lim.GetFrom() && m_fshifts[i].Loc() < cds_lim.GetTo())
        {
            len += (m_fshifts[i].IsDeletion() ? m_fshifts[i].Len() : -m_fshifts[i].Len());
        }
    }
    
    return len;
}

CNcbiIstream& InputError(CNcbiIstream& s, CT_POS_TYPE pos)
{
    s.clear();
    s.seekg(pos);
    s.setstate(ios::failbit);
    return s;
}

CNcbiIstream& operator>>(CNcbiIstream& s, CAlignVec& a)
{
    CT_POS_TYPE pos = s.tellg();
    
    string strandname;
    EStrand strand;
    s >> strandname;
    if(strandname == "+") strand = ePlus;
    else if(strandname == "-") strand = eMinus;
    else return InputError(s,pos);
    
    char c;
    s >> c >> c;
    int id;
    if(!(s >> id)) return InputError(s,pos);
    
    string typenm;
    s >> typenm;
    
    TSignedSeqRange cds_limits;
    bool open_cds = false;
    bool confirmed_start = false;
    if(typenm == "CDS" || typenm == "ConfCDS" || typenm == "OpenCDS") {
        if(typenm == "OpenCDS") open_cds = true;
        if(typenm == "ConfCDS") confirmed_start = true;
        TSignedSeqPos x;
        if(!(s >> x)) return InputError(s,pos);
        cds_limits.SetFrom(x);
        if(!(s >> x)) return InputError(s,pos);
        cds_limits.SetTo(x);
        s >> typenm;
    }
    
    TSignedSeqRange max_cds_limits;
    if(typenm == "MaxCDS")
    {
	TSignedSeqPos x;
        if(!(s >> x)) return InputError(s,pos);
	max_cds_limits.SetFrom(x);
        if(!(s >> x)) return InputError(s,pos);
	max_cds_limits.SetTo(x);
        s >> typenm;
    }
    
    int type;
    if(typenm == "mRNA") type = CAlignVec::emRNA;
    else if(typenm == "EST") type = CAlignVec::eEST;
    else if(typenm == "Prot") type = CAlignVec::eProt;
    else if(typenm == "Nested") type = CAlignVec::eNested;
    else if(typenm == "Wall") type = CAlignVec::eWall;
    else return InputError(s,pos);
    
    a.Init();
    a.SetStrand(strand);
    a.SetType(type);
    a.SetID(id);
    a.SetCdsLimits(cds_limits);
    a.SetMaxCdsLimits(max_cds_limits);
    a.SetOpenCds(open_cds);
    a.SetConfirmedStart(confirmed_start);
    
    string line;
    if(!getline(s,line)) return InputError(s,pos);
    CNcbiIstrstream istr_line(line.c_str()); 

    string exon;
    bool splice = false;
    while(getline(istr_line,exon,')'))
    {
        int bracket = exon.find_first_not_of(' ');
        if(exon[bracket] != '(') return InputError(s,pos);
        exon = exon.substr(bracket+1);
        CNcbiIstrstream istr(exon.c_str());
        
        int start = -1;
        int stop = -1;
        string word;
        while(istr >> word)
        {
            if(word[0] == 'I')      // insertion
            {
                CNcbiIstrstream istr_w(word.c_str());
                char c;
                int left,right;
                if(!(istr_w >> c >> left >> c >> right) || c != '-') return InputError(s,pos);
                int len = right-left+1;
                CFrameShiftInfo fsi(left,len,true);
                a.FrameShifts().push_back(fsi);
            }
            else if(word[0] == 'D')      // deletion
            {
                unsigned int dash = word.find('-');
                if(dash == string::npos) return InputError(s,pos);
                string number = word.substr(dash+1);
                int loc = atoi(number.c_str());
                string del_v = word.substr(1,dash-1);
                int len = del_v.size();
                CFrameShiftInfo fsi(loc,len,false,del_v);
                a.FrameShifts().push_back(fsi);
            }
            else
            {
                int left = atoi(word.c_str());
                if(!(istr >> stop)) return InputError(s,pos);
                if(start < 0) start = left;
            }
        }
        a.Insert(CAlignExon(start,stop,splice,false));

        if(istr_line >> c)
        {
            if(c == '(') 
            {
                istr_line.unget();
                splice = true;
                a.back().m_ssplice = splice;
            }
            else if(c == '.')
            {
                if(!(istr_line >> c) || c != '.') return InputError(s,pos);
                splice = false;
            }
            else
            {
                return InputError(s,pos);
            }
        }
    }
    
    sort(a.FrameShifts().begin(),a.FrameShifts().end());
    
    return s;
}

CNcbiOstream& operator<<(CNcbiOstream& s, const CAlignVec& a)
{
    s << (a.Strand() == ePlus ? '+' : '-') << " ID" << a.ID() << ' ';
    
    if(a.CdsLimits().NotEmpty())
    {
        if(a.OpenCds()) s << "Open";
        if(a.ConfirmedStart()) s << "Conf";
        s << "CDS " << a.CdsLimits().GetFrom() << ' ' << a.CdsLimits().GetTo() << ' ';
    }
    
    if(a.MaxCdsLimits().NotEmpty())
    {
        s << "MaxCDS " << a.MaxCdsLimits().GetFrom() << ' ' << a.MaxCdsLimits().GetTo() << ' ';
    }
    
    switch(a.Type())
    {
        case CAlignVec::emRNA: s << "mRNA "; break;
        case CAlignVec::eEST: s << "EST "; break;
        case CAlignVec::eProt: s << "Prot "; break;
        case CAlignVec::eNested: s << "Nested "; break;
        case CAlignVec::eWall: s << "Wall "; break;
    }
    
    const TFrameShifts& fshifts = a.FrameShifts();
    TFrameShifts::const_iterator fsi = fshifts.begin(); 
    
    for(int i = 0; i < (int)a.size(); ++i) 
    {
        s << '(';
        TSignedSeqPos aa = a[i].GetFrom();
        string space;
        for( ; fsi != fshifts.end() && fsi->Loc() <= aa; ++fsi)        // insertions/deletions after first splice
        {
            if(fsi->IsDeletion()) 
            {
                s << space << 'D' << fsi->DeletedValue() << '-' << fsi->Loc();
            }
            else
            {
                s << space << 'I' << fsi->Loc() << '-' << fsi->Loc()+fsi->Len()-1;
            }
            space = " ";
        }
        while(aa <= a[i].GetTo())
        {
            s << space << aa << ' ';
            space = " ";
            TSignedSeqPos bb = (fsi != fshifts.end() && fsi->Loc() <= a[i].GetTo()) ? fsi->Loc()-1 : a[i].GetTo();
            s << bb;

            for( ; fsi != fshifts.end() && fsi->Loc() == bb+1; ++fsi)    // insertions/deletions in the middle and before the GetTo() splice
            {
                if(fsi->IsDeletion()) 
                {
                    s << " D" << fsi->DeletedValue() << '-' << fsi->Loc();
                }
                else
                {
                    bb = fsi->Loc()+fsi->Len()-1;
                    s << " I" << fsi->Loc() << '-' << bb;
                }
            }
            aa = bb+1;
        }
        s << ')';

        if(i < (int)a.size()-1)
        {
            if(a[i].m_ssplice) s << "  ";
            else s << "..";
        }
    }
    s << '\n';
    
    return s;
}

void CCluster::Insert(const CAlignVec& a)
{
    m_limits.CombineWith(a.Limits());
    m_type = max(m_type,a.Type());
    push_back(a);
}

void CCluster::Insert(const CCluster& c)
{
    m_limits.CombineWith(c.Limits());
    for(TConstIt it = c.begin(); it != c.end(); ++it) Insert(*it);
}

void CCluster::Splice(CCluster& c)
{
    m_limits.CombineWith(c.Limits());
    m_type = max(m_type,c.Type());
    splice(end(),c);
}

void CCluster::Init(TSignedSeqPos first, TSignedSeqPos second, int t)
{
    clear();
    m_limits.SetFrom( first );
    m_limits.SetTo( second );
    m_type = t;
}

CNcbiIstream& operator>>(CNcbiIstream& s, CCluster& c)
{
    CT_POS_TYPE pos = s.tellg();
    
    string cluster;
    s >> cluster;
    if(cluster != "Cluster") return InputError(s,pos);
    
    TSignedSeqPos first, second;
    int size;
    string typenm;
    if(!(s >> first >> second >> size >> typenm)) return InputError(s,pos);
    
    int type;
    if(typenm == "mRNA") type = CAlignVec::emRNA;
    else if(typenm == "EST") type = CAlignVec::eEST;
    else if(typenm == "Prot") type = CAlignVec::eProt;
    else if(typenm == "Nested") type = CAlignVec::eNested;
    else if(typenm == "Wall") type = CAlignVec::eWall;
    else return InputError(s,pos);

    c.Init(first,second,type);
    for(int i = 0; i < size; ++i)
    {
        CAlignVec a;
        if(!(s >> a)) return InputError(s,pos);
        c.Insert(a);
    }    
        
    return s;
}

CNcbiOstream& operator<<(CNcbiOstream& s, const CCluster& c)
{
    s << "Cluster ";
    s << c.Limits().GetFrom() << ' ';
    s << c.Limits().GetTo() << ' ';
    s << (int)c.size() << ' ';
    switch(c.Type())
    {
        case CAlignVec::emRNA: s << "mRNA "; break;
        case CAlignVec::eEST: s << "EST "; break;
        case CAlignVec::eProt: s << "Prot "; break;
        case CAlignVec::eNested: s << "Nested "; break;
        case CAlignVec::eWall: s << "Wall "; break;
    }
    s << '\n';
    for(CCluster::TConstIt it = c.begin(); it != c.end(); ++it) s << *it; 
    
    return s;
}

void CClusterSet::InsertAlignment(const CAlignVec& a)
{
    CCluster clust;
    clust.Insert(a);

    pair<TIt,TIt> lim = equal_range(clust);
    for(TIt it = lim.first; it != lim.second;) {
        clust.Splice(const_cast<CCluster&>(*it));
        erase(it++);
    }

    const_cast<CCluster&>(*insert(lim.second,CCluster(clust.Limits()))).Splice(clust);
}

END_SCOPE(gnomon)
END_NCBI_SCOPE
