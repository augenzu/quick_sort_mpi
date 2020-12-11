import os
import sys
from time import sleep


def run_task(nprocs, nelms, path):
    wtime = '00:01:00' if nelms < 8192000 and nprocs < 512 else '00:02:00'
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

    ntryes = int(sys.argv[-1])

    for ntry in range(ntryes):
        path = TIMINGS_DIR + str(ntry)

        if not os.path.exists(path):
            os.makedirs(path)

        for nprocs in [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]:
            for nelms in [256000, 512000, 1024000, 2048000, 4096000, 8192000, 16384000, 32768000]:
                run_task(nprocs, nelms, path)

        sleep(20 * 60)  # 20 min


if __name__ == '__main__':
    run_all_tasks()