
#include <util/regexp.hpp>
#include "find_pattern.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


// find all occurences of a pattern (regular expression) in a sequence
void CFindPattern::Find(CSeqVector& vec, const string& pattern,
                       vector<TSeqPos>& starts, vector<TSeqPos>& ends) {

    string seq;
    vec.GetSeqData( (TSeqPos) 0, vec.size(), seq );

    // do a case-insensitive r. e. search for "all" (non-overlapping) occurences
    // note that it's somewhat ambiguous what this means

    // want to ignore case, and to allow white space (and comments) in pattern
    CRegexp re(pattern, PCRE_CASELESS | PCRE_EXTENDED);

    unsigned int offset = 0;
    while (!re.GetMatch(seq.data(), offset).empty()) {  // empty string means no match
        const int *res = re.GetResults(0);
        starts.push_back(res[0]);
        ends.push_back(res[1] - 1);
        offset = res[1];
    }
}

END_NCBI_SCOPE
