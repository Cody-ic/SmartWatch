"""Behavior checks for process restart, rejected data, and prototype arithmetic.
Run: python -m unittest discover -s . -p test_examples.py
"""
import csv
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import numpy as np
from prototype_demo import normalize, prototype
from train_activity import load_windows, make_demo_csv

HERE = Path(__file__).parent


class PrototypeTests(unittest.TestCase):
    def test_query_prefers_matching_center(self):
        centers = np.stack([prototype([[.9,.1],[.8,.2]]),
                            prototype([[.1,.9],[.2,.8]])])
        self.assertEqual(int((centers @ normalize([.7,.3])).argmax()),0)
        self.assertEqual(int((centers @ normalize([.3,.7])).argmax()),1)

    def test_undefined_direction_is_rejected(self):
        for value in ([0,0],[float('nan'),1],[float('inf'),1]):
            with self.assertRaises(ValueError): normalize(value)


class DataTests(unittest.TestCase):
    def test_shapes_and_person_leakage(self):
        with tempfile.TemporaryDirectory() as name:
            path=Path(name)/'demo.csv'
            make_demo_csv(path)
            windows=load_windows(path)
            self.assertEqual(windows['train'][0].shape,(18,6,512))
            self.assertEqual(windows['test'][0].shape,(9,6,512))
            with path.open(encoding='utf-8',newline='') as f:
                reader=csv.DictReader(f);fields=reader.fieldnames;rows=list(reader)
            rows[-1]['person']='p1'
            with path.open('w',encoding='utf-8',newline='') as f:
                writer=csv.DictWriter(f,fieldnames=fields);writer.writeheader();writer.writerows(rows)
            with self.assertRaisesRegex(ValueError,'cross splits'):load_windows(path)


class FileUpgradeTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory()
        self.base=Path(self.temporary.name)
        self.root=self.base/'state'
        self.image=self.base/'new.bin'
        self.image.write_bytes(b'new-demo-app')
        self.run_command('init')

    def tearDown(self):self.temporary.cleanup()

    def run_command(self,*args,ok=True):
        # Every operation runs in a fresh process; no state is shared in memory.
        result=subprocess.run([sys.executable,str(HERE/'ota_files.py'),'--root',str(self.root),*map(str,args)],capture_output=True,text=True)
        if not ok:
            self.assertNotEqual(result.returncode,0);return result
        self.assertEqual(result.returncode,0,result.stderr)
        return json.loads(result.stdout)

    def install_candidate(self):
        self.run_command('begin',self.image)
        self.run_command('chunk',self.image,'--offset',0,'--length',4)
        self.run_command('chunk',self.image,'--offset',4,'--length',8)
        return self.run_command('finish')

    def test_partial_retry_and_rollback(self):
        self.run_command('begin',self.image)
        self.run_command('chunk',self.image,'--offset',0,'--length',4)
        repeated=self.run_command('chunk',self.image,'--offset',0,'--length',4)
        self.assertEqual(repeated['receiving']['written'],4)
        self.run_command('finish',ok=False)
        self.assertEqual(self.run_command('boot')['active'],'slot_a')
        self.run_command('chunk',self.image,'--offset',8,'--length',4,ok=False)
        self.run_command('chunk',self.image,'--offset',4,'--length',8)
        self.run_command('finish')
        self.assertEqual(self.run_command('boot')['active'],'slot_b')
        self.assertEqual(self.run_command('boot')['active'],'slot_a')

    def test_confirmation_survives_restart(self):
        self.install_candidate()
        self.run_command('boot')
        self.run_command('confirm')
        self.assertEqual(self.run_command('boot')['active'],'slot_b')

    def test_bad_content_target_and_version(self):
        self.run_command('begin',self.image,'--target','other-device',ok=False)
        self.run_command('begin',self.image,'--version',1,ok=False)
        self.run_command('begin',self.image)
        self.run_command('chunk',self.image,'--offset',0,'--length',12)
        (self.root/'slot_b.bin').write_bytes(b'broken')
        self.run_command('finish',ok=False)
        self.assertEqual(self.run_command('boot')['active'],'slot_a')


if __name__=='__main__':unittest.main()
