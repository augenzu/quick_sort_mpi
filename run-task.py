import os
from pathlib import Path
import sys
import time


def run_task(nprocs, nelms, path):
    wtime = '00:01:00' if nelms < 8192000 else '00:02:01'
    fname = str(nprocs) + '-' + str(nelms)
    fout = path + '/' + fname + '.out'
    ferr = path + '/' + fname + '.err'

    cmd = 'mpisubmit.bg' + \
            ' -n ' + str(nprocs) + \
            ' -w ' + wtime + \
            ' --stdout ' + fout + \
            ' --stderr ' + ferr + \
            ' main -- ' + str(nelms)

    os.system(cmd)


TIMINGS_DIR = './timings/'


if __name__ == '__main__':
    nelms_list = list(map(int, sys.argv[1:-1]))
    try_num = sys.argv[-1]

    path = TIMINGS_DIR + try_num

    Path(path).mkdir(parents=True, exist_ok=True)

    for nprocs in [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]:
        for nelms in nelms_list:
            run_task(nprocs, nelms, path)