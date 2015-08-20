Submit-block ::= {
  contact {
    contact {
      name name {
        last "Gobu",
        first "Vasuki",
        middle "",
        initials "",
        suffix "",
        title ""
      },
      affil std {
        affil "NIH",
        div "NCBI",
        city "Bethesda",
        sub "MD",
        country "United States of America",
        street "45, Center Drive",
        email "gobu@ncbi.nlm.nih.gov",
        postal-code "20814"
      }
    }
  },
  cit {
    authors {
      names std {
        {
          name name {
            last "Gobu",
            first "Vasuki",
            middle "",
            initials "",
            suffix "",
            title ""
          }
        },
        {
          name name {
            last "Kornbluh",
            first "Mike",
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
        sub "MD",
        country "United States of America",
        street "45, Center Drive",
        postal-code "20814"
      }
    }
  },
  subtype new,
  comment "SUB175178"
}
Seqdesc ::= pub {
  pub {
    article {
      title {
        name "Testing backend submit2genbank script"
      },
      authors {
        names std {
          {
            name name {
              last "Gobu",
              first "Vasuki",
              middle "",
              initials "",
              suffix "",
              title ""
            }
          },
          {
            name name {
              last "Sinyakov",
              first "Denis",
              middle "",
              initials "",
              suffix "",
              title ""
            }
          }
        }
      },
      from journal {
        title {
          jta "SMART scripts"
        },
        imp {
          date std {
            year 2014
          },
          volume "",
          issue "",
          pages "",
          prepub in-press
        }
      }
    }
  }
}
Seqdesc ::= user {
  type str "Submission",
  data {
    {
      label str "AdditionalComment",
      data str "GAP:unknown length: introns"
    }
  }
}
Seqdesc ::= user {
  type str "Submission",
  data {
    {
      label str "AdditionalComment",
      data str "ALT EMAIL:vasuki_gobu@yahoo.com"
    }
  }
}
Seqdesc ::= user {
  type str "Submission",
  data {
    {
      label str "AdditionalComment",
      data str "Submission#: SUB175178"
    }
  }
}
