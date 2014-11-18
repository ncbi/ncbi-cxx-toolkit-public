Submit-block ::= {
  contact {
    contact {
      name name {
        last "Clark",
        first "Karen",
        middle "",
        initials "",
        suffix "",
        title ""
      },
      affil std {
        affil "NIH",
        div "NCBI",
        city "Bethesda",
        sub "Maryland",
        country "United States of America",
        street "Center Drive",
        email "kclark@ncbi.nlm.nih.gov",
        postal-code "20895"
      }
    }
  },
  cit {
    authors {
      names std {
        {
          name name {
            last "clark",
            first "karen",
            middle "",
            initials "",
            suffix "",
            title ""
          }
        }
      },
      affil std {
        affil "NIH",
        div "NCBI",
        city "Bethesda",
        sub "Maryland",
        country "United States of America",
        street "Center Drive",
        postal-code "20895"
      }
    }
  },
  subtype new
}
Seqdesc ::= pub {
  pub {
    gen {
      cit "unpublished",
      authors {
        names std {
          {
            name name {
              last "clark",
              first "karen",
              middle "",
              initials "",
              suffix "",
              title ""
            }
          }
        }
      },
      title "testing fasta"
    }
  }
}
Seqdesc ::= user {
  type str "DBLink",
  data {
    {
      label str "BioProject",
      num 1,
      data strs {
        "PRJNA262839"
      }
    },
    {
      label str "BioSample",
      num 1,
      data strs {
        "SAMN02068351"
      }
    }
  }
}
