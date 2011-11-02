#ifndef OBJTOOLS_READERS___READER_IDGEN__HPP
#define OBJTOOLS_READERS___READER_IDGEN__HPP

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
 * Author: Frank Ludwig, Wratko Hlavina
 *
 * File Description:
 *   Repeat Masker file reader
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbicntr.hpp>

#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqfeat/Feat_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// The IIdGenerator, ISeqIdResolver, and ITaxonomyResolver interfaces
// really belong higher up in the Toolkit. To be moved when convenient.

/// Templated interface for a generator of identifiers (IDs) of any type.
///
template <class T>
class IIdGenerator : public CObject
{
public:
    typedef IIdGenerator<T> TThisType;

    /// Type for the ID, which is a template parameter.
    ///
    typedef T TId;

    /// Enforce virtual destructor.
    virtual ~IIdGenerator() { }

    /// Generates the "next" id.
    ///
    /// There is no specific constraint on the next id, except that
    /// it be effectively, if not actually, unique. Uniqueness is not
    /// a hard-core requirement, since wrap-around, limited reuse,
    /// and probabilistic uniqueness guarantees are all permitted,
    /// depending on the implementation details.
    ///
    /// @throws if an ID cannot be generated, perhaps because the generator
    ///         has been excausted.
    ///
    /// @note TId should be a CRef in the case of CObject instance,
    ///       an AutoPtr for other non copy-constructible objects,
    ///       and otherwise, should have value semantics.
    ///
    virtual TId GenerateId() = 0;

    /// Identifies if the implementation happens to be thread-safe.
    ///
    /// By default, implementations are not assumed thread-safe.
    ///
    /// @return true if the implementation is known to be thread-safe,
    ///     false otherwise (may be thread-safe, but not assured).
    ///
    /// @note This function admits a client that will respond dynamically
    ///     to the issue of thread-safety. Callers might use this to
    ///     optimize away mutual exclusion in MT situations, but if they
    ///     require thread-safety, it is advisable to require an
    ///     ITheadSafeIdGenerator.
    ///
    ///     In dynamic situations, if a user requires a thread-safe ID
    ///     generator, it is trivial to test for thread-safety, and
    ///     if necessary, wrap all calls with a mutex (perhaps
    ///     using a wrapper instance of this interface, that merely
    ///     checks a mutex and delegates to the original). The
    ///     wrapping could be made into a function, say,
    ///     CIRef<IIdGenerator<> > MakeThreadSafe(IIdGenerator<>&).
    ///     The design considered requiring the implementation
    ///     to be thread-safe always, but this imposes possibly
    ///     unnecessary requirements on all implementations.
    ///     Defining a separate interface or member function
    ///     to generate an ID with thread-safety was also considered,
    ///     but some algorithms are thread-safe depending only
    ///     on their injected dependencies, and calling different
    ///     interfaces/functions complicates their implementation
    ///     (e.g. may requires templates).
    ///
    virtual bool IsThreadSafe() { return false; }
};

/// Thread-safe version of IIdGenerator.
///
template <class T>
class IThreadSafeIdGenerator : public IIdGenerator<T>
{
public:
    typedef IIdGenerator<T> TThisType;

    /// This implementeation IS thread-safe.
    /// Please do not override!
    ///
    bool IsThreadSafe() { return true; }
};

/// Default implementation for a generator of identifiers,
/// as integers, mashalled as CFeat_id objects.
///
class COrdinalFeatIdGenerator : public IThreadSafeIdGenerator< CRef<CFeat_id> >
{
public:
    COrdinalFeatIdGenerator() { }

    TId GenerateId()
    {
        CRef<CFeat_id> id(new CFeat_id);
        id->SetLocal().SetId(m_Id.Add(1) - 1);
        return id;
    }

private:
    CAtomicCounter m_Id;
};


/// Interface for resolving a sequence identifier given
/// a textual representation.
class ISeqIdResolver : public CObject
{
public:
    typedef ISeqIdResolver TThisType;

    /// Enforce virtual destructor.
    virtual ~ISeqIdResolver() { }

    /// Returns a normalized representation of a sequence
    /// identifier, as Seq-id handle.
    virtual CSeq_id_Handle ResolveSeqId(const string& id) const = 0;
};


/// Default implementation of a Seq-id resolver,
/// which knows about FASTA-formatted sequence identifiers.
///
class NCBI_XOBJREAD_EXPORT CFastaIdsResolver : public ISeqIdResolver
{
public:
    CSeq_id_Handle ResolveSeqId(const string& id) const;
};


END_SCOPE(objects)
END_NCBI_SCOPE


#endif // OBJTOOLS_READERS___READER_IDGEN__HPP
