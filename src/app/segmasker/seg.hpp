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
 * File Description:
 *   CSegMasker class definition.
 *
 */

#ifndef __SEG_MASKER__HPP
#define __SEG_MASKER__HPP

#include <algo/blast/core/blast_def.h>
#include <objmgr/seq_vector.hpp>
#include <util/range.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief This class encapsulates the SEG filtering algorithm
 **
 **/
class CSegMasker
{
public:

    /**\brief Type representing a list of masked segments. */
    typedef vector< pair<TSeqPos, TSeqPos> > TMaskList;

    /**
     **\brief Object constructor.
     **
     **\param window seg window
     **\param locut seg locut
     **\param hicut seg hicut
     **
     **/
    CSegMasker(int window = kSegWindow, 
               double locut = kSegLocut, 
               double hicut = kSegHicut);

    /**
     **\brief Object destructor.
     **
     **/
    ~CSegMasker();

    /**
     **\brief Function performing the actual dusting.
     **
     **\param data sequence data in NCBISTDAA format
     **\return pointer to a list of filtered regions
     **
     **/
    TMaskList * operator()(const objects::CSeqVector & data);

private:
    struct SegParameters* m_SegParameters; ///< Parameters to SEG algorithm
};

END_NCBI_SCOPE

#endif /* __SEG_MASKER__HPP */
