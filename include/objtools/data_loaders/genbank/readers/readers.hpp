#ifndef GENBANK___READERS__HPP
#define GENBANK___READERS__HPP

/* $Id$
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
 * Author:  Aleksey Grichenko
 *   
 * File Description:  Data readers' registration
 *
 */


BEGIN_NCBI_SCOPE

extern "C" {

extern void NCBI_XREADER_ID1_EXPORT      GenBankReaders_Register_Id1   (void);
extern void NCBI_XREADER_ID2_EXPORT      GenBankReaders_Register_Id2   (void);
extern void NCBI_XREADER_PUBSEQOS_EXPORT GenBankReaders_Register_Pubseq(void);
extern void NCBI_XREADER_CACHE_EXPORT    GenBankReaders_Register_Cache (void);
extern void NCBI_XREADER_CACHE_EXPORT    GenBankWriters_Register_Cache (void);

}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/03/11 00:02:50  vasilche
 * Register functions made exported from their dlls.
 *
 * Revision 1.2  2005/03/10 20:55:06  vasilche
 * New CReader/CWriter schema of CGBDataLoader.
 *
 * Revision 1.1  2004/12/22 19:30:36  grichenk
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* GENBANK___READERS__HPP */
