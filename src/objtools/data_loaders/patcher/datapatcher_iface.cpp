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
*  Author: Maxim Didenko
*
*  File Description:
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>

#include <objtools/data_loaders/patcher/datapatcher_iface.hpp>

#include <objmgr/seq_id_translator.hpp>
#include <objmgr/impl/tse_assigner.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

void IDataPatcher::Patch(CSeq_entry& entry)
{
    CRef<ISeq_id_Translator> tr = GetSeqIdTranslator();
    if (tr) 
        PatchSeqId(entry, *tr);
}

IDataPatcher::~IDataPatcher() 
{
}



END_SCOPE(objects)
END_NCBI_SCOPE


/* ========================================================================== 
 * $Log$
 * Revision 1.1  2005/09/06 13:22:11  didenko
 * IDataPatcher interface moved to a separate file
 *
 * ========================================================================== */
