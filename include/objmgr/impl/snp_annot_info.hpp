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

#include <vector>

BEGIN_NCBI_SCOPE

class CObjectIStream;

BEGIN_SCOPE(objects)

class CSeq_feat;
class CSeq_annot;
class CSeq_annot_SNP_Info;

struct NCBI_XOBJMGR_EXPORT SSNP_Info
{
public:
    TSeqPos GetPosition(void) const;
    bool MinusStrand(void) const;

    bool operator<(const SSNP_Info& snp) const
        {
            return
                m_DoublePosition < snp.m_DoublePosition ||
                (m_DoublePosition == snp.m_DoublePosition &&
                 m_SNP_Id < snp.m_SNP_Id);
        }
    bool operator==(const SSNP_Info& snp) const
        {
            return
                m_DoublePosition == snp.m_DoublePosition &&
                m_SNP_Id == snp.m_SNP_Id;
        }

    bool operator<(TSeqPos double_pos) const
        {
            return m_DoublePosition < double_pos;
        }

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
        eSNP_Complex_AlleleCountIsNotTwo,
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
        kMax_AllelesCount = 2
    };

    // CSeq_annot_SNP_Info*    m_Seq_annot_SNP_Info;
    // for minus strands: m_DoublePosition is odd
    TSeqPos     m_DoublePosition;
    int         m_SNP_Id;
    Int1        m_CommentIndex;
    Uint1       m_Weight;
    char        m_Alleles[kMax_AllelesCount];
};


class NCBI_XOBJMGR_EXPORT CSeq_annot_SNP_Info : public CObject
{
public:
    CSeq_annot_SNP_Info(void);
    ~CSeq_annot_SNP_Info(void);

    CRef<CSeq_entry> Read(CObjectIStream& in);

    typedef vector<SSNP_Info> TSNP_Set;
    typedef TSNP_Set::const_iterator const_iterator;

    bool empty(void) const
        {
            return m_SNP_Set.empty();
        }
    const_iterator LowerBoundByPosition(TSeqPos pos) const;
    const_iterator begin(void) const;
    const_iterator end(void) const;

    int GetGi(void) const
        {
            return m_SNP_Gi;
        }

    const CSeq_annot& GetSeq_annot(void) const
        {
            return *m_Seq_annot;
        }

protected:
    typedef vector<string> TSNP_Comments;

    Int1 x_GetSNP_CommentIndex(const string& comment);
    const string& x_GetComment(Int1 index) const;
    bool x_SetSNP_Gi(int gi);
    void x_AddSNP(const SSNP_Info& snp_info);

private:
    CSeq_annot_SNP_Info(const CSeq_annot_SNP_Info&);
    CSeq_annot_SNP_Info& operator=(const CSeq_annot_SNP_Info&);

    friend class CSNP_Seq_feat_hook;
    friend struct SSNP_Info;

    int                         m_SNP_Gi;
    TSNP_Set                    m_SNP_Set;
    TSNP_Comments               m_SNP_Comments;
    CRef<CSeq_annot>            m_Seq_annot;
};


/////////////////////////////////////////////////////////////////////////////
// SSNP_Info
/////////////////////////////////////////////////////////////////////////////

inline
TSeqPos SSNP_Info::GetPosition(void) const
{
    return m_DoublePosition >> 1;
}


inline
bool SSNP_Info::MinusStrand(void) const
{
    return m_DoublePosition & 1;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_SNP_Info
/////////////////////////////////////////////////////////////////////////////

inline
bool CSeq_annot_SNP_Info::x_SetSNP_Gi(int gi)
{
    if ( gi == m_SNP_Gi ) {
        return true;
    }
    if ( m_SNP_Gi < 0 ) {
        m_SNP_Gi = gi;
        return true;
    }
    return false;
}


inline
const string& CSeq_annot_SNP_Info::x_GetComment(Int1 index) const
{
    _ASSERT(size_t(index) < m_SNP_Comments.size());
    return m_SNP_Comments[index];
}



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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

