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
 * Author:  Irena Zaretskaya
 *
 * File Description:
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbistl.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <util/image/image.hpp>
#include <util/image/image_util.hpp>
#include <util/image/image_io.hpp>

#include <objmgr/util/sequence.hpp>

#include <connect/services/netcache_client.hpp>
#include "blast_hitmatrix.hpp"

BEGIN_NCBI_SCOPE

void CBlastHitMatrix::x_InitObjectManager()
{
    m_ObjMgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);

    // create our object manager
    m_Scope.Reset(new CScope(*m_ObjMgr));
	m_Scope->AddDefaults();
}


void CBlastHitMatrix::x_InitPort()
{
	m_Port.SetViewport(TVPRect(0, 0, m_Width, m_Height));
			
	TModelRect rc_m(0, 0, 1000, 1000);
	CBioseq_Handle s_handle = m_DataSource->GetSubjectHandle();
	if(s_handle)    {
	   rc_m.SetHorz(0, s_handle.GetBioseqLength());
	} else {
	   TSeqRange s_r = m_DataSource->GetSubjectHitsRange();
	   rc_m.SetHorz(s_r.GetFrom(), s_r.GetToOpen());
	}

	CBioseq_Handle q_handle = m_DataSource->GetQueryHandle();
	if(q_handle)    {
	   rc_m.SetVert(0, q_handle.GetBioseqLength());
	} else {
	   TSeqRange q_r = m_DataSource->GetQueryHitsRange();
	   rc_m.SetVert(q_r.GetFrom(), q_r.GetToOpen());
	}
	
	m_Port.SetModelLimitsRect(rc_m);
}


CBlastHitMatrix::CBlastHitMatrix(const list< CRef< CSeq_align > > &seqAligns, 
                                 int height,
                                 int width,
                                 CImageIO::EType format)
{
    ITERATE (CSeq_annot::TData::TAlign, iter, seqAligns) {
        CRef< CSeq_align > seq_align = *iter;
        m_Aligns.push_back(CConstRef<CSeq_align>(seq_align));
    }
    m_Height = height;
    m_Width = width;
    m_Format = format;
    m_FileOut = false;
}


void CBlastHitMatrix::x_GetLabels(void)
{
    for(size_t i=0; i < m_Aligns.size();i++) {        
        const CSeq_id& queryID = m_Aligns[i]->GetSeq_id(0);                        
        if(queryID.IsLocal()) {
            (*sequence::GetId(queryID,*m_Scope,sequence:: eGetId_Best).GetSeqId()).GetLabel(&m_QueryID);                  
        }
        else {
            CLabel::GetLabel(queryID,&m_QueryID,CLabel::eDefault,m_Scope.GetPointer());
        }
        
        const CSeq_id& subjectID = m_Aligns[i]->GetSeq_id(1);         
        if(subjectID.IsLocal()) {
            (*sequence::GetId(subjectID,*m_Scope,sequence:: eGetId_Best).GetSeqId()).GetLabel(&m_SubjectID);    
        }
        else {
            CLabel::GetLabel(subjectID,&m_SubjectID,CLabel::eDefault,m_Scope.GetPointer());
        }
        //m_SubjectID = "Subj: " + m_SubjectID;
        break;
    }
}


//extracting arguments, verifying and loading data
void CBlastHitMatrix::x_PreProcess(void)
{
    
    x_InitObjectManager();
    x_GetLabels();
		
    // create a Data Source
    CHitMatrixDSBuilder builder;
    //builder.Init(*m_Scope, *m_Annot);
    builder.Init(*m_Scope, m_Aligns);       
	m_DataSource = builder.CreateDataSource();			
	m_DataSource->SelectDefaultIds();

    x_InitPort();       
        
    m_Renderer.Update(m_DataSource.GetPointer(), m_Port);
}




void CBlastHitMatrix::x_Render(void)
{
    if(m_Aligns.size() > 0) {
        m_Renderer.Resize(m_Width, m_Height, m_Port);
        m_Renderer.GetBottomRuler().SetDisplayOptions(CRuler::fShowTextLabel);        
        m_Renderer.GetLeftRuler().SetDisplayOptions(CRuler::fShowTextLabel);

        m_Renderer.GetBottomRuler().SetTextLabel(m_QueryID);        
        m_Renderer.GetLeftRuler().SetTextLabel(m_SubjectID);

        //m_Renderer.GetBottomRuler().SetColor(CRuler::eText,CRgbaColor(24,0,0));        
        //m_Renderer.GetLeftRuler().SetColor(CRuler::eBackground,CRgbaColor(175,238,238));
        //m_Renderer.GetBottomRuler().SetColor(CRuler::eText,CRgbaColor(0,0,0));        
        //m_Renderer.GetLeftRuler().SetColor(CRuler::eBackground,CRgbaColor("173 255 47"));
        
        // adjust visible space
        m_Port.SetViewport(TVPRect(0, 0, m_Width, m_Height)); ///### this have to be eliminated
        m_Port.ZoomAll();

        m_Renderer.Render(m_Port);
    }   
}

bool CBlastHitMatrix::Display(CNcbiOstream & out)
{
    bool success = x_RenderImage();            
    if(success) {
        CImageIO::WriteImage(m_Context->GetBuffer(), out, m_Format);
    }
    return success;
}


bool CBlastHitMatrix::WriteToFile(void)
{
    bool success = x_RenderImage();
    if(success) {
        if(IsFileOut()) {
            CImageIO::WriteImage(m_Context->GetBuffer(), m_File, m_Format);
        }
        else {//netcache
            CNetCacheClient_LB nc_client("blast_hitmatrix", "NC_HitMatrix");
            m_ImageKey = nc_client.PutData(m_Context->GetBuffer().GetData(),  m_Width*m_Height*3);
        }
         
    }
    return success;
}


bool CBlastHitMatrix::x_RenderImage(void)
{
  bool success = false;
  try {
         x_PreProcess();

         m_Context.Reset(new CGlOsContext(m_Width, m_Height));
         m_Context->MakeCurrent();         

         x_Render();

         glFinish();
         m_Context->SetBuffer().SetDepth(3);
         CImageUtil::FlipY(m_Context->SetBuffer());
         success = true;
    }
    catch (CException& e) {        
        m_ErrorMessage = "Error rendering image:" + e.GetMsg();                        
    }
    catch (exception& e) {        
        m_ErrorMessage = "Error rendering image:" + (string)e.what();                        
    }
    catch (...) {        
        m_ErrorMessage = "Error rendering image: unknown error";                
    }    
    return success;
}

END_NCBI_SCOPE



