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
 * Author: Vladimir Ivanov
 * File Description:
 *     API to diff two texts and finds the differences.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/hash_map.hpp>
#include <util/diff/diff.hpp>
#include "dtl/dtl/dtl.hpp"


#define NCBI_USE_ERRCODE_X   Util_Diff

BEGIN_NCBI_SCOPE


// Macro to check bits
#define F_ISSET(mask) (((flags) & (mask)) == (mask))

// Local defines to simplicity a reading the code
#define DIFF_DELETE CDiffOperation::eDelete
#define DIFF_EQUAL  CDiffOperation::eEqual
#define DIFF_INSERT CDiffOperation::eInsert


/////////////////////////////////////////////////////////////////////////////
//
// CDiffOperation
//

CDiffOperation::CDiffOperation(EType operation, CTempString str)
    : m_Operation(operation),
      m_String(str),
      m_Length(str.length())
{ }


/////////////////////////////////////////////////////////////////////////////
//
// CDiffList
//

CDiffList::size_type CDiffList::GetEditDistance(void) const
{
    const CDiffList::TList& diffs = GetList();
    if (diffs.empty()) {
        NCBI_THROW(CDiffException, eEmpty, "The diff list is empty");
    }
    size_type edit_distance = 0;
    size_type len_insert = 0;
    size_type len_delete = 0;
        
    ITERATE(CDiffList::TList, it, diffs) {
        switch (it->GetOperation()) {
            case DIFF_INSERT:
                len_insert += it->GetString().length();
                break;
            case DIFF_DELETE:
                len_delete += it->GetString().length();
                break;
            case DIFF_EQUAL:
                // A deletion and an insertion is one substitution.
                edit_distance += max(len_insert, len_delete);
                len_insert = 0;
                len_delete = 0;
                break;
        }
    }
    edit_distance += max(len_insert, len_delete);
    return edit_distance;
}


CTempString CDiffList::GetLongestCommonSubstring(void) const
{
    const CDiffList::TList& diffs = GetList();
    if (diffs.empty()) {
        NCBI_THROW(CDiffException, eEmpty, "The diff list is empty");
    }
    TList::const_iterator max_pos = diffs.end();
    size_type max_len = 0;
    ITERATE(CDiffList::TList, it, diffs) {
        if (it->IsEqual()) {
            size_type len = it->GetString().length();
            if (len > max_len) {
                max_len = len;
                max_pos = it;
            }
        }
    }
    if (!max_len  ||  max_pos == diffs.end()) {
        return CTempString();
    }
    return max_pos->GetString();
}


void CDiffList::CleanupAndMerge(void)
{
    // Check on incorrect usage
    ITERATE(TList, it, m_List) {
        if (it->GetString().length() != it->GetLength()) {
            // Possible the line-based diff with fRemoveEOL flag was used,
            // It is impossible to cleanup and merge equalities in this mode.
            NCBI_THROW(CDiffException, eBadFlags,
                       "Possible fRemoveEOL flag was used, it is impossible "
                       "to perform cleanup and merge in this mode");
        }
    }
    bool changes;
    do {
        // First pass: merge adjacent parts with the same operation 
        x_CleanupAndMerge_Equities();
        // Second pass: look for single edits surrounded on both sides
        // by equalities which can be shifted sideways to eliminate
        // an equality. e.g: A<ins>BA</ins>C -> <ins>AB</ins>AC
        changes = x_CleanupAndMerge_SingleEdits();
    }
    // If shifts were made, the diff needs reordering and another shift sweep
    while (changes);
    return;
}


void CDiffList::CalculateOffsets(void)
{
    CDiffList::TList& diffs = m_List;
    if (diffs.empty()) {
        NCBI_THROW(CDiffException, eEmpty, "The diff list is empty");
    }
    size_type off_first  = 0;
    size_type off_second = 0;

    NON_CONST_ITERATE(CDiffList::TList, it, diffs) {
        switch (it->GetOperation()) {
            case DIFF_INSERT:
                it->SetOffset(CDiffOperation::SPos(NPOS, off_second));
                off_second += it->GetLength();
                break;
            case DIFF_DELETE:
                it->SetOffset(CDiffOperation::SPos(off_first, NPOS));
                off_first += it->GetLength();
                break;
            case DIFF_EQUAL:
                it->SetOffset(CDiffOperation::SPos(off_first, off_second));
                off_first += it->GetLength();
                off_second += it->GetLength();
                break;
        }
    }
    return;
}


void CDiffList::x_CleanupAndMerge_Equities(void)
{
    // Counters for inserted/deleted parts
    size_t count_delete = 0;
    size_t count_insert = 0;
    // Assign iterators to current, insertion, deletion and equal diffs
    CDiffList::TList::iterator this_diff  = m_List.begin();
    CDiffList::TList::iterator ins_diff   = m_List.end();
    CDiffList::TList::iterator del_diff   = m_List.end();
    CDiffList::TList::iterator equal_diff = m_List.end();

    while (this_diff != m_List.end()) {
        const CTempString& current = this_diff->GetString();
        switch (this_diff->GetOperation()) {

            case DIFF_INSERT:
                // Merge insertions
                if (count_insert) {
                    CTempString s = ins_diff->GetString();
                    ins_diff->SetString(CTempString(s.data(), s.length() + current.length()));
                    this_diff = m_List.erase(this_diff);
                } else {
                    ins_diff = this_diff++;
                }
                count_insert++;
                break;

            case DIFF_DELETE:
                // Merge deletions
                if (count_delete) {
                    CTempString s = del_diff->GetString();
                    del_diff->SetString(CTempString(s.data(), s.length() + current.length()));
                    this_diff = m_List.erase(this_diff);
                } else {
                    del_diff = this_diff++;
                }
                count_delete++;
                break;

            case DIFF_EQUAL:
                // Have INSERT or DELETE previously?
                if (count_delete + count_insert >= 1) {
                    // Have both INSERT and DELETE?
                    if (count_delete != 0  &&  count_insert != 0) {
                        // Factor out common prefix
                        CTempString ins_str = ins_diff->GetString();
                        CTempString del_str = del_diff->GetString();
                        size_type common_prefix_len = NStr::CommonPrefixSize(ins_str, del_str);
                        if (common_prefix_len) {
                            // Append common prefix to the previous equal part
                            if (equal_diff == m_List.end()) {
                                // If no EQUAL node before, insert common prefix before any INSERT or DELETE
                                Prepend(DIFF_EQUAL, ins_str.substr(0, common_prefix_len));
                            } else {
                                CTempString s = equal_diff->GetString();
                                equal_diff->SetString(CTempString(s.data(), s.length() + common_prefix_len));
                            }
                            ins_str = ins_str.substr(common_prefix_len);
                            del_str = del_str.substr(common_prefix_len);
                        }

                        // Factor out common suffix
                        size_type common_suffix_len = NStr::CommonSuffixSize(ins_str, del_str);
                        if (common_suffix_len) {
                            // Append suffix to the current node
                            CTempString common_suffix = ins_str.substr(ins_str.length() - common_suffix_len);
                            this_diff->SetString(
                                CTempString(common_suffix.data(),
                                            common_suffix.length() + current.length()));
                            // Cut off suffix from INSERT and DELETE strings
                            ins_str = ins_str.substr(0, ins_str.length() - common_suffix_len);
                            del_str = del_str.substr(0, del_str.length() - common_suffix_len);
                        }
                        // Update nodes
                        if (common_prefix_len || common_suffix_len) {
                            ins_diff->SetString(ins_str);
                            del_diff->SetString(del_str);
                        }
                    }
                    // Save pointer to the first equal part to merge
                    equal_diff = this_diff++;

                } else {
                    // EQUAL, and not an INSERT or DELETE before
                    if (equal_diff == m_List.end()) {
                        // Save pointer to the first equality to merge
                        equal_diff = this_diff++;
                    } else {
                        // Merge current equality with previous
                        CTempString s = equal_diff->GetString();
                        equal_diff->SetString(CTempString(s.data(), s.length() + current.length()));
                        this_diff = m_List.erase(this_diff);
                    }
                }
                count_insert = 0;
                count_delete = 0;
                break;

        } // switch
    } // while

    return;
}


bool CDiffList::x_CleanupAndMerge_SingleEdits(void)
{
    if (m_List.size() < 3) {
        // Don't meet algorithm criteria
        return false;
    }
    // Assign iterators to previous, current and next diffs
    CDiffList::TList::iterator next_diff = m_List.begin();
    CDiffList::TList::iterator prev_diff = next_diff++;
    CDiffList::TList::iterator this_diff = next_diff++;
        
    bool changes = false;

    while (next_diff != m_List.end()) {
        // Intentionally ignore the first and last element (don't need checking)
        if (prev_diff->GetOperation() == DIFF_EQUAL  &&
            next_diff->GetOperation() == DIFF_EQUAL) 
        {
            _VERIFY(this_diff->GetOperation() != DIFF_EQUAL);

            CTempString prev_str = prev_diff->GetString();
            CTempString this_str = this_diff->GetString();
            CTempString next_str = next_diff->GetString();

            // NOTE:
            // CTempString data should be contiguous and be a part of
            // the same source string.
            // If the middle (this_diff) is inserted, always use 'this_str'
            // (new based tmp string) as base for expanding/shifting strings,
            // if deleted - any (old based).

            // This is a single edit surrounded by equalities.
            if (NStr::StartsWith(this_str, next_str)) {
                // Case: A<CB>C --> AC<BC>
                // Shift the edit over the next equality.
                // 1) A -> AC
                prev_diff->SetString(
                    CTempString(this_str.data() - prev_str.length(),
                                prev_str.length() + next_str.length()));
                // 2) CB -> BC
                this_diff->SetString(
                    CTempString(this_str.data() + next_str.length(), this_str.length()));
                // 3) Delete next_diff and move to next element
                next_diff = m_List.erase(next_diff);
                if (next_diff == m_List.end()) {
                    next_diff = this_diff;
                }
                changes = true;

            } else if (NStr::EndsWith(this_str, prev_str)) {
                // Case: A<BA>C --> <AB>AC
                // Shift the edit over the previous equality (starting from the back).
                // 1) C -> AC
                next_diff->SetString(
                    CTempString(this_str.data() + this_str.length() - prev_str.length(),
                                prev_str.length() + next_str.length()));
                // 2) BA -> AB
                this_diff->SetString(
                    CTempString(this_str.data() - prev_str.length(), this_str.length()));
                // 3) Delete prev_diff
                m_List.erase(prev_diff);
                changes = true;
            }
        }
        prev_diff = this_diff;
        this_diff = next_diff;
        next_diff++;
    }
    return changes;
}


//////////////////////////////////////////////////////////////////////////////
//
//  CDiff
//

CDiffList& CDiff::Diff(CTempString s1, CTempString s2, TFlags flags)
{
    Reset();

    // Set deadline time if timeout is specified
    auto_ptr<CDeadline> real_deadline;
    if (!m_Timeout.IsInfinite()) {
        real_deadline.reset(new CDeadline(m_Timeout));
        m_Deadline = real_deadline.get();
    }

    // Check for equality (speedup)
    if (s1 == s2) {
        if (!s1.empty()) {
            m_Diffs.Append(DIFF_EQUAL, s1);
        }
        return m_Diffs;
    }

    // Trim off common prefix (speedup)
    size_type common_prefix_len = NStr::CommonPrefixSize(s1, s2);
    CTempString common_prefix;
    if (common_prefix_len) {
        common_prefix = s1.substr(0, common_prefix_len);
        s1 = s1.substr(common_prefix_len);
        s2 = s2.substr(common_prefix_len);
    }

    // Trim off common suffix (speedup).
    size_type common_suffix_len = NStr::CommonSuffixSize(s1, s2);
    CTempString common_suffix;
    if (common_suffix_len) {
        common_suffix = s1.substr(s1.length() - common_suffix_len);
        s1 = s1.substr(0, s1.length() - common_suffix_len);
        s2 = s2.substr(0, s2.length() - common_suffix_len);
    }

    // Compute the diff on the middle block.
    x_Diff(s1, s2, m_Diffs);
    // Restore the prefix and suffix.
    if (common_prefix_len) {
        m_Diffs.Prepend(DIFF_EQUAL, common_prefix);
    }
    if (common_suffix_len) {
        m_Diffs.Append(DIFF_EQUAL, common_suffix);
    }

    // Post-process diff list
    if ( !F_ISSET(fNoCleanup) ) {
        if ( !IsTimeoutExpired() ) {
            m_Diffs.CleanupAndMerge();
        }
    }
    if ( F_ISSET(fCalculateOffsets) ) {
        // Do not check timeout here, it is fast and
        // can be executed even in this case
        m_Diffs.CalculateOffsets();
    }
    // Cleanup
    m_Deadline = NULL;
    // Return result
    return m_Diffs;
}


/// Do the two texts share a substring which is at least half the length
/// of the longer text? This speedup can produce non-minimal diffs.
/// @param s1
///   First string.
/// @param s2
///   Second string.
/// @param hm
///   Five element array, containing the prefix of 's1', the suffix of 's1',
///   the prefix of 's2', the suffix of 's2' and the common middle. 
/// @return
///   FALSE if there was no match.
/// @sa Diff
/// @private
bool CDiff::x_DiffHalfMatch(CTempString s1, CTempString s2, TDiffHalfMatchList& hm) const
{
    if ( m_Timeout.IsInfinite() ) {
        // Don't risk returning a non-optimal diff if we have unlimited time.
        return false;
    }

    const CTempString long_str  = s1.length() > s2.length() ? s1 : s2;
    const CTempString short_str = s1.length() > s2.length() ? s2 : s1;
    if (long_str.length() < 4 || short_str.length() * 2 < long_str.length()) {
        return false;  // Pointless
    }

    // Reserve 5 elements
    TDiffHalfMatchList hm1(5), hm2(5);

    // First check if the second quarter is the seed for a half-match.
    bool hm1_res = x_DiffHalfMatchI(long_str, short_str, (long_str.length() + 3) / 4, hm1);
    // Check again based on the third quarter.
    bool hm2_res = x_DiffHalfMatchI(long_str, short_str, (long_str.length() + 1) / 2, hm2);
    if (!hm1_res && !hm2_res) {
        return false;
    } else if (!hm1_res) {
        hm = hm2;
    } else if (!hm2_res) {
        hm = hm1;
    } else {
        // Both matched.  Select the longest.
        hm = hm1[4].length() > hm2[4].length() ? hm1 : hm2;
    }
    // A half-match was found, sort out the return data.
    if (s1.length() > s2.length()) {
        // return hm;
    } else {
        TDiffHalfMatchList tmp(5);
        tmp[0] = hm[2];
        tmp[1] = hm[3];
        tmp[2] = hm[0];
        tmp[3] = hm[1];
        tmp[4] = hm[4];
        hm.swap(tmp);
    }
    return true;
}


/// Does a substring of short string exist within long string such that
/// the substring is at least half the length of long string?
///
/// @param long_str
///   Longer string.
/// @param short_str
///   Shorter string.
/// @param i
///   Start index of quarter length substring within long string.
/// @param hm
///   Five element Array, containing the prefix of 'long_str', the suffix
///   of 'long_str', the prefix of 'short_str', the suffix of 'short_str'
///   and the common middle.
/// @return
///   FALSE if there was no match.
/// @sa Diff, x_DiffHalfMatch
/// @private
bool CDiff::x_DiffHalfMatchI(CTempString long_str, CTempString short_str,
                             size_type i, TDiffHalfMatchList& hm) const
{
    // Start with a 1/4 length substring at position i as a seed.
    const CTempString seed = long_str.substr(i, long_str.length() / 4);
    size_type j = NPOS;
    CTempString best_common;
    CTempString best_longtext_a, best_longtext_b;
    CTempString best_shorttext_a, best_shorttext_b;

    while ((j = short_str.find(seed, j + 1)) != NPOS) {
        const size_type prefix_len = 
            NStr::CommonPrefixSize(long_str.substr(i), short_str.substr(j));
        const size_type suffix_len = 
            NStr::CommonSuffixSize(long_str.substr(0, i), short_str.substr(0, j));
        if (best_common.length() < suffix_len + prefix_len) {
            best_common      = short_str.substr(j - suffix_len, suffix_len + prefix_len);
            best_longtext_a  = long_str.substr(0, i - suffix_len);
            best_longtext_b  = long_str.substr(i + prefix_len);
            best_shorttext_a = short_str.substr(0, j - suffix_len);
            best_shorttext_b = short_str.substr(j + prefix_len);
        }
    }
    if (best_common.length() * 2 >= long_str.length()) {
        hm[0] = best_longtext_a;
        hm[1] = best_longtext_b;
        hm[2] = best_shorttext_a;
        hm[3] = best_shorttext_b;
        hm[4] = best_common;
        return true;
    }
    return false;
}


/// Find the 'middle snake' of a diff, split the problem in two
/// and return the recursively constructed diff.
/// See Myers 1986 paper: An O(ND) Difference Algorithm and Its Variations.
/// @param s1 
///   Old string to be diffed.
/// @param s2
///   New string to be diffed.
/// @param diffs
///   Resulting list of the diff operations.
/// @sa Diff
/// @private
void CDiff::x_DiffBisect(CTempString s1, CTempString s2, CDiffList& diffs) const
{
    // Cache the text lengths to prevent multiple calls.
    const int s1_len = (int)s1.length();
    const int s2_len = (int)s2.length();
    const int max_d = (s1_len + s2_len + 1) / 2;
    const int v_offset = max_d;
    const int v_length = 2 * max_d;
    int *v1 = new int[v_length];
    int *v2 = new int[v_length];
    for (int x = 0; x < v_length; x++) {
        v1[x] = -1;
        v2[x] = -1;
    }
    v1[v_offset + 1] = 0;
    v2[v_offset + 1] = 0;
    const int delta = s1_len - s2_len;
    // If the total number of characters is odd, then the front path will
    // collide with the reverse path.
    const bool front = (delta % 2 != 0);
    // Offsets for start and end of k loop.
    // Prevents mapping of space beyond the grid.
    int k1start = 0;
    int k1end = 0;
    int k2start = 0;
    int k2end = 0;

    for (int d = 0; d < max_d; d++)
    {
        if ( IsTimeoutExpired() ) {
            break;
        }
        // Walk the front path one step.
        for (int k1 = -d + k1start; k1 <= d - k1end; k1 += 2) {
            const int k1_offset = v_offset + k1;
            int x1;
            if (k1 == -d || (k1 != d && v1[k1_offset - 1] < v1[k1_offset + 1])) {
                x1 = v1[k1_offset + 1];
            } else {
                x1 = v1[k1_offset - 1] + 1;
            }
            int y1 = x1 - k1;
            while (x1 < s1_len && y1 < s2_len && s1[x1] == s2[y1]) {
                x1++;
                y1++;
            }
            v1[k1_offset] = x1;
            if (x1 > s1_len) {
                // Ran off the right of the graph.
                k1end += 2;
            } else if (y1 > s2_len) {
                // Ran off the bottom of the graph.
                k1start += 2;
            } else if (front) {
                int k2_offset = v_offset + delta - k1;
                if (k2_offset >= 0 && k2_offset < v_length && v2[k2_offset] != -1) {
                    // Mirror x2 onto top-left coordinate system.
                    int x2 = s1_len - v2[k2_offset];
                    if (x1 >= x2) {
                        // Overlap detected.
                        delete [] v1;
                        delete [] v2;
                        x_DiffBisectSplit(s1, s2, x1, y1, diffs);
                        return;
                    }
                }
            }
        }

        // Walk the reverse path one step.
        for (int k2 = -d + k2start; k2 <= d - k2end; k2 += 2) {
            const int k2_offset = v_offset + k2;
            int x2;
            if (k2 == -d || (k2 != d && v2[k2_offset - 1] < v2[k2_offset + 1])) {
                x2 = v2[k2_offset + 1];
            } else {
                x2 = v2[k2_offset - 1] + 1;
            }
            int y2 = x2 - k2;
            while (x2 < s1_len && y2 < s2_len &&
                    s1[s1_len - x2 - 1] == s2[s2_len - y2 - 1]) {
                x2++;
                y2++;
            }
            v2[k2_offset] = x2;
            if (x2 > s1_len) {
                // Ran off the left of the graph.
                k2end += 2;
            } else if (y2 > s2_len) {
                // Ran off the top of the graph.
                k2start += 2;
            } else if (!front) {
                int k1_offset = v_offset + delta - k2;
                if (k1_offset >= 0 && k1_offset < v_length && v1[k1_offset] != -1) {
                    int x1 = v1[k1_offset];
                    int y1 = v_offset + x1 - k1_offset;
                    // Mirror x2 onto top-left coordinate system.
                    x2 = s1_len - x2;
                    if (x1 >= x2) {
                        // Overlap detected.
                        delete [] v1;
                        delete [] v2;
                        x_DiffBisectSplit(s1, s2, x1, y1, diffs);
                        return;
                    }
                }
            }
        }
    }
    delete [] v1;
    delete [] v2;
    // Diff took too long and hit the deadline or
    // number of diffs equals number of characters, no commonality at all.
    diffs.Append(DIFF_DELETE, s1);
    diffs.Append(DIFF_INSERT, s2);
}


/// Given the location of the 'middle snake', split the diff in two parts
/// and recurse.
/// @param s1 
///   Old string to be diffed.
/// @param s2
///   New string to be diffed.
/// @param x
///   Index of split point in 's1'.
/// @param y
///   Index of split point in 's2'.
/// @param diffs
///   Resulting list of the diff operations.
/// @sa Diff, x_DiffBisect
/// @private
void CDiff::x_DiffBisectSplit(CTempString s1, CTempString s2, int x, int y, CDiffList& diffs) const
{
    CTempString s1a = s1.substr(0, x);
    CTempString s2a = s2.substr(0, y);
    CTempString s1b = s1.substr(x);
    CTempString s2b = s2.substr(y);
    // Compute both diffs serially.
    x_Diff(s1a, s2a, diffs);
    x_Diff(s1b, s2b, diffs);
}


/// Find the differences between two texts.
/// Assumes that the texts do not have any common prefix or suffix.
/// @param s1 
///   Old string to be diffed.
/// @param s2
///   New string to be diffed.
/// @param diffs
///   Resulting list of the diff operations.
/// @sa Diff
/// @private
void CDiff::x_Diff(CTempString s1, CTempString s2, CDiffList& diffs) const
{
    if (s1.empty()) {
        // Just add some text (speedup)
        diffs.Append(DIFF_INSERT, s2);
        return;
    }
    if (s2.empty()) {
        // Just delete some text (speedup)
        diffs.Append(DIFF_DELETE, s1);
        return;
    }
    {
        // Shorter text is inside the longer text (speedup).
        const CTempString long_str  = s1.length() > s2.length() ? s1 : s2;
        const CTempString short_str = s1.length() > s2.length() ? s2 : s1;
        const size_type i = long_str.find(short_str);
        if ( i != NPOS) {
            const CDiffOperation::EType op = (s1.length() > s2.length()) ? DIFF_DELETE : DIFF_INSERT;
            diffs.Append(op, long_str.substr(0, i));
            diffs.Append(DIFF_EQUAL, short_str);
            diffs.Append(op, long_str.substr(i + short_str.length()));
            return;
        }
        if (short_str.length() == 1) {
            // Single character string.
            // After the previous speedup, the character can't be an equality.
            diffs.Append(DIFF_DELETE, s1);
            diffs.Append(DIFF_INSERT, s2);
            return;
        }
    }

    // Check to see if the problem can be split in two
    TDiffHalfMatchList hm(5);
    if ( x_DiffHalfMatch(s1, s2, hm) ) {
        // A half-match was found. Send both pairs off for separate processing
        CDiffList diffs_a, diffs_b;
        x_Diff(hm[0], hm[2], diffs_a);
        x_Diff(hm[1], hm[3], diffs_b);
        // Merge the results
        diffs.m_List = diffs_a.m_List;
        diffs.Append(DIFF_EQUAL, hm[4]);
        // difs += diffs_b
        diffs.m_List.insert(diffs.m_List.end(), diffs_b.m_List.begin(), diffs_b.m_List.end());
        return;
    }

    // Perform a real diff
    x_DiffBisect(s1, s2, diffs);
    return;
}


// Hash map to convert strings to line numbers.
// We use std::string here instead of CTempString because 
// to have full compatibility with 'patch' utility we need 
// a "correct" diff. to get that we should add "\n" to all
// hashed strings in the text except last one.
typedef hash_map<string, CDiffList::size_type > TStringToLineNumMap;

CDiffList& CDiffText::Diff(CTempString s1, CTempString s2, TFlags flags)
{
    // Check incompatible flags
    if ( F_ISSET(fCleanup + fRemoveEOL) ) {
        NCBI_THROW(CDiffException, eBadFlags, "Usage of incompatible flags fCleanup and fRemoveEOL");
    }
    Reset();

    // Set deadline time if timeout is specified
    auto_ptr<CDeadline> real_deadline;
    if (!m_Timeout.IsInfinite()) {
        real_deadline.reset(new CDeadline(m_Timeout));
        m_Deadline = real_deadline.get();
    }

    // Split both strings to lines
    // (use '\n' as delimiter, for CRLF cases '\r' will be part of CTempString)
    vector<CTempString> lines; 
    size_type s1_num_lines, s2_num_lines;
    NStr::Split(s1, "\n", lines, NStr::fSplit_NoMergeDelims);
    s1_num_lines = lines.size();
    NStr::Split(s2, "\n", lines, NStr::fSplit_NoMergeDelims);
    s2_num_lines = lines.size() - s1_num_lines;

    // Create a map of unique strings with its positions in lines[].
    // We reserve as maximum size for hash_map and use insert_noresize()
    // to avoid its re-balancing on an elements insertion.
    // insert_noresize() have better performance than: hm[lines[i]] = i
    TStringToLineNumMap hm(lines.size());
    for (size_type i = 0; i < lines.size(); i++) {
        string s = lines[i];
        size_type len = s.length();
        if (F_ISSET(fIgnoreEOL)  &&  len  && s[len-1] == '\r') {
            // Remove trailing 'r' if present
            s.resize(len-1);
        }
        // Add trailing "\n" back to all lines except last one.
        if (i != s1_num_lines-1  &&  i != lines.size()-1) {
            s += "\n";
        }
        hm.insert_noresize(TStringToLineNumMap::value_type(s, i));
    }

    // "Convert" both our texts to an array of integer indexes of
    // each line in lines[]. Each number/index represent unique string.
    typedef vector<size_type> TSequence;
    TSequence idx1(s1_num_lines);
    TSequence idx2(s2_num_lines);

    for (size_type i = 0; i < s1_num_lines; i++) {
        TStringToLineNumMap::const_iterator it;
        string s = lines[i];
        size_type len = s.length();
        if (F_ISSET(fIgnoreEOL)  &&  len  && s[len-1] == '\r') {
            // Remove trailing 'r' if present
            s.resize(len-1);
        }
        // Add trailing "\n" back to all lines except last one.
        if (i < s1_num_lines-1) {
            s += "\n";
        }
        it = hm.find(s);
        _VERIFY(it != hm.end());
        idx1[i] = it->second;
    }
    for (size_type i = 0; i < s2_num_lines; i++) {
        size_type pos = s1_num_lines + i;
        TStringToLineNumMap::const_iterator it;
        string s = lines[pos];
        size_type len = s.length();
        if (F_ISSET(fIgnoreEOL)  &&  len  && s[len-1] == '\r') {
            // Remove trailing 'r' if present
            s.resize(len-1);
        }
        // Add trailing "\n" back to all lines except last one.
        if (i < s2_num_lines-1) {
            s += "\n";
        }
        it = hm.find(s);
        _VERIFY(it != hm.end());
        idx2[i] = it->second;
    }

    // Run a diff for integer vectors
    dtl::Diff<size_type> d(idx1, idx2);
    if (s1_num_lines > 10000  &&  s2_num_lines > 10000) {
        d.onHuge();
    }
    d.compose();
    vector< pair<size_type, dtl::elemInfo> > ses_seq = d.getSes().getSequence();
    vector< pair<size_type, dtl::elemInfo> >::iterator it;

    // Convert obtained line-num-based diff to string-based.
    // We cannot use strings from hash_map directly, as well
    // as indexes to get line from lines[], because "equal"
    // lines can have a difference in EOLs, and accordingly
    // -- different lengths, that can be fatal for CTempString.
    // Instead, we will use our own counters for line positions for
    // both texts, assuming that DELETE and COMMON parts refers
    // to the first text, and ADD -- to the second.
    // Resulting diff should have all lines from both texts anyway.

    // Positions in the lines[] for 's1' and 's2'
    size_type pos1 = 0;
    size_type pos2 = s1_num_lines;

    // Lists of insertions and deletions
    CDiffList::TList insertions, deletions;

    for (it = ses_seq.begin(); it != ses_seq.end(); it++)
    {
        // Type of current difference
        dtl::edit_t d_type = it->second.type;
        CDiffOperation::EType op_type = DIFF_EQUAL;
        // Position of the current string in lines[]
        size_type pos = 0;
        size_type line1 = NPOS, line2 = NPOS;
        bool is_last_line = false;

        switch (d_type) {
        case dtl::SES_ADD :
            _VERIFY(pos2 < lines.size());
            op_type = DIFF_INSERT;
            pos = pos2;
            line2 = pos2 - s1_num_lines + 1;
            is_last_line = (line2 >= s2_num_lines);
            break;
        case dtl::SES_DELETE :
            _VERIFY(pos1 < s1_num_lines);
            op_type = DIFF_DELETE;
            pos = pos1;
            line1 = pos1 + 1;
            is_last_line = (line1 >= s1_num_lines);
            break;
        case dtl::SES_COMMON :
            _VERIFY(pos1 < s1_num_lines);
            _VERIFY(pos2 < lines.size());
            op_type = DIFF_EQUAL;
            pos = pos1;
            line1 = pos1 + 1;
            line2 = pos2 - s1_num_lines + 1;
            is_last_line = (line2 >= s2_num_lines);
            break;
        default :
            _TROUBLE;
        }

        // Get string
        CTempString& s = lines[pos];
        size_type len = s.length();
        size_type real_len = len;
        if (!is_last_line) {
            real_len++;
        }
        if ( F_ISSET(fRemoveEOL) ) {
            if (s[len-1] == '\r') {
                len--;
            }
        } else {
            // Restore truncated '\n' on NStr::Tokenize()
            len = real_len;
        }
        CTempString str(s.data(), len);

        // Add a difference into the list. Use Append(CDiffOperation),
        // 'str' can be an empty line and Append("") will be ignored.
        // The dtl::Diff() produce a diff where insertions go before
        // deletions -- we will fix this, swapping it in the result.

        CDiffOperation op(op_type, str);
        op.SetLength(real_len);
        op.SetLine(CDiffOperation::SPos(line1, line2));

        switch (d_type) {
        case dtl::SES_ADD :
            insertions.push_back(op);
            pos2++;
            break;
        case dtl::SES_DELETE :
            deletions.push_back(op);
            pos1++;
            break;
        case dtl::SES_COMMON :
            if (!deletions.empty()) {
                m_Diffs.m_List.insert(m_Diffs.m_List.end(), deletions.begin(), deletions.end());
                deletions.clear();
            }
            if (!insertions.empty()) {
                m_Diffs.m_List.insert(m_Diffs.m_List.end(), insertions.begin(), insertions.end());
                insertions.clear();
            }
            m_Diffs.Append(op);
            pos1++;
            pos2++;
            break;
        }
    }
    if (!deletions.empty()) {
        m_Diffs.m_List.insert(m_Diffs.m_List.end(), deletions.begin(), deletions.end());
    }
    if (!insertions.empty()) {
        m_Diffs.m_List.insert(m_Diffs.m_List.end(), insertions.begin(), insertions.end());
    }
    _VERIFY(pos1 == s1_num_lines);
    _VERIFY(pos2 == lines.size());

    // Post-process diff list
    if ( F_ISSET(fCleanup) ) {
        if ( !IsTimeoutExpired() ) {
            m_Diffs.CleanupAndMerge();
        }
    }
    if ( F_ISSET(fCalculateOffsets) ) {
        // Do not check timeout here, it is fast and
        // can be executed even in this case
        m_Diffs.CalculateOffsets();
    }
    // Cleanup
    m_Deadline = NULL;
    // Return result
    return m_Diffs;
}


// Print the hunk, ranged with iterators, in unified format to 'out'.
//
CNcbiOstream& s_PrintUnifiedHunk(CNcbiOstream& out,
                                 CDiffList::TList::const_iterator start_iter,
                                 CDiffList::TList::const_iterator end_iter,
                                 CDiffList::size_type hunk_s1,
                                 CDiffList::size_type hunk_s2)
{
    const char eol = '\n';
    unsigned long n1 = 0;  // Number of lines in first text
    unsigned long n2 = 0;  // Number of lines in second text
    _VERIFY(hunk_s1);
    _VERIFY(hunk_s2);

    // Find numbers for header
    CDiffList::TList::const_iterator it = start_iter;
    while (it != end_iter)
    {
        string op;
        switch (it->GetOperation()) {
            case DIFF_INSERT:
                n2++;
                break;
            case DIFF_DELETE:
                n1++;
                break;
            case DIFF_EQUAL:
                n1++;
                n2++;
                break;
        }
        it++;
    }
    
    // @@ -s1,n1 +s2,n2 @@
    out << "@@ -" << (unsigned long)hunk_s1 << "," << n1 << " +" 
                  << (unsigned long)hunk_s2 << "," << n2 << " @@" << eol;

    // Print lines
    it = start_iter;
    while (it != end_iter)
    {
        string op;
        switch (it->GetOperation()) {
            case DIFF_INSERT:
                op = "+";
                break;
            case DIFF_DELETE:
                op = "-";
                break;
            case DIFF_EQUAL:
                op = " ";
                break;
        }
        out << op << it->GetString() << eol;
        it++;
    }
    return out;
}


CNcbiOstream& CDiffText::PrintUnifiedDiff(CNcbiOstream& out,
                                          CTempString text1, CTempString text2,
                                          unsigned int num_common_lines)
{
    if (!out.good()) {
        return out;
    }
    const CDiffList::TList& diffs = Diff(text1, text2, fRemoveEOL).GetList();
    if (diffs.empty()) {
        return out;
    }

    // Assign iterators to current, insertion, deletion and equal diffs
    CDiffList::TList::const_iterator it         = diffs.begin();
    CDiffList::TList::const_iterator hunk_start = diffs.end();
    CDiffList::TList::const_iterator hunk_end   = diffs.end();

    bool is_hunk = false;        // TRUE if we found a hunk
    unsigned int num_equal = 0;  // Current number of common lines
    
    // Line numbers (current and hunk's start).
    // We cannot get it from CDiffOperation in common case, so we should
    // calculate it, counting all operations.
    size_type s1 = 0;
    size_type s2 = 0;
    size_type hunk_s1 = 0;
    size_type hunk_s2 = 0;

    // Find hunks
    while (it != diffs.end()) {
        switch (it->GetOperation()) {
            case DIFF_INSERT:
            case DIFF_DELETE:
                if (it->GetOperation() == DIFF_DELETE) {
                    s1++;
                } else {
                    s2++;
                }
                if (is_hunk) {
                    num_equal = 0;
                } else {
                    is_hunk = true;
                    if (!num_common_lines  ||  !num_equal) {
                        hunk_start = it;
                        hunk_s1 = s1;
                        hunk_s2 = s2;
                        if (it->GetOperation() == DIFF_DELETE) {
                            // Don't have INSERT or EQUAL blocks, the line number
                            // for second text need to be updated
                            hunk_s2++;
                        } else {
                            // Don't have DELETE or EQUAL blocks, the line number
                            // for second text need to be updated
                            hunk_s1++;
                        }
                    }
                }
                break;
            case DIFF_EQUAL:
                s1++;
                s2++;
                if (is_hunk) {
                    // Try to find end of the current hunk
                    if (num_common_lines == 0) {
                        hunk_end = it;
                        if (!s_PrintUnifiedHunk(out, hunk_start, hunk_end, hunk_s1, hunk_s2)) {
                            return out;
                        }
                        is_hunk = false;
                    } else if (!num_equal) {
                        hunk_end = it;
                        num_equal++;
                    } else if (num_equal <= num_common_lines) {
                        hunk_end++;
                        num_equal++;
                    } else {
                        // Found new hunk -- write it
                        if (!s_PrintUnifiedHunk(out, hunk_start, hunk_end, hunk_s1, hunk_s2)) {
                            return out;
                        }
                        is_hunk = false;
                        num_equal = 0;
                    }
                } else {
                    // Update iterator to the first common line that can be a part of next hunk
                    if (!num_equal) {
                        hunk_start = it;
                        hunk_s1 = s1;
                        hunk_s2 = s2;
                        num_equal++;
                    } else if (num_equal < num_common_lines) {
                        num_equal++;
                    } else {
                        hunk_start++;
                        hunk_s1 = s1;
                        hunk_s2 = s2;
                    }
                }
                break;
        } // switch
        it++;
    } // while

    if (is_hunk) {
        s_PrintUnifiedHunk(out, hunk_start, diffs.end(), hunk_s1, hunk_s2);
    }
    return out;
}


//////////////////////////////////////////////////////////////////////////////
//
//  CDiffException
//

const char* CDiffException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
        case eEmpty:      return "eEmpty";
        case eBadFlags:   return "eBadFlags";
        default:          return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
