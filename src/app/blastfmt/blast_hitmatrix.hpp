#ifndef OBJTOOLS_BLAST_FORMAT___BLASTHITMATRIX_HPP
#define OBJTOOLS_BLAST_FORMAT___BLASTHITMATRIX_HPP
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
 */

/** @file blast_hitmatrix.hpp
 * Declares class to display hitmatrix image view for blast 2 seq 
 */
#include <cgi/cgictx.hpp>

#include <gui/opengl/mesa/glcgi_image.hpp>
#include <gui/opengl/mesa/gloscontext.hpp>
#include <gui/opengl/glutils.hpp>
#include <gui/opengl/glpane.hpp>
#include <gui/objutils/label.hpp>

#include <objmgr/object_manager.hpp>

#include <gui/widgets/hit_matrix/hit_matrix_renderer.hpp>
#include <gui/widgets/hit_matrix/hit_matrix_ds_builder.hpp>

/** @addtogroup BlastFormatting
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

///This class displays the image of the hitmatrix view for blast 2 seq results
///
///Usage:
///blastHitMatrix = new CBlastHitMatrix(...);
///If displasy on the screen
///     blastHitMatrix->Dispaly(out)
///If write to the file:
///     blastHitMatrix->SetFileName(<filename>) 
///     blastHitMatrix->WriteToFile()

/**
 * This class displays the defline for BLAST result.
 *
 * Example:
 * @code
 * blastHitMatrix = new CBlastHitMatrix(...);
 * If display on the screen:
 *      string encoding("image/png");
 *      CCgiResponse& response = ctx.GetResponse();
 *      response.SetContentType(encoding);
 *      response.WriteHeader();
 *      blastHitMatrix->Display(response.out()); 
 * If write to the file:
 *      blastHitMatrix->SetFileName(<filename>) 
 *      blastHitMatrix->WriteToFile() 
 * @endcode
 */
class NCBI_XBLASTFORMAT_EXPORT CBlastHitMatrix
{
public:
    ///Constructor
    ///
    ///@param seqAligns: input seqalign list
    ///@param height: image height
    ///@param width: image width
    ///@param format: image type (png, bmp etc)    
    CBlastHitMatrix(const list< CRef< CSeq_align > > &seqAligns,
                                 int height = 600,
                                 int width = 800,
                                 CImageIO::EType format = CImageIO::ePng);
    ///Destructor    
    ~CBlastHitMatrix(){};
    
    ///Inits file name if image is written to the file
    ///
    ///@param fileName: file name for image output
    void SetFileName(string fileName) {m_File = fileName;m_FileOut = true;}
    
    ///Checks if image is to be written to the file
    ///
    ///@return : true if image is to be written to the file
    bool IsFileOut(void){return  m_FileOut;}

    ///Get netcache ID for the image stored in netcache
    ///
    ///@return : string netcache ID
    string GetNetcacheID(void) {return m_ImageKey;}

    ///Get error message
    ///
    ///@return : string error message
    string GetErrorMessage(void) {return m_ErrorMessage;}

    ///Outputs the image into CNcbiOstream
    ///
    ///@param out: stream to output
    ///@return : true if successful
    bool Display(CNcbiOstream & out);

    ///Outputs the image into the file (m_FileOut=true) or netcache
    ///    
    ///@return : true if successful
    bool WriteToFile(void);   
    
protected:
    ///Initializes Object Manager    
	void	x_InitObjectManager();

    ///Initializes CGlPane    
	void	x_InitPort();	    

    ///Creates Query and subject labels info    
    void    x_GetLabels(void);
    
    
    ///Renders a pairwise alignments between the first two Seq-id in the alignment
    bool    x_RenderImage(void); 

    ///Performs pre-processing for image rendering    
    void    x_PreProcess(void);

    ///Inits renderer display options and text labels
    void    x_Render(void);

private:    
    ///Object manager
    CRef<CObjectManager>    m_ObjMgr;

    ///Current scope
    CRef<CScope>            m_Scope;
    
    ///Vector of seqaligns    
    vector< CConstRef<CSeq_align> > m_Aligns;

    ///Query label id
    string                  m_QueryID; 

    ///Subject label id
    string                  m_SubjectID;

    ///File name
    string                  m_File;

    ///true if output to the file
    bool                    m_FileOut;

    ///Image height
    int                     m_Height;

    ///Image width
    int                     m_Width;

    ///Image format (png,bmp etc)
    CImageIO::EType         m_Format;    

    ///netcacheID 
    string                   m_ImageKey;    

    ///Error message
    string                  m_ErrorMessage;

    /// Renderer setup Parameter
    CIRef<IHitMatrixDataSource> m_DataSource;

    /// Renderer setup Parameter
    CGlPane m_Port;

    /// Renderer setup Parameter
    CHitMatrixRenderer   m_Renderer;

    ///CGlOsContext context
    CRef <CGlOsContext> m_Context;
};

END_SCOPE(objects)
END_NCBI_SCOPE

/* @} */

#endif /* OBJTOOLS_BLAST_FORMAT___BLASTHITMATRIX_HPP */

