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
 * Authors:  Ning Ma, Jian Jee, Yury Merezhuk
 *
 */
#ifndef __IGBLAST_TYPES_HPP__
#define __IGBLAST_TYPES_HPP__

struct SCloneNuc {
        string na;
        string chain_type;
        string v_gene;
        string d_gene;
        string j_gene;
};

struct SAaInfo {
        string seqid;
        int count;
        string all_seqid;
        double min_identity;
        double max_identity;
        double total_identity;
};

struct SAaStatus {
        string aa;
        string productive;
};

struct sort_order {
        bool operator()(const SCloneNuc s1, const SCloneNuc s2) const
        {
            
            if (s1.na != s2.na)
                return s1.na < s2.na ? true : false;
            
            if (s1.chain_type != s2.chain_type)
                return s1.chain_type < s2.chain_type ? true : false;

            if (s1.v_gene != s2.v_gene)
                return s1.v_gene < s2.v_gene ? true : false;
            
            if (s1.d_gene != s2.d_gene)
                return s1.d_gene < s2.d_gene ? true : false;

            if (s1.j_gene != s2.j_gene)
                return s1.j_gene < s2.j_gene ? true : false;
            
            
            return false;
        }
};

struct sort_order_aa_status {
        bool operator()(const SAaStatus s1, const SAaStatus s2) const
        {
            if (s1.aa != s2.aa)
                return s1.aa < s2.aa ? true : false;
            
            if (s1.productive != s2.productive)
                return s1.productive < s2.productive ? true : false;
                                                                        
            return false;
        }
};

typedef map<SAaStatus, SAaInfo*, sort_order_aa_status> AaMap;
typedef map<SCloneNuc, AaMap*, sort_order> CloneInfo;

static bool x_SortByCount(const pair<const SCloneNuc*, AaMap*> *c1, const pair<const SCloneNuc*, AaMap*> *c2) {
        
        int c1_total = 0;
        int c2_total = 0;
        ITERATE(AaMap, iter, *(c1->second)){
            c1_total += iter->second->count;
        }
        ITERATE(AaMap, iter, *(c2->second)){
            c2_total += iter->second->count;
        }
 
        
        return c1_total > c2_total;
}



#endif
