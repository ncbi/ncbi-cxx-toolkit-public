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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   WindowMasker helper functions (prototypes).
 *
 */

#ifndef C_WIN_MASK_UTIL_HPP
#define C_WIN_MASK_UTIL_HPP

#include <set>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include "win_mask_config.hpp"

BEGIN_NCBI_SCOPE

class CWinMaskUtil
{
    public:

        /**
            \brief Check if the given bioseq should be considered for 
                   processing.

            ids and exclude_ids should not be simultaneousely non empty.

            \param bsh bioseq handle in question
            \param ids set of seq ids to consider
            \param exclude_ids set of seq ids excluded from consideration
            \return true if ids is not empty and bsh is among ids, or else
                         if exclude_ids is not empty and bsh is not among
                            exclude_ids;
                    false otherwise
         */
        static bool consider( 
            const objects::CBioseq_Handle & bsh,
            const CWinMaskConfig::CIdSet * ids,
            const CWinMaskConfig::CIdSet * exclude_ids );
};

END_NCBI_SCOPE

#endif
