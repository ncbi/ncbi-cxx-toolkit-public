#ifndef UTIL___ID_MUX__HPP
#define UTIL___ID_MUX__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Id Multiplexer
 *
 */

/// @file id_mux.hpp
/// Classes and interfaces to map integer ids into multi-dimension
/// coordinates.


#include <corelib/ncbistd.hpp>
#include <corelib/ncbimisc.hpp>

#include <util/bitset/bmfwd.h>
#include <util/bitset/bmconst.h>

#include <vector>

BEGIN_NCBI_SCOPE

template<class TBV>
class CBvGapFactory
{
public:
    static TBV* Create() { return new TBV(bm::BM_GAP); }
};

/// Id to coordinates Demultiplexer
///
/// This class converts single id into N-dimensional point 
/// (vector of coordinates)
///
/// <pre>
/// Example:
/// 
/// Database of BLOBs of various sizes:
/// Dimention 1  -  DB volumes (every volume holds up to 4GB of BLOBs)
/// Dimention 2  -  Approximate size of the BLOB (small, medium, large)
///
///
/// 2D matrix:
///
///              small(1)    medium(2)     large(3)
///          +-------------+-------------+-------------+ 
/// vol= 1   |  1,3,5      |  8          | 11, 9       |
/// vol= 2   |  6,10       |  15,16      | 2, 4        |
///          +-------------+-------------+-------------+ 
///
/// Request to get coordinates of 
///      BLOB 8 returns (1, 2)
///           2         (2, 3)
///           1         (1, 1)
/// </pre>
/// 
/// This coordinate remapper stores lists of ids for every projection. 
/// In the example above "vol= 1" projection is (1,3,5,8,9,11)
/// "small" projection is (1,3,5,6,10). 
/// Coordinate search is implemented as scan in all available projections
/// until the id is found.
///
template<class TBV, class TBVFact=CBvGapFactory<TBV> >
class CIdDeMux
{
public:
    /// Point in N-dimentional space
    typedef vector<unsigned> TDimentionalPoint; 

    /// Bitvector describing members of dimension
    typedef TBV      TBitVector;

    typedef TBVFact  TBVFactory;

    typedef AutoPtr<TBitVector> TBitVectorPtr;

    /// Dimension vector 
    ///
    /// Each element of the dimension vector is a bitset of elements
    /// belonging to this projection.
    typedef vector<TBitVectorPtr> TDimVector; 

    /// Vector of space projections
    typedef vector<TDimVector> TCoordinateSpace;

public:

    /// @param N
    ///     Number of dimensions (order of demultiplexer)
    ///
    CIdDeMux(size_t N);

    /// Get order of Multiplexer
    size_t GetN() const { return m_DimSpace.size(); }

    /// Find id in the coordinate space
    /// 
    /// @param id
    ///    Input id
    /// @param coords
    ///    Coordinates of the id (N-dim point)
    ///
    /// @return FALSE if id cannot be found
    ///
    bool GetCoordinates(unsigned id, TDimentionalPoint* coord) const;

    /// Get dimension vector
    const TDimVector&  GetDimVector(size_t i) const;

    /// Get modification access to dimension vector
    TDimVector& PutDimVector(size_t i);

    /// Resize dimesion, create bitset content
    void InitDim(size_t i, size_t dim_size);
private: 
    // TO DO: implement copy semantics
    CIdDeMux(const CIdDeMux&);
    CIdDeMux& operator=(const CIdDeMux&);
private:
    TCoordinateSpace   m_DimSpace;  ///< Dimensions
};


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


template<class TBV, class TBVFact>
CIdDeMux<TBV, TBVFact>::CIdDeMux(size_t N)
: m_DimSpace(N)
{
}

template<class TBV, class TBVFact>
bool CIdDeMux<TBV, TBVFact>::GetCoordinates(unsigned id, TDimentionalPoint* coord) const
{
    _ASSERT(coord);
    size_t N = m_DimSpace.size();
    coord->resize(N);
    for (size_t i = 0; i < N; ++i) {
        bool dim_found = false;
        const TDimVector& dv = GetDimVector(i);
        size_t M = dv.size();
        for (size_t j = 0; j < M; ++j) {
            if (dv[j].get() == 0) 
                continue;
            const TBitVector& bv = *(dv[j]);
            dim_found = bv[id];
            if (dim_found) {
                (*coord)[i] = j;
                break;
            }
        } // for j
        if (!dim_found) 
            return dim_found;
    } // for i
    return true;
}


template<class TBV, class TBVFact>
const typename CIdDeMux<TBV, TBVFact>::TDimVector&  
CIdDeMux<TBV, TBVFact>::GetDimVector(size_t i) const
{
    _ASSERT(i < m_DimSpace.size());
    return m_DimSpace[i];
}

template<class TBV, class TBVFact>
typename CIdDeMux<TBV, TBVFact>::TDimVector&  
CIdDeMux<TBV, TBVFact>::PutDimVector(size_t i)
{
    _ASSERT(i < m_DimSpace.size());
    return m_DimSpace[i];
}


template<class TBV, class TBVFact>
void CIdDeMux<TBV, TBVFact>::InitDim(size_t i, size_t dim_size)
{
    TDimVector& dv = PutDimVector(i);

    size_t old_size = dv.size();
    dv.resize(dim_size);
    for (size_t i = old_size; i < dim_size; ++i) {
        dv[i] = TBVFactory::Create();
    }
}


END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2006/03/23 13:32:10  gouriano
* Sync PutDimVector declaration and definition
*
* Revision 1.1  2006/03/22 19:30:36  kuznets
* initial revision
*
* ---------------------------------------------------------------------------
*/

#endif  /* UTIL___ID_MUX__HPP */
