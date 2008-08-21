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
#include <syslog.h>
#include <corelib/ncbistl.hpp>
#include <cgi/cgictx.hpp>
#include <serial/serial.hpp>    
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include "blast_hitmatrix.hpp"


USING_NCBI_SCOPE;



class CBlastHitMatrixCGIException : public CException
{
public:
    enum EErrCode   {
        eInvalidSeqAnnot, 
        eImageRenderError,
        eCurrentException
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch(GetErrCode())    {        
        case eInvalidSeqAnnot: return "eInvalidSeqAnnot";
        case eImageRenderError: return "eImageRenderError";        
        default:    return CException::GetErrCodeString();
        };
    }

    NCBI_EXCEPTION_DEFAULT(CBlastHitMatrixCGIException, CException);
};

/////////////////////////////////////////////////////////////////////////////
///  CBlastHitMatrixCGIApplication
/// This CGI loads an accession from GenBank, extract a Seq-annot from the
/// sequence and renders a pairwise alignment between the first two Seq-id
/// in the alignment

class CBlastHitMatrixCGIApplication : public CCgiApplication
{
public:
    CBlastHitMatrixCGIApplication();

    
    virtual int     ProcessRequest(CCgiContext &ctx);

protected:	
    void    x_GetSeqAnnot(CCgiContext& ctx);
    void    x_GetCGIContextParams(CCgiContext &ctx);    
    void    x_InitAppData(CCgiContext& ctx);

private:    

    CRef <CSeq_annot>       m_Annot;
    
    

    string                  m_File;
    string                  m_RID;
    string                  m_Thumbnail;  //Indicated that thumbnail image should be used

    CBlastHitMatrix         *m_BlastHitMatrix;
    int                     m_Width;
    int                     m_Height;

};




CBlastHitMatrixCGIApplication::CBlastHitMatrixCGIApplication()
{
}

string s_GetRequestParam(const TCgiEntries &entries,const string &paramName)
{
    string param = "";

    TCgiEntries::const_iterator iter  = entries.find(paramName);
    
    if (iter != entries.end()){
        param = iter->second;        
    }  
    return param;
}

void CBlastHitMatrixCGIApplication::x_GetCGIContextParams(CCgiContext &ctx)
{
    const CCgiRequest& request  = ctx.GetRequest();
    const TCgiEntries& entries = request.GetEntries();

    m_RID = s_GetRequestParam(entries,"rid");

    string paramVal = s_GetRequestParam(entries,"width");    
    
    m_Width = !paramVal.empty() ? NStr::StringToInt(paramVal) : 800;
    
    paramVal = s_GetRequestParam(entries,"height");    
    
    m_Height = !paramVal.empty() ? NStr::StringToInt(paramVal) : 600;
      
    //Indicates that image should output in the file
    m_File = s_GetRequestParam(entries,"file");        

    m_Thumbnail = s_GetRequestParam(entries,"thumbnail");    
}


void CBlastHitMatrixCGIApplication::x_GetSeqAnnot(CCgiContext& ctx)
{
        const CNcbiRegistry & reg = ctx.GetConfig();
        string blastURL = reg.Get("NetParams", "BlastURL");
        string url = (string)blastURL + "?CMD=Get&RID=" + m_RID + "&FORMAT_TYPE=ASN.1&FORMAT_OBJECT=Alignment";
        
        SConnNetInfo* net_info = ConnNetInfo_Create(NULL);
        // create HTTP connection
        CConn_HttpStream inHttp(url,net_info);   

        try {
            m_Annot.Reset(new CSeq_annot);
            auto_ptr<CObjectIStream> is
                    (CObjectIStream::Open(eSerial_AsnText, inHttp));
            *is >> *m_Annot;                                    
        }
         catch (CException& e) {
              m_Annot.Reset();              
              NCBI_THROW(CBlastHitMatrixCGIException, eInvalidSeqAnnot, "Exception reading SeqAnnot via url " + url + ", exception message: " + e.GetMsg());              
        }               
}

//Use it for testing
CRef <CSeq_annot> createAnnot(string rid)
{
        CRef <CSeq_annot> m_Annot(new CSeq_annot);

        string path_cgi = CNcbiApplication::Instance()->GetProgramExecutablePath();
        string path_base = CDirEntry(path_cgi).GetDir();  
        string ASN1FilePath  = CDirEntry::MakePath(path_base, "testSeqAnnot.txt");

        LOG_POST(path_cgi);
        LOG_POST(path_base);
        LOG_POST(ASN1FilePath);
        
        try {
            auto_ptr<CObjectIStream> in
             (CObjectIStream::Open(eSerial_AsnText,ASN1FilePath));
            *in >> *m_Annot;        
        }
        catch (CException& e) {
            cerr << "Exception reading SeqAnnot exception message: " + e.GetMsg() << endl;                        
        }
        cerr << MSerial_AsnText << *m_Annot;
        return m_Annot;
}


void CBlastHitMatrixCGIApplication::x_InitAppData(CCgiContext& ctx)
{
    if(!m_RID.empty()) {           
        x_GetSeqAnnot(ctx);   
        m_BlastHitMatrix = new CBlastHitMatrix((*m_Annot).GetData().GetAlign(),m_Height, m_Width,CImageIO::ePng);
        if(!m_File.empty()) {
            m_BlastHitMatrix->SetFileName(m_File);
        }
        if(!m_Thumbnail.empty()) {
            m_BlastHitMatrix->SetThumbnail(true);
        }
    }
}

int CBlastHitMatrixCGIApplication::ProcessRequest(CCgiContext &ctx)
{
    // retrieve our CGI rendering params
    x_GetCGIContextParams(ctx);            
    x_InitAppData(ctx);        
        
    bool success = true;       
    if(m_BlastHitMatrix->IsFileOut()) {
        success = m_BlastHitMatrix->WriteToFile();
    }
    else {        
        string encoding("image/png");
        CCgiResponse& response = ctx.GetResponse();
        response.SetContentType(encoding);
        response.WriteHeader();        
        success = m_BlastHitMatrix->Display(response.out());
    }
    if(!success) {
        NCBI_THROW(CBlastHitMatrixCGIException, eImageRenderError, "Exception occured, exception message: " + m_BlastHitMatrix->GetErrorMessage()); 
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    //Applog
    SetSplitLogFile(true);
    GetDiagContext().SetOldPostFormat(false);

    openlog("hitmtrix", LOG_PID, LOG_LOCAL7);

    int result = CBlastHitMatrixCGIApplication().AppMain(argc, argv, 0, eDS_Default, "");    
    _TRACE("back to normal diags");
    return result;
}
