CREATE TABLE accession_to_ipg (
    accession ascii PRIMARY KEY,
    created timestamp,
    hash blob,
    ipg bigint
) WITH compaction = {'sstable_size_in_mb': '160', 'class': 'LeveledCompactionStrategy'}
    AND compression = {};

CREATE TABLE ipg_report (
    ipg bigint,
    accession ascii,
    nuc_accession ascii,
    assembly ascii,
    cds frozen<tuple<int, int, int>>,
    created timestamp,
    div ascii,
    length int,
    product_name text,
    src_db int,
    src_refseq frozen<list<ascii>>,
    taxid int,
    updated timestamp,
    strain text,
    gb_state int,
    weights frozen<list<double>>,
    bioproject text,
    compartment int,
    flags int,
    pubmedids frozen<list<int>>,
    def_line text,
    PRIMARY KEY (ipg, accession, nuc_accession)
) WITH CLUSTERING ORDER BY (accession ASC, nuc_accession ASC)  
    AND compaction = {'sstable_size_in_mb': '160', 'class': 'LeveledCompactionStrategy'}
    AND compression = {};
    
CREATE TABLE ipg_report_huge (
    ipg bigint,
    subgroup int,
    accession ascii,
    nuc_accession ascii,
    assembly ascii,
    cds frozen<tuple<int, int, int>>,
    created timestamp,
    div ascii,
    length int,
    product_name text,
    src_db int,
    src_refseq frozen<list<ascii>>,
    taxid int,
    updated timestamp,
    strain text,
    gb_state int,
    weights frozen<list<double>>,
    bioproject text,
    compartment int,
    flags int,
    pubmedids frozen<list<int>>,
    def_line text,
    PRIMARY KEY ((ipg, subgroup), accession, nuc_accession)
) WITH CLUSTERING ORDER BY (accession ASC, nuc_accession ASC)     
    AND compaction = {'sstable_size_in_mb': '160', 'class': 'LeveledCompactionStrategy'}
    AND compression = {'sstable_compression': 'LZ4Compressor', 'chunk_length_kb': 64, 'crc_check_chance': 1.0};
 
CREATE TABLE ipg_report_config (
    ipg bigint,
    subgroups_hash int,
    subgroups set<int>,
    subgroups_status int,
    updated timestamp,
    PRIMARY KEY (ipg)
) WITH compaction = {'sstable_size_in_mb': '160', 'class': 'LeveledCompactionStrategy'}
    AND compression = {};

