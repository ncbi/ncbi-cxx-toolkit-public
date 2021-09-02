#ifndef __FEATURE_MATCH_HPP__
#define __FEATURE_MATCH_HPP__

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CMappedFeat;
class CSeq_feat;

enum ENoteCDS {
    eNoteCDS_No,
    eNoteCDS_Yes
};

class CMatchmRNA;
class CMatchCDS;
class CMatchmRNA;

class CmRNAAndCDSIndex
{
public:
    CmRNAAndCDSIndex();
    ~CmRNAAndCDSIndex();
    void SetBioseq(const std::vector<CMappedFeat> *feat_list, CScope* scope);
    CRef<CMatchmRNA> FindMatchmRNA(const CMappedFeat& mrna);
    bool MatchmRNAToCDSEnd(const CMappedFeat& mrna, unsigned int partial_type);

private:
    vector < CRef<CMatchCDS> > m_CdsList;
    vector < CRef<CMatchmRNA> > m_mRNAList;

};

class CMatchCDS;

class CMatchFeat : public CObject
{
public:
    CMatchFeat(const CMappedFeat &feat);
    ~CMatchFeat(void) {};

    const CSeq_feat& GetFeat() const
    {
        return *m_feat;
    }
    bool operator < (const CMatchFeat& o) const;

    bool FastMatchX(const CMatchFeat& o) const
    {
        if (m_pos_start <= o.m_pos_start && o.m_pos_start <= m_pos_stop)
            return true;

        if (o.m_pos_start <= m_pos_stop && m_pos_stop <= o.m_pos_stop)
            return true;

        if (m_pos_start < o.m_pos_start && o.m_pos_stop < m_pos_stop)
            return true;

        if (o.m_pos_start < m_pos_start && m_pos_stop < o.m_pos_stop)
            return true;

        return false;
    }
    TSeqPos minpos() const
    {
        //return min(m_pos_start, m_pos_stop);
        return m_pos_start;
    }
    TSeqPos maxpos() const
    {
        //return max(m_pos_start, m_pos_stop);
        return m_pos_stop;
    }
protected:
    CConstRef<CSeq_feat> m_feat;
private:
    TSeqPos m_pos_start;
    TSeqPos m_pos_stop;
};


class CMatchmRNA : public CMatchFeat
{
public:
    CMatchmRNA(const CMappedFeat &mrna) : CMatchFeat(mrna), m_AccountedFor(false) {};
    ~CMatchmRNA(void) {};

    void SetCDS(const CSeq_feat& cds)
    {
        m_Cds = Ref(&cds);
        m_AccountedFor = true;
    }

    void AddCDS(CMatchCDS& cds) { m_UnderlyingCDSs.push_back(Ref(&cds)); }
    bool IsAccountedFor(void) const { return m_AccountedFor; }
    void SetAccountedFor(bool val) { m_AccountedFor = val; }
    bool MatchesUnderlyingCDS(unsigned int partial_type) const;
    bool MatchAnyUnderlyingCDS(unsigned int partial_type) const;
    bool HasCDSMatch(void);

private:
    CConstRef<CSeq_feat> m_Cds;
    vector < CRef<CMatchCDS> > m_UnderlyingCDSs;
    bool m_AccountedFor;
};


class CMatchCDS : public CMatchFeat
{
public:
    CMatchCDS(const CMappedFeat &cds) :
        CMatchFeat(cds),
        m_AssignedMrna(nullptr),
        m_XrefMatch(false),
        m_NeedsmRNA(true)
    {};

    ~CMatchCDS(void) {};

    void AddmRNA(CMatchmRNA& mrna) { m_OverlappingmRNAs.push_back(Ref(&mrna)); }
    void SetXrefMatch(CMatchmRNA& mrna) { m_XrefMatch = true; m_AssignedMrna = &mrna; }
    bool IsXrefMatch(void) { return m_XrefMatch; }
    bool HasmRNA(void) const { return m_AssignedMrna != nullptr; };

    bool NeedsmRNA(void) { return m_NeedsmRNA; }
    void SetNeedsmRNA(bool val) { m_NeedsmRNA = val; }
    void AssignSinglemRNA(void);
    int GetNummRNA(bool &loc_unique);

    const CRef<CMatchmRNA> & GetmRNA(void) { return m_AssignedMrna; }

private:
    vector < CRef<CMatchmRNA> > m_OverlappingmRNAs;
    CRef<CMatchmRNA> m_AssignedMrna;
    bool m_XrefMatch;
    bool m_NeedsmRNA;
};






END_SCOPE(objects)
END_NCBI_SCOPE

#endif

