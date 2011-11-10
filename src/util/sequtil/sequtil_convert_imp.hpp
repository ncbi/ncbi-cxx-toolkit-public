#ifndef UTIL_SEQUTIL___SEQUTIL_CONVERT_IMP__HPP
#define UTIL_SEQUTIL___SEQUTIL_CONVERT_IMP__HPP

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
 * Author:  Mati Shomrat
 *
 * File Description:
 *   
 */   
#include <corelib/ncbistd.hpp>

#include <util/sequtil/sequtil_convert.hpp>
#include "sequtil_shared.hpp"

BEGIN_NCBI_SCOPE


class CSeqConvert_imp
{
public:
    typedef CSeqUtil::TCoding TCoding;
    typedef CSeqConvert::IPackTarget IPackTarget;

    // Conversion

    template <typename SrcCont, typename DstCont>
    static SIZE_TYPE Convert
    (const SrcCont& src,
     TCoding src_coding,
     TSeqPos pos,
     TSeqPos length,
     DstCont& dst, 
     TCoding dst_coding)
    {
        _ASSERT(!OutOfRange(pos, src, src_coding));
        
        if ( src.empty()  ||  (length == 0) ) {
            return 0;
        }
        
        AdjustLength(src, src_coding, pos, length);
        ResizeDst(dst, dst_coding, length);
        
        return Convert(&*src.begin(), src_coding, pos, length,
            &*dst.begin(), dst_coding);
    }


    static SIZE_TYPE Convert(const char* src, TCoding src_coding,
                             TSeqPos pos, TSeqPos length,
                             char* dst, TCoding dst_coding);

    // Subseq

    template <typename SrcCont, typename DstCont>
    static SIZE_TYPE Subseq
    (const SrcCont& src,
     TCoding coding,
     TSeqPos pos,
     TSeqPos length,
     DstCont& dst)
    {
        _ASSERT(!OutOfRange(pos, src, coding));

        if ( src.empty()  ||  (length == 0) ) {
            return 0;
        }
        
        AdjustLength(src, coding, pos, length);
        ResizeDst(dst, coding, length);
        
        return Subseq(&*src.begin(), coding, pos, length, &*dst.begin());
    }

    static SIZE_TYPE Subseq(const char* src, TCoding coding,
                            TSeqPos pos, TSeqPos length,
                            char* dst);

    // Pack

    template <typename SrcCont, typename DstCont>
    static SIZE_TYPE Pack
    (const SrcCont& src,
     TCoding src_coding,
     DstCont& dst,
     TCoding& dst_coding,
     TSeqPos length)
    {
        if ( src.empty()  ||  (length == 0) ) {
            return 0;
        }
        
        AdjustLength(src, src_coding, 0, length);
        // we allocate enough memory for ncbi4na coding
        // if the result will be ncbi2na coding we'll resize (see below)
        ResizeDst(dst, CSeqUtil::e_Ncbi4na, length);
        
        SIZE_TYPE res = Pack(&*src.begin(), length, src_coding, 
                             &*dst.begin(), dst_coding);
        if ( dst_coding == CSeqUtil::e_Ncbi2na ) {
            size_t new_size = res / 4;
            if ( (res % 4) != 0 ) {
                ++new_size;
            }
            dst.resize(new_size);
        }
        return res;
    }

    static SIZE_TYPE Pack(const char* src, TSeqPos length, TCoding src_coding,
                          char* dst, TCoding& dst_coding);

    template <typename SrcCont>
    static SIZE_TYPE Pack
    (const SrcCont& src,
     TCoding src_coding,
     IPackTarget& dst,
     TSeqPos length)
    {
        if ( src.empty()  ||  (length == 0) ) {
            return 0;
        }
        
        AdjustLength(src, src_coding, 0, length);
        return Pack(&*src.begin(), length, src_coding, dst);
    }

    static SIZE_TYPE Pack(const char* src, TSeqPos length, TCoding src_coding,
                          IPackTarget& dst);

private:

    // Conversion methods:

    // --- NA conversions:

    // iupacna -> ...
    static SIZE_TYPE x_ConvertIupacnaToIupacna(const char* src, TSeqPos pos,
        TSeqPos length, char* dst);
    static SIZE_TYPE x_ConvertIupacnaTo2na(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);
    static SIZE_TYPE x_ConvertIupacnaTo2naExpand(const char* src, TSeqPos pos,
        TSeqPos length, char* dst);
    static SIZE_TYPE x_ConvertIupacnaTo4na(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);
    static SIZE_TYPE x_ConvertIupacnaTo8na(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);

    // ncbi2na -> ...
    static SIZE_TYPE x_Convert2naToIupacna(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);
    static SIZE_TYPE x_Convert2naTo2naExpand(const char* src, TSeqPos pos,
        TSeqPos length, char* dst);
    static SIZE_TYPE x_Convert2naTo4na(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);
    static SIZE_TYPE x_Convert2naTo8na(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);

    // ncbi2na_expand -> ...
    static SIZE_TYPE x_Convert2naExpandToIupacna(const char* src, TSeqPos pos,
        TSeqPos length, char* dst);
    static SIZE_TYPE x_Convert2naExpandTo2na(const char* src, TSeqPos pos,
        TSeqPos length, char* dst);
    static SIZE_TYPE x_Convert2naExpandTo4na(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);
    static SIZE_TYPE x_Convert2naExpandTo8na(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);

    // ncbi4na -> ...
    static SIZE_TYPE x_Convert4naToIupacna(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);
    static SIZE_TYPE x_Convert4naTo2na(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);
    static SIZE_TYPE x_Convert4naTo2naExpand(const char* src, TSeqPos pos,
        TSeqPos length, char* dst);
    static SIZE_TYPE x_Convert4naTo8na(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);

    // ncbi8na (ncbi4na_expand) -> ...
    static SIZE_TYPE x_Convert8naToIupacna(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);
    static SIZE_TYPE x_Convert8naTo2na(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);
    static SIZE_TYPE x_Convert8naTo2naExpand(const char* src, TSeqPos pos,
        TSeqPos length, char* dst);
    static SIZE_TYPE x_Convert8naTo4na(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);

    // --- AA conversions:

    // iupacaa -> ...
    static SIZE_TYPE x_ConvertIupacaaToEaa(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);
    static SIZE_TYPE x_ConvertIupacaaToStdaa(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);

    // ncbieaa -> ...
    static SIZE_TYPE x_ConvertEaaToIupacaa(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);
    static SIZE_TYPE x_ConvertEaaToStdaa(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);

    // ncbistdaa (ncbi8aa) -> ...
    static SIZE_TYPE x_ConvertStdaaToIupacaa(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);
    static SIZE_TYPE x_ConvertStdaaToEaa(const char* src, TSeqPos pos, 
        TSeqPos length, char* dst);

    // Test for amibiguous bases (not A,C,G or T) starting at position 0.
    static bool x_HasAmbig(const char* src, TCoding src_coding, size_t length);
    static bool x_HasAmbigNcbi8na(const char* src, size_t length);
    static bool x_HasAmbigNcbi4na(const char* src, size_t length);
    static bool x_HasAmbigIupacna(const char* src, size_t length);

    // Advanced packing

    // General approach: always keep track of the best option ending
    // in a full-width chunk, which may prove to be useful if a
    // following short region wouldn't be worth the overhead.
    // (Also, try to keep partial nucleotide bytes to a minimum.)

    class CPacker {
    public:
        CPacker(TCoding src_coding, const TCoding* best_coding, bool gaps_ok,
                IPackTarget& dst)
            : m_SrcCoding(src_coding), m_BestCoding(best_coding),
              m_Target(dst), m_SrcDensity(GetBasesPerByte(src_coding)),
              m_GapsOK(gaps_ok), m_WideCoding(x_GetWideCoding(src_coding))
            { }

        SIZE_TYPE Pack(const char* src, TSeqPos length);

    private:
        void x_AddBoundary(TSeqPos pos, TCoding new_coding);
        static TCoding x_GetWideCoding(const TCoding coding);

        struct SArrangement {
            vector<TCoding> codings;
            SIZE_TYPE       cost;
        };

        const TCoding        m_SrcCoding;
        const TCoding* const m_BestCoding;
        IPackTarget&         m_Target;
        const size_t         m_SrcDensity;
        const bool           m_GapsOK;
        const TCoding        m_WideCoding;

        vector<TSeqPos> m_Boundaries;
        SArrangement    m_EndingNarrow;
        SArrangement    m_EndingWide;

        static const TCoding kNoCoding;
    };
};



END_NCBI_SCOPE


#endif  /* UTIL_SEQUTIL___SEQUTIL_CONVERT_IMP__HPP */
