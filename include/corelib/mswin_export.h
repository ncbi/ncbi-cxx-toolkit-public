#ifndef CORELIB___MSWIN_EXPORT__HPP
#define CORELIB___MSWIN_EXPORT__HPP

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
 * Author:  Mike DiCuccio
 *
 * File Description:
 *    Defines to provide correct exporting from DLLs in Windows.
 *    These are necessary to compile DLLs with Visual C++ - exports must be
 *    explicitly labeled as such.
 */



#ifdef _LIB
#  undef NCBI_DLL_BUILD
#endif


#if defined(NCBI_OS_MSWIN)  &&  defined(NCBI_DLL_BUILD)

#ifndef _MSC_VER
#  error "This toolkit is not buildable with a compiler other than MSVC."
#endif


//
// Dumping ground for Windows-specific stuff
//
#pragma warning (disable : 4786 4251 4275)


//
// -------------------------------------------------
// DLL clusters
//


//
// Definitions for NCBI_CORE.DLL
//
#ifdef NCBI_CORE_EXPORTS
#  define NCBI_XCONNECT_EXPORTS
#  define NCBI_XNCBI_EXPORTS
#  define NCBI_XSERIAL_EXPORTS
#  define NCBI_XUTIL_EXPORTS
#endif


//
// Definitions for NCBI_PUB.DLL
//
#ifdef NCBI_PUB_EXPORTS
#  define NCBI_BIBLIO_EXPORTS
#  define NCBI_MEDLINE_EXPORTS
#  define NCBI_MEDLARS_EXPORTS
#  define NCBI_PUBMED_EXPORTS
#endif


//
// Definitions for NCBI_SEQ.DLL
//
#ifdef NCBI_SEQ_EXPORTS
#  define NCBI_ID1_EXPORTS
#  define NCBI_OBJPRT_EXPORTS
#  define NCBI_SEQALIGN_EXPORTS
#  define NCBI_SEQBLOCK_EXPORTS
#  define NCBI_SEQCODE_EXPORTS
#  define NCBI_SEQFEAT_EXPORTS
#  define NCBI_SEQLOC_EXPORTS
#  define NCBI_SEQRES_EXPORTS
#  define NCBI_SEQSET_EXPORTS
#  define NCBI_SUBMIT_EXPORTS
#  define NCBI_XALNMGR_EXPORTS
#  define NCBI_XOBJMGR_EXPORTS
#  define NCBI_XOBJUTIL_EXPORTS
#endif


//
// Definitions for NCBI_MISC.DLL
//
#ifdef NCBI_MISC_EXPORTS
#  define NCBI_ACCESS_EXPORTS
#  define NCBI_DOCSUM_EXPORTS
#  define NCBI_ENTREZ2_EXPORTS
#  define NCBI_FEATDEF_EXPORTS
#  define NCBI_GBSEQ_EXPORTS
#  define NCBI_MIM_EXPORTS
#  define NCBI_TINYSEQ_EXPORTS
#endif


//
// Definitions for NCBI_MMDB.DLL
//
#ifdef NCBI_MMDB_EXPORTS
#  define NCBI_CDD_EXPORTS
#  define NCBI_CN3D_EXPORTS
#  define NCBI_NCBIMIME_EXPORTS
#  define NCBI_MMDB1_EXPORTS
#  define NCBI_MMDB2_EXPORTS
#  define NCBI_MMDB3_EXPORTS
#endif


//
// Definitsions for GUI_UTILS.DLL
//
#ifdef NCBI_GUIUTILS_EXPORTS
#  define NCBI_GUIOPENGL_EXPORTS
#endif

// -------------------------------------------------

//
// Export specifier for library xncbi
//
#ifdef NCBI_XNCBI_EXPORTS
#  define NCBI_XNCBI_EXPORT         __declspec(dllexport)
#else
#  define NCBI_XNCBI_EXPORT         __declspec(dllimport)
#endif

//
// Export specifier for library xconnect
//
#ifdef NCBI_XCONNECT_EXPORTS
#  define NCBI_XCONNECT_EXPORT      __declspec(dllexport)
#else
#  define NCBI_XCONNECT_EXPORT      __declspec(dllimport)
#endif

//
// Export specifier for library xutil
//
#ifdef NCBI_XUTIL_EXPORTS
#  define NCBI_XUTIL_EXPORT         __declspec(dllexport)
#else
#  define NCBI_XUTIL_EXPORT         __declspec(dllimport)
#endif

//
// Export specifier for library xser
//
#ifdef NCBI_XSERIAL_EXPORTS
#  define NCBI_XSERIAL_EXPORT       __declspec(dllexport)
#else
#  define NCBI_XSERIAL_EXPORT       __declspec(dllimport)
#endif

//
// Export specifier for library objects_biblio
//
#ifdef NCBI_BIBLIO_EXPORTS
#  define NCBI_BIBLIO_EXPORT        __declspec(dllexport)
#else
#  define NCBI_BIBLIO_EXPORT        __declspec(dllimport)
#endif

//
// Export specifier for library objects_medline
//
#ifdef NCBI_MEDLINE_EXPORTS 
#  define NCBI_MEDLINE_EXPORT       __declspec(dllexport)
#else
#  define NCBI_MEDLINE_EXPORT       __declspec(dllimport)
#endif

//
// Export specifier for library objects_medlars
//
#ifdef NCBI_MEDLARS_EXPORTS
#  define NCBI_MEDLARS_EXPORT       __declspec(dllexport)
#else
#  define NCBI_MEDLARS_EXPORT       __declspec(dllimport)
#endif

//
// Export specifier for library objects_pub
//
#ifdef NCBI_PUB_EXPORTS
#  define NCBI_PUB_EXPORT           __declspec(dllexport)
#else
#  define NCBI_PUB_EXPORT           __declspec(dllimport)
#endif

//
// Export specifier for library objects_pubmed
//
#ifdef NCBI_PUBMED_EXPORTS
#  define NCBI_PUBMED_EXPORT        __declspec(dllexport)
#else
#  define NCBI_PUBMED_EXPORT        __declspec(dllimport)
#endif

//
// Export specifier for library objects_seq
//
#ifdef NCBI_SEQALIGN_EXPORTS
#  define NCBI_SEQALIGN_EXPORT      __declspec(dllexport)
#else
#  define NCBI_SEQALIGN_EXPORT      __declspec(dllimport)
#endif

#ifdef NCBI_SEQ_EXPORTS
#  define NCBI_SEQ_EXPORT           __declspec(dllexport)
#else
#  define NCBI_SEQ_EXPORT           __declspec(dllimport)
#endif


//
// Export specifier for library objects_seqcode
//
#ifdef NCBI_SEQCODE_EXPORTS
#  define NCBI_SEQCODE_EXPORT       __declspec(dllexport)
#else
#  define NCBI_SEQCODE_EXPORT       __declspec(dllimport)
#endif

//
// Export specifier for library objects_seqfeat
//
#ifdef NCBI_SEQFEAT_EXPORTS
#  define NCBI_SEQFEAT_EXPORT       __declspec(dllexport)
#else
#  define NCBI_SEQFEAT_EXPORT       __declspec(dllimport)
#endif

//
// Export specifier for library objects_seqloc
//
#ifdef NCBI_SEQLOC_EXPORTS
#  define NCBI_SEQLOC_EXPORT        __declspec(dllexport)
#else
#  define NCBI_SEQLOC_EXPORT        __declspec(dllimport)
#endif

//
// Export specifier for library objects_seqres
//
#ifdef NCBI_SEQRES_EXPORTS
#  define NCBI_SEQRES_EXPORT        __declspec(dllexport)
#else
#  define NCBI_SEQRES_EXPORT        __declspec(dllimport)
#endif

//
// Export specifier for library objects_seqset
//
#ifdef NCBI_SEQSET_EXPORTS
#  define NCBI_SEQSET_EXPORT        __declspec(dllexport)
#else
#  define NCBI_SEQSET_EXPORT        __declspec(dllimport)
#endif

//
// Export specifier for library objects_seqblock
//
#ifdef NCBI_SEQBLOCK_EXPORTS
#  define NCBI_SEQBLOCK_EXPORT      __declspec(dllexport)
#else
#  define NCBI_SEQBLOCK_EXPORT      __declspec(dllimport)
#endif

//
// Export specifier for library objects_id1
//
#ifdef NCBI_ID1_EXPORTS
#  define NCBI_ID1_EXPORT           __declspec(dllexport)
#else
#  define NCBI_ID1_EXPORT           __declspec(dllimport)
#endif

//
// Export specifier for library objects_xobjmgr
//
#ifdef NCBI_XOBJMGR_EXPORTS
#  define NCBI_XOBJMGR_EXPORT       __declspec(dllexport)
#else
#  define NCBI_XOBJMGR_EXPORT       __declspec(dllimport)
#endif

//
// Export specifier for library objects_xalnmgr
//
#ifdef NCBI_XALNMGR_EXPORTS
#  define NCBI_XALNMGR_EXPORT       __declspec(dllexport)
#else
#  define NCBI_XALNMGR_EXPORT       __declspec(dllimport)
#endif

//
// Export specifier for library objects_xobjutil
//
#ifdef NCBI_XOBJUTIL_EXPORTS
#  define NCBI_XOBJUTIL_EXPORT      __declspec(dllexport)
#else
#  define NCBI_XOBJUTIL_EXPORT      __declspec(dllimport)
#endif

//
// Export specifier for library objects_objprt
//
#ifdef NCBI_OBJPRT_EXPORTS
#  define NCBI_OBJPRT_EXPORT        __declspec(dllexport)
#else
#  define NCBI_OBJPRT_EXPORT        __declspec(dllimport)
#endif

//
// Export specifier for library objects_featdef
//
#ifdef NCBI_FEATDEF_EXPORTS
#  define NCBI_FEATDEF_EXPORT       __declspec(dllexport)
#else
#  define NCBI_FEATDEF_EXPORT       __declspec(dllimport)
#endif


//
// Export specifier for library objects_submit
//
#ifdef NCBI_SUBMIT_EXPORTS
#  define NCBI_SUBMIT_EXPORT        __declspec(dllexport)
#else
#  define NCBI_SUBMIT_EXPORT        __declspec(dllimport)
#endif


//
// Export specifier for library objects_mim
//
#ifdef NCBI_MIM_EXPORTS
#  define NCBI_MIM_EXPORT           __declspec(dllexport)
#else
#  define NCBI_MIM_EXPORT           __declspec(dllimport)
#endif

//
// Export specifier for library objects_tinyseq
//
#ifdef NCBI_TINYSEQ_EXPORTS
#  define NCBI_TINYSEQ_EXPORT       __declspec(dllexport)
#else
#  define NCBI_TINYSEQ_EXPORT       __declspec(dllimport)
#endif

//
// Export specifier for library objects_access
//
#ifdef NCBI_ACCESS_EXPORTS
#  define NCBI_ACCESS_EXPORT        __declspec(dllexport)
#else
#  define NCBI_ACCESS_EXPORT        __declspec(dllimport)
#endif

//
// Export specifier for library objects_docsum
//
#ifdef NCBI_DOCSUM_EXPORTS
#  define NCBI_DOCSUM_EXPORT        __declspec(dllexport)
#else
#  define NCBI_DOCSUM_EXPORT        __declspec(dllimport)
#endif

//
// Export specifier for library objects_entrez2
//
#ifdef NCBI_ENTREZ2_EXPORTS
#  define NCBI_ENTREZ2_EXPORT       __declspec(dllexport)
#else
#  define NCBI_ENTREZ2_EXPORT       __declspec(dllimport)
#endif

//
// Export specifier for library objects_gbseq
//
#ifdef NCBI_GBSEQ_EXPORTS
#  define NCBI_GBSEQ_EXPORT         __declspec(dllexport)
#else
#  define NCBI_GBSEQ_EXPORT         __declspec(dllimport)
#endif

//
// Export specifier for library objects_mmdb1
//
#ifdef NCBI_MMDB1_EXPORTS
#  define NCBI_MMDB1_EXPORT         __declspec(dllexport)
#else
#  define NCBI_MMDB1_EXPORT         __declspec(dllimport)
#endif

//
// Export specifier for library objects_mmdb2
//
#ifdef NCBI_MMDB2_EXPORTS
#  define NCBI_MMDB2_EXPORT         __declspec(dllexport)
#else
#  define NCBI_MMDB2_EXPORT         __declspec(dllimport)
#endif

//
// Export specifier for library objects_mmdb3
//
#ifdef NCBI_MMDB3_EXPORTS
#  define NCBI_MMDB3_EXPORT         __declspec(dllexport)
#else
#  define NCBI_MMDB3_EXPORT         __declspec(dllimport)
#endif

//
// Export specifier for library objects_cn3d
//
#ifdef NCBI_CN3D_EXPORTS
#  define NCBI_CN3D_EXPORT          __declspec(dllexport)
#else
#  define NCBI_CN3D_EXPORT          __declspec(dllimport)
#endif

//
// Export specifier for library objects_cdd
//
#ifdef NCBI_CDD_EXPORTS
#  define NCBI_CDD_EXPORT           __declspec(dllexport)
#else
#  define NCBI_CDD_EXPORT           __declspec(dllimport)
#endif

//
// Export specifier for library objects_mime
//
#ifdef NCBI_NCBIMIME_EXPORTS
#  define NCBI_NCBIMIME_EXPORT      __declspec(dllexport)
#else
#  define NCBI_NCBIMIME_EXPORT      __declspec(dllimport)
#endif

//
// Export specifier for library objects_general
//
#ifdef NCBI_GENERAL_EXPORTS
#  define NCBI_GENERAL_EXPORT       __declspec(dllexport)
#else
#  define NCBI_GENERAL_EXPORT       __declspec(dllimport)
#endif

//
// Export specifier for library dbapi_driver
//
#ifdef NCBI_DBAPIDRIVER_EXPORTS
#  define NCBI_DBAPIDRIVER_EXPORT   __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_EXPORT   __declspec(dllimport)
#endif

//
// Export specifier for library gui_core
//
#ifdef NCBI_GUICORE_EXPORTS
#  define NCBI_GUICORE_EXPORT       __declspec(dllexport)
#else
#  define NCBI_GUICORE_EXPORT       __declspec(dllimport)
#endif

//
// Export specifier for library gui_utils
//
#ifdef NCBI_GUIUTILS_EXPORTS
#  define NCBI_GUIUTILS_EXPORT      __declspec(dllexport)
#else
#  define NCBI_GUIUTILS_EXPORT      __declspec(dllimport)
#endif

//
// Export specifier for library gui_opengl
//
#ifdef NCBI_GUIOPENGL_EXPORTS
#  define NCBI_GUIOPENGL_EXPORT     __declspec(dllexport)
#else
#  define NCBI_GUIOPENGL_EXPORT     __declspec(dllimport)
#endif

//
// Export specifier for library gui_widgets
//
#ifdef NCBI_GUIWIDGETS_EXPORTS
#  define NCBI_GUIWIDGETS_EXPORT    __declspec(dllexport)
#else
#  define NCBI_GUIWIDGETS_EXPORT    __declspec(dllimport)
#endif



#else  //  !defined(NCBI_OS_MSWIN)  ||  !defined(NCBI_DLL_BUILD)

//
// NULL operations for other cases
//

#  define NCBI_ACCESS_EXPORT
#  define NCBI_BIBLIO_EXPORT
#  define NCBI_CDD_EXPORT
#  define NCBI_CN3D_EXPORT
#  define NCBI_DBAPIDRIVER_EXPORT
#  define NCBI_DOCSUM_EXPORT
#  define NCBI_ENTREZ2_EXPORT
#  define NCBI_FEATDEF_EXPORT
#  define NCBI_GBSEQ_EXPORT
#  define NCBI_GENERAL_EXPORT
#  define NCBI_GUICORE_EXPORT
#  define NCBI_GUIOPENGL_EXPORT
#  define NCBI_GUIUTILS_EXPORT
#  define NCBI_GUIWIDGETS_EXPORT
#  define NCBI_ID1_EXPORT
#  define NCBI_MEDLARS_EXPORT
#  define NCBI_MEDLINE_EXPORT
#  define NCBI_MIM_EXPORT
#  define NCBI_MMDB1_EXPORT
#  define NCBI_MMDB2_EXPORT
#  define NCBI_MMDB3_EXPORT
#  define NCBI_NCBIMIME_EXPORT
#  define NCBI_OBJPRT_EXPORT
#  define NCBI_PUBMED_EXPORT
#  define NCBI_PUB_EXPORT
#  define NCBI_SEQALIGN_EXPORT
#  define NCBI_SEQBLOCK_EXPORT
#  define NCBI_SEQCODE_EXPORT
#  define NCBI_SEQFEAT_EXPORT
#  define NCBI_SEQLOC_EXPORT
#  define NCBI_SEQRES_EXPORT
#  define NCBI_SEQSET_EXPORT
#  define NCBI_SEQUENCE_EXPORT
#  define NCBI_SEQ_EXPORT
#  define NCBI_TINYSEQ_EXPORT
#  define NCBI_XALNMGR_EXPORT
#  define NCBI_XCONNECT_EXPORT
#  define NCBI_XNCBI_EXPORT
#  define NCBI_XOBJMGR_EXPORT
#  define NCBI_XOBJUTIL_EXPORT
#  define NCBI_XSERIAL_EXPORT
#  define NCBI_XUTIL_EXPORT

#endif


#endif  //  CORELIB___MSWIN_EXPORT__HPP

/*
 * ==========================================================================
 * $Log$
 * Revision 1.3  2002/12/31 15:08:23  dicuccio
 * Moved featdef and gbseq into ncbi_misc.dll
 *
 * Revision 1.2  2002/12/26 12:51:41  dicuccio
 * Fixed some minor niggling errors with export specifiers in the wrong places.
 *
 * Revision 1.1  2002/12/18 22:52:19  dicuccio
 * Initial revision.
 *
 * ==========================================================================
 */
