Files in this directory and sub-directories are in one of the following forms:

1. <test_name>.agp
   * This mandatory file is the input fed to CAgpToSeqEntry.
2. <test_name>.expected_seq_entry.#
   * This is the expected output from CAgpToSeqEntry, numbered starting at 0.
   * If no expected_seq_entry file is given, it means that CAgpToSeqEntry
     is expected to fail and output no Seq-entries.
3. <test_name>.flags
   * This optional file holds lines that indicate various special conditions,
     one per line.
     * For example, a line with "fSetSeqGap" (no quotes in file) means to 
       set the fSetSeqGap flag in CAgpToSeqEntry.  Similarly for the flag
       fForceLocalId.
     * More possible lines may be added in the future, such as specifying particular
       custom CAgpErrs classes.

In the future, this should be expanded to allow us to expect certain errors so
we can check the error-checking code.

Or, maybe .expected_seq_entry. files could instead list an error instead 
of a Seq-entry when that's what's expected.

