#ifndef ANNOT_MAPPER_EXCEPTION__HPP
#define ANNOT_MAPPER_EXCEPTION__HPP

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
* Author: Aleksey Grichenko
*
* File Description:
*   Seq-loc mapper
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>
#include <util/rangemap.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/general/Int_fuzz.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerCore
 *
 * @{
 */


/// Seq-loc and seq-align mapper exceptions
class NCBI_SEQ_EXPORT CAnnotMapperException : public CException
{
public:
    enum EErrCode {
        eBadLocation,    ///< Attempt to map from/to invalid seq-loc
        eUnknownLength,  ///< Can not resolve sequence length
        eBadAlignment,   ///< Unsuported or invalid alignment
        eBadFeature,     ///< Feature can not be used for mapping
        eOtherError
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CAnnotMapperException, CException);
};


/* @} */

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ANNOT_MAPPER_EXCEPTION__HPP
