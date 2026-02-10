#!/bin/env python3

from argparse import ArgumentParser
import ccmlib.cluster
from ccmlib import common
import logging
import subprocess
import sys
import pprint
import os

sys.dont_write_bytecode = True


class CcmManager:

    def __init__(self):
        self.__version = '4.1.10'
        self.__cqlsh = '/netmnt/vast01/seqdb/id_dumps/id_software/cassandra/test/bin/cqlsh'
        self.__cluster_name = 'ipg_storage_test'
        self.__start_timeout = 180
        self.__datasets = {
            "fetch": {
                "keyspace": "test_fetch",
                "data": "dataset_fetch.cql",
            }
        }

    def ExecuteCqlsh(self, node, args):
        subprocess.check_call([self.__cqlsh] + args + [node.address()])

    def PopulateTestCluster(self, node):
        current_dir = os.path.dirname(os.path.realpath(__file__))
        data_dir = os.path.join(current_dir, 'test_data');
        for dataset in self.__datasets.values():
            print(f"Creating keyspace {dataset['keyspace']}")
            self.ExecuteCqlsh(node, ['-e', f"CREATE KEYSPACE {dataset['keyspace']} WITH replication = {{'class': 'SimpleStrategy', 'replication_factor': 1}};"])
            print(f"Creating keyspace tables {dataset['keyspace']}")
            self.ExecuteCqlsh(node, ['-k', dataset['keyspace'], '-f', os.path.join(data_dir, 'ipg_storage.cql')])
            print(f"Loading data {dataset['keyspace']}")
            self.ExecuteCqlsh(node, ['-k', dataset['keyspace'], '-f', os.path.join(data_dir, dataset['data'])])

    def Run(self):
        parser = ArgumentParser()
        parser.add_argument('--binary', dest='binary', help="Binary to execute after cluster is ready", required=True)
        parser.add_argument('--gtest_output', dest='gtest_output', help="Test output", required=True)
        args = parser.parse_args()
        print("Creating test cluster")
        cluster = ccmlib.cluster.Cluster(common.get_default_path(), self.__cluster_name, cassandra_version=self.__version)
        try:
            print("Starting test cluster")
            cluster.populate(1).start(wait_for_binary_proto=False)
            nodelist = cluster.nodelist()
            nodelist[0].wait_for_binary_interface(timeout=self.__start_timeout)
            print("Populating test data")
            self.PopulateTestCluster(nodelist[0])
            current_dir = os.path.dirname(os.path.realpath(__file__))
            os.environ['CASSANDRA_CLUSTER'] = nodelist[0].address()
            os.environ['INI_CONFIG_LOCATION'] = os.path.join(current_dir, 'test_data', '')
            print(f"Executing test binary '{args.binary}'")
            result = subprocess.run([args.binary, f"--gtest_output={args.gtest_output}"])
            return result.returncode
        finally:
            print("Removing test cluster")
            sys.stdout.flush()
            sys.stderr.flush()
            cluster.remove()

        return 0


if __name__ == "__main__":
    exit(CcmManager().Run())
