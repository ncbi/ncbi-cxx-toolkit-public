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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *    CImageIOSgi -- interface class for reading/writing SGI RGB files
 */

#include <ncbi_pch.hpp>
#include "image_io_sgi.hpp"
#include <util/image/image.hpp>
#include <util/image/image_exception.hpp>

BEGIN_NCBI_SCOPE

CImage* CImageIOSgi::ReadImage(CNcbiIstream&)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOSgi::ReadImage(): SGI format read unimplemented");
}

CImage* CImageIOSgi::ReadImage(CNcbiIstream&,
                               size_t, size_t, size_t, size_t)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOSgi::ReadImage(): SGI format partial "
               "read unimplemented");
}

void CImageIOSgi::WriteImage(const CImage&, CNcbiOstream&,
                             CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOSgi::WriteImage(): SGI format write unimplemented");
}

void CImageIOSgi::WriteImage(const CImage&, CNcbiOstream&,
                             size_t, size_t, size_t, size_t,
                             CImageIO::ECompress)
{
    NCBI_THROW(CImageException, eUnsupported,
               "CImageIOSgi::WriteImage(): SGI format partial "
               "write unimplemented");
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/05/17 21:07:58  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/12/16 15:49:37  dicuccio
 * Large re-write of image handling.  Added improved error-handling and support
 * for streams-based i/o (via hooks into each client library).
 *
 * Revision 1.2  2003/11/03 15:19:57  dicuccio
 * Added optional compression parameter
 *
 * Revision 1.1  2003/06/03 15:17:13  dicuccio
 * Initial revision of image library
 *
 * ===========================================================================
 */

