#! /bin/sh

cat <<EOF
Seq-entry ::= set {
  seq-set {
EOF

length=1000
from=400
to=600
for i in 0 1 2 3 4 5 6 7 8 9; do
    gi="$i"500
    cat <<EOF
    seq {
      id {
        gi $gi
      },
      inst {
        repr delta,
        mol dna,
        ext delta {
          literal {
            length $length
          }
        }
      },
      annot {
        {
          data ftable {
            {
              data region "$gi: $from-$to",
              location int {
                from $from,
                to $to,
                id gi $gi
              }
            },
            {
              data region "$gi: $from-$to+",
              partial TRUE,
              location int {
                from $from,
                to $to,
                id gi $gi,
                fuzz-to lim gt
              }
            },
            {
              data region "$gi:+$from-$to",
              partial TRUE,
              location int {
                from $from,
                to $to,
                id gi $gi,
                fuzz-from lim lt
              }
            },
            {
              data region "$gi:+$from-$to+",
              partial TRUE,
              location int {
                from $from,
                to $to,
                id gi $gi,
                fuzz-from lim lt,
                fuzz-to lim gt
              }
            },
            {
              data region "$gi: $to-$from",
              location int {
                from $from,
                to $to,
                strand minus,
                id gi $gi
              }
            },
            {
              data region "$gi:+$to-$from",
              partial TRUE,
              location int {
                from $from,
                to $to,
                strand minus,
                id gi $gi,
                fuzz-to lim gt
              }
            },
            {
              data region "$gi: $to-$from+",
              partial TRUE,
              location int {
                from $from,
                to $to,
                strand minus,
                id gi $gi,
                fuzz-from lim lt
              }
            },
            {
              data region "$gi:+$to-$from+",
              partial TRUE,
              location int {
                from $from,
                to $to,
                strand minus,
                id gi $gi,
                fuzz-from lim lt,
                fuzz-to lim gt
              }
            }
          }
        }
      }
    },
EOF
done
cat <<EOF
    seq {
      id {
        other {
          accession "MASTER",
          version 1
        },
        gi 1
      },
      inst {
        repr seg,
        mol dna,
        topology circular,
        ext seg {
          int {
            from 500,
            to 999,
            id gi 9500
          },
          int {
            from 0,
            to 999,
            id gi 0500
          },
          int {
            from 0,
            to 999,
            strand minus,
            id gi 1500
          },
          int {
            from 0,
            to 449,
            id gi 2500
          },
          int {
            from 0,
            to 449,
            strand minus,
            id gi 3500
          },
          int {
            from 500,
            to 999,
            id gi 4500
          },
          int {
            from 500,
            to 999,
            strand minus,
            id gi 5500
          },
          int {
            from 450,
            to 549,
            id gi 6500
          },
          int {
            from 450,
            to 549,
            strand minus,
            id gi 7500
          },
          int {
            from 0,
            to 999,
            id gi 8500
          },
          int {
            from 0,
            to 499,
            id gi 9500
          }
        }
      }
    }
  }
}
EOF
