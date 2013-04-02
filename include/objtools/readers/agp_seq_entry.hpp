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
 * Authors:  Mike DiCuccio, Michael Kornbluh
 *
 * File Description:
 *     Convert an AGP file into a vector of Seq-entries
 *
 */

#include <objtools/readers/agp_util.hpp>

BEGIN_NCBI_SCOPE

namespace objects {
    class CBioseq;
    class CSeq_entry;
    class CSeq_id;
}

/// This class is used to turn an AGP file into a vector of Seq-entry's
class NCBI_XOBJREAD_EXPORT CAgpToSeqEntry : public CAgpReader {
public:

    /// This is the way the results will be returned
    /// Each Seq-entry contains just one Bioseq, built from the AGP file(s).
    typedef vector< CRef<objects::CSeq_entry> > TSeqEntryRefVec;

    CAgpToSeqEntry(void);

    /// This gets the results found, but don't call before finalizing.  We are intentionally
    /// giving a non-const reference because the caller is free to 
    /// take the seq-entries inside and do whatever they like with them.
    /// Each Seq-entry contains just one Bioseq, built from the AGP file(s).
    TSeqEntryRefVec & GetResult(void) { return m_entries; }

    /// This is the default method used to turn strings into Seq-ids in AGP contexts.
    ///
    /// @sa x_GetSeqIdFromStr
    static CRef<objects::CSeq_id> s_DefaultSeqIdFromStr( const std::string & str );

protected:

    /// Builds new part of delta-seq in current bioseq, or adds bioseq
    /// and starts building a new one.
    virtual void OnGapOrComponent(void);

    /// Parent finalize plus making sure last m_bioseq is added.
    virtual int Finalize(void);
    
    /// If you must change exactly how strings are turned into Seq-ids,
    /// you can override this in a subclass.  The default 
    // is to use s_DefaultSeqIdFromStr.
    virtual CRef<objects::CSeq_id> x_GetSeqIdFromStr( const std::string & str ) {
        return s_DefaultSeqIdFromStr(str);
    }

    /// Our own finalization after parent's finalization.
    void x_FinishedBioseq(void);

    /// This is the bioseq currently being built
    CRef<objects::CBioseq> m_bioseq;
    /// Holds the results
    vector< CRef<objects::CSeq_entry> > m_entries;
};

END_NCBI_SCOPE
