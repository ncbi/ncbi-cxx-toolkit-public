#ifndef SNP_ANNOT_INFO__HPP
#define SNP_ANNOT_INFO__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   SNP Seq-annot object information
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_limits.hpp>

#include <util/range.hpp>

#include <vector>
#include <map>
#include <algorithm>

BEGIN_NCBI_SCOPE

class CObjectIStream;

BEGIN_SCOPE(objects)

class CSeq_feat;
class CSeq_annot;
class CSeq_annot_SNP_Info;

struct NCBI_XOBJMGR_EXPORT SSNP_Info
{
public:
    typedef CRange<TSeqPos> TRange;

    TSeqPos GetPosition(void) const;
    TSeqPos GetEndPosition(void) const;
    bool MinusStrand(void) const;

    bool operator<(const SSNP_Info& snp) const;
    bool operator<(TSeqPos end_position) const;

    bool NoMore(const TRange& range) const;
    bool NotThis(const TRange& range) const;

    // type of SNP feature returned by parsing method
    enum ESNP_Type {
        eSNP_Simple,
        eSNP_Bad_WrongMemberSet,
        eSNP_Bad_WrongTextId,
        eSNP_Complex_HasComment,
        eSNP_Complex_LocationIsNotPoint,
        eSNP_Complex_LocationIsNotGi,
        eSNP_Complex_LocationGiIsBad,
        eSNP_Complex_LocationStrandIsBad,
        eSNP_Complex_IdCountTooLarge,
        eSNP_Complex_IdCountIsNotOne,
        eSNP_Complex_AlleleLengthBad,
        eSNP_Complex_AlleleCountTooLarge,
        eSNP_Complex_AlleleCountIsNonStandard,
        eSNP_Complex_WeightBadValue,
        eSNP_Complex_WeightCountIsNotOne,
        eSNP_Type_last
    };
    // names of types for logging
    static const char* const s_SNP_Type_Label[eSNP_Type_last];

    // parser, if returned value is eSNP_Simple, then
    // other members are filled and can be stored.
    ESNP_Type ParseSeq_feat(const CSeq_feat& feat,
                            CSeq_annot_SNP_Info& annot_info);
    // restore Seq-feat object from parsed info.
    CRef<CSeq_feat> CreateSeq_feat(const CSeq_annot_SNP_Info& annot_info) const;
    void UpdateSeq_feat(CRef<CSeq_feat>& seq_feat,
                        const CSeq_annot_SNP_Info& annot_info) const;
    
    CRef<CSeq_feat> x_CreateSeq_feat(void) const;
    void x_UpdateSeq_feat(CSeq_feat& feat,
                          const CSeq_annot_SNP_Info& annot_info) const;

    enum {
        kMax_AllelesCount = 4
    };

    TSeqPos     m_EndPosition;
    int         m_SNP_Id;
    bool        m_MinusStrand;
    Int1        m_PositionDelta;
    Int1        m_CommentIndex;
    Uint1       m_Weight;
    Int1        m_AllelesIndices[kMax_AllelesCount];
};


class CIndexedStrings
{
public:
    void ClearIndices(void)
        {
            m_Indices.clear();
        }
    void Clear(void)
        {
            ClearIndices();
            m_Strings.clear();
        }
    bool IsEmpty(void) const
        {
            return m_Indices.empty();
        }

    int GetIndex(const string& s, int max_index);

    const string& GetString(int index) const
        {
            return m_Strings[index];
        }

private:
    typedef vector<string> TStrings;
    typedef map<string, int> TIndices;

    TStrings m_Strings;
    TIndices m_Indices;
};


class NCBI_XOBJMGR_EXPORT CSeq_annot_SNP_Info : public CObject
{
public:
    CSeq_annot_SNP_Info(void);
    ~CSeq_annot_SNP_Info(void);

    CRef<CSeq_entry> Read(CObjectIStream& in);

    typedef vector<SSNP_Info> TSNP_Set;
    typedef TSNP_Set::const_iterator const_iterator;
    typedef CRange<TSeqPos> TRange;

    bool empty(void) const;
    const_iterator begin(void) const;
    const_iterator end(void) const;
    const_iterator LowerBound(const TRange& range) const;

    int GetGi(void) const;

    const CSeq_annot& GetSeq_annot(void) const;

protected:
    Int1 x_GetCommentIndex(const string& comment);
    const string& x_GetComment(Int1 index) const;
    Int1 x_GetAlleleIndex(const string& allele);
    const string& x_GetAllele(Int1 index) const;

    bool x_SetGi(int gi);
    void x_AddSNP(const SSNP_Info& snp_info);

private:
    CSeq_annot_SNP_Info(const CSeq_annot_SNP_Info&);
    CSeq_annot_SNP_Info& operator=(const CSeq_annot_SNP_Info&);

    friend class CSNP_Seq_feat_hook;
    friend struct SSNP_Info;

    int                         m_Gi;
    TSNP_Set                    m_SNP_Set;
    CIndexedStrings             m_Comments;
    CIndexedStrings             m_Alleles;
    CRef<CSeq_annot>            m_Seq_annot;
};


/////////////////////////////////////////////////////////////////////////////
// SSNP_Info
/////////////////////////////////////////////////////////////////////////////

inline
TSeqPos SSNP_Info::GetPosition(void) const
{
    return m_EndPosition - m_PositionDelta;
}


inline
TSeqPos SSNP_Info::GetEndPosition(void) const
{
    return m_EndPosition;
}


inline
bool SSNP_Info::MinusStrand(void) const
{
    return m_MinusStrand;
}


inline
bool SSNP_Info::operator<(const SSNP_Info& snp) const
{
    return m_EndPosition < snp.m_EndPosition;
}


inline
bool SSNP_Info::operator<(TSeqPos end_position) const
{
    return m_EndPosition < end_position;
}


inline
bool SSNP_Info::NoMore(const TRange& range) const
{
    return GetEndPosition() >=
        min(kInvalidSeqPos-kMax_I1, range.GetToOpen()) + kMax_I1;
}


inline
bool SSNP_Info::NotThis(const TRange& range) const
{
    return GetPosition() >= range.GetToOpen();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_SNP_Info
/////////////////////////////////////////////////////////////////////////////

inline
bool CSeq_annot_SNP_Info::empty(void) const
{
    return m_SNP_Set.empty();
}


inline
CSeq_annot_SNP_Info::const_iterator
CSeq_annot_SNP_Info::begin(void) const
{
    return m_SNP_Set.begin();
}


inline
CSeq_annot_SNP_Info::const_iterator
CSeq_annot_SNP_Info::end(void) const
{
    return m_SNP_Set.end();
}


inline
CSeq_annot_SNP_Info::const_iterator
CSeq_annot_SNP_Info::LowerBound(const CRange<TSeqPos>& range) const
{
    return lower_bound(m_SNP_Set.begin(), m_SNP_Set.end(), range.GetFrom());
}


inline
int CSeq_annot_SNP_Info::GetGi(void) const
{
    return m_Gi;
}


inline
bool CSeq_annot_SNP_Info::x_SetGi(int gi)
{
    if ( gi == m_Gi ) {
        return true;
    }
    if ( m_Gi < 0 ) {
        m_Gi = gi;
        return true;
    }
    return false;
}


inline
const CSeq_annot& CSeq_annot_SNP_Info::GetSeq_annot(void) const
{
    return *m_Seq_annot;
}


inline
Int1 CSeq_annot_SNP_Info::x_GetCommentIndex(const string& comment)
{
    return m_Comments.GetIndex(comment, kMax_I1);
}


inline
const string& CSeq_annot_SNP_Info::x_GetComment(Int1 index) const
{
    return m_Comments.GetString(index);
}


inline
const string& CSeq_annot_SNP_Info::x_GetAllele(Int1 index) const
{
    return m_Alleles.GetString(index);
}


inline
void CSeq_annot_SNP_Info::x_AddSNP(const SSNP_Info& snp_info)
{
    m_SNP_Set.push_back(snp_info);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2003/08/15 19:34:53  vasilche
* Added missing #include <algorigm>
*
* Revision 1.3  2003/08/15 19:19:15  vasilche
* Fixed memory leak in string packing hooks.
* Fixed processing of 'partial' flag of features.
* Allow table packing of non-point SNP.
* Allow table packing of SNP with long alleles.
*
* Revision 1.2  2003/08/14 21:26:04  kans
* fixed inconsistent line endings that stopped Mac compiler
*
* Revision 1.1  2003/08/14 20:05:19  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* ===========================================================================
*/

#endif  // SNP_ANNOT_INFO__HPP

