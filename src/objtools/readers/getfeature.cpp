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
 *  Fetching features
 *
 */

#include <ncbi_pch.hpp>
#include <util/range.hpp>
#include <objtools/readers/getfeature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE (objects)

CGetFeature::CGetFeature(string feat_file, string index_file){
    m_FeatFile = new CNcbiIfstream(feat_file.c_str(), IOS_BASE::binary);
    m_FeatFileIndex  = new CNcbiIfstream(index_file.c_str(), IOS_BASE::binary);
    m_5FeatInfo = NULL;
    m_3FeatInfo = NULL;
  
}


vector<SFeatInfo*>& CGetFeature::GetFeatInfo(const string& id_str,
                                                 const CRange<TSeqPos>& seq_range, 
                                                 SFeatInfo*& feat5, 
                                                 SFeatInfo*& feat3,
                                                 int max_feature ){
    x_Clear();
    m_5FeatInfo = NULL;
    m_3FeatInfo = NULL;   
    if(m_FeatFileIndex && m_FeatFile && *m_FeatFileIndex && *m_FeatFile){
        unsigned int offset = 0;    
        map<string, unsigned int>::const_iterator iter = m_OffsetMap.find(id_str);
        if ( iter != m_OffsetMap.end() ){
            offset  = iter->second; 
        } else{
            m_FeatFileIndex->seekg(0);
            while(!m_FeatFileIndex->eof()){
                SOffsetInfo offset_info;
                m_FeatFileIndex->read((char*)(&offset_info), sizeof(SOffsetInfo));
                if(!(*m_FeatFileIndex)){
                    m_FeatFileIndex->clear();
                    break;
                }
                
                if(offset_info.id == id_str){                   
                    offset = offset_info.offset;
                    m_OffsetMap.insert(map<string, unsigned int>::value_type(offset_info.id, offset_info.offset));
                    m_FeatFileIndex->clear();
                    break;
                }
            }
            m_FeatFileIndex->clear();
        }
        
        m_FeatFile->seekg(offset); 
        int count = 0;
        //now look to retrieve feature info
        while(!m_FeatFile->eof() && count < max_feature){   
            SFeatInfo* feat_info = new SFeatInfo;
            m_FeatFile->read((char*)feat_info, sizeof(SFeatInfo));
            if(*m_FeatFile){
                
                if(id_str != feat_info->id) { //next id already
                    delete feat_info;
                    m_FeatFile->clear();
                    break;
                }
                
                if(seq_range.IntersectingWith(feat_info->range)){
                    m_FeatInfoList.push_back(feat_info);
                    count ++;
                } else {
                    //track the flank features that are 5' and 3' of the range      
                    if(feat_info->range < seq_range ){
                        if(m_5FeatInfo){
                            delete m_5FeatInfo;
                            m_5FeatInfo = feat_info;
                        } else { //first one
                            m_5FeatInfo = feat_info;
                        }
                        
                    } else {
                        m_3FeatInfo = feat_info;
                        break; //already past the range as range was sorted
                        //from low to high
                    }
                }
            } else {
                delete feat_info;
                m_FeatFile->clear();
                break;
            }              
        } 
        m_FeatFile->clear(); //reset
    } 

    if(m_5FeatInfo){
        feat5 = m_5FeatInfo;
    }
    
    if(m_3FeatInfo){
        feat3 = m_3FeatInfo;
    }

    return m_FeatInfoList;
}

CGetFeature::~CGetFeature(){
    x_Clear();
    if(m_FeatFile) {
        delete m_FeatFile;
    }
    if(m_FeatFileIndex){
        delete m_FeatFileIndex;
    }
}

void CGetFeature::x_Clear(){
    ITERATE(vector<SFeatInfo*>, iter, m_FeatInfoList){
        delete *iter;
    }
    m_FeatInfoList.clear();

    if(m_5FeatInfo){
        delete m_5FeatInfo;
    }
    if(m_3FeatInfo){
        delete m_3FeatInfo;
    }
    
}

END_SCOPE(objects)
END_NCBI_SCOPE
