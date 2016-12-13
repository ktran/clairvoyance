#!/usr/bin/env python

from __future__ import division
import glob, getopt, sys, os, re, csv
import numpy as np
import jobs

def usage():
    print 'Usage:', sys.argv[0], '-d (experiment_dir)'

def get_finished_job_dirs(dir):
    perf_out_files = glob.glob(dir + '/*/*/stderr.txt')
    run_dirs = [os.path.dirname(i) for i in perf_out_files]
    correct_run_dirs = []
    for d in run_dirs:
        if not os.path.exists(os.path.join(d, 'error')):
            correct_run_dirs.append(d)

    return correct_run_dirs
    
def parse_perf(file):
    perf_data = {}
    with open(file, 'r') as f:
        for line in f:
            tokens = re.split('\s+', line.strip())
            if len(tokens) < 2:
                continue
            elif tokens[1] == 'seconds':
                perf_data.update({'total':float(tokens[0])})
            elif re.match('^[,\.\d]+$', tokens[0]):
                perf_data.update({tokens[1]:long(tokens[0].replace(',',''))})
    return perf_data

def parse_dvfs(file):
    dvfs_data = dict.fromkeys(['execute_time', 'prefetch_time'],None)

    with open(file, 'r') as f:
        for line in f:
            tokens = re.split('\s+', line.strip())
            if len(tokens) < 4:
                continue

            elif tokens[1] != 'time':
                continue
            
            elif tokens[0] == 'Compute':
                dvfs_data.update({'execute_time': float(tokens[3])})

            elif tokens[0] == 'PreFetch':
                dvfs_data.update({'prefetch_time': float(tokens[3])})

    return dvfs_data
     
def gather_data(dir):
    data = []
    finished_job_dirs = get_finished_job_dirs(dir)
    for job_dir in finished_job_dirs:
        out_file = os.path.join(job_dir, 'stderr.txt')
        perf_results = parse_perf(out_file)
        dvfs_results = parse_dvfs(out_file)
        jobname = os.path.basename(os.path.basename(job_dir))
        jobname_config = jobs.parse_jobname(jobname)

        job_config = jobs.merge_two_dicts(dvfs_results,
            jobs.merge_two_dicts(perf_results, jobname_config))
        data.append(job_config)
    return data

def normalize(data, benchmarks):
    original_runs = {}
    normalized_data = {}
    for d in data:
        binary = d['binary']
        if d['type'] == 'original':
            original_runs.update({binary:d})

    for d in data:
        if d['type'] != 'original':
            binary = d['binary']
            reference = original_runs[binary]
            for key in d:
                value = d[key]
                if isinstance(value, (long, int)) and key in reference:
                    d[key] = value / reference[key]

def output_data(data, file):
    with open(file, 'wb') as f:  # Just use 'w' mode in 3.x
        w = csv.DictWriter(f, data[0].keys())
        w.writeheader()
        w.writerows(data)

if __name__ == '__main__':
    experiment_dir = None
    out_file = None

    try:
        opts, args = getopt.getopt(sys.argv[1:], "d:o:", ['dir=','outputfile='])
    except getopt.GetoptError, e:
        print e
        usage()
        sys.exit()
    for o, a in opts:
        print 'opts', o, a
        if o == '-h' or o == '--help':
            usage()
            sys.exit()
        if o == '--dir' or o == '-d':
            experiment_dir = a
        if o == '--outputfile' or o == '-o':
            out_file = a

    if args:
        usage()
        sys.exit(-1)

    if not experiment_dir or not os.path.isdir(experiment_dir):
        usage()
        sys.exit(-1) 

    if not out_file:
        out_file =  os.path.join(experiment_dir, 'results.csv')

    data = gather_data(experiment_dir)
    output_data(data, out_file)
    
