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
#include <memory>

#include <objects/seqloc/Seq_id.hpp>

#include <objmgr/impl/tse_info_object.hpp>
#include <objmgr/impl/snp_info.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStream;
class IWriter;
class IReader;

BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_feat;
class CSeq_annot;
class CSeq_annot_Info;
class CSeq_annot_SNP_Info;
class CSeq_point;
class CSeq_interval;

class NCBI_XOBJMGR_EXPORT CIndexedStrings
{
public:
    CIndexedStrings(void);
    CIndexedStrings(const CIndexedStrings& ss);

    void ClearIndices(void);
    void Clear(void);

    bool IsEmpty(void) const
        {
            return m_Strings.empty();
        }
    size_t GetSize(void) const
        {
            return m_Strings.size();
        }

    size_t GetIndex(const string& s, size_t max_index);

    const string& GetString(size_t index) const
        {
            return m_Strings[index];
        }

    void Resize(size_t new_size);
    string& SetString(size_t index)
        {
            return m_Strings[index];
        }

private:
    typedef vector<string> TStrings;
    typedef map<string, size_t> TIndices;

    TStrings m_Strings;
    auto_ptr<TIndices> m_Indices;
};


class NCBI_XOBJMGR_EXPORT CIndexedOctetStrings
{
public:
    typedef vector<char> TOctetString;

    CIndexedOctetStrings(const CIndexedOctetStrings& ss);
    CIndexedOctetStrings(void);

    void ClearIndices(void);
    void Clear(void);

    bool IsEmpty(void) const
        {
            return m_Strings.empty();
        }
    size_t GetElementSize(void) const
        {
            return m_ElementSize;
        }
    size_t GetTotalSize(void) const
        {
            return m_Strings.size();
        }
    size_t GetSize(void) const
        {
            size_t size = GetTotalSize();
            if ( size ) {
                size /= GetElementSize();
            }
            return size;
        }
    const TOctetString& GetTotalString(void) const
        {
            return m_Strings;
        }
    void SetTotalString(size_t element_size, TOctetString& s);

    size_t GetIndex(const TOctetString& s, size_t max_index);

    void GetString(size_t index, TOctetString&) const;

private:
    typedef vector<char> TStrings;
    typedef map<CTempString, size_t> TIndices;

    size_t m_ElementSize;
    TStrings m_Strings;
    auto_ptr<TIndices> m_Indices;
};


class NCBI_XOBJMGR_EXPORT CSeq_annot_SNP_Info : public CTSE_Info_Object
{
    typedef CTSE_Info_Object TParent;
public:
    CSeq_annot_SNP_Info(void);
    CSeq_annot_SNP_Info(CSeq_annot& annot);
    CSeq_annot_SNP_Info(const CSeq_annot_SNP_Info& info);
    ~CSeq_annot_SNP_Info(void);

    const CSeq_annot_Info& GetParentSeq_annot_Info(void) const;
    CSeq_annot_Info& GetParentSeq_annot_Info(void);

    const CSeq_entry_Info& GetParentSeq_entry_Info(void) const;
    CSeq_entry_Info& GetParentSeq_entry_Info(void);

    // tree initialization
    void x_ParentAttach(CSeq_annot_Info& parent);
    void x_ParentDetach(CSeq_annot_Info& parent);

    void x_UpdateAnnotIndexContents(CTSE_Info& tse);
    void x_UnmapAnnotObjects(CTSE_Info& tse);
    void x_DropAnnotObjects(CTSE_Info& tse);

    typedef vector<SSNP_Info> TSNP_Set;
    typedef TSNP_Set::const_iterator const_iterator;
    typedef CRange<TSeqPos> TRange;

    bool empty(void) const;
    size_t size(void) const;
    const_iterator begin(void) const;
    const_iterator end(void) const;

    const_iterator FirstIn(const TRange& range) const;

    const CSeq_id& GetSeq_id(void) const;
    void SetSeq_id(const CSeq_id& id);
    NCBI_DEPRECATED
    void SetGi(TGi gi);
    void OffsetGi(TIntId gi_offset);

    size_t GetSize(void) const;
    const SSNP_Info& GetInfo(size_t index) const;
    size_t GetIndex(const SSNP_Info& info) const;

    CSeq_annot& GetRemainingSeq_annot(void);
    void Reset(void);

    // filling SNP table from parser
    void x_AddSNP(const SSNP_Info& snp_info);
    void x_FinishParsing(void);

    SSNP_Info::TCommentIndex x_GetCommentIndex(const string& comment);
    const string& x_GetComment(SSNP_Info::TCommentIndex index) const;
    SSNP_Info::TAlleleIndex x_GetAlleleIndex(const string& allele);
    const string& x_GetAllele(SSNP_Info::TAlleleIndex index) const;
    SSNP_Info::TQualityCodesIndex x_GetQualityCodesIndex(const string& str);
    typedef vector<char> TOctetString;
    SSNP_Info::TQualityCodesIndex x_GetQualityCodesIndex(const TOctetString& os);
    const string& x_GetQualityCodesStr(SSNP_Info::TQualityCodesIndex index) const;
    void x_GetQualityCodesOs(SSNP_Info::TQualityCodesIndex index, TOctetString& os) const;
    SSNP_Info::TExtraIndex x_GetExtraIndex(const string& str);
    const string& x_GetExtra(SSNP_Info::TExtraIndex index) const;

    const CIndexedStrings& x_GetComments(void) const {
        return m_Comments;
    }
    const CIndexedStrings& x_GetAlleles(void) const {
        return m_Alleles;
    }
    const CIndexedStrings& x_GetQualityCodesStr(void) const {
        return m_QualityCodesStr;
    }
    const CIndexedOctetStrings& x_GetQualityCodesOs(void) const {
        return m_QualityCodesOs;
    }

protected:
    bool x_CheckId(const CSeq_id& id);

    void x_DoUpdate(TNeedUpdateFlags flags);

private:
    CSeq_annot_SNP_Info& operator=(const CSeq_annot_SNP_Info&);

    friend class CSeq_annot_Info;
    friend class CSeq_annot_SNP_Info_Reader;
    friend struct SSNP_Info;
    friend class CSeq_feat_Handle;

    CRef<CSeq_id>               m_Seq_id;
    TSNP_Set                    m_SNP_Set;
    CIndexedStrings             m_Comments;
    CIndexedStrings             m_Alleles;
    CIndexedStrings             m_QualityCodesStr;
    CIndexedOctetStrings        m_QualityCodesOs;
    CIndexedStrings             m_Extra;
    CRef<CSeq_annot>            m_Seq_annot;
};


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_SNP_Info
/////////////////////////////////////////////////////////////////////////////

inline
bool CSeq_annot_SNP_Info::empty(void) const
{
    return m_SNP_Set.empty();
}


inline
size_t CSeq_annot_SNP_Info::size(void) const
{
    return m_SNP_Set.size();
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
CSeq_annot_SNP_Info::FirstIn(const CRange<TSeqPos>& range) const
{
    return lower_bound(m_SNP_Set.begin(), m_SNP_Set.end(), range.GetFrom());
}


inline
const CSeq_id& CSeq_annot_SNP_Info::GetSeq_id(void) const
{
    return *m_Seq_id;
}


inline
CSeq_annot& CSeq_annot_SNP_Info::GetRemainingSeq_annot(void)
{
    return *m_Seq_annot;
}


inline
SSNP_Info::TCommentIndex
CSeq_annot_SNP_Info::x_GetCommentIndex(const string& comment)
{
    size_t index =
        m_Comments.GetIndex(comment, SSNP_Info::kMax_CommentIndex);
    // index cannot be bigger than SSNP_Info::kMax_CommentIndex
    return SSNP_Info::TCommentIndex(index);
}


inline
SSNP_Info::TExtraIndex
CSeq_annot_SNP_Info::x_GetExtraIndex(const string& str)
{
    size_t index =
        m_Extra.GetIndex(str, SSNP_Info::kMax_ExtraIndex);
    // index cannot be bigger than SSNP_Info::kMax_ExtraIndex
    return SSNP_Info::TExtraIndex(index);
}


inline
SSNP_Info::TQualityCodesIndex
CSeq_annot_SNP_Info::x_GetQualityCodesIndex(const string& str)
{
    size_t index =
        m_QualityCodesStr.GetIndex(str, SSNP_Info::kMax_QualityCodesIndex);
    // index cannot be bigger than SSNP_Info::kMax_QualityCodesIndex
    return SSNP_Info::TQualityCodesIndex(index);
}


inline
SSNP_Info::TQualityCodesIndex
CSeq_annot_SNP_Info::x_GetQualityCodesIndex(const TOctetString& os)
{
    size_t index = 
        m_QualityCodesOs.GetIndex(os, SSNP_Info::kMax_QualityCodesIndex);
    // index cannot be bigger than SSNP_Info::kMax_QualityCodesIndex
    return SSNP_Info::TQualityCodesIndex(index);
}


inline
const string&
CSeq_annot_SNP_Info::x_GetComment(SSNP_Info::TCommentIndex index) const
{
    return m_Comments.GetString(index);
}


inline
const string&
CSeq_annot_SNP_Info::x_GetAllele(SSNP_Info::TAlleleIndex index) const
{
    return m_Alleles.GetString(index);
}


inline
const string&
CSeq_annot_SNP_Info::x_GetQualityCodesStr(SSNP_Info::TQualityCodesIndex index) const
{
    return m_QualityCodesStr.GetString(index);
}


inline
void CSeq_annot_SNP_Info::x_GetQualityCodesOs(SSNP_Info::TQualityCodesIndex index,
                                              TOctetString& os) const
{
    m_QualityCodesOs.GetString(index, os);
}


inline
const string&
CSeq_annot_SNP_Info::x_GetExtra(SSNP_Info::TExtraIndex index) const
{
    return m_Extra.GetString(index);
}


inline
void CSeq_annot_SNP_Info::x_AddSNP(const SSNP_Info& snp_info)
{
    m_SNP_Set.push_back(snp_info);
}


inline
size_t CSeq_annot_SNP_Info::GetSize(void) const
{
    return m_SNP_Set.size();
}


inline
const SSNP_Info& CSeq_annot_SNP_Info::GetInfo(size_t index) const
{
    _ASSERT(index < m_SNP_Set.size());
    return m_SNP_Set[index];
}


inline
size_t CSeq_annot_SNP_Info::GetIndex(const SSNP_Info& info) const
{
    _ASSERT(&info >= &m_SNP_Set.front() && &info <= &m_SNP_Set.back());
    return &info - &m_SNP_Set.front();
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SNP_ANNOT_INFO__HPP
