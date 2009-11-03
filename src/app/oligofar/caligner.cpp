#include <ncbi_pch.hpp>
#include "caligner.hpp"
#include "cseqcoding.hpp"
#include "cscoringfactory.hpp"

USING_OLIGOFAR_SCOPES;

map<double,Uint8> CAligner::s_maxPenalty;

CAligner::~CAligner()
{
}

void CAligner::PrintStatistics( ostream& out )
{
    out << "/====ALIGNER STATISTICS================\\\t\t(to be removed)\n"
         << "|     calls to aligner: " << setw(14) << right << m_totalCalls << " |\n"
         << "|     aligns succeeded: " << setw(14) << right << m_successAligns << " |\n"
         << "|    input query bases: " << setw(14) << right << m_queryBases << " |\n"
         << "|  checked query bases: " << setw(14) << right << m_queryBasesChecked << " |\n"
         << "|     score func calls: " << setw(14) << right << m_scoreCalls << " |\n"
         << "|      algo X0 for all: " << setw(14) << right << m_algoX0all << " |\n"
         << "|      algo X0 for end: " << setw(14) << right << m_algoX0end << " |\n"
         << "|      algo X0 for win: " << setw(14) << right << m_algoX0win << " |\n"
         << "|      algo SW for end: " << setw(14) << right << m_algoSWend << " |\n"
         << "|      algo SW for win: " << setw(14) << right << m_algoSWwin << " |\n"
         << "\\______________________________________/" << endl;
    if( s_maxPenalty.size() ) {
        out << "/ MAX PENALTY USED:\n";
        double mpb = s_maxPenalty.begin()->first + 1;
        int cnt = 0;
        for( map<double,Uint8>::const_iterator mp = s_maxPenalty.begin(); mp != s_maxPenalty.end(); ++mp ) {
            if( mp->first >= mpb ) {
                if( cnt ) out << "| " << setw(14) << right << cnt << " [" << (mpb-1) << ".." << mpb << ")\n";
                cnt = 0;
                ++mpb;
            } else cnt += mp->second;
        }
        if( cnt ) out << "| " << setw(14) << right << cnt << " [" << (mpb-1) << ".." << mpb << ")\n";
        out << "\\" << string( 30, '_' ) << endl;
    }
    out << "Matrix memory usage: " << m_matrix.size() << " for " << m_matrixSize << " length\n";
    if( m_alignParam )
        out << "SpliceSet size is: " << GetSpliceSet().size() << "\n";
}
