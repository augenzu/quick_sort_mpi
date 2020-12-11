import os
import sys
from time import sleep


def run_task(nprocs, nelms, path, expected_time=1):
    wtime = '00:0' + str(expected_time) + ':00'
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


def run_all_tasks():
    TIMINGS_DIR = './timings/'

    ntry = sys.argv[-1]

    path = TIMINGS_DIR + ntry

    if not os.path.exists(path):
        os.makedirs(path)

    for nprocs in [2, 4, 8, 16, 32, 64, 128, 256, 512]:#, 1024, 2048]:
        for nelms in [256000, 512000, 1024000, 2048000]:
            run_task(nprocs, nelms, path, expected_time=1)
        sleep(5 * 60)
        for nelms in [4096000, 8192000]:
            run_task(nprocs, nelms, path, expected_time=2)
        sleep(5 * 60)
        for nelms in [16384000, 32768000]:
            run_task(nprocs, nelms, path, expected_time=4)
        sleep(5 * 60)


if __name__ == '__main__':
    run_all_tasks()