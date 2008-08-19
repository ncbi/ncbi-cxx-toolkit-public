#ifndef LDS_READER_HPP__
#define LDS_READER_HPP__
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  LDS reader.
 *
 */

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <objtools/lds/lds.hpp>
#include <objtools/lds/lds_query.hpp>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// Load top level seq entry.
CRef<CSeq_entry> NCBI_LDS_EXPORT LDS_LoadTSE(CLDS_Database&         lds_db,
                                             int object_id,
                                             bool trace_to_top = true);
CRef<CSeq_entry> NCBI_LDS_EXPORT LDS_LoadTSE(const CLDS_Query::SObjectDescr& obj_descr);

// Load annotation.
CRef<CSeq_annot> 
NCBI_LDS_EXPORT LDS_LoadAnnot(SLDS_TablesCollection& lds_db, 
                              const CLDS_Query::SObjectDescr& obj_descr);

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
