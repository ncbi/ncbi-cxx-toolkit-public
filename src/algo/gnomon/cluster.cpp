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
 * Authors:  Alexandre Souvorov, Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include "gene_finder.hpp"

BEGIN_NCBI_SCOPE


IPair AlignVec::RealCdsLimits() const
{
    IPair cds_lim = CdsLimits();

    for(int i = 0; i < (int)size(); ++i)
    {
        if((*this)[i].Include(cds_lim.first))
        {
            if((*this)[i].first <= cds_lim.first-3)
            {
                cds_lim.first -= 3;
            }
            else if(i > 0)
            {
                int remain = 3-(cds_lim.first-(*this)[i].first);
                cds_lim.first = (*this)[i-1].second-remain+1;
            }
            break;
        }
    }

    for(int i = (int)size()-1; i >=0; --i)
    {
        if((*this)[i].Include(cds_lim.second))
        {
            if((*this)[i].second >= cds_lim.second+3)
            {
                cds_lim.second += 3;
            }
            else if(i < (int)size()-1)
            {
                int remain = 3-((*this)[i].second-cds_lim.second);
                cds_lim.second = (*this)[i+1].first+remain-1;
            }
            break;
        }
    }

    return cds_lim;
}

bool AlignVec::MutualExtension(const AlignVec& a) const
{
    if(!Limits().Intersect(a.Limits()) || 
       Limits().Include(a.Limits()) || 
       a.Limits().Include(Limits())) return false;

    int mutual_min = max(Limits().first,a.Limits().first);
    int mutual_max = min(Limits().second,a.Limits().second);

    unsigned int imin = 0;
    while(imin < size() && !(*this)[imin].Include(mutual_min)) {
        ++imin;
    }
    if(imin == size()) {
        return false;
    }
    size_t imax = size()-1;
    while(imax >=0 && !(*this)[imax].Include(mutual_max)) {
        --imax;
    }
    if(imax < 0) {
        return false;
    }


    unsigned int jmin = 0;
    while(jmin < a.size() && !a[jmin].Include(mutual_min)) {
        ++jmin;
    }
    if(jmin == a.size()) {
        return false;
    }
    int jmax = (int)a.size()-1;
    while(jmax >=0 && !a[jmax].Include(mutual_max)) {
        --jmax;
    }
    if(jmax < 0) {
        return false;
    }

    if(imax-imin-(jmax-jmin) != 0) {
        return false;
    }

    for( ; imin <= imax; ++imin, ++jmin)
    {
        if(max(mutual_min,(*this)[imin].first) != max(mutual_min,a[jmin].first)) return false;
        if(min(mutual_max,(*this)[imin].second) != min(mutual_max,a[jmin].second)) return false;
    }

    return true;
}

bool AlignVec::SubAlign(const AlignVec& a) const
{
    if(IdenticalAlign(a)) return false;

    if(size() == 1)
    {
        for(int i = 0; i < (int)a.size(); ++i)
        {
            if(front().first >= a[i].first && front().second <= a[i].second) return true;
        }

        return false;
    }
    else
    {
        int first_exon = -1;
        for(int i = 0; i < (int)a.size(); ++i)
        {
            if(front().second == a[i].second)
            {
                first_exon = i;
                break;
            }
        }
        if(first_exon < 0 || front().first < a[first_exon].first) return false;

        int last_exon = -1;
        for(int i = 0; i < (int)a.size(); ++i)
        {
            if(back().first == a[i].first)
            {
                last_exon = i;
                break;
            }
        }
        if(last_exon < 0 || back().second > a[last_exon].second) return false;
    }

    if(size() > 2)
    {
        if(search(a.begin(),a.end(),begin()+1,end()-1) == a.end()) return false;
    }

    return true;
}

void AlignVec::Insert(IPair p)
{
    limits.first = min(limits.first,p.first);
    limits.second = max(limits.second,p.second);
    push_back(p);
}

void AlignVec::Erase(int i)
{
    erase(begin()+i);
    if(!empty())
    {
        limits.first = front().first;
        limits.second = back().second;
    }
}

void AlignVec::Init()
{
    clear();
    limits.first = numeric_limits<int>::max();
    limits.second = 0;
    cds_limits.first = cds_limits.second = -1;
    full_cds = false;
}

void AlignVec::Extend(const AlignVec& a)
{
    AlignVec tmp(*this);
    Init();
    cds_limits.first = min(tmp.cds_limits.first,a.cds_limits.first);
    cds_limits.second = max(tmp.cds_limits.second,a.cds_limits.second);

    int i;
    for(i = 0; a[i].second < tmp.front().first; ++i) Insert(a[i]);
    if(i == 0) tmp.front().first = min(tmp.front().first,a[i].first);
    for(i = 0; i < (int)tmp.size(); ++i) Insert(tmp[i]);
    for(i = 0; i < (int)a.size(); ++i) if(a[i].first > tmp.back().second) Insert(a[i]);
    if(back().second == tmp.back().second) back().second = max(back().second,a.back().second);
}

int AlignVec::AlignLen() const
{
    int len = 0;
    for(int i = 0; i < (int)size(); ++i) len += (*this)[i].second-(*this)[i].first+1;
    return len;
}

int AlignVec::CdsLen() const
{
    int len = 0;
    if(CdsLimits().first < 0) return len;

    for(int i = 0; i < (int)size(); ++i) 
    {
        if((*this)[i].second < CdsLimits().first) continue;
        if((*this)[i].first > CdsLimits().second) continue;

        len += min(CdsLimits().second,(*this)[i].second)
            -max(CdsLimits().first,(*this)[i].first)+1;
    }
    return len;
}

static istream& InputError(istream& s, ios::pos_type pos)
{
    s.clear();
    s.seekg(pos);
    s.setstate(ios::failbit);
    return s;
}

istream& operator>>(istream& s, AlignVec& a)
{
    ios::pos_type pos = s.tellg();

    string strandname;
    int strand;
    s >> strandname;
    if(strandname == "+") strand = Plus;
    else if(strandname == "-") strand = Minus;
    else return InputError(s,pos);

    char c;
    s >> c >> c;
    int id;
    if(!(s >> id)) return InputError(s,pos);

    string typenm;
    s >> typenm;

    /*
       if(typenm == "ACC")
       {
       string evidence;
       s >> evidence;
       istringstream istr(evidence);
       string acc;
       while(getline(istr,acc,'+')) a.Evidence().insert(acc);

       s >> typenm;
       }
       */	
    IPair cds_limits(-1,-1);
    bool full_cds = false;
    if(typenm == "CDS" || typenm == "FullCDS")
    {
        if(typenm == "FullCDS") full_cds = true;
        if(!(s >> cds_limits.first)) return InputError(s,pos);
        if(!(s >> cds_limits.second)) return InputError(s,pos);
        s >> typenm;
    }

    int type;
    if(typenm == "RefSeqBest") type = AlignVec::RefSeqBest;
    else if(typenm == "RefSeq") type = AlignVec::RefSeq;
    else if(typenm == "mRNA") type = AlignVec::mRNA;
    else if(typenm == "EST") type = AlignVec::EST;
    else if(typenm == "Prot") type = AlignVec::Prot;
    else if(typenm == "Wall") type = AlignVec::Wall;
    else return InputError(s,pos);

    int start, stop;
    s >> start >> stop;
    if(!s) return InputError(s,pos);

    a.Init();
    a.SetStrand(strand);
    a.SetType(type);
    a.SetID(id);
    a.SetCdsLimits(cds_limits);
    a.SetFullCds(full_cds);
    a.Insert(IPair(start,stop));
    ios::pos_type lastpos = s.tellg();
    while(s >> start) 
    {
        if(!(s >> stop)) return InputError(s,pos);
        a.Insert(IPair(start,stop));
        lastpos = s.tellg();
    }
    s.clear();
    s.seekg(lastpos);

    return s;
}

CNcbiOstream& operator<<(CNcbiOstream& s, const AlignVec& a)
{
    s << (a.Strand() == Plus ? '+' : '-') << " ID" << a.ID() << ' ';

    /*    const set<string>& ev = a.Evidence();
          if(!ev.empty())
          {
          s << "ACC ";
          for(set<string>::iterator it = a.Evidence().begin(); it != a.Evidence().end(); ++it) 
          {
          if(it != a.Evidence().begin()) s << '+';
          s << *it;
          }
          s << ' ';
          }
          */	
    if(a.CdsLimits().first >= 0)
    {
        if(a.FullCds()) s << "Full";
        s << "CDS " << a.CdsLimits().first << ' ' << a.CdsLimits().second << ' ';
    }

    switch(a.Type())
    {
    case AlignVec::RefSeqBest: s << "RefSeqBest "; break;
    case AlignVec::RefSeq: s << "RefSeq "; break;
    case AlignVec::mRNA: s << "mRNA "; break;
    case AlignVec::EST: s << "EST "; break;
    case AlignVec::Prot: s << "Prot "; break;
    case AlignVec::Wall: s << "Wall "; break;
    }

    for(int i = 0; i < (int)a.size(); ++i) 
    {
        s << a[i].first << ' ' << a[i].second << ' ';
    }
    s << '\n';

    return s;
}

void Cluster::Insert(const AlignVec& a)
{
    limits.first = min(limits.first,a.Limits().first);
    limits.second = max(limits.second,a.Limits().second);

    bool need_insert = true;
    if(a.Type() != AlignVec::Prot && a.Type() != AlignVec::Wall)
    {
        for(It it_loop = begin(); it_loop != end(); )
        {
            It it = it_loop++;
            if(it->Strand() != a.Strand() || it->Type() == AlignVec::Prot || it->Type() == AlignVec::Wall) continue;

            if(a.SubAlign(*it))           // a smaller - skip it
            {
                need_insert = false;
                break;
            }
            else if(a.IdenticalAlign(*it))
            {
                if(a.Type() > it->Type())  // a identical but "better" type
                {
                    erase(it);
                    break;
                }
                else
                {
                    need_insert = false;
                    break;
                }
            }
            else if(it->SubAlign(a))       // a bigger - keep it
            {
                erase(it);
                break;
            }
            else if(it->MutualExtension(a))   // both have sticking out ends
            {
                if(a.AlignLen() > it->AlignLen())    //  a is longer
                {
                    erase(it);
                }
                else
                {
                    need_insert = false;
                }
                break;
            }
        }
    }

    if(need_insert)
    {
        push_back(a);
        type = 0;
        for(It it = begin(); it != end(); ++it)
        {
            type = max(type,it->Type());
        }
    }
}

void Cluster::Insert(const Cluster& c)
{
    limits.first = min(limits.first,c.Limits().first);
    limits.second = max(limits.second,c.Limits().second);
    for(ConstIt it = c.begin(); it != c.end(); ++it) Insert(*it);
}

void Cluster::Init(int first, int second, int t)
{
    clear();
    limits.first = first;
    limits.second = second;
    type = t;
}

istream& operator>>(istream& s, Cluster& c)
{
    ios::pos_type pos = s.tellg();

    string cluster;
    s >> cluster;
    if(cluster != "Cluster") return InputError(s,pos);

    int first, second, size;
    string typenm;
    if(!(s >> first >> second >> size >> typenm)) return InputError(s,pos);

    int type;
    if(typenm == "RefSeqBest") type = AlignVec::RefSeqBest;
    else if(typenm == "RefSeq") type = AlignVec::RefSeq;
    else if(typenm == "mRNA") type = AlignVec::mRNA;
    else if(typenm == "EST") type = AlignVec::EST;
    else if(typenm == "Prot") type = AlignVec::Prot;
    else if(typenm == "Wall") type = AlignVec::Wall;
    else return InputError(s,pos);

    c.Init(first,second,type);
    for(int i = 0; i < size; ++i)
    {
        AlignVec a;
        if(!(s >> a)) return InputError(s,pos);
        c.Insert(a);
    }	

    return s;
}

CNcbiOstream& operator<<(CNcbiOstream& s, const Cluster& c)
{
    s << "Cluster ";
    s << c.Limits().first << ' ';
    s << c.Limits().second << ' ';
    s << (int)c.size() << ' ';
    switch(c.Type())
    {
    case AlignVec::RefSeqBest: s << "RefSeqBest "; break;
    case AlignVec::RefSeq: s << "RefSeq "; break;
    case AlignVec::mRNA: s << "mRNA "; break;
    case AlignVec::EST: s << "EST "; break;
    case AlignVec::Prot: s << "Prot "; break;
    case AlignVec::Wall: s << "Wall "; break;
    }
    s << '\n';
    for(Cluster::ConstIt it = c.begin(); it != c.end(); ++it) s << *it; 

    return s;
}

void CClusterSet::InsertAlignment(const AlignVec& a)
{
    Cluster clust;
    clust.Insert(a);
    InsertCluster(clust);
}

void CClusterSet::InsertCluster(Cluster clust)
{
    pair<It,It> lim = equal_range(clust);
    for(It it = lim.first; it != lim.second;)
    {
        clust.Insert(*it);
        erase(it++);
    }
    insert(lim.second,clust);
}

void CClusterSet::Init(string cnt)
{
    clear();
    contig = cnt;
}


istream& operator>>(istream& s, CClusterSet& cls)
{
    ios::pos_type pos = s.tellg();

    string contig;
    s >> contig;
    if(contig != "Contig") return InputError(s,pos);
    int num;
    s >> contig >> num;

    cls.Init(contig);
    for(int i = 0; i < num; ++i)
    {
        Cluster c;
        if(!(s >> c)) return InputError(s,pos);
        cls.InsertCluster(c);
    }

    return s;
}

CNcbiOstream& operator<<(CNcbiOstream& s, const CClusterSet& cls)
{
    int num = (int)cls.size();
    if (num) {
        s << "Contig " << cls.Contig() << ' ' << num << '\n';
    }
    for (CClusterSet::ConstIt it = cls.begin(); it != cls.end(); ++it) {
        s << *it;
    }

    return s;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/07/28 12:33:18  dicuccio
 * Sync with Sasha's working tree
 *
 * Revision 1.4  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/11/06 15:02:21  ucko
 * Use iostream interface from ncbistre.hpp for GCC 2.95 compatibility.
 *
 * Revision 1.2  2003/11/06 02:47:22  ucko
 * Fix usage of iterators -- be consistent about constness.
 *
 * Revision 1.1  2003/10/24 15:07:25  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
