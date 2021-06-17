#ifndef OBJTOOLS_IDMAPPER___IDMAPPER__HPP
#define OBJTOOLS_IDMAPPER___IDMAPPER__HPP

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
 * Author:  Frank Ludwig
 *
 * File Description: Definition of the IIdMapper interface and its
 *          implementation
 *
 */

#include <corelib/ncbistd.hpp>
#include <objects/seq/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

/// General IdMapper interface
///
/// This interface should suffice for typical IdMapper use, regardless of the
/// actual inplementation.
///
class NCBI_XOBJREAD_EXPORT IIdMapper
{
public:
    virtual ~IIdMapper() {};

    /// Map a single given CSeq_id_Handle to another.
    /// @return
    ///   the mapped handle, or an invalid handle if a mapping is not possible.
    virtual CSeq_id_Handle Map(const CSeq_id_Handle& id) = 0;

    virtual CRef<CSeq_loc> Map(const CSeq_loc& loc) = 0;

    /// Map all embedded IDs in a given object at once.
    virtual void MapObject(CSerialObject&) = 0;
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_IDMAPPER___IDMAPPER__HPP
