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

    void Resize(size_t new_size)
        {
            m_Strings.resize(new_size);
        }
    string& SetString(size_t index)
        {
            return m_Strings[index];
        }

private:
    typedef vector<string> TStrings;
    typedef map<string, size_t> TIndices;

    TStrings m_Strings;
    TIndices m_Indices;
};


class NCBI_XOBJMGR_EXPORT CSeq_annot_SNP_Info : public CTSE_Info_Object
{
    typedef CTSE_Info_Object TParent;
public:
    CSeq_annot_SNP_Info(void);
    CSeq_annot_SNP_Info(const CSeq_annot& annot);
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
    const_iterator begin(void) const;
    const_iterator end(void) const;

    const_iterator FirstIn(const TRange& range) const;

    int GetGi(void) const;
    const CSeq_id& GetSeq_id(void) const;

    const SSNP_Info& GetSNP_Info(size_t index) const;

    const CSeq_annot& GetRemainingSeq_annot(void) const;
    void Reset(void);

    // filling SNP table from parser
    void x_AddSNP(const SSNP_Info& snp_info);
    void x_FinishParsing(void);

protected:
    SSNP_Info::TCommentIndex x_GetCommentIndex(const string& comment);
    const string& x_GetComment(SSNP_Info::TCommentIndex index) const;
    SSNP_Info::TAlleleIndex x_GetAlleleIndex(const string& allele);
    const string& x_GetAllele(SSNP_Info::TAlleleIndex index) const;

    bool x_CheckGi(int gi);
    void x_SetGi(int gi);

    void x_DoUpdate(TNeedUpdateFlags flags);

private:
    CSeq_annot_SNP_Info& operator=(const CSeq_annot_SNP_Info&);

    friend class CSeq_annot_Info;
    friend class CSeq_annot_SNP_Info_Reader;
    friend struct SSNP_Info;
    friend class CSeq_feat_Handle;

    int                         m_Gi;
    CRef<CSeq_id>               m_Seq_id;
    TSNP_Set                    m_SNP_Set;
    CIndexedStrings             m_Comments;
    CIndexedStrings             m_Alleles;
    CConstRef<CSeq_annot>       m_Seq_annot;
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
int CSeq_annot_SNP_Info::GetGi(void) const
{
    return m_Gi;
}


inline
const CSeq_id& CSeq_annot_SNP_Info::GetSeq_id(void) const
{
    return *m_Seq_id;
}


inline
bool CSeq_annot_SNP_Info::x_CheckGi(int gi)
{
    if ( gi == m_Gi ) {
        return true;
    }
    if ( m_Gi < 0 ) {
        x_SetGi(gi);
        return true;
    }
    return false;
}


inline
const CSeq_annot& CSeq_annot_SNP_Info::GetRemainingSeq_annot(void) const
{
    return *m_Seq_annot;
}


inline
SSNP_Info::TCommentIndex
CSeq_annot_SNP_Info::x_GetCommentIndex(const string& comment)
{
    return comment.size() > SSNP_Info::kMax_CommentLength?
        SSNP_Info::kNo_CommentIndex:
        m_Comments.GetIndex(comment, SSNP_Info::kMax_CommentIndex);
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
void CSeq_annot_SNP_Info::x_AddSNP(const SSNP_Info& snp_info)
{
    m_SNP_Set.push_back(snp_info);
}


inline
const SSNP_Info& CSeq_annot_SNP_Info::GetSNP_Info(size_t index) const
{
    _ASSERT(index < m_SNP_Set.size());
    return m_SNP_Set[index];
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.19  2005/05/17 17:54:06  grichenk
* Removed StoreTo() and LoadFrom() from members of CIndexedStrings
*
* Revision 1.18  2005/03/28 20:40:44  jcherry
* Added export specifiers
*
* Revision 1.17  2005/03/15 19:09:52  vasilche
* SSNP_Info structure is defined in separate header.
*
* Revision 1.16  2004/08/12 14:17:30  vasilche
* Understand "weight" param in qual field.
*
* Revision 1.15  2004/07/12 16:57:32  vasilche
* Fixed loading of split Seq-descr and Seq-data objects.
* They are loaded correctly now when GetCompleteXxx() method is called.
*
* Revision 1.14  2004/05/04 18:08:47  grichenk
* Added CSeq_feat_Handle, CSeq_align_Handle and CSeq_graph_Handle
*
* Revision 1.13  2004/03/24 18:30:29  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.12  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.11  2004/02/06 16:13:19  vasilche
* Added parsing "replace" as a synonym of "allele" in SNP qualifiers.
* More compact format of SNP table in cache. SNP table version increased.
* Fixed null pointer exception when SNP features are loaded from cache.
*
* Revision 1.10  2004/01/28 20:54:35  vasilche
* Fixed mapping of annotations.
*
* Revision 1.9  2004/01/13 16:55:31  vasilche
* CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
* Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
*
* Revision 1.8  2003/10/21 16:29:14  vasilche
* Added check for errors in SNP table loaded from cache.
*
* Revision 1.7  2003/10/21 14:27:35  vasilche
* Added caching of gi -> sat,satkey,version resolution.
* SNP blobs are stored in cache in preprocessed format (platform dependent).
* Limit number of connections to GenBank servers.
* Added collection of ID1 loader statistics.
*
* Revision 1.6  2003/09/30 16:22:01  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.5  2003/08/27 14:28:51  vasilche
* Reduce amount of object allocations in feature iteration.
*
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

