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
    print('Submit task {0}-{1}'.format(nprocs, nelms))


def run_all_tasks(sleep_min=3):
    TIMINGS_DIR = './timings/'

    ntry = sys.argv[-1]

    path = TIMINGS_DIR + ntry

    if not os.path.exists(path):
        os.makedirs(path)

    for nprocs in [2, 4, 8, 16, 32, 64, 128, 256, 512]:#, 1024, 2048]:
        for nelms in [256000, 512000, 1024000, 2048000]:
            run_task(nprocs, nelms, path, expected_time=1)
        print('Sleep for {} min...'.format(sleep_min))
        sleep(sleep_min * 60)
        for nelms in [4096000, 8192000]:
            run_task(nprocs, nelms, path, expected_time=2)
        print('Sleep for {} min...'.format(sleep_min))
        sleep(sleep_min * 60)
        for nelms in [16384000, 32768000]:
            run_task(nprocs, nelms, path, expected_time=4)
        print('Sleep for {} min...'.format(sleep_min))
        sleep(sleep_min * 60)


if __name__ == '__main__':
    run_all_tasks()
    print('All the tasks have been submitted successfully')