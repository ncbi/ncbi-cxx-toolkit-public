#ifndef CORELIB___MSWIN_EXPORT__H
#define CORELIB___MSWIN_EXPORT__H

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


/*
 * Dumping ground for Windows-specific stuff
 */
#pragma warning (disable : 4786 4251 4275)


/*
 * -------------------------------------------------
 * DLL clusters
 */


/*
 * Definitions for NCBI_CORE.DLL
 */
#ifdef NCBI_CORE_EXPORTS
#  define NCBI_XNCBI_EXPORTS
#  define NCBI_XSERIAL_EXPORTS
#  define NCBI_XUTIL_EXPORTS
#endif


/*
 * Definitions for NCBI_PUB.DLL
 */
#ifdef NCBI_PUB_EXPORTS
#  define NCBI_BIBLIO_EXPORTS
#  define NCBI_MEDLINE_EXPORTS
#  define NCBI_MEDLARS_EXPORTS
#  define NCBI_MLA_EXPORTS
#  define NCBI_PUBMED_EXPORTS
#endif


/*
 * Definitions for NCBI_SEQ.DLL
 */
#ifdef NCBI_SEQ_EXPORTS
#  define NCBI_BLAST_EXPORTS
#  define NCBI_BLASTDB_EXPORTS
#  define NCBI_ID1_EXPORTS
#  define NCBI_ID2_EXPORTS
#  define NCBI_SCOREMAT_EXPORTS
#  define NCBI_SEQALIGN_EXPORTS
#  define NCBI_SEQBLOCK_EXPORTS
#  define NCBI_SEQCODE_EXPORTS
#  define NCBI_SEQFEAT_EXPORTS
#  define NCBI_SEQLOC_EXPORTS
#  define NCBI_SEQRES_EXPORTS
#  define NCBI_SEQSET_EXPORTS
#  define NCBI_SUBMIT_EXPORTS
#endif


/*
 * Definitions for NCBI_SEQEXT.DLL
 */
#ifdef NCBI_SEQEXT_EXPORTS
#  define NCBI_FLAT_EXPORTS
#  define NCBI_XALNMGR_EXPORTS
#  define NCBI_XALNUTIL_EXPORTS
#  define NCBI_XOBJMGR_EXPORTS
#  define NCBI_XOBJREAD_EXPORTS
#  define NCBI_XOBJUTIL_EXPORTS
#endif


/*
 * Definitions for NCBI_MISC.DLL
 */
#ifdef NCBI_MISC_EXPORTS
#  define NCBI_ACCESS_EXPORTS
#  define NCBI_DOCSUM_EXPORTS
#  define NCBI_ENTREZ2_EXPORTS
#  define NCBI_FEATDEF_EXPORTS
#  define NCBI_GBSEQ_EXPORTS
#  define NCBI_MIM_EXPORTS
#  define NCBI_OBJPRT_EXPORTS
#  define NCBI_TINYSEQ_EXPORTS
#endif


/*
 * Definitions for NCBI_MMDB.DLL
 */
#ifdef NCBI_MMDB_EXPORTS
#  define NCBI_CDD_EXPORTS
#  define NCBI_CN3D_EXPORTS
#  define NCBI_MMDB1_EXPORTS
#  define NCBI_MMDB2_EXPORTS
#  define NCBI_MMDB3_EXPORTS
#  define NCBI_NCBIMIME_EXPORTS
#endif

/*
 * Definitions for NCBI_ALGO.DLL
 */
#ifdef NCBI_XALGO_EXPORTS
#  define NCBI_XALGOALIGN_EXPORTS
#  define NCBI_XALGOSEQ_EXPORTS
#  define NCBI_XBLAST_EXPORTS
#endif

/*
 * Definitsions for GUI_UTILS.DLL
 */
#ifdef NCBI_GUIUTILS_EXPORTS
#  define NCBI_GUIOPENGL_EXPORTS
#endif


/*
 * Definitsions for GUI_CORE.DLL
 */
#ifdef NCBI_GUICORE_EXPORTS
#  define NCBI_XGBPLUGIN_EXPORTS
#endif


/* ------------------------------------------------- */

/*
 * Export specifier for library xncbi
 */
#ifdef NCBI_XNCBI_EXPORTS
#  define NCBI_XNCBI_EXPORT         __declspec(dllexport)
#else
#  define NCBI_XNCBI_EXPORT         __declspec(dllimport)
#endif

/*
 * Export specifier for library xconnect:
 * included from <connect/connect_export.h>
 */


/*
 * Export specifier for library xutil
 */
#ifdef NCBI_XUTIL_EXPORTS
#  define NCBI_XUTIL_EXPORT         __declspec(dllexport)
#else
#  define NCBI_XUTIL_EXPORT         __declspec(dllimport)
#endif

/*
 * Export specifier for library ximage
 */
#ifdef NCBI_XIMAGE_EXPORTS
#  define NCBI_XIMAGE_EXPORT       __declspec(dllexport)
#else
#  define NCBI_XIMAGE_EXPORT       __declspec(dllimport)
#endif

 
/*
 * Export specifier for library xser
 */
#ifdef NCBI_XSERIAL_EXPORTS
#  define NCBI_XSERIAL_EXPORT       __declspec(dllexport)
#else
#  define NCBI_XSERIAL_EXPORT       __declspec(dllimport)
#endif

/*
 * Export specifier for library xalgoalign
 */
#ifdef NCBI_XALGOALIGN_EXPORTS
#  define NCBI_XALGOALIGN_EXPORT    __declspec(dllexport)
#else
#  define NCBI_XALGOALIGN_EXPORT    __declspec(dllimport)
#endif

/*
 * Export specifier for library xalgoseq
 */
#ifdef NCBI_XALGOSEQ_EXPORTS
#  define NCBI_XALGOSEQ_EXPORT      __declspec(dllexport)
#else
#  define NCBI_XALGOSEQ_EXPORT      __declspec(dllimport)
#endif

/*
 * Export specifier for library xblast
 */
#ifdef NCBI_XBLAST_EXPORTS
#  define NCBI_XBLAST_EXPORT         __declspec(dllexport)
#else
#  define NCBI_XBLAST_EXPORT         __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_flat
 */
#ifdef NCBI_FLAT_EXPORTS
#  define NCBI_FLAT_EXPORT          __declspec(dllexport)
#else
#  define NCBI_FLAT_EXPORT          __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_biblio
 */
#ifdef NCBI_BIBLIO_EXPORTS
#  define NCBI_BIBLIO_EXPORT        __declspec(dllexport)
#else
#  define NCBI_BIBLIO_EXPORT        __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_blast
 */
#ifdef NCBI_BLAST_EXPORTS
#  define NCBI_BLAST_EXPORT         __declspec(dllexport)
#else
#  define NCBI_BLAST_EXPORT         __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_blastdb
 */
#ifdef NCBI_BLASTDB_EXPORTS
#  define NCBI_BLASTDB_EXPORT         __declspec(dllexport)
#else
#  define NCBI_BLASTDB_EXPORT         __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_scoremat
 */
#ifdef NCBI_SCOREMAT_EXPORTS
#  define NCBI_SCOREMAT_EXPORT      __declspec(dllexport)
#else
#  define NCBI_SCOREMAT_EXPORT      __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_medline
 */
#ifdef NCBI_MEDLINE_EXPORTS
#  define NCBI_MEDLINE_EXPORT       __declspec(dllexport)
#else
#  define NCBI_MEDLINE_EXPORT       __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_medlars
 */
#ifdef NCBI_MEDLARS_EXPORTS
#  define NCBI_MEDLARS_EXPORT       __declspec(dllexport)
#else
#  define NCBI_MEDLARS_EXPORT       __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_mla
 */
#ifdef NCBI_MLA_EXPORTS
#  define NCBI_MLA_EXPORT           __declspec(dllexport)
#else
#  define NCBI_MLA_EXPORT           __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_pub
 */
#ifdef NCBI_PUB_EXPORTS
#  define NCBI_PUB_EXPORT           __declspec(dllexport)
#else
#  define NCBI_PUB_EXPORT           __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_pubmed
 */
#ifdef NCBI_PUBMED_EXPORTS
#  define NCBI_PUBMED_EXPORT        __declspec(dllexport)
#else
#  define NCBI_PUBMED_EXPORT        __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_seqalign
 */
#ifdef NCBI_SEQALIGN_EXPORTS
#  define NCBI_SEQALIGN_EXPORT      __declspec(dllexport)
#else
#  define NCBI_SEQALIGN_EXPORT      __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_seq
 */
#ifdef NCBI_SEQ_EXPORTS
#  define NCBI_SEQ_EXPORT           __declspec(dllexport)
#else
#  define NCBI_SEQ_EXPORT           __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_seqcode
 */
#ifdef NCBI_SEQCODE_EXPORTS
#  define NCBI_SEQCODE_EXPORT       __declspec(dllexport)
#else
#  define NCBI_SEQCODE_EXPORT       __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_seqfeat
 */
#ifdef NCBI_SEQFEAT_EXPORTS
#  define NCBI_SEQFEAT_EXPORT       __declspec(dllexport)
#else
#  define NCBI_SEQFEAT_EXPORT       __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_seqloc
 */
#ifdef NCBI_SEQLOC_EXPORTS
#  define NCBI_SEQLOC_EXPORT        __declspec(dllexport)
#else
#  define NCBI_SEQLOC_EXPORT        __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_seqres
 */
#ifdef NCBI_SEQRES_EXPORTS
#  define NCBI_SEQRES_EXPORT        __declspec(dllexport)
#else
#  define NCBI_SEQRES_EXPORT        __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_seqset
 */
#ifdef NCBI_SEQSET_EXPORTS
#  define NCBI_SEQSET_EXPORT        __declspec(dllexport)
#else
#  define NCBI_SEQSET_EXPORT        __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_seqblock
 */
#ifdef NCBI_SEQBLOCK_EXPORTS
#  define NCBI_SEQBLOCK_EXPORT      __declspec(dllexport)
#else
#  define NCBI_SEQBLOCK_EXPORT      __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_id1
 */
#ifdef NCBI_ID1_EXPORTS
#  define NCBI_ID1_EXPORT           __declspec(dllexport)
#else
#  define NCBI_ID1_EXPORT           __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_id2
 */
#ifdef NCBI_ID2_EXPORTS
#  define NCBI_ID2_EXPORT           __declspec(dllexport)
#else
#  define NCBI_ID2_EXPORT           __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_xobjmgr
 */
#ifdef NCBI_XOBJMGR_EXPORTS
#  define NCBI_XOBJMGR_EXPORT       __declspec(dllexport)
#else
#  define NCBI_XOBJMGR_EXPORT       __declspec(dllimport)
#endif

/*
 * Export specifier for library objtools_xobjread
 */
#ifdef NCBI_XOBJREAD_EXPORTS
#  define NCBI_XOBJREAD_EXPORT      __declspec(dllexport)
#else
#  define NCBI_XOBJREAD_EXPORT      __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_xalnmgr
 */
#ifdef NCBI_XALNMGR_EXPORTS
#  define NCBI_XALNMGR_EXPORT       __declspec(dllexport)
#else
#  define NCBI_XALNMGR_EXPORT       __declspec(dllimport)
#endif

#ifdef NCBI_XALNUTIL_EXPORTS
#  define NCBI_XALNUTIL_EXPORT       __declspec(dllexport)
#else
#  define NCBI_XALNUTIL_EXPORT       __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_xobjutil
 */
#ifdef NCBI_XOBJUTIL_EXPORTS
#  define NCBI_XOBJUTIL_EXPORT      __declspec(dllexport)
#else
#  define NCBI_XOBJUTIL_EXPORT      __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_objprt
 */
#ifdef NCBI_OBJPRT_EXPORTS
#  define NCBI_OBJPRT_EXPORT        __declspec(dllexport)
#else
#  define NCBI_OBJPRT_EXPORT        __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_featdef
 */
#ifdef NCBI_FEATDEF_EXPORTS
#  define NCBI_FEATDEF_EXPORT       __declspec(dllexport)
#else
#  define NCBI_FEATDEF_EXPORT       __declspec(dllimport)
#endif


/*
 * Export specifier for library objects_submit
 */
#ifdef NCBI_SUBMIT_EXPORTS
#  define NCBI_SUBMIT_EXPORT        __declspec(dllexport)
#else
#  define NCBI_SUBMIT_EXPORT        __declspec(dllimport)
#endif


/*
 * Export specifier for library objects_taxon1
 */
#ifdef NCBI_TAXON1_EXPORTS
#  define NCBI_TAXON1_EXPORT        __declspec(dllexport)
#else
#  define NCBI_TAXON1_EXPORT        __declspec(dllimport)
#endif


/*
 * Export specifier for library objects_mim
 */
#ifdef NCBI_MIM_EXPORTS
#  define NCBI_MIM_EXPORT           __declspec(dllexport)
#else
#  define NCBI_MIM_EXPORT           __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_tinyseq
 */
#ifdef NCBI_TINYSEQ_EXPORTS
#  define NCBI_TINYSEQ_EXPORT       __declspec(dllexport)
#else
#  define NCBI_TINYSEQ_EXPORT       __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_access
 */
#ifdef NCBI_ACCESS_EXPORTS
#  define NCBI_ACCESS_EXPORT        __declspec(dllexport)
#else
#  define NCBI_ACCESS_EXPORT        __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_docsum
 */
#ifdef NCBI_DOCSUM_EXPORTS
#  define NCBI_DOCSUM_EXPORT        __declspec(dllexport)
#else
#  define NCBI_DOCSUM_EXPORT        __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_entrez2
 */
#ifdef NCBI_ENTREZ2_EXPORTS
#  define NCBI_ENTREZ2_EXPORT       __declspec(dllexport)
#else
#  define NCBI_ENTREZ2_EXPORT       __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_entrezgene
 */
#ifdef NCBI_ENTREZGENE_EXPORTS
#  define NCBI_ENTREZGENE_EXPORT    __declspec(dllexport)
#else
#  define NCBI_ENTREZGENE_EXPORT    __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_gbseq
 */
#ifdef NCBI_GBSEQ_EXPORTS
#  define NCBI_GBSEQ_EXPORT         __declspec(dllexport)
#else
#  define NCBI_GBSEQ_EXPORT         __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_mmdb1
 */
#ifdef NCBI_MMDB1_EXPORTS
#  define NCBI_MMDB1_EXPORT         __declspec(dllexport)
#else
#  define NCBI_MMDB1_EXPORT         __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_mmdb2
 */
#ifdef NCBI_MMDB2_EXPORTS
#  define NCBI_MMDB2_EXPORT         __declspec(dllexport)
#else
#  define NCBI_MMDB2_EXPORT         __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_mmdb3
 */
#ifdef NCBI_MMDB3_EXPORTS
#  define NCBI_MMDB3_EXPORT         __declspec(dllexport)
#else
#  define NCBI_MMDB3_EXPORT         __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_cn3d
 */
#ifdef NCBI_CN3D_EXPORTS
#  define NCBI_CN3D_EXPORT          __declspec(dllexport)
#else
#  define NCBI_CN3D_EXPORT          __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_cdd
 */
#ifdef NCBI_CDD_EXPORTS
#  define NCBI_CDD_EXPORT           __declspec(dllexport)
#else
#  define NCBI_CDD_EXPORT           __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_mime
 */
#ifdef NCBI_NCBIMIME_EXPORTS
#  define NCBI_NCBIMIME_EXPORT      __declspec(dllexport)
#else
#  define NCBI_NCBIMIME_EXPORT      __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_general
 */
#ifdef NCBI_GENERAL_EXPORTS
#  define NCBI_GENERAL_EXPORT       __declspec(dllexport)
#else
#  define NCBI_GENERAL_EXPORT       __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_driver
 */
#ifdef NCBI_DBAPIDRIVER_EXPORTS
#  define NCBI_DBAPIDRIVER_EXPORT   __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_EXPORT   __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_driver_ctlib
 */
#ifdef NCBI_DBAPIDRIVER_CTLIB_EXPORTS
#  define NCBI_DBAPIDRIVER_CTLIB_EXPORT   __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_CTLIB_EXPORT   __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_driver_dblib
 */
#ifdef NCBI_DBAPIDRIVER_DBLIB_EXPORTS
#  define NCBI_DBAPIDRIVER_DBLIB_EXPORT     __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_DBLIB_EXPORT     __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_driver_msdblib
 */
#ifdef NCBI_DBAPIDRIVER_MSDBLIB_EXPORTS
#  define NCBI_DBAPIDRIVER_MSDBLIB_EXPORT   __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_MSDBLIB_EXPORT   __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_driver_odbc
 */
#ifdef NCBI_DBAPIDRIVER_ODBC_EXPORTS
#  define NCBI_DBAPIDRIVER_ODBC_EXPORT      __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_ODBC_EXPORT      __declspec(dllimport)
#endif

/*
 * Export specifier for library dbapi_driver_mysql
 */
#ifdef NCBI_DBAPIDRIVER_MYSQL_EXPORTS
#  define NCBI_DBAPIDRIVER_MYSQL_EXPORT     __declspec(dllexport)
#else
#  define NCBI_DBAPIDRIVER_MYSQL_EXPORT     __declspec(dllimport)
#endif


/*
 * Export specifier for library gui_core
 */
#ifdef NCBI_GUICORE_EXPORTS
#  define NCBI_GUICORE_EXPORT       __declspec(dllexport)
#else
#  define NCBI_GUICORE_EXPORT       __declspec(dllimport)
#endif

/*
 * Export specifier for library xgbplugin
 */
#ifdef NCBI_XGBPLUGIN_EXPORTS
#  define NCBI_XGBPLUGIN_EXPORT     __declspec(dllexport)
#else
#  define NCBI_XGBPLUGIN_EXPORT     __declspec(dllimport)
#endif

/*
 * Export specifier for library gui_utils
 */
#ifdef NCBI_GUIUTILS_EXPORTS
#  define NCBI_GUIUTILS_EXPORT      __declspec(dllexport)
#else
#  define NCBI_GUIUTILS_EXPORT      __declspec(dllimport)
#endif

/*
 * Export specifier for library gui_graph
 */
#ifdef NCBI_GUIGRAPH_EXPORTS
#  define NCBI_GUIGRAPH_EXPORT      __declspec(dllexport)
#else
#  define NCBI_GUIGRAPH_EXPORT      __declspec(dllimport)
#endif

/*
 * Export specifier for library gui_opengl
 */
#ifdef NCBI_GUIOPENGL_EXPORTS
#  define NCBI_GUIOPENGL_EXPORT     __declspec(dllexport)
#else
#  define NCBI_GUIOPENGL_EXPORT     __declspec(dllimport)
#endif

/*
 * Export specifier for library gui_dialogs
 */
#ifdef NCBI_GUIDIALOGS_EXPORTS
#  define NCBI_GUIDIALOGS_EXPORT    __declspec(dllexport)
#else
#  define NCBI_GUIDIALOGS_EXPORT    __declspec(dllimport)
#endif

/*
 * Export specifier for library gui_widgets
 */
#ifdef NCBI_GUIWIDGETS_EXPORTS
#  define NCBI_GUIWIDGETS_EXPORT    __declspec(dllexport)
#else
#  define NCBI_GUIWIDGETS_EXPORT    __declspec(dllimport)
#endif

/*
 * Export specifier for library gui_widgets_aln
 */
#ifdef NCBI_GUIWIDGETSALN_EXPORTS
#  define NCBI_GUIWIDGETSALN_EXPORT    __declspec(dllexport)
#else
#  define NCBI_GUIWIDGETSALN_EXPORT    __declspec(dllimport)
#endif

/*
 * Export specifier for library gui_widgets_seq
 */
#ifdef NCBI_GUIWIDGETSSEQ_EXPORTS
#  define NCBI_GUIWIDGETSSEQ_EXPORT    __declspec(dllexport)
#else
#  define NCBI_GUIWIDGETSSEQ_EXPORT    __declspec(dllimport)
#endif

/*
 * Export specifier for library objects_validator
 */
#ifdef NCBI_VALIDATOR_EXPORTS
#  define NCBI_VALIDATOR_EXPORT     __declspec(dllexport)
#else
#  define NCBI_VALIDATOR_EXPORT     __declspec(dllimport)
#endif

/*
 * Export specifier for library bdb
 */
#ifdef NCBI_BDB_EXPORTS
#  define NCBI_BDB_EXPORT     __declspec(dllexport)
#else
#  define NCBI_BDB_EXPORT     __declspec(dllimport)
#endif


/*
 * Export specifier for library lds
 */
#ifdef NCBI_LDS_EXPORTS
#  define NCBI_LDS_EXPORT     __declspec(dllexport)
#else
#  define NCBI_LDS_EXPORT     __declspec(dllimport)
#endif

#ifdef NCBI_XLOADER_LDS_EXPORTS
#  define NCBI_XLOADER_LDS_EXPORT     __declspec(dllexport)
#else
#  define NCBI_XLOADER_LDS_EXPORT    __declspec(dllimport)
#endif

#ifdef NCBI_XLOADER_BLASTDB_EXPORTS
#  define NCBI_XLOADER_BLASTDB_EXPORT     __declspec(dllexport)
#else
#  define NCBI_XLOADER_BLASTDB_EXPORT    __declspec(dllimport)
#endif


#else  /*  !defined(NCBI_OS_MSWIN)  ||  !defined(NCBI_DLL_BUILD)  */

/*
 * NULL operations for other cases
 */

#  define NCBI_ACCESS_EXPORT
#  define NCBI_BDB_EXPORT
#  define NCBI_BIBLIO_EXPORT
#  define NCBI_BLASTDB_EXPORT
#  define NCBI_BLAST_EXPORT
#  define NCBI_CDD_EXPORT
#  define NCBI_CN3D_EXPORT
#  define NCBI_DBAPIDRIVER_CTLIB_EXPORT
#  define NCBI_DBAPIDRIVER_DBLIB_EXPORT
#  define NCBI_DBAPIDRIVER_EXPORT
#  define NCBI_DBAPIDRIVER_MSDBLIB_EXPORT
#  define NCBI_DBAPIDRIVER_MYSQL_EXPORT
#  define NCBI_DBAPIDRIVER_ODBC_EXPORT
#  define NCBI_DOCSUM_EXPORT
#  define NCBI_ENTREZ2_EXPORT
#  define NCBI_ENTREZGENE_EXPORT
#  define NCBI_FEATDEF_EXPORT
#  define NCBI_FLAT_EXPORT
#  define NCBI_GBSEQ_EXPORT
#  define NCBI_GENERAL_EXPORT
#  define NCBI_GUICORE_EXPORT
#  define NCBI_GUIDIALOGS_EXPORT
#  define NCBI_GUIGRAPH_EXPORT
#  define NCBI_GUIOPENGL_EXPORT
#  define NCBI_GUIUTILS_EXPORT
#  define NCBI_GUIWIDGETS_EXPORT
#  define NCBI_ID1_EXPORT
#  define NCBI_ID2_EXPORT
#  define NCBI_LDS_EXPORT
#  define NCBI_MEDLARS_EXPORT
#  define NCBI_MEDLINE_EXPORT
#  define NCBI_MIM_EXPORT
#  define NCBI_MLA_EXPORT
#  define NCBI_MMDB1_EXPORT
#  define NCBI_MMDB2_EXPORT
#  define NCBI_MMDB3_EXPORT
#  define NCBI_NCBIMIME_EXPORT
#  define NCBI_OBJPRT_EXPORT
#  define NCBI_PUBMED_EXPORT
#  define NCBI_PUB_EXPORT
#  define NCBI_SCOREMAT_EXPORT
#  define NCBI_SEQALIGN_EXPORT
#  define NCBI_SEQBLOCK_EXPORT
#  define NCBI_SEQCODE_EXPORT
#  define NCBI_SEQFEAT_EXPORT
#  define NCBI_SEQLOC_EXPORT
#  define NCBI_SEQRES_EXPORT
#  define NCBI_SEQSET_EXPORT
#  define NCBI_SEQUENCE_EXPORT
#  define NCBI_SEQ_EXPORT
#  define NCBI_SUBMIT_EXPORT
#  define NCBI_TAXON1_EXPORT
#  define NCBI_TINYSEQ_EXPORT
#  define NCBI_VALIDATOR_EXPORT
#  define NCBI_XALGOALIGN_EXPORT
#  define NCBI_XALGOSEQ_EXPORT
#  define NCBI_XALGO_EXPORT
#  define NCBI_XALNMGR_EXPORT
#  define NCBI_XALNUTIL_EXPORT
#  define NCBI_XBLAST_EXPORT
#  define NCBI_XGBPLUGIN_EXPORT
#  define NCBI_XIMAGE_EXPORT
#  define NCBI_XLOADER_BLASTDB_EXPORT
#  define NCBI_XLOADER_LDS_EXPORT
#  define NCBI_XNCBI_EXPORT
#  define NCBI_XOBJMGR_EXPORT
#  define NCBI_XOBJREAD_EXPORT
#  define NCBI_XOBJUTIL_EXPORT
#  define NCBI_XSERIAL_EXPORT
#  define NCBI_XUTIL_EXPORT

#endif


#endif  /*  CORELIB___MSWIN_EXPORT__H  */

/*
 * ==========================================================================
 * $Log$
 * Revision 1.36  2003/09/12 12:56:52  dicuccio
 * Added new export specifiers for gui_widgets_seq, gui_widgets_aln
 *
 * Revision 1.35  2003/09/09 18:53:44  grichenk
 * +ID2
 *
 * Revision 1.34  2003/09/04 14:58:51  dicuccio
 * Added export specifier for GUIGRAPH library
 *
 * Revision 1.33  2003/08/27 16:41:43  ivanov
 * * Added export specifier NCBI_XIMAGE_EXPORT
 *
 * Revision 1.32  2003/08/06 16:09:21  jianye
 * Add specifiers for new libraries: blastdb, xalnutil, xloader_blastdb.
 *
 * Revision 1.31  2003/08/04 15:44:12  dicuccio
 * Added export specifier for libxblast.  Modified layout of algorithm export
 * specifiers
 *
 * Revision 1.30  2003/07/30 16:35:17  kuznets
 * Fixed typo with NCBI_XLOADER_LDS_EXPORT
 *
 * Revision 1.29  2003/07/16 20:15:00  kuznets
 * + NCBI_XLOADER_LDS_EXPORTS (export/import macro for lds dataloader)
 *
 * Revision 1.28  2003/06/27 19:00:08  dicuccio
 * Moved BLAST, scoremat, and ID1 from ncbi_seq.dll to ncbi_seqext.dll
 *
 * Revision 1.27  2003/06/04 17:05:35  ucko
 * +NCBI_XOBJREAD_EXPORT(S), under NCBI_SEQEXT_EXPORTS.
 *
 * Revision 1.26  2003/06/03 19:22:20  dicuccio
 * Fixed specification of MMDB lib exports
 *
 * Revision 1.25  2003/06/03 18:48:54  kuznets
 * + export defines for bdb and lds libraries.
 *
 * Revision 1.24  2003/06/02 16:01:29  dicuccio
 * Rearranged include/objects/ subtree.  This includes the following shifts:
 *     - include/objects/alnmgr --> include/objtools/alnmgr
 *     - include/objects/cddalignview --> include/objtools/cddalignview
 *     - include/objects/flat --> include/objtools/flat
 *     - include/objects/objmgr/ --> include/objmgr/
 *     - include/objects/util/ --> include/objmgr/util/
 *     - include/objects/validator --> include/objtools/validator
 *
 * Revision 1.23  2003/05/23 16:19:48  ivanov
 * Fixed typo
 *
 * Revision 1.22  2003/05/23 15:22:40  ivanov
 * Added export specifier NCBI_MLA_EXPORT
 *
 * Revision 1.21  2003/04/25 21:05:02  ucko
 * +SCOREMAT (under SEQEXT)
 *
 * Revision 1.20  2003/04/15 16:30:38  dicuccio
 * Moved BLAST object files into ncbi_seqext.dll
 *
 * Revision 1.19  2003/04/14 19:37:42  ivanov
 * Added master export group NCBI_SEQEXT_EXPORTS. Moved a part code from NCBI_SEQ.DLL to NCBI_SEQEXT.DLLmswin_export.h
 *
 * Revision 1.18  2003/04/10 13:31:40  dicuccio
 * Added BLAST objects to NCBI_SEQ
 *
 * Revision 1.17  2003/04/09 16:12:05  ivanov
 * Fix for the previous commit
 *
 * Revision 1.16  2003/04/08 20:07:05  ivanov
 * Added export specifiers NCBI_BLAST_EXPORT and NCBI_ENTREZGENE_EXPORT
 *
 * Revision 1.15  2003/03/28 20:29:42  kans
 * define NCBI_FLAT_EXPORT at end
 *
 * Revision 1.14  2003/03/28 17:44:54  dicuccio
 * Added export specifier for flatfile generator library.  Made this specifier
 * part of the NCBI_SEQ_EXPORTS master group
 *
 * Revision 1.13  2003/02/25 19:34:21  kuznets
 * Added NCBI_DBAPIDRIVER_MYSQL_EXPORTS
 *
 * Revision 1.12  2003/02/21 16:42:16  dicuccio
 * Added export specifiers for XALGO, XGBPLUGIN
 *
 * Revision 1.11  2003/02/12 22:02:29  coremake
 * Added export specifiers NCBI_DBAPIDRIVER_*_EXPORT for ctlib, dblib, msdblib and odbc DBAI driver libraries
 *
 * Revision 1.10  2003/02/06 18:49:58  dicuccio
 * Added NCBI_TAXON1_EXPORT specifier
 *
 * Revision 1.9  2003/01/17 19:44:28  lavr
 * Reduce dependencies
 *
 * Revision 1.8  2003/01/16 19:53:13  dicuccio
 * Add NCBI_GUIDIALOGS_EXPORT to the null define list...
 *
 * Revision 1.7  2003/01/16 18:23:15  dicuccio
 * Added export specifiers for library GUI_DIALOGS.DLL
 *
 * Revision 1.6  2003/01/07 22:17:25  lavr
 * Move '#include <connect/connect_export.h>' up
 *
 * Revision 1.5  2003/01/07 19:58:25  shomrat
 * Added NCBI_VALIDATOR_EXPORT
 *
 * Revision 1.4  2002/12/31 16:15:46  dicuccio
 * Added missing NCBI_SUBMIT_EXPORT to empty define list
 *
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
