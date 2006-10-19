#ifndef UTIL__DIAG_RNG_COLL__HPP
#define UTIL__DIAG_RNG_COLL__HPP
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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Collection of diagonal alignment ranges.
*
*/


#include <util/align_range.hpp>
#include <util/align_range_coll.hpp>


BEGIN_NCBI_SCOPE


class CDiagRngColl : public CAlignRangeCollection<CAlignRange<TSeqPos> >
{
public:
    typedef CAlignRange<TSeqPos>                 TAlnRng;
    typedef CAlignRangeCollection<TAlnRng>       TAlnRngColl;
    typedef CAlignRangeCollExtender<TAlnRngColl> TAlnRngCollExt;

    enum EBaseWidth {
        eUndefinedBaseWidth = 0,
        eNucBaseWidth = 1,
        eProtBaseWidth = 3
    };
    typedef int TBaseWidth;


    /// Constructor
    CDiagRngColl(TBaseWidth first_base_width = eUndefinedBaseWidth,
                 TBaseWidth second_base_width = eUndefinedBaseWidth);

    /// Modifiers
    CDiagRngColl& SetBaseWidths(TBaseWidth first_base_width,
                                TBaseWidth second_base_width) {
        m_FirstBaseWidth = first_base_width;
        m_SecondBaseWidth = second_base_width;
        return *this;
    }

    /// Accessors
    TBaseWidth GetFirstBaseWidth() const {
        if (m_FirstBaseWidth == eUndefinedBaseWidth) {
            NCBI_THROW(CException, eUnknown,
                       "BaseWidths not initialized.  "
                       "Please SetBaseWidths() !");
        }
        return m_FirstBaseWidth;
    }
    TBaseWidth GetSecondBaseWidth() const {
        if (m_SecondBaseWidth == eUndefinedBaseWidth) {
            NCBI_THROW(CException, eUnknown,
                       "BaseWidths not initialized.  "
                       "Please SetBaseWidths() !");
        }
        return m_SecondBaseWidth;
    }
    
    /// Calculate a difference
    void Diff(const TAlnRngColl& substrahend,
              TAlnRngColl& difference);

    /// Trimming methods
    void TrimFirstFrom (TAlnRng& rng, int trim);
    void TrimFirstTo   (TAlnRng& rng, int trim);
    void TrimSecondFrom(TAlnRng& rng, int trim);
    void TrimSecondTo  (TAlnRng& rng, int trim);

private:
    void x_Diff(const TAlnRng& rng,
                TAlnRngColl&   result,
                TAlnRngColl::const_iterator& r_it);

    void x_DiffSecond(const TAlnRng& rng,
                      TAlnRngColl&   result,
                      TAlnRngCollExt::const_iterator& r_it);

    struct PItLess
    {
        typedef TAlnRng::position_type position_type;
        bool operator()
            (const TAlnRngCollExt::TFrom2Range::value_type& p,
             position_type pos)
        { 
            return p.second->GetSecondTo() < pos;  
        }    
        bool operator()
            (position_type pos,
             const TAlnRngCollExt::TFrom2Range::value_type& p)
        { 
            return pos < p.second->GetSecondTo();  
        }    
        bool operator()
            (const TAlnRngCollExt::TFrom2Range::value_type& p1,
             const TAlnRngCollExt::TFrom2Range::value_type& p2)
        { 
            return p1.second->GetSecondTo() < p2.second->GetSecondTo();  
        }    
    };

    TAlnRngCollExt m_Extender;
    int m_FirstBaseWidth;
    int m_SecondBaseWidth;
};


inline
void CDiagRngColl::TrimFirstFrom(TAlnRng& rng, int trim)
{
    rng.SetLength(rng.GetLength() - trim);
    rng.SetFirstFrom(rng.GetFirstFrom() + trim * GetFirstBaseWidth());
    if (rng.IsDirect()) {
        rng.SetSecondFrom(rng.GetSecondFrom() + trim * GetSecondBaseWidth());
    }
}

inline
void CDiagRngColl::TrimFirstTo(TAlnRng& rng, int trim)
{
    if (rng.IsReversed()) {
        rng.SetSecondFrom(rng.GetSecondFrom() +  trim * GetSecondBaseWidth());
    }
    rng.SetLength(rng.GetLength() - trim);
}

inline
void CDiagRngColl::TrimSecondFrom(TAlnRng& rng, int trim)
{
    rng.SetLength(rng.GetLength() - trim);
    rng.SetSecondFrom(rng.GetSecondFrom() + trim * GetSecondBaseWidth());
    if (rng.IsDirect()) {
        rng.SetFirstFrom(rng.GetFirstFrom() + trim * GetFirstBaseWidth());
    }
}

inline
void CDiagRngColl::TrimSecondTo(TAlnRng& rng, int trim)
{
    if (rng.IsReversed()) {
        rng.SetFirstFrom(rng.GetFirstFrom() + trim * GetFirstBaseWidth());
    }
    rng.SetLength(rng.GetLength() - trim);
}


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2006/10/19 18:49:47  todorov
* Fixed a typo.
*
* Revision 1.3  2006/10/19 17:21:14  todorov
* Added exceptions.
*
* Revision 1.2  2006/10/19 17:18:05  todorov
* A few minor fixes.
*
* Revision 1.1  2006/10/19 17:07:07  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif // UTIL__DIAG_RNG_COLL__HPP
