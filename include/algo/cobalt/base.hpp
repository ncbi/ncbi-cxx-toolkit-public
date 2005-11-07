/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: base.hpp

Author: Jason Papadopoulos

Contents: Definitions used by all COBALT aligner components

******************************************************************************/

/// @file multi_base.hpp
/// Definitions used by all COBALT aligner components

#ifndef _ALGO_COBALT_BASE_HPP_
#define _ALGO_COBALT_BASE_HPP_

#include <util/math/matrix.hpp>

#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_stat.h>

#include <algo/phy_tree/dist_methods.hpp>
#include <algo/align/nw/nw_pssm_aligner.hpp>

#include <objtools/data_loaders/blastdb/bdbloader.hpp> 
#include <objmgr/util/sequence.hpp>
#include <objmgr/scope.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Seq_align.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

typedef int TOffset;
typedef pair<TOffset, TOffset> TOffsetPair;

template<class Position>
class CLocalRange : public CRange<Position>
{
public:
    typedef CRange<Position> TParent;
    typedef typename TParent::position_type position_type;
    typedef CLocalRange<Position> TThisType;

    CLocalRange() {}
    CLocalRange(position_type from, position_type to)
        : TParent(from, to) {}
    CLocalRange(const TParent& range)
        : TParent(range) {}

    bool Contains(const TThisType& r)
    {
        return !TParent::Empty() && 
               TParent::GetFrom() <= r.GetFrom() &&
               TParent::GetToOpen() >= r.GetFrom() &&
               TParent::GetFrom() <= r.GetToOpen() &&
               TParent::GetToOpen() >= r.GetToOpen();
    }

    bool StrictlyBelow(const TThisType& r)
    {
        return TParent::GetToOpen() <= r.TParent::GetFrom();
    }

    void SetEmpty()
    {
        TParent::Set(TParent::GetPositionMax(), TParent::GetPositionMin());
    }
};

typedef CLocalRange<TOffset> TRange;

static const int kAlphabetSize = 26;

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif // _ALGO_COBALT_BASE_HPP_

/*--------------------------------------------------------------------
  $Log$
  Revision 1.1  2005/11/07 18:15:52  papadopo
  Initial revision

--------------------------------------------------------------------*/
