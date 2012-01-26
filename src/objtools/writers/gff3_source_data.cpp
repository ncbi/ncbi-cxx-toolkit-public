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
 * File Description:
 *   GFF file writer, generate source record
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objmgr/util/sequence.hpp>

#include <objtools/writers/write_util.hpp>
#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gff3_source_data.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
bool CGff3SourceRecord::AssignData(
    CGffFeatureContext& fc,
    const CSeqdesc& desc )
//  ----------------------------------------------------------------------------
{
    CBioseq_Handle bsh = fc.BioseqHandle();

    // id
    CConstRef<CSeq_id> pId = bsh.GetNonLocalIdOrNull();
    if (!pId  ||  !CWriteUtil::GetBestId(CSeq_id_Handle::GetHandle( *pId ),
            bsh.GetScope(), m_strId)) {
        m_strId = "<unknown>";
    }

    m_strType = "region";
    m_strSource = ".";
    CWriteUtil::GetIdType(bsh, m_strSource);
    m_uSeqStart = 0;
    m_uSeqStop = bsh.GetBioseqLength() - 1;
    //score
    if ( bsh.CanGetInst_Strand() ) {
        m_peStrand = new ENa_strand( eNa_strand_plus );
    }
    // phase

    //  attributes:
    SetAttribute("gbkey", "Src");
    string value;
    if (CWriteUtil::GetBiomol(bsh, value)) {
        SetAttribute("mol_type", value);
    } 
    if (CWriteUtil::GetBiomol(bsh, value)) {
        SetAttribute("mol_type", value);
    } 

    const CBioSource& bs = desc.GetSource();
    if ( ! x_AssignBiosrcAttributes( bs ) ) {
        return false;
    }
    if ( CWriteUtil::IsSequenceCircular(bsh)) {
       SetAttribute("is_circular", "true");
    }

    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
