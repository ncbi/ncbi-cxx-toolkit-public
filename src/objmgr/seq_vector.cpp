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


#include <ncbi_pch.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <corelib/ncbimtx.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <algorithm>
#include <map>
#include <vector>
#include <util/random_gen.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CNcbi2naRandomizer::
//

CNcbi2naRandomizer::CNcbi2naRandomizer(CRandom& gen)
{
    unsigned int bases[4]; // Count of each base in the random distribution
    for (int na4 = 0; na4 < 16; na4++) {
        int bit_count = 0;
        char set_bit = 0;
        for (int bit = 0; bit < 4; bit++) {
            // na4 == 0 is special case (gap) should be treated as 0xf
            if ( !na4  ||  (na4 & (1 << bit)) ) {
                bit_count++;
                bases[bit] = 1;
                set_bit = bit;
            }
            else {
                bases[bit] = 0;
            }
        }
        if (bit_count == 1) {
            // Single base
            m_RandomTable[na4][0] = 0;
            m_RandomTable[na4][1] = set_bit;
            continue;
        }
        // Ambiguity: create random distribution with possible bases
        m_RandomTable[na4][0] = bit_count; // need any non-zero value
        for (int bit = 0; bit < 4; bit++) {
            bases[bit] *= kRandomDataSize/bit_count +
                kRandomDataSize % bit_count;
        }
        for (int i = kRandomDataSize - 1; i >= 0; i--) {
            CRandom::TValue rnd = gen.GetRand(0, i);
            for (int base = 0; base < 4; base++) {
                if (!bases[base]  ||  rnd > bases[base]) {
                    rnd -= bases[base];
                    continue;
                }
                m_RandomTable[na4][i + 1] = base;
                bases[base]--;
                break;
            }
        }
    }
}


CNcbi2naRandomizer::~CNcbi2naRandomizer(void)
{
    return;
}


void CNcbi2naRandomizer::RandomizeData(TData data,
                                       size_t count,
                                       TSeqPos pos)
{
    for (TData stop = data + count; data < stop; ++data, ++pos) {
        if ( m_RandomTable[(unsigned char)(*data)][0] ) {
            // Ambiguity, use random value
            *data = m_RandomTable[(unsigned char)(*data)]
                [(pos & kRandomizerPosMask) + 1];
        }
        else {
            // Normal base
            *data = m_RandomTable[(unsigned char)(*data)][1];
        }
    }
}


////////////////////////////////////////////////////////////////////
//
//  CSeqVector::
//


CSeqVector::CSeqVector(void)
    : m_Size(0)
{
    m_Iterator.x_SetVector(*this);
}


CSeqVector::CSeqVector(const CSeqVector& vec)
    : m_Scope(vec.m_Scope),
      m_SeqMap(vec.m_SeqMap),
      m_TSE(vec.m_TSE),
      m_Size(vec.m_Size),
      m_Mol(vec.m_Mol),
      m_Strand(vec.m_Strand),
      m_Coding(vec.m_Coding)
{
    m_Iterator.x_SetVector(*this);
}


CSeqVector::CSeqVector(const CBioseq_Handle& bioseq,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(bioseq.GetScope()),
      m_SeqMap(&bioseq.GetSeqMap()),
      m_TSE(bioseq.GetTSE_Handle()),
      m_Size(bioseq.GetBioseqLength()),
      m_Mol(bioseq.GetBioseqMolType()),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Iterator.x_SetVector(*this);
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeqMap& seqMap, CScope& scope,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(&scope),
      m_SeqMap(&seqMap),
      m_Size(seqMap.GetLength(&scope)),
      m_Mol(seqMap.GetMol()),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Iterator.x_SetVector(*this);
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeqMap& seqMap, const CTSE_Handle& top_tse,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(top_tse.GetScope()),
      m_SeqMap(&seqMap),
      m_TSE(top_tse),
      m_Size(seqMap.GetLength(&top_tse.GetScope())),
      m_Mol(seqMap.GetMol()),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Iterator.x_SetVector(*this);
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeq_loc& loc, CScope& scope,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(&scope),
      m_SeqMap(CSeqMap::CreateSeqMapForSeq_loc(loc, &scope)),
      m_Size(m_SeqMap->GetLength(&scope)),
      m_Mol(m_SeqMap->GetMol()),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Iterator.x_SetVector(*this);
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeq_loc& loc, const CTSE_Handle& top_tse,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(top_tse.GetScope()),
      m_SeqMap(CSeqMap::CreateSeqMapForSeq_loc(loc, &top_tse.GetScope())),
      m_TSE(top_tse),
      m_Size(m_SeqMap->GetLength(&top_tse.GetScope())),
      m_Mol(m_SeqMap->GetMol()),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Iterator.x_SetVector(*this);
    SetCoding(coding);
}


CSeqVector::~CSeqVector(void)
{
}


CSeqVector& CSeqVector::operator= (const CSeqVector& vec)
{
    if ( &vec != this ) {
        m_Scope  = vec.m_Scope;
        m_SeqMap = vec.m_SeqMap;
        m_TSE = vec.m_TSE;
        m_Size   = vec.m_Size;
        m_Mol    = vec.m_Mol;
        m_Strand = vec.m_Strand;
        m_Coding = vec.m_Coding;
        m_Iterator.x_SetVector(*this);
    }
    return *this;
}


bool CSeqVector::CanGetRange(TSeqPos from, TSeqPos to) const
{
    SSeqMapSelector sel;
    sel.SetRange(from, to-from).SetStrand(m_Strand).SetLinkUsedTSE(m_TSE)
        .SetResolveCount(size_t(-1));
    return m_SeqMap->CanResolveRange(m_Scope.GetScopeOrNull(), sel);
}


CSeqVector::TResidue CSeqVector::x_GetGapChar(TCoding coding)
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
        // Exception is not good here because it conflicts with CSeqVector_CI.
        return 0xff;

    case CSeq_data::e_Ncbipaa: //### Not sure about this
    case CSeq_data::e_Ncbipna: //### Not sure about this
    default:
        NCBI_THROW(CSeqVectorException, eCodingError,
                   "Can not indicate gap using the selected coding");
    }
}


DEFINE_STATIC_FAST_MUTEX(s_ConvertTableMutex2);

const char* CSeqVector::sx_GetConvertTable(TCoding src,
                                           TCoding dst,
                                           bool reverse,
                                           TCaseConversion case_conversion)
{
    CFastMutexGuard guard(s_ConvertTableMutex2);
    typedef pair<TCoding, TCoding> TMainConversion;
    typedef pair<bool, CSeqVector_CI::ECaseConversion> TConversionFlags;
    typedef pair<TMainConversion, TConversionFlags> TConversionKey;
    typedef vector<char> TConversionTable;
    typedef map<TConversionKey, TConversionTable> TTables;
    static TTables tables;

    TConversionKey key;
    key.first = TMainConversion(src, dst);
    key.second = TConversionFlags(reverse, case_conversion);
    TTables::iterator it = tables.find(key);
    if ( it != tables.end() ) {
        // already created
        return it->second.empty()? 0: &it->second[0];
    }
    TConversionTable& table = tables[key];
    if ( !CSeqportUtil::IsCodeAvailable(src) ||
         !CSeqportUtil::IsCodeAvailable(dst) ) {
        // invalid types
        return 0;
    }

    const size_t COUNT = kMax_UChar+1;
    const unsigned kInvalidCode = kMax_UChar;

    pair<unsigned, unsigned> srcIndex = CSeqportUtil::GetCodeIndexFromTo(src);
    if ( srcIndex.second >= COUNT ) {
        // too large range
        return 0;
    }

    if ( reverse ) {
        // check if src needs complement conversion
        try {
            CSeqportUtil::GetIndexComplement(src, srcIndex.first);
        }
        catch ( exception& /*noComplement*/ ) {
            reverse = false;
        }
    }
    if ( case_conversion != CSeqVector_CI::eCaseConversion_none ) {
        // check if dst is text format
        if ( dst != CSeq_data::e_Iupacaa && dst != CSeq_data::e_Iupacna ) {
            case_conversion = CSeqVector_CI::eCaseConversion_none;
        }
    }

    if ( dst != src ) {
        pair<unsigned, unsigned> dstIndex =
            CSeqportUtil::GetCodeIndexFromTo(dst);
        if ( dstIndex.second >= COUNT ) {
            // too large range
            return 0;
        }

        try {
            // check for types compatibility
            CSeqportUtil::GetMapToIndex(src, dst, srcIndex.first);
        }
        catch ( exception& /*badType*/ ) {
            // incompatible types
            return 0;
        }
    }
    else if ( !reverse &&
              case_conversion == CSeqVector_CI::eCaseConversion_none ) {
        // no need to convert at all
        return 0;
    }

    table.resize(COUNT, char(kInvalidCode));
    for ( unsigned i = srcIndex.first; i <= srcIndex.second; ++i ) {
        try {
            unsigned code = i;
            if ( reverse ) {
                code = CSeqportUtil::GetIndexComplement(src, code);
            }
            if ( dst != src ) {
                code = CSeqportUtil::GetMapToIndex(src, dst, code);
            }
            code = min(kInvalidCode, code);
            if ( case_conversion == CSeqVector_CI::eCaseConversion_upper ) {
                code = toupper(char(code));
            }
            else if( case_conversion == CSeqVector_CI::eCaseConversion_lower ) {
                code = tolower(char(code));
            }
            table[i] = char(code);
        }
        catch ( exception& /*noConversion or noComplement*/ ) {
        }
    }
    return &table[0];
}


void CSeqVector::SetCoding(TCoding coding)
{
    if (m_Coding != coding) {
        m_Coding = coding;
    }
    m_Iterator.SetCoding(coding);
}


void CSeqVector::SetIupacCoding(void)
{
    SetCoding(IsProtein()? CSeq_data::e_Iupacaa: CSeq_data::e_Iupacna);
}


void CSeqVector::SetNcbiCoding(void)
{
    SetCoding(IsProtein()? CSeq_data::e_Ncbistdaa: CSeq_data::e_Ncbi4na);
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


void CSeqVector::x_InitRandomizer(CRandom& random_gen)
{
    m_Randomizer.Reset(new CNcbi2naRandomizer(random_gen));
    m_Iterator.x_SetRandomizer(*m_Randomizer);
}


void CSeqVector::SetRandomizeAmbiguities(void)
{
    CRandom random_gen;
    x_InitRandomizer(random_gen);
}


void CSeqVector::SetRandomizeAmbiguities(Uint4 seed)
{
    CRandom random_gen(seed);
    x_InitRandomizer(random_gen);
}


void CSeqVector::SetRandomizeAmbiguities(CRandom& random_gen)
{
    x_InitRandomizer(random_gen);
}


void CSeqVector::SetNoAmbiguities(void)
{
    m_Randomizer.Reset();
    m_Iterator.SetNoAmbiguities();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.73  2005/04/11 15:23:23  vasilche
* Added option to change letter case in CSeqVector_CI.
*
* Revision 1.72  2004/12/22 15:56:17  vasilche
* Added CTSE_Handle.
* Added CSeqVector constructor from CBioseq_Handle to allow used TSE linking.
*
* Revision 1.71  2004/12/06 17:10:50  grichenk
* Removed constructor accepting CConstRef<CSeqMap>
*
* Revision 1.70  2004/10/27 16:41:39  vasilche
* Use 0xff to represent gaps in CSeqVector with NCBI2na encoding.
*
* Revision 1.69  2004/06/14 18:30:08  grichenk
* Added ncbi2na randomizer to CSeqVector
*
* Revision 1.68  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.67  2004/04/12 16:49:16  vasilche
* Allow null scope in CSeqMap_CI and CSeqVector.
*
* Revision 1.66  2004/02/25 18:58:47  shomrat
* Added a new construtor based on Seq-loc in scope
*
* Revision 1.65  2003/11/19 22:18:04  grichenk
* All exceptions are now CException-derived. Catch "exception" rather
* than "runtime_error".
*
* Revision 1.64  2003/10/08 14:16:55  vasilche
* Removed circular reference CSeqVector <-> CSeqVector_CI.
*
* Revision 1.63  2003/09/30 16:22:04  vasilche
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
* Revision 1.62  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.61  2003/08/29 13:34:47  vasilche
* Rewrote CSeqVector/CSeqVector_CI code to allow better inlining.
* CSeqVector::operator[] made significantly faster.
* Added possibility to have platform dependent byte unpacking functions.
*
* Revision 1.60  2003/08/21 18:43:29  vasilche
* Added CSeqVector::IsProtein() and CSeqVector::IsNucleotide() methods.
*
* Revision 1.59  2003/08/21 17:04:10  vasilche
* Fixed bug in making conversion tables.
*
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
