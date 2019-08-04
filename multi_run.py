#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Fri Jun 14 22:51:12 2019

@author: irhamta
"""

import os
import multiprocessing
import tqdm
import numpy as np


def process(ind, genInputTrees=False):

    script = 'ps1_rndReg_v1.py'

    if genInputTrees==True:
        command = 'python scripts/'+script+' --randomRegression --genInputTrees --isBatch'
    else:
        command = 'nohup python scripts/'+script+' --randomRegression --train --isBatch --trainIndex=%i' %ind

    with open('train_{}.sh'.format(ind), 'w') as text_file:
        text_file.write('#!/bin/bash')
        text_file.write('\n\nexport PYTHONPATH=/home/andika/annz:$PYTHONPATH')
        text_file.write('\nexport PYTHONPATH=/home/irhamta/annz:$PYTHONPATH')
        text_file.write('\nsource ../root/bin/thisroot.sh')
        text_file.write('\nsource activate astro')
        text_file.write('\n'+command)

    os.system('bash train_{}.sh'.format(ind))
    os.system('rm train_{}.sh'.format(ind))

if __name__ == '__main__':


    print ('Execute multiprocess pool')
    n_cpu = 32
    pool = multiprocessing.Pool(n_cpu)

    temp_data = []
    for _ in tqdm.tqdm(pool.imap(process, np.arange(100)), total=100):
        temp_data.append(_)
        pass

    # close the pool
    pool.close()
    pool.join()
