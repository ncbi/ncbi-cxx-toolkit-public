#ifndef PCSF__HPP
#define PCSF__HPP

/* 
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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <algo/gnomon/gnomon_model.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

class CPhyloCSFData {
public:
    CPhyloCSFData(istream& from, const string& acc, int len, CAlignMap* map = nullptr, int shift = 0, double factor = 1) : m_contig_acc(acc), m_map(map), m_factor(factor), m_shift(shift) {
        m_scores[0].resize(len);
        m_scores[1].resize(len);

        size_t data_length;
        if(!from.read(reinterpret_cast<char*>(&data_length), sizeof data_length)) {
            cerr << "Error in PhyloCSF read\n";
            exit(1);
        }
        from.seekg(data_length, ios_base::cur);
        int slen;
        while(from.read(reinterpret_cast<char*>(&slen), sizeof slen)) { // acc string size
            vector<char> buf(slen);
            from.read(buf.data(), slen);
            string contig(buf.begin(), buf.end());
            size_t shift;
            size_t contig_len;
            from.read(reinterpret_cast<char*>(&shift), sizeof shift);            // data shift in file
            from.read(reinterpret_cast<char*>(&contig_len), sizeof contig_len);  // contig length
            if(contig == acc) {
                _ASSERT(len == (int)contig_len);
                from.seekg(shift,ios_base::beg); 
                from.read(reinterpret_cast<char*>(m_scores[0].data()), len*sizeof(float));
                if(!from.read(reinterpret_cast<char*>(m_scores[1].data()), len*sizeof(float))) {
                    cerr << "Error in PhyloCSF read\n";
                    exit(1);
                }
                break;
            }
        }
    }
    double Score(int s, int codon_left) const {
        if(m_map != nullptr) {
            codon_left = m_map->MapEditedToOrig(codon_left);
            if(codon_left < 0)
                return 0;
        }
        _ASSERT(s >= 0 && s < 2 && codon_left+m_shift < (int)m_scores[s].size());
        return m_scores[s][codon_left+m_shift]; 
    }
    string ContigAcc() const { return m_contig_acc; }
    double Factor() const { return m_factor; }

private:
    array<vector<float>, 2> m_scores; 
    string m_contig_acc;
    CAlignMap* m_map;
    double m_factor;
    int m_shift;
};

END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // PCSF__HPP
