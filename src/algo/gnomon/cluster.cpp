/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software / database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software / database is freely available
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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */


#include <ncbi_pch.hpp>
#include "gene_finder.hpp"

BEGIN_NCBI_SCOPE

void AlignVec::Insert(IPair p)
{
    limits.first  = min(limits.first,p.first);
    limits.second = max(limits.second,p.second);
    push_back(p);
}


void AlignVec::Erase(int i)
{
    _ASSERT(i < size());

    erase(begin() + i);
    if ( !empty()  ) {
        limits.first  = front().first;
        limits.second = back().second;
    }
}


void AlignVec::Init()
{
    clear();
    limits.first     = numeric_limits<int>::max();
    limits.second    = 0;
    cds_limits.first = cds_limits.second = -1;
}


void AlignVec::Extend(const AlignVec& a)
{
    AlignVec tmp(*this);
    Init();
    cds_limits.first  = min(tmp.cds_limits.first,a.cds_limits.first);
    cds_limits.second = max(tmp.cds_limits.second,a.cds_limits.second);

    int i;
    for (i = 0;  a[i].second < tmp.front().first;  ++i) {
        Insert(a[i]);
    }
    if (i == 0) {
        tmp.front().first = min(tmp.front().first,a[i].first);
    }

    for (i = 0;  i < tmp.size();  ++i) {
        Insert(tmp[i]);
    }

    for (i = 0;  i < a.size();  ++i) {
        if (a[i].first > tmp.back().second) {
            Insert(a[i]);
        }
    }
    if (back().second == tmp.back().second) {
        back().second = max(back().second,a.back().second);
    }
}


static CNcbiIstream& InputError(CNcbiIstream& s, CT_POS_TYPE pos)
{
    s.clear();
    s.seekg(pos);
    s.setstate(ios::failbit);
    return s;
}


CNcbiIstream& operator>>(CNcbiIstream& s, AlignVec& a)
{
    CT_POS_TYPE pos = s.tellg();

    string strandname;
    int strand;
    s >> strandname;
    if (strandname == "+") {
        strand = Plus;
    } else if (strandname == "-") {
        strand = Minus;
    } else {
        return InputError(s,pos);
    }

    char c;
    s >> c >> c;
    int id;
    if ( !(s >> id)  ) {
        return InputError(s,pos);
    }

    string typenm;
    s >> typenm;

    IPair cds_limits(-1,-1);
    if (typenm == "CDS") {
        if ( !(s >> cds_limits.first)  ) {
            return InputError(s,pos);
        }
        if ( !(s >> cds_limits.second)  ) {
            return InputError(s,pos);
        }
        s >> typenm;
    }

    int type;
    if (typenm == "RefSeqBest") {
        type = AlignVec::RefSeqBest;
    } else if (typenm == "RefSeq") {
        type = AlignVec::RefSeq;
    } else if (typenm == "mRNA") {
        type = AlignVec::mRNA;
    } else if (typenm == "EST") {
        type = AlignVec::EST;
    } else if (typenm == "Prot") {
        type = AlignVec::Prot;
    } else if (typenm == "Wall") {
        type = AlignVec::Wall;
    } else {
        return InputError(s,pos);
    }

    int start, stop;
    s >> start >> stop;
    if ( !s  ) {
        return InputError(s,pos);
    }

    a.Init();
    a.SetStrand(strand);
    a.SetType(type);
    a.SetID(id);
    a.SetCdsLimits(cds_limits);
    a.Insert(IPair(start,stop));
    CT_POS_TYPE lastpos = s.tellg();
    while (s >> start) {
        if ( !(s >> stop)  ) {
            return InputError(s,pos);
        }
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

    if (a.CdsLimits().first >= 0) {
        s << "CDS " << a.CdsLimits().first
            << ' ' << a.CdsLimits().second << ' ';
    }

    switch (a.Type()) {
    case AlignVec::RefSeqBest: s << "RefSeqBest ";  break;
    case AlignVec::RefSeq:     s << "RefSeq ";      break;
    case AlignVec::mRNA:       s << "mRNA ";        break;
    case AlignVec::EST:        s << "EST ";         break;
    case AlignVec::Prot:       s << "Prot ";        break;
    case AlignVec::Wall:       s << "Wall ";        break;
    }

    for (int i = 0;  i < a.size();  ++i) {
        s << a[i].first << ' ' << a[i].second << ' ';
    }
    s << endl;

    return s;
}


void CCluster::Insert(const AlignVec& a)
{
    limits.first  = min(limits.first,a.Limits().first);
    limits.second = max(limits.second,a.Limits().second);
    push_back(a);
    type = max(type,a.Type());
}


void CCluster::Insert(const CCluster& c)
{
    limits.first  = min(limits.first,c.Limits().first);
    limits.second = max(limits.second,c.Limits().second);
    insert(end(),c.begin(),c.end());
    type = max(type,c.Type());
}


void CCluster::Init(int first, int second, int t)
{
    clear();
    limits.first = first;
    limits.second = second;
    type = t;
}


CNcbiIstream& operator>>(CNcbiIstream& s, CCluster& c)
{
    CT_POS_TYPE pos = s.tellg();

    string cluster;
    s >> cluster;
    if ( cluster != "CCluster" ) {
        return InputError(s,pos);
    }

    int first, second, size;
    string typenm;
    if ( !(s >> first >> second >> size >> typenm)  ) {
        return InputError(s,pos);
    }

    int type;
    if (typenm == "RefSeqBest") {
        type = AlignVec::RefSeqBest;
    } else if (typenm == "RefSeq") {
        type = AlignVec::RefSeq;
    } else if (typenm == "mRNA") {
        type = AlignVec::mRNA;
    } else if (typenm == "EST") {
        type = AlignVec::EST;
    } else if (typenm == "Prot") {
        type = AlignVec::Prot;
    } else if (typenm == "Wall") {
        type = AlignVec::Wall;
    } else {
        return InputError(s,pos);
    }

    c.Init(first,second,type);
    for (int i = 0;  i < size;  ++i) {
        AlignVec a;
        if ( !(s >> a)  ) {
            return InputError(s,pos);
        }
        c.Insert(a);
    }   

    return s;
}


CNcbiOstream& operator<<(CNcbiOstream& s, const CCluster& c)
{
    s << "CCluster ";
    s << c.Limits().first << ' ';
    s << c.Limits().second << ' ';
    s << c.size() << ' ';
    switch (c.Type()) {
    case AlignVec::RefSeqBest:  s << "RefSeqBest "; break;
    case AlignVec::RefSeq:      s << "RefSeq ";     break;
    case AlignVec::mRNA:        s << "mRNA ";       break;
    case AlignVec::EST:         s << "EST ";        break;
    case AlignVec::Prot:        s << "Prot ";       break;
    case AlignVec::Wall:        s << "Wall ";       break;
    }
    s << endl;

    for (CCluster::ConstIt it = c.begin();  it != c.end();  ++it) {
        s << *it;
    }

    return s;
}


void CClusterSet::InsertAlignment(const AlignVec& a)
{
    CCluster clust;
    clust.Insert(a);
    InsertCluster(clust);
}


void CClusterSet::InsertCluster(CCluster clust)
{
    pair<It,It> lim = equal_range(clust);
    for (It it = lim.first;  it != lim.second;  ) {
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


CNcbiIstream& operator>>(CNcbiIstream& s, CClusterSet& cls)
{
    CT_POS_TYPE pos = s.tellg();

    string contig;
    s >> contig;
    if ( contig != "Contig" ) {
        return InputError(s,pos);
    }
    int num;
    s >> contig >> num;

    cls.Init(contig);
    for (int i = 0;  i < num;  ++i) {
        CCluster c;
        if ( !(s >> c)  ) {
            return InputError(s,pos);
        }
        cls.InsertCluster(c);
    }

    return s;
}


CNcbiOstream& operator<<(CNcbiOstream& s, const CClusterSet& cls)
{
    int num = cls.size();
    if (num) {
        s << "Contig " << cls.Contig() << ' ' << num << endl;
    }
    ITERATE (CClusterSet, it, cls) {
        s << *it;
    }

    return s;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
