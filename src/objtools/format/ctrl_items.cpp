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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   flat-file generator -- control item implementation
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/Date.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqblock/SP_block.hpp>
#include <objects/seqblock/PDB_block.hpp>
#include <objects/seqblock/PDB_replace.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/ctrl_items.hpp>
#include <objtools/format/context.hpp>
#include <objmgr/util/objutil.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);

//  ----------------------------------------------------------------------------
CStartItem::CStartItem( CSeq_entry_Handle seh )
//  ----------------------------------------------------------------------------
    : CCtrlItem(0)
{
    x_SetDate( seh );
}

//  ----------------------------------------------------------------------------
void 
CStartItem::x_SetDate( 
    CSeq_entry_Handle seh )
//  ----------------------------------------------------------------------------
{
    if ( ! seh.IsSetDescr() ) {
        m_Date = CurrentTime().AsString("Y-M-D");
        return;
    }
    const list< CRef< CSeqdesc > > lsd = seh.GetDescr().Get();
    if ( lsd.empty() ) {
        m_Date = CurrentTime().AsString("Y-M-D");
        return;
    }
    
    for ( list< CRef< CSeqdesc > >::const_iterator cit = lsd.begin(); 
        cit != lsd.end(); ++cit ) 
    {
        const CSeqdesc& sd = **cit;
        switch( sd.Which() ) {
        
            default:
                break;
                
            case CSeqdesc::e_Create_date:
                sd.GetCreate_date().GetDate( &m_Date, "%Y-%2M-%2D" );
                break;
                
            case CSeqdesc::e_Update_date:
                sd.GetUpdate_date().GetDate( &m_Date, "%Y-%2M-%2D" );
                return; 
        }        
    }
    if ( m_Date.empty() ) {
        m_Date = CurrentTime().AsString("Y-M-D");
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE

