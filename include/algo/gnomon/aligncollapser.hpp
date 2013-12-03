#ifndef ALGO_GNOMON___ALIGNCOLLAPSER__HPP
#define ALGO_GNOMON___ALIGNCOLLAPSER__HPP

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

BEGIN_SCOPE(ncbi);
BEGIN_SCOPE(gnomon);

struct SAlignIndividual {
    SAlignIndividual() : m_weight(0) {};
    SAlignIndividual(const CAlignModel& align, deque<char>& target_id_pool) : m_range(align.Limits()), m_align_id(align.ID()), m_weight(align.Weight()) {
        m_target_id = target_id_pool.size();
        string acc = align.TargetAccession();
        copy(acc.begin(),acc.end(),back_inserter(target_id_pool));
        target_id_pool.push_back(0);
        if(align.Status()&CGeneModel::eChangedByFilter)
            m_align_id = -m_align_id;
    };

    TSignedSeqRange m_range;
    Int8 m_align_id;   // < 0 used for eChangedByFilter
    float m_weight;    // < 0 used for deleting
    int m_target_id;   // shift in deque<char> for 0 terminated string; deque is maintained by CAlignCollapser
};


struct SIntron {
    SIntron(int a, int b, int strand, bool oriented, const string& sig) : m_range(a,b), m_strand(strand), m_oriented(oriented), m_sig(sig) {}
    bool operator<(const SIntron& i) const {
        if(m_oriented != i.m_oriented)
            return m_oriented < i.m_oriented;
        else if(m_oriented && m_strand != i.m_strand)
            return m_strand < i.m_strand;
        else
            return m_range < i.m_range;
            
    }
    TSignedSeqRange m_range;
    int m_strand;
    bool m_oriented;
    string m_sig;
};


class CAlignCommon {
public:
    typedef vector<SIntron> Tintrons;
    CAlignCommon() : m_flags(0) {}
    CAlignCommon(const CGeneModel& align);
    const Tintrons& GetIntrons() const { return m_introns; }
    CAlignModel GetAlignment(const SAlignIndividual& ali, const deque<char>& target_id_pool) const;
    int GetFlags() const { return m_flags; }
    bool isSR() const { return (m_flags&esr); }
    bool isEST() const { return (m_flags&eest); }
    bool isPolyA() const { return (m_flags&epolya); }
    bool isCap() const { return (m_flags&ecap); }
    bool isUnknown() const { return (m_flags&eunknownorientation); }
    bool isPlus() const { return (m_flags&eplus); }
    bool isMinus() const { return (m_flags&eminus); }
    bool operator<(const CAlignCommon& cas) const {
        if(m_flags != cas.m_flags)
            return m_flags < cas.m_flags;
        else if(m_introns.size() != cas.m_introns.size())
            return m_introns.size() < cas.m_introns.size();
        else
            return m_introns < cas.m_introns;
    }    

private:
    enum {
        esr = 1,
        eest = 2,
        epolya = 4,
        ecap = 8,
        eunknownorientation = 16,
        eplus = 32,
        eminus = 64
    };

    Tintrons m_introns;
    int m_flags;
};


class CAlignCollapser {
public:
    CAlignCollapser(string contig = "", CScope* scope = 0);
    void AddAlignment(const CAlignModel& align);
    void FilterAlignments();
    void GetCollapsedAlgnments(TAlignModelClusterSet& clsset);
    TInDels GetGenomicGaps() const { return m_align_gaps; };

    static void SetupArgDescriptions(CArgDescriptions* arg_desc);

    struct SIntronData {
        SIntronData() : m_weight(0.), m_ident(0.), m_est_support(0), m_keep_anyway(false), m_selfsp_support(false) {}
        double m_weight;
        double m_ident;
        int m_est_support;
        bool m_keep_anyway;
        bool m_selfsp_support;
    };
    typedef map<SIntron,SIntronData> TAlignIntrons;

private:
    void CollapsIdentical();
    enum {
        efill_left = 1, 
        efill_right = 2, 
        efill_middle = 4
    };
    void CleanExonEdge(int ie, CAlignModel& align, const string& transcript, bool right_edge) const; 
    CAlignModel FillGapsInAlignmentAndAddToGenomicGaps(const CAlignModel& align, int fill);
    bool CheckAndInsert(const CAlignModel& align, TAlignModelClusterSet& clsset) const;
    void ClipProteinToStartStop(CAlignModel& align);
    bool RemoveNotSupportedIntronsFromProt(CAlignModel& align);
    bool RemoveNotSupportedIntronsFromTranscript(CAlignModel& align) const;
    void ClipNotSupportedFlanks(CAlignModel& align, double clip_threshold);

    typedef map< CAlignCommon,deque<SAlignIndividual> > Tdata;
    Tdata m_aligns;
    typedef map< CAlignCommon,deque<char> > Tidpool;
    Tidpool m_target_id_pool;
    TAlignIntrons m_align_introns;
    TAlignModelList m_aligns_for_filtering_only;

    int m_count;
    bool m_filtersr;
    bool m_filterest;
    bool m_collapsest;
    bool m_collapssr;
    bool m_filtermrna;
    bool m_filterprots;
    bool m_fillgenomicgaps;

    CScope* m_scope;
    typedef map<int,int> TIntMap;
    TIntMap m_genomic_gaps_len;
    TInDels m_align_gaps;
    string m_contig;
    string m_contig_name;
    CResidueVec m_contigrv;

    int m_left_end;
    vector<double> m_coverage;
};

END_SCOPE(gnomon)
END_SCOPE(ncbi)


#endif  // ALGO_GNOMON___ALIGNCOLLAPSER__HPP
