INSERT INTO accession_to_ipg JSON '
    {"accession": "EGB0689184.1", "created": "2020-08-11 14:39:04.000Z", "ipg": 642300}';
INSERT INTO accession_to_ipg JSON '
    {"accession": "EGB0709986.1", "created": "2020-08-11 14:21:00.000Z", "ipg": 216356}';

-- Test data for CIpgStorageTaskFetchReportTest.StatusNew
-- Test data for CIpgStorageTaskFetchReportTest.NormalWithFilter
INSERT INTO ipg_report_config JSON '
    {"ipg": 216356, "subgroups_status": 1, "subgroups_hash": 99}';    
INSERT INTO ipg_report JSON '    
    {"ipg": 216356, "accession": "EGB0688235.1", "nuc_accession": "AAVJDK010000003.1", "assembly": "", "bioproject": "PRJNA248792",
     "cds": [195756, 196181, 1], "compartment": 1, "created": "2020-08-11 14:20:00.000Z", "def_line": "", "div": "BCT", "flags": 0,
     "gb_state": 0, "length": 141, "product_name": "TonB system transport protein ExbD", "pubmedids": null, "src_db": 5, "src_refseq": [],
     "strain": "970289", "taxid": 149539, "updated": "2020-08-11 14:20:28.000Z", "weights": [15488.084150000002]}';
INSERT INTO ipg_report JSON '
    {"ipg": 216356, "accession": "EGB0709986.1", "nuc_accession": "AAVJDV010000007.1", "assembly": "", "bioproject": "PRJNA230403",
     "cds": [197412, 197837, 1], "compartment": 1, "created": "2020-08-11 14:21:00.000Z", "def_line": "", "div": "BCT", "flags": 0,
     "gb_state": 0, "length": 141, "product_name": "TonB system transport protein ExbD", "pubmedids": null, "src_db": 5, "src_refseq": [],
     "strain": "PNUSAS159687", "taxid": 28901, "updated": "2020-08-11 14:20:45.000Z", "weights": [15488.084150000002]}';
     
-- Test data for CIpgStorageTaskFetchReportTest.HugeEmpty
INSERT INTO ipg_report_config JSON '
    {"ipg": 1111, "subgroups_status": 2, "subgroups_hash": 99, "subgroups": null}';
    
-- CIpgStorageTaskFetchReportTest.Huge
-- CIpgStorageTaskFetchReportTest.HugeWithFilter
-- CIpgStorageTaskResolveWritetimeTest
INSERT INTO ipg_report_config JSON '
    {"ipg": 642300, "subgroups_status": 2, "subgroups_hash": 1, "subgroups": [6, 1]}';
INSERT INTO ipg_report_huge JSON '    
    {"ipg": 642300, "subgroup": 1, "accession": "EGB0689184.1", "nuc_accession": "AAVJDK010000006.1", "assembly": "", "bioproject": "PRJNA248792",
     "cds": [180390, 181301, 2], "compartment": 1, "created": "2020-08-11 14:20:00.000Z", "def_line": "", "div": "BCT", "flags": 0,
     "gb_state": 0, "length": 303, "product_name": "glutathione ABC transporter permease GsiD", "src_db": 5, "src_refseq": [],
     "strain": "970289", "taxid": 149539, "updated": "2020-08-11 14:20:28.000Z", "weights": [33012.9892]}' USING TIMESTAMP 1650572783000000;
INSERT INTO ipg_report_huge JSON '    
    {"ipg": 642300, "subgroup": 1, "accession": "EGB0689185.1", "nuc_accession": "", "assembly": "", "bioproject": "PRJNA248792",
     "cds": [180390, 181301, 2], "compartment": 1, "created": "2020-08-11 14:20:00.000Z", "def_line": "", "div": "BCT", "flags": 0,
     "gb_state": 0, "length": 303, "product_name": "glutathione ABC transporter permease GsiD", "src_db": 5, "src_refseq": [],
     "strain": "970289", "taxid": 149539, "updated": "2020-08-11 14:20:28.000Z"}';
INSERT INTO ipg_report_huge JSON '    
    {"ipg": 642300, "subgroup": 6, "accession": "EGB0689183.1", "nuc_accession": "", "assembly": "", "bioproject": "PRJNA248792",
     "cds": [180390, 181301, 2], "compartment": 1, "created": "2020-08-11 14:20:00.000Z", "def_line": "", "div": "BCT", "flags": 0,
     "gb_state": 0, "length": 303, "product_name": "glutathione ABC transporter permease GsiD", "src_db": 5, "src_refseq": [],
     "strain": "970289", "taxid": 149539, "updated": "2020-08-11 14:20:28.000Z"}';
     
-- CIpgStorageTaskFetchReportTest.HugeDisabled
INSERT INTO ipg_report_config JSON '
    {"ipg": 2222, "subgroups_status": 2, "subgroups_hash": 1, "subgroups": [6, 1]}';
INSERT INTO ipg_report JSON '    
    {"ipg": 2222, "accession": "EGB0689184.1", "nuc_accession": "AAVJDK010000006.1", "assembly": "", "bioproject": "PRJNA248792",
     "cds": [180390, 181301, 2], "compartment": 1, "created": "2020-08-11 14:20:00.000Z", "def_line": "", "div": "BCT", "flags": 0,
     "gb_state": 0, "length": 303, "product_name": "glutathione ABC transporter permease GsiD", "src_db": 5, "src_refseq": [],
     "strain": "970289", "taxid": 149539, "updated": "2020-08-11 14:20:28.000Z", "weights": [33012.9892]}';
     
-- Test data for CIpgStorageTaskFetchReportRangeTest.FetchNormal
-- Data range 9122941424592257440:9122941424592257442

-- Test data for CIpgStorageTaskFetchReportRangeTest.FetchHuge
-- Data range TOKEN(ipg) > 9122753677821853769 and TOKEN(ipg) < 9130897194740463872
INSERT INTO ipg_report_config JSON '
    {"ipg": 4649267, "subgroups_status": 1, "subgroups_hash": 1, "subgroups": [6, 1]}';
INSERT INTO ipg_report_config JSON '
    {"ipg": 847375, "subgroups_status": 2, "subgroups_hash": 1, "subgroups": [1,3,7,9]}';
INSERT INTO ipg_report_config JSON '
    {"ipg": 18498125, "subgroups_status": 2, "subgroups_hash": 1, "subgroups": [6]}';
INSERT INTO ipg_report JSON '
    {"ipg": 4649267, "accession": "EGB0860198.1", "nuc_accession": "AAVJFJ010000017.1", "bioproject": "PRJNA283914",
     "cds": [107372, 109069, 1], "compartment": 1, "created": "2020-08-11 14:23:00.000Z", "def_line": "", "div": "BCT",
     "flags": 0, "gb_state": 0, "length": 565, "product_name": "protein-disulfide reductase DsbD", "pubmedids": null,
     "src_db": 5, "src_refseq": [], "strain": "PNUSAE057417", "taxid": 562, "updated": "2020-08-11 14:22:45.000Z", "weights": [61654.19845000001]}';
INSERT INTO ipg_report JSON '
    {"ipg": 4649267, "accession": "EGB0862362.1", "nuc_accession": "AAVJFJ010000088.1", "bioproject": "PRJNA283914",
     "cds": [26487, 28184, 1], "compartment": 1, "created": "2020-08-11 14:23:00.000Z", "def_line": "", "div": "BCT",
     "flags": 0, "gb_state": 0, "length": 565, "product_name": "protein-disulfide reductase DsbD", "pubmedids": null,
     "src_db": 5, "src_refseq": [], "strain": "PNUSAE057417", "taxid": 562, "updated": "2020-08-11 14:22:45.000Z", "weights": [61654.19845000001]}';
---- This record should be skipped if HUGE is enabled
INSERT INTO ipg_report JSON '    
    {"ipg": 847375, "accession": "EGB0689184.1", "nuc_accession": "AAVJDK010000006.1", "bioproject": "PRJNA248792",
     "cds": [180390, 181301, 2], "compartment": 1, "created": "2020-08-11 14:20:00.000Z", "def_line": "", "div": "BCT",
    "flags": 0, "gb_state": 0, "length": 303, "product_name": "glutathione ABC transporter permease GsiD", "pubmedids": null,
     "src_db": 5, "src_refseq": [], "strain": "970289", "taxid": 149539, "updated": "2020-08-11 14:20:28.000Z", "weights": [33012.9892]}';
INSERT INTO ipg_report_huge JSON '        
    {"ipg": 847375, "subgroup": 1, "accession": "EGB0710995.1", "nuc_accession": "AAVJDV010000013.1", "bioproject": "PRJNA230403",
     "cds": [60260, 61171, 2], "compartment": 1, "created": "2020-08-11 14:21:00.000Z", "def_line": "", "div": "BCT",
     "flags": 0, "gb_state": 0, "length": 303, "product_name": "glutathione ABC transporter permease GsiD", "pubmedids": null,
     "src_db": 5, "src_refseq": [], "strain": "PNUSAS159687", "taxid": 28901, "updated": "2020-08-11 14:20:45.000Z", "weights": [33012.9892]}';
INSERT INTO ipg_report_huge JSON '     
    {"ipg": 847375, "subgroup": 1, "accession": "EGB0743882.1", "nuc_accession": "AAVJDX010000007.1", "bioproject": "PRJNA230403",
     "cds": [82045, 82956, 2], "compartment": 1, "created": "2020-08-11 14:21:00.000Z", "def_line": "", "div": "BCT",
     "flags": 0, "gb_state": 0, "length": 303, "product_name": "glutathione ABC transporter permease GsiD", "pubmedids": null,
     "src_db": 5, "src_refseq": [], "strain": "PNUSAS159668", "taxid": 28901, "updated": "2020-08-11 14:20:49.000Z", "weights": [33012.9892]}';
INSERT INTO ipg_report_huge JSON '     
    {"ipg": 847375, "subgroup": 1, "accession": "EGB0747930.1", "nuc_accession": "AAVJDY010000001.1", "bioproject": "PRJNA230403",
     "cds": [1348762, 1349673, 2], "compartment": 1, "created": "2020-08-11 14:21:00.000Z", "def_line": "", "div": "BCT",
     "flags": 0, "gb_state": 0, "length": 303, "product_name": "glutathione ABC transporter permease GsiD", "pubmedids": null,
     "src_db": 5, "src_refseq": [], "strain": "PNUSAS159686", "taxid": 28901, "updated": "2020-08-11 14:20:49.000Z", "weights": [33012.9892]}';
INSERT INTO ipg_report_huge JSON ' 
    {"ipg": 847375, "subgroup": 3, "accession": "EGB0754741.1", "nuc_accession": "AAVJDZ010000012.1", "bioproject": "PRJNA230403",
     "cds": [30047, 30958, 1], "compartment": 1, "created": "2020-08-11 14:21:00.000Z", "def_line": "", "div": "BCT",
     "flags": 0, "gb_state": 0, "length": 303, "product_name": "glutathione ABC transporter permease GsiD", "pubmedids": null,
     "src_db": 5, "src_refseq": [], "strain": "PNUSAS159685", "taxid": 28901, "updated": "2020-08-11 14:20:44.000Z", "weights": [33012.9892]}';
INSERT INTO ipg_report_huge JSON '     
    {"ipg": 847375, "subgroup": 3, "accession": "EGB0759488.1", "nuc_accession": "AAVJEA010000013.1","bioproject": "PRJNA230403",
     "cds": [37588, 38499, 1], "compartment": 1, "created": "2020-08-11 14:21:00.000Z", "def_line": "", "div": "BCT",
     "flags": 0, "gb_state": 0, "length": 303, "product_name": "glutathione ABC transporter permease GsiD", "pubmedids": null,
     "src_db": 5, "src_refseq": [], "strain": "PNUSAS159683", "taxid": 28901, "updated": "2020-08-11 14:20:53.000Z", "weights": [33012.9892]}';
INSERT INTO ipg_report_huge JSON '     
    {"ipg": 847375, "subgroup": 7, "accession": "EGB0762876.1", "nuc_accession": "AAVJEB010000005.1", "bioproject": "PRJNA230403",
     "cds": [220057, 220968, 2], "compartment": 1, "created": "2020-08-11 14:21:00.000Z", "def_line": "", "div": "BCT",
     "flags": 0, "gb_state": 0, "length": 303, "product_name": "glutathione ABC transporter permease GsiD", "pubmedids": null,
     "src_db": 5, "src_refseq": [], "strain": "PNUSAS159666", "taxid": 28901, "updated": "2020-08-11 14:20:56.000Z", "weights": [33012.9892]}';
INSERT INTO ipg_report_huge JSON '   
    {"ipg": 847375, "subgroup": 9, "accession": "EGB0771594.1", "nuc_accession": "AAVJEF010000003.1", "bioproject": "PRJNA230403",
     "cds": [60083, 60994, 2], "compartment": 1, "created": "2020-08-11 14:21:00.000Z", "def_line": "", "div": "BCT",
     "flags": 0, "gb_state": 0, "length": 303, "product_name": "glutathione ABC transporter permease GsiD", "pubmedids": null,
     "src_db": 5, "src_refseq": [], "strain": "PNUSAS159656", "taxid": 28901, "updated": "2020-08-11 14:20:59.000Z", "weights": [33012.9892]}';
INSERT INTO ipg_report_huge JSON '   
    {"ipg": 18498125, "subgroup": 6, "accession": "EGB0000054.1", "nuc_accession": "AAVJEF010000054.1", "bioproject": "PRJNA230403",
     "cds": [60083, 60994, 2], "compartment": 1, "created": "2020-08-11 14:21:00.000Z", "def_line": "", "div": "BCT",
     "flags": 0, "gb_state": 0, "length": 303, "product_name": "glutathione ABC transporter permease GsiD", "pubmedids": null,
     "src_db": 5, "src_refseq": [], "strain": "PNUSAS159656", "taxid": 28901, "updated": "2020-08-11 14:20:59.000Z", "weights": [33012.9892]}';     
     
     
