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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Sequence data container for object manager
*
*/


#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <corelib/ncbimtx.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/seq_map.hpp>
#include <algorithm>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CSeqVector::
//


CSeqVector::CSeqVector(void)
    : m_Scope(0),
      m_Size(0),
      m_Iterator(0)
{
}


CSeqVector::CSeqVector(const CSeqVector& vec)
    : m_SeqMap(vec.m_SeqMap),
      m_Scope(vec.m_Scope),
      m_Coding(vec.m_Coding),
      m_Strand(vec.m_Strand),
      m_Size(vec.m_Size),
      m_Mol(vec.m_Mol),
      m_Iterator(0)
{
}


CSeqVector::CSeqVector(CConstRef<CSeqMap> seqMap, CScope& scope,
                       EVectorCoding coding, ENa_strand strand)
    : m_SeqMap(seqMap),
      m_Scope(&scope),
      m_Coding(CSeq_data::e_not_set),
      m_Strand(strand),
      m_Size(seqMap->GetLength(&scope)),
      m_Mol(seqMap->GetMol()),
      m_Iterator(0)
{
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeqMap& seqMap, CScope& scope,
                       EVectorCoding coding, ENa_strand strand)
    : m_SeqMap(&seqMap),
      m_Scope(&scope),
      m_Coding(CSeq_data::e_not_set),
      m_Strand(strand),
      m_Size(seqMap.GetLength(&scope)),
      m_Mol(seqMap.GetMol()),
      m_Iterator(0)
{
    SetCoding(coding);
}


CSeqVector::~CSeqVector(void)
{
}


CSeqVector& CSeqVector::operator= (const CSeqVector& vec)
{
    if (&vec != this) {
        m_SeqMap = vec.m_SeqMap;
        m_Scope = vec.m_Scope;
        m_Coding = vec.m_Coding;
        m_Strand = vec.m_Strand;
        m_Size = vec.m_Size;
        m_Mol = vec.m_Mol;
        m_Iterator.reset();
    }
    return *this;
}


CSeqVector::TResidue CSeqVector::x_GetGapChar(TCoding coding) const
{
    switch (coding) {
    case CSeq_data::e_Iupacna: // DNA - N
        return 'N';
    
    case CSeq_data::e_Ncbi8na: // DNA - bit representation
    case CSeq_data::e_Ncbi4na:
        return 0x0f;  // all bits set == any base

    case CSeq_data::e_Ncbieaa: // Proteins - X
    case CSeq_data::e_Iupacaa:
        return 'X';
    
    case CSeq_data::e_Ncbi8aa: // Protein - numeric representation
    case CSeq_data::e_Ncbistdaa:
        return 21;

    case CSeq_data::e_not_set:
        return 0;     // It's not good to throw an exception here

    case CSeq_data::e_Ncbi2na: // Codings without gap symbols
    case CSeq_data::e_Ncbipaa: //### Not sure about this
    case CSeq_data::e_Ncbipna: //### Not sure about this
    default:
        THROW1_TRACE(runtime_error,
                     "CSeqVector::GetGapChar() -- "
                     "Can not indicate gap using the selected coding");
    }
}


DEFINE_STATIC_FAST_MUTEX(s_ConvertTableMutex2);
//DEFINE_STATIC_FAST_MUTEX(s_ConvertTableMutex);
//DEFINE_STATIC_FAST_MUTEX(s_ComplementTableMutex);
//DEFINE_STATIC_FAST_MUTEX(s_ChainedTableMutex);

const char* CSeqVector::sx_GetConvertTable(TCoding src, TCoding dst, bool reverse)
{
    CFastMutexGuard guard(s_ConvertTableMutex2);
    typedef pair<pair<TCoding, TCoding>, bool> TKey;
    typedef map<TKey, char*> TTables;
    static TTables tables;

    TKey key(pair<TCoding, TCoding>(src, dst), reverse);
    TTables::iterator it = tables.find(key);
    if ( it != tables.end() ) {
        // already created
        return it->second;
    }
    it = tables.insert(TTables::value_type(key, 0)).first;
    if ( !CSeqportUtil::IsCodeAvailable(src) ||
         !CSeqportUtil::IsCodeAvailable(dst) ) {
        // invalid types
        return 0;
    }
    pair<unsigned, unsigned> srcIndex = CSeqportUtil::GetCodeIndexFromTo(src);
    pair<unsigned, unsigned> dstIndex = CSeqportUtil::GetCodeIndexFromTo(dst);
    const size_t COUNT = kMax_UChar+1;
    if ( srcIndex.second >= COUNT || dstIndex.second >= COUNT ) {
        // too large range
        return 0;
    }
    try {
        CSeqportUtil::GetMapToIndex(src, dst, srcIndex.first);
    }
    catch ( runtime_error& /*badType*/ ) {
        // incompatible types
        return 0;
    }
    char* table = it->second = new char[COUNT];
    fill(table, table+COUNT, char(255));
    for ( unsigned i = srcIndex.first; i <= srcIndex.second; ++i ) {
        unsigned code = i;
        if ( reverse ) {
            try {
                code = CSeqportUtil::GetIndexComplement(src, code);
            }
            catch ( runtime_error& /*noComplement*/ ) {
            }
        }
        try {
            code = CSeqportUtil::GetMapToIndex(src, dst, code);
            code = min(255u, code);
        }
        catch ( runtime_error& /*noConversion*/ ) {
            code = 255;
        }
        table[i] = char(code);
    }
    return table;
}


void CSeqVector::SetCoding(TCoding coding)
{
    if (m_Coding != coding) {
        m_Coding = coding;
        m_Iterator.reset();
    }
}


void CSeqVector::SetIupacCoding(void)
{
    switch ( GetSequenceType() ) {
    case CSeq_inst::eMol_aa:
        SetCoding(CSeq_data::e_Iupacaa);
        break;
    case CSeq_inst::eMol_dna:
    case CSeq_inst::eMol_rna:
    case CSeq_inst::eMol_na:
        SetCoding(CSeq_data::e_Iupacna);
        break;
    default:
        SetCoding(CSeq_data::e_Iupacna);
        break;
    }
}


void CSeqVector::SetNcbiCoding(void)
{
    switch ( GetSequenceType() ) {
    case CSeq_inst::eMol_aa:
        SetCoding(CSeq_data::e_Ncbistdaa);
        break;
    case CSeq_inst::eMol_dna:
    case CSeq_inst::eMol_rna:
    case CSeq_inst::eMol_na:
        SetCoding(CSeq_data::e_Ncbi4na);
        break;
    default:
        SetCoding(CSeq_data::e_Ncbi4na);
        break;
    }
}


void CSeqVector::SetCoding(EVectorCoding coding)
{
    switch ( coding ) {
    case CBioseq_Handle::eCoding_Iupac:
        SetIupacCoding();
        break;
    case CBioseq_Handle::eCoding_Ncbi:
        SetNcbiCoding();
        break;
    default:
        SetCoding(CSeq_data::e_not_set);
        break;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.58  2003/08/21 13:32:04  vasilche
* Optimized CSeqVector iteration.
* Set some CSeqVector values (mol type, coding) in constructor instead of detecting them while iteration.
* Remove unsafe bit manipulations with coding.
*
* Revision 1.57  2003/06/24 19:46:43  grichenk
* Changed cache from vector<char> to char*. Made
* CSeqVector::operator[] inline.
*
* Revision 1.56  2003/06/17 20:35:39  grichenk
* CSeqVector_CI-related improvements
*
* Revision 1.55  2003/06/13 17:22:28  grichenk
* Check if seq-map is not null before using it
*
* Revision 1.54  2003/06/12 18:39:21  vasilche
* Added default constructor of CSeqVector.
* Cleared cache iterator on CSeqVector assignment.
*
* Revision 1.53  2003/06/11 19:32:55  grichenk
* Added molecule type caching to CSeqMap, simplified
* coding and sequence type calculations in CSeqVector.
*
* Revision 1.52  2003/06/04 13:48:56  grichenk
* Improved double-caching, fixed problem with strands.
*
* Revision 1.51  2003/06/02 16:06:38  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.50  2003/05/27 19:44:06  grichenk
* Added CSeqVector_CI class
*
* Revision 1.49  2003/05/20 15:44:38  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.48  2003/05/05 21:00:26  vasilche
* Fix assignment of empty CSeqVector.
*
* Revision 1.47  2003/04/29 19:51:13  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.46  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.45  2003/02/07 16:28:05  vasilche
* Fixed delayed seq vector coding setting.
*
* Revision 1.44  2003/02/06 19:05:28  vasilche
* Fixed old cache data copying.
* Delayed sequence type (protein/dna) resolution.
*
* Revision 1.43  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.42  2003/02/05 15:05:28  vasilche
* Added string::reserve()
*
* Revision 1.41  2003/01/28 17:17:06  vasilche
* Fixed bug processing minus strands.
* Used CSeqMap::ResolvedRangeIterator with strand coordinate translation.
*
* Revision 1.40  2003/01/23 19:33:57  vasilche
* Commented out obsolete methods.
* Use strand argument of CSeqVector instead of creation reversed seqmap.
* Fixed ordering operators of CBioseqHandle to be consistent.
*
* Revision 1.39  2003/01/23 18:26:02  ucko
* Explicitly #include <algorithm>
*
* Revision 1.38  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.37  2003/01/03 19:45:12  dicuccio
* Replaced kPosUnknwon with kInvalidSeqPos (non-static variable; worka-round for
* MSVC)
*
* Revision 1.36  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.35  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.34  2002/10/03 13:45:39  grichenk
* CSeqVector::size() made const
*
* Revision 1.33  2002/09/26 20:15:11  grichenk
* Fixed cache lengths checks
*
* Revision 1.32  2002/09/16 20:13:01  grichenk
* Fixed Iupac coding setter
*
* Revision 1.31  2002/09/16 13:52:49  dicuccio
* Fixed bug in calculating total range of cached interval to retrieve -
* must clamp the cached range to the desired range.
*
* Revision 1.30  2002/09/12 19:59:25  grichenk
* Fixed bugs in calculating cached intervals
*
* Revision 1.29  2002/09/10 19:55:52  grichenk
* Fixed reverse-complement position
*
* Revision 1.28  2002/09/09 21:38:37  grichenk
* Fixed problem with GetSeqData() for minus strand
*
* Revision 1.27  2002/09/03 21:27:01  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.26  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.25  2002/05/31 20:58:19  grichenk
* Fixed GetSeqData() bug
*
* Revision 1.24  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.23  2002/05/17 17:14:53  grichenk
* +GetSeqData() for getting a range of characters from a seq-vector
*
* Revision 1.22  2002/05/09 14:16:31  grichenk
* sm_SizeUnknown -> kPosUnknown, minor fixes for unsigned positions
*
* Revision 1.21  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.20  2002/05/03 21:28:10  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.19  2002/05/03 18:36:19  grichenk
* Fixed members initialization
*
* Revision 1.18  2002/05/02 20:42:38  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.17  2002/04/29 16:23:28  grichenk
* GetSequenceView() reimplemented in CSeqVector.
* CSeqVector optimized for better performance.
*
* Revision 1.16  2002/04/26 14:37:21  grichenk
* Limited CSeqVector cache size
*
* Revision 1.15  2002/04/25 18:14:47  grichenk
* e_not_set coding gap symbol set to 0
*
* Revision 1.14  2002/04/25 16:37:21  grichenk
* Fixed gap coding, added GetGapChar() function
*
* Revision 1.13  2002/04/23 19:01:07  grichenk
* Added optional flag to GetSeqVector() and GetSequenceView()
* for switching to IUPAC encoding.
*
* Revision 1.12  2002/04/22 20:03:08  grichenk
* Updated comments
*
* Revision 1.11  2002/04/17 21:07:59  grichenk
* String pre-allocation added
*
* Revision 1.10  2002/04/03 18:06:48  grichenk
* Fixed segmented sequence bugs (invalid positioning of literals
* and gaps). Improved CSeqVector performance.
*
* Revision 1.8  2002/03/28 18:34:58  grichenk
* Fixed convertions bug
*
* Revision 1.7  2002/03/08 21:24:35  gouriano
* fixed errors with unresolvable references
*
* Revision 1.6  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.4  2002/01/30 22:09:28  gouriano
* changed CSeqMap interface
*
* Revision 1.3  2002/01/23 21:59:32  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:58  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:24  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
