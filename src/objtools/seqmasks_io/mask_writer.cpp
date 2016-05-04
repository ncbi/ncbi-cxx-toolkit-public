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
 *   CMaskWriter class member and method definitions.
 *
 */
#include <ncbi_pch.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/util/create_defline.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/seqmasks_io/mask_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//-------------------------------------------------------------------------
void CMaskWriter::PrintId( objects::CBioseq_Handle& bsh, bool parsed_id )
{
    os << IdToString(bsh, parsed_id);
}

string CMaskWriter::IdToString( objects::CBioseq_Handle& bsh, bool parsed_id )
{ 
    CNcbiOstrstream oss;
    oss << ">";

    if( parsed_id ) {
        oss << CSeq_id::GetStringDescr(*bsh.GetCompleteBioseq(), 
                                       CSeq_id::eFormat_FastA) + " ";
    }

    oss << sequence::CDeflineGenerator().GenerateDefline(bsh);
    return CNcbiOstrstreamToString(oss);
}

END_NCBI_SCOPE
