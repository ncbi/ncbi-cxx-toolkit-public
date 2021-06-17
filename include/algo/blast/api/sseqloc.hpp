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
 * Author:  Christiam Camacho
 *
 */

/** @file sseqloc.hpp
 * Definition of SSeqLoc structure
 */

#ifndef ALGO_BLAST_API___SSEQLOC__HPP
#define ALGO_BLAST_API___SSEQLOC__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <objmgr/util/seq_loc_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Structure to represent a single sequence to be fed to BLAST
struct SSeqLoc {
    /// Seq-loc describing the sequence to use as query/subject to BLAST
    /// The types of Seq-loc currently supported are: whole and seq-interval
    CConstRef<objects::CSeq_loc>     seqloc;
    
    /// Scope where the sequence referenced can be found by the toolkit's
    /// object manager
    mutable CRef<objects::CScope>    scope;
    
    /// Seq-loc describing regions to mask in the seqloc field
    /// Acceptable types of Seq-loc are Seq-interval and Packed-int
    /// @sa ignore_strand_in_mask
    CRef<objects::CSeq_loc>          mask;

    /// This member dictates how the strand in the mask member is interpreted.
    /// If true, it means that the Seq-loc in mask is assumed to be on the plus
    /// strand AND that the complement of this should also be applied (i.e.:
    /// the strand specification of the mask member will be ignored). If it's
    /// false, then the strand specification of the mask member will be obeyed
    /// and only those regions on specific strands will be masked.
    /// @note the default value of this field is true
    /// @sa mask
    bool                             ignore_strand_in_mask;

    /// Genetic code id if this sequence should be translated.
    /// @note BLAST_GENETIC_CODE is the default, even though the sequence might
    /// not need to be translated (i.e.: program type determines whether this
    /// is used or not). The sentinel value to indicate that this field is not
    /// applicable is numeric_limits<Uint4>::max().
    Uint4                            genetic_code_id;
    
    /// Default constructor
    SSeqLoc()
        : seqloc(), scope(), mask(), ignore_strand_in_mask(true), 
          genetic_code_id(BLAST_GENETIC_CODE) {}

    /// Parameterized constructor
    /// @param sl Sequence location [in]
    /// @param s Scope to retrieve sl [in]
    SSeqLoc(const objects::CSeq_loc* sl, objects::CScope* s)
        : seqloc(sl), scope(s), mask(0), ignore_strand_in_mask(true),
          genetic_code_id(BLAST_GENETIC_CODE) {}

    /// Parameterized constructor
    /// @param sl Sequence location [in]
    /// @param s Scope to retrieve sl [in]
    SSeqLoc(const objects::CSeq_loc& sl, objects::CScope& s)
        : seqloc(&sl), scope(&s), mask(0), ignore_strand_in_mask(true),
          genetic_code_id(BLAST_GENETIC_CODE) {}

    /// Parameterized constructor
    /// @param sl Sequence location [in]
    /// @param s Scope to retrieve sl [in]
    /// @param m Masking location(s) applicable to sl [in]
    /// @param ignore_mask_strand Ignore the mask specified in m? [in]
    SSeqLoc(const objects::CSeq_loc* sl, objects::CScope* s,
            objects::CSeq_loc* m, bool ignore_mask_strand = true)
        : seqloc(sl), scope(s), mask(m), 
          ignore_strand_in_mask(ignore_mask_strand),
          genetic_code_id(BLAST_GENETIC_CODE) {
        if (m != NULL && ignore_strand_in_mask) {
              mask->ResetStrand();
        }
    }

    /// Parameterized constructor
    /// @param sl Sequence location [in]
    /// @param s Scope to retrieve sl [in]
    /// @param m Masking location(s) applicable to sl [in]
    /// @param ignore_mask_strand Ignore the mask specified in m? [in]
    SSeqLoc(const objects::CSeq_loc& sl, objects::CScope& s,
            objects::CSeq_loc& m, bool ignore_mask_strand = true)
        : seqloc(&sl), scope(&s), mask(&m),
          ignore_strand_in_mask(ignore_mask_strand),
          genetic_code_id(BLAST_GENETIC_CODE) {
        if (ignore_strand_in_mask) {
              mask->ResetStrand();
        }
    }
};

/// Vector of sequence locations
typedef vector<SSeqLoc>   TSeqLocVector;

/// Convert a TSeqLocVector to a CBioseq_set
/// @param input TSeqLocVector to convert [in]
/// @return CBioseq_set with CBioseqs from the input, or NULL of input is empty
NCBI_XBLAST_EXPORT 
CRef<objects::CBioseq_set>
TSeqLocVector2Bioseqs(const TSeqLocVector& input);


/// Search Query
///
/// This class represents the data relevant to one query in a blast
/// search.  The types of Seq-loc currently supported are "whole" and
/// "int".  The scope is expected to contain this Seq-loc, and the
/// mask represents the regions of this query that are disabled for
/// this search, or for some frames of this search, via one of several
/// algorithms, or that are specified by the user as masked regions.
class CBlastSearchQuery : public CObject {
public:
    /// Constructor
    ///
    /// Build a CBlastSearchQuery object with no masking locations assigned
    ///
    /// @param sl The query itself.
    /// @param sc The scope containing the query.
    CBlastSearchQuery(const objects::CSeq_loc & sl,
                      objects::CScope         & sc)
        : seqloc(& sl), scope(& sc), genetic_code_id(BLAST_GENETIC_CODE) 
    {
        x_Validate();
    }

    /// Constructor
    ///
    /// Build a CBlastSearchQuery object.
    ///
    /// @param sl The query itself.
    /// @param sc The scope containing the query.
    /// @param m Regions of the query that are masked.
    CBlastSearchQuery(const objects::CSeq_loc & sl,
                      objects::CScope         & sc,
                      TMaskedQueryRegions       m)
        : seqloc(& sl), scope(& sc), mask(m), 
          genetic_code_id(BLAST_GENETIC_CODE) 
    {
        x_Validate();
    }
    
    /// Default constructor
    ///
    /// This is necessary in order to add this type to a std::vector.
    CBlastSearchQuery() {}
    
    /// Get the query Seq-loc.
    /// @return The Seq-loc representing the query
    CConstRef<objects::CSeq_loc> GetQuerySeqLoc() const {
        return seqloc;
    }

    /// Get the query Seq-id.
    /// @return The Seq-id representing the query
    CConstRef<objects::CSeq_id> GetQueryId() const {
        return CConstRef<objects::CSeq_id>(seqloc->GetId());
    }
    
    /// Get the query CScope.
    /// @return The CScope containing the query
    CRef<objects::CScope> GetScope() const {
        return scope;
    }

    /// Get the genetic code id
    void SetGeneticCodeId(Uint4 gc_id) {
        genetic_code_id = gc_id;
    }
    
    /// Get the genetic code id
    Uint4 GetGeneticCodeId() const {
        return genetic_code_id;
    }
    
    /// Get the masked query regions.
    ///
    /// The masked regions of the query, or of some frames or strands of the
    /// query, are returned.
    ///
    /// @return The masked regions of the query.
    TMaskedQueryRegions GetMaskedRegions() const {
        return mask;
    }
    
    /// Set the masked query regions.
    ///
    /// The indicated set of masked regions is applied to this query,
    /// replacing any existing masked regions.
    ///
    /// @param mqr The set of regions to mask.
    void SetMaskedRegions(TMaskedQueryRegions mqr) {
        mask = mqr;
    }
    
    /// Masked a region of this query.
    ///
    /// The CSeqLocInfo object is added to the list of masked regions
    /// of this query.
    ///
    /// @param sli A CSeqLocInfo indicating the region to mask.
    void AddMask(CRef<CSeqLocInfo> sli)
    {
        mask.push_back(sli);
    }
    
    /// Get the length of the sequence represented by this object
    TSeqPos GetLength() const {
        return objects::sequence::GetLength(*seqloc, scope);
    }
private:
    /// The Seq-loc representing the query.
    CConstRef<objects::CSeq_loc> seqloc;
    
    /// This scope contains the query.
    mutable CRef<objects::CScope> scope;
    
    /// These regions of the query are masked.
    TMaskedQueryRegions mask;

    /// Genetic code id if this sequence should be translated.
    /// If its value is numeric_limits<Uint4>::max(), it means that it's not
    /// applicable
    Uint4                            genetic_code_id;

    /// Currently we only support whole or int.  Throw exception otherwise
    void x_Validate() {
        if (seqloc->IsWhole() || seqloc->IsInt()) return;
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Only whole or int typed seq_loc is supported for CBlastQueryVector");
    }
};


/// Query Vector
///
/// This class represents the data relevant to all queries in a blast
/// search.  The queries are represented as CBlastSearchQuery objects.
/// Each contains a Seq-loc, scope, and a list of filtered regions.

class CBlastQueryVector : public CObject {
public:
    // data type contained by this container
    typedef CRef<CBlastSearchQuery> value_type;

    /// size_type type definition
    typedef vector<value_type>::size_type size_type;

    /// const_iterator type definition
    typedef vector<value_type>::const_iterator const_iterator;

    /// Add a query to the set.
    ///
    /// The CBlastSearchQuery is added to the list of queries for this
    /// search.
    ///
    /// @param q A query to add to the set.
    void AddQuery(CRef<CBlastSearchQuery> q)
    {
        m_Queries.push_back(q);
    }
    
    /// Returns true if this query vector is empty.
    bool Empty() const
    {
        return m_Queries.empty();
    }
    
    /// Returns the number of queries found in this query vector.
    size_type Size() const
    {
        return m_Queries.size();
    }
    
    /// Get the query Seq-loc for a query by index.
    /// @param i The index of a query.
    /// @return The Seq-loc representing the query.
    CConstRef<objects::CSeq_loc> GetQuerySeqLoc(size_type i) const
    {
        _ASSERT(i < m_Queries.size());
        return m_Queries[i]->GetQuerySeqLoc();
    }
    
    /// Get the scope containing a query by index.
    /// @param i The index of a query.
    /// @return The CScope containing the query.
    CRef<objects::CScope> GetScope(size_type i) const
    {
        _ASSERT(i < m_Queries.size());
        return m_Queries[i]->GetScope();
    }
    
    /// Get the masked regions for a query by number.
    /// @param i The index of a query.
    /// @return The masked (filtered) regions of that query.
    TMaskedQueryRegions GetMaskedRegions(size_type i) const
    {
        _ASSERT(i < m_Queries.size());
        return m_Queries[i]->GetMaskedRegions();
    }

    /// Convenience method to get a CSeq_loc representing the masking locations
    /// @param i The index of a query.
    /// @return The masked (filtered) regions of that query.
    /// @throws CBlastException in case of errors in conversion
    CRef<objects::CSeq_loc> GetMasks(size_type i) const
    {
        TMaskedQueryRegions mqr = GetMaskedRegions(i);
        return MaskedQueryRegionsToPackedSeqLoc(mqr);
    }
    
    /// Assign a list of masked regions to one query.
    /// @param i The index of the query.
    /// @param mqr The masked regions for this query.
    void SetMaskedRegions(size_type i, TMaskedQueryRegions mqr)
    {
        _ASSERT(i < m_Queries.size());
        m_Queries[i]->SetMaskedRegions(mqr);
    }
    
    /// Add a masked region to the set for a query.
    /// @param i The index of the query.
    /// @param sli The masked region to add.
    void AddMask(size_type i, CRef<CSeqLocInfo> sli)
    {
        m_Queries[i]->AddMask(sli);
    }
    
    /// Get the CBlastSearchQuery object at index i
    /// @param i The index of a query.
    CRef<CBlastSearchQuery>
    GetBlastSearchQuery(size_type i) const
    {
        _ASSERT(i < m_Queries.size());
        return m_Queries[i];
    }

    /// Get the CBlastSearchQuery object at index i
    /// @param i The index of a query.
    CRef<CBlastSearchQuery>
    operator[](size_type i) const
    {
        return GetBlastSearchQuery(i);
    }
    
    /// Identical to Size, provided to facilitate STL-style iteration
    size_type size() const { return Size(); }

    /// Returns const_iterator to beginning of container, provided to
    /// facilitate STL-style iteration
    const_iterator begin() const { return m_Queries.begin(); }

    /// Returns const_iterator to end of container, provided to
    /// facilitate STL-style iteration
    const_iterator end() const { return m_Queries.end(); }

    /// Clears the contents of this object
    void clear() { m_Queries.clear(); }

    /// Add a value to the back of this container
    /// @param element element to add [in]
    void push_back(const value_type& element) { m_Queries.push_back(element); }

private:
    /// The set of queries used for a search.
    vector< CRef<CBlastSearchQuery> > m_Queries;
};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_API___SSEQLOC__HPP */


