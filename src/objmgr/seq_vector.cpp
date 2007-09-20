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

INcbi2naRandomizer::~INcbi2naRandomizer(void)
{
}


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
            m_FixedTable[na4] = set_bit;
            continue;
        }
        m_FixedTable[na4] = kRandomValue;
        // Ambiguity: create random distribution with possible bases
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
                m_RandomTable[na4][i] = base;
                bases[base]--;
                break;
            }
        }
    }
}


CNcbi2naRandomizer::~CNcbi2naRandomizer(void)
{
}


void CNcbi2naRandomizer::RandomizeData(char* data,
                                       size_t count,
                                       TSeqPos pos)
{
    for (char* stop = data + count; data < stop; ++data, ++pos) {
        int base4na = *data;
        char base2na = m_FixedTable[base4na];
        if ( base2na == kRandomValue ) {
            // Ambiguity, use random value
            base2na = m_RandomTable[base4na][(pos & kRandomizerPosMask)];
        }
        *data = base2na;
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
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Size = bioseq.GetBioseqLength();
    m_Mol = bioseq.GetSequenceType();
    m_Iterator.x_SetVector(*this);
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeqMap& seqMap, CScope& scope,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(&scope),
      m_SeqMap(&seqMap),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Size = m_SeqMap->GetLength(m_Scope);
    m_Mol = m_SeqMap->GetMol();
    m_Iterator.x_SetVector(*this);
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeqMap& seqMap, const CTSE_Handle& top_tse,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(top_tse.GetScope()),
      m_SeqMap(&seqMap),
      m_TSE(top_tse),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Size = m_SeqMap->GetLength(m_Scope);
    m_Mol = m_SeqMap->GetMol();
    m_Iterator.x_SetVector(*this);
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeq_loc& loc, CScope& scope,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(&scope),
      m_SeqMap(CSeqMap::CreateSeqMapForSeq_loc(loc, &scope)),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Size = m_SeqMap->GetLength(m_Scope);
    m_Mol = m_SeqMap->GetMol();
    m_Iterator.x_SetVector(*this);
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeq_loc& loc, const CTSE_Handle& top_tse,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(top_tse.GetScope()),
      m_SeqMap(CSeqMap::CreateSeqMapForSeq_loc(loc, &top_tse.GetScope())),
      m_TSE(top_tse),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Size = m_SeqMap->GetLength(m_Scope);
    m_Mol = m_SeqMap->GetMol();
    m_Iterator.x_SetVector(*this);
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CBioseq& bioseq,
                       CScope* scope,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(scope),
      m_SeqMap(CSeqMap::CreateSeqMapForBioseq(bioseq)),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Size = m_SeqMap->GetLength(scope);
    m_Mol = bioseq.GetInst().GetMol();
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
        m_TSE    = vec.m_TSE;
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


CSeqVectorTypes::TResidue
CSeqVectorTypes::sx_GetGapChar(TCoding coding, ECaseConversion case_cvt)
{
    switch (coding) {
    case CSeq_data::e_Iupacna: // DNA - N
        return case_cvt == eCaseConversion_lower? 'n': 'N';
    
    case CSeq_data::e_Ncbi8na: // DNA - bit representation
    case CSeq_data::e_Ncbi4na:
        return 0x0f;  // all bits set == any base

    case CSeq_data::e_Ncbieaa: // Proteins - X
    case CSeq_data::e_Iupacaa:
        return case_cvt == eCaseConversion_lower? 'x': 'X';
    
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

const char*
CSeqVectorTypes::sx_GetConvertTable(TCoding src, TCoding dst,
                                    bool reverse, ECaseConversion case_cvt)
{
    CFastMutexGuard guard(s_ConvertTableMutex2);
    typedef pair<TCoding, TCoding> TMainConversion;
    typedef pair<bool, ECaseConversion> TConversionFlags;
    typedef pair<TMainConversion, TConversionFlags> TConversionKey;
    typedef vector<char> TConversionTable;
    typedef map<TConversionKey, TConversionTable> TTables;
    static TTables tables;

    TConversionKey key;
    key.first = TMainConversion(src, dst);
    key.second = TConversionFlags(reverse, case_cvt);
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
    if ( case_cvt != eCaseConversion_none ) {
        // check if dst is text format
        if ( dst != CSeq_data::e_Iupacaa &&
             dst != CSeq_data::e_Iupacna &&
             dst != CSeq_data::e_Ncbieaa ) {
            case_cvt = eCaseConversion_none;
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
    else if ( !reverse && case_cvt == eCaseConversion_none ) {
        // no need to convert at all
        return 0;
    }

    table.resize(COUNT, char(kInvalidCode));
    bool different = false;
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
            if ( case_cvt == eCaseConversion_upper ) {
                code = toupper((unsigned char) code);
            }
            else if( case_cvt == eCaseConversion_lower ) {
                code = tolower((unsigned char) code);
            }
            if ( code != i ) {
                different = true;
            }
            table[i] = char(code);
        }
        catch ( exception& /*noConversion or noComplement*/ ) {
            different = true;
        }
    }
    if ( !different ) {
        table.clear();
        return 0;
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
