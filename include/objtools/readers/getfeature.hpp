#ifndef GETFEATURE_HPP
#define GETFEATURE_HPP

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
 * Author:  Jian Ye
 *
 * File Description:
 *   Get features from pre-formated files
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/readers/featuredump.hpp>

BEGIN_NCBI_SCOPE 
BEGIN_SCOPE(objects)

  
class NCBI_XOBJREAD_EXPORT CGetFeature {
public:
    typedef vector<SFeatInfo*> Tfeatinfo;
    //feat_file contains feature info, index_file contains byte offset for 
    //seqids in feat_file
    CGetFeature(string feat_file, string index_file);
   
    ~CGetFeature();
    //return features with the specified range and the closest 5' and 3' features
    Tfeatinfo& GetFeatInfo(const string& id_str,
                           const CRange<TSeqPos>& seq_range, 
                           SFeatInfo*& feat5, 
                           SFeatInfo*& feat3,
                           int max_feature = 3);
    
private:
    CNcbiIfstream* m_FeatFile;
    CNcbiIfstream* m_FeatFileIndex;
    map <string, unsigned int > m_OffsetMap;  //cache previous id offset
    vector<SFeatInfo*> m_FeatInfoList;
    SFeatInfo* m_5FeatInfo;
    SFeatInfo* m_3FeatInfo;
    void x_Clear();
};


/***********************Inlines************************/

END_SCOPE(objects)
END_NCBI_SCOPE

/* 
*===========================================
*$Log$
*Revision 1.1  2004/08/05 16:20:29  jianye
*Initial check in
*
*===========================================
*/
#endif
