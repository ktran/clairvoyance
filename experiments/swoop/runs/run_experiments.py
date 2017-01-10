#!/usr/bin/env python
# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.

import os, time, itertools, datetime, getopt
import time, sys, errno, subprocess, filecmp, socket
from itertools import izip
import jobs, mails
import getpass
from shutil import copytree
from collections import namedtuple
from sets import Set

def usage():
    print 'Usage:', sys.argv[0], '[-h (help)]'

def create_dir(path, dirname):
    benchmark_dir = os.path.join(path, dirname)
    if not os.path.exists(benchmark_dir):
            os.makedirs(benchmark_dir)
    return benchmark_dir


def copy_file(src, dst):
    dst_path = os.path.join(dst, os.path.basename(src))
    try:
        os.symlink(src, dst_path)
    except OSError, e:
        if e.errno == errno.EEXIST:
            os.remove(dst_path)
            os.symlink(src, dst_path)
        
def copy_dir(src, dst):
    for content in os.listdir(src):
        copy_file(os.path.join(src, content), dst)
        
def copy(src, dst):
    if os.path.isfile(src):
        copy_file(src, dst)
    else:
        copy_dir(src, dst)

def recursive_copy(src, dest):
    try:
        copytree(src, dest)
    except OSError as e:
    # If the error was caused because the source wasn't a directory
        if e.errno == errno.ENOTDIR:
            copy(src, dest)
        else:
            print('Directory not copied. Error: %s' % e)

def make_dir(directory):
    try:
        os.makedirs(directory)
    except OSError, e:
        if e.errno != 17:
            raise # This was not a "directory exist" error..

def create_experiment_dir(path, jobname, benchname):
#    experiment_dir = os.path.join(path, datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S'))
    experiment_dir = os.path.join(path, 'test')
    make_dir(experiment_dir)
    
    experiment_benchmark_dir = create_dir(experiment_dir, benchname)
    experiment_job_dir = create_dir(experiment_benchmark_dir, jobname)
    return experiment_job_dir

def prepare_jobs(jobname, benchname, binary, inp, stdin_inp, ref_output, frequency, config):
    experiment_job_dir = os.path.join(config['runs_path'], 'test')
    if not config['pretend']:
        experiment_job_dir = create_experiment_dir(config['runs_path'], jobname, benchname)
    
    benchmark_src_dir = os.path.join(config['sources_path'], benchname)
    benchmark_input_dir =  jobs.get_bm_input_dir(benchmark_src_dir, config['input'])
    benchmark_all_input_dir =  jobs.get_bm_input_dir(benchmark_src_dir, 'all')
    benchmark_output_dir =  jobs.get_bm_output_dir(benchmark_src_dir, config['input'])
    benchmark_binary_dir =  jobs.get_bm_bin_dir(benchmark_src_dir)
    binary_path = os.path.join(benchmark_binary_dir, binary)

    if not os.path.exists(binary_path):
        print "No such binary: ", binary
        return None

    if not config['pretend']:
        # copy benchmark data
        if os.path.isdir(benchmark_input_dir):
            copy(benchmark_input_dir, experiment_job_dir)

        if os.path.isdir(benchmark_all_input_dir):
            copy(benchmark_all_input_dir, experiment_job_dir)
        
        # copy binary
        copy(binary_path, experiment_job_dir)

    # create cmd
    cmd = jobs.get_cmd(benchname, binary, config['repeat'], inp)

    # get reference output file
    ref_output_path = os.path.join(benchmark_output_dir, ref_output)

    stdout = os.path.join(experiment_job_dir, 'stdout.txt')
    stderr = os.path.join(experiment_job_dir, 'stderr.txt')
    stdin = None
    if stdin_inp:
        stdin = os.path.join(experiment_job_dir, stdin_inp)

    test_script = os.path.join(experiment_job_dir, 'test_script.sh')
        
    job = {'cmd': cmd, 'dir': experiment_job_dir, 'stdout': stdout, 'cmp_file': jobs.bm_get_cmp_file(benchname, stdout, inp), 'stderr': stderr, 'reference': ref_output_path, 'stdin': stdin, 'frequency': frequency, 'test_script': test_script}
    return job

def job_already_executed(job):
    if not os.path.isdir(job['dir']):
        return False
    
    output_files_exist = os.path.isfile(job['stdout']) and os.path.isfile(job['stderr'])
    errornous_exec = os.path.isfile(os.path.join(job['dir'], 'error'))
    interrupted_exec = output_files_exist and not is_correct_output(job['cmp_file'], job['reference'], job['test_script'])
    return output_files_exist and not errornous_exec and not interrupted_exec

def is_correct_output(output, reference, test_script):
    if os.path.isfile(test_script):
        print 'Using test script to compare run with reference.'
        cmd = [test_script, reference, output]
        return_code = subprocess.call(cmd, cwd=os.path.dirname(output))
        if return_code == 0:
            return True

    # if no such script is given, compare the output files
    same = True
    fillval='-'
    try:
        for ref_line, out_line in itertools.izip_longest(open(reference), open(output), fillvalue=fillval):
            if ref_line == fillval:
                break
            elif out_line == fillval or ref_line.strip() != out_line.strip():
                same = False
                break
    except Exception as e:
        print "Could not compare to reference: ", output,  reference
        same = False

    return same

def create_empty_file(file):
    print 'Validation failed, creating error file.'
    open(file, 'a').close()

def send_mail(subject, content):
    host = socket.gethostname()
    user = getpass.getuser()
    from_address = user + '@' + host + ".it.uu.se"
    to_address = jobs.mail_address
    mails.send_mail(subject, from_address, to_address, content)

def create_report_entry(jobname, success, time):
    report = [jobname + '\t\t']
    
    if success:
        report.append('success')
    else:
        report.append('failure')

    report.append(str(time))

    return '\t'.join(report)

def reset_frequency_change():
    try:
        subprocess.call(['./cfreq.sh', 'ondemand'])
        print 'Set frequency to ondemand frequency'
    except:
        print 'Failed resetting frequency.'

    time.sleep(1)
    

def change_frequency(old_freq, desired_freq):
    if desired_freq == 'default':
        return True

    if old_freq == desired_freq:
        return True

    success = True

    try:
        print desired_freq
        subprocess.call(['./cfreq.sh', 'userspace', str(desired_freq)])
        print 'Set frequency to ', desired_freq
    except Exception as e:
        success = False
        print 'Failed setting frequency.'

    time.sleep(1)
    return success
    
def submit_jobs(experiments, dry_run, mail_notification):
    # set environment variables for spec omp benchmarks
    print os.path.join(jobs.get_lib_dir(), 'libomp.so')
    if not os.path.isfile(os.path.join(jobs.get_lib_dir(), 'libomp.so')):
        raise  IOError('Cannot find LLVM libomp.so! Set LLVM_BIN in jobs.py')

    os.environ['LD_LIBRARY_PATH'] = jobs.get_lib_dir()
    os.environ['OMP_NUM_THREADS'] = '1'
    
    report = []
    current_frequency = 'default'

    if mail_notification and not dry_run:
        subject = 'Starting experiments..'
        content = ''
        send_mail(subject, content)

    for job in experiments:
        jobname = os.path.basename(job['dir'])
        print 'Job: ', jobname
        if job_already_executed(job):
            print 'Job already executed. Skipping.. '
            continue
        
        print 'Processing..  ', ' '.join(job['cmd'])

        if dry_run:
            continue

        if current_frequency != job['frequency']:
            changed = change_frequency(current_frequency, job['frequency'])
            if changed:
                print 'Changed frequency to ', job['frequency']
                current_frequency = job['frequency']
        
        stdout = open(job['stdout'], 'wb')
        stderr = open(job['stderr'], 'wb')
        stdin = None

        if (job['stdin']):
            stdin = open(job['stdin'], 'r')
            
        error_file = os.path.join(job['dir'], 'error')

        # Try to run job, in case of an exception dump error file
        try:
            print 'Running..  ', ' '.join(job['cmd'])
            start_time = time.time()
            return_code = subprocess.call(job['cmd'], cwd=job['dir'], stdout=stdout, stderr=stderr, stdin=stdin)
            end_time = time.time()
        except (KeyboardInterrupt, SystemExit):
            if current_frequency != 'default':
                reset_frequency_change()
            create_empty_file(error_file)
            subject = 'Experiments failed!'
            content = ''
            if mail_notification:
                send_mail(subject, content)

            raise

        success = return_code == 0 and is_correct_output(job['cmp_file'], job['reference'], job['test_script'])
        if not success:
            create_empty_file(error_file)
        elif os.path.exists(error_file):
            os.remove(error_file)

        subject = jobname + ' finished: ' + str(success) + ' time: ' + str(end_time - start_time)
        content = ''

        if mail_notification:
            send_mail(subject, content)

        report.append(create_report_entry(jobname, success, end_time - start_time))
        
        stdout.close()
        stderr.close()

    if current_frequency != 'default':
        reset_frequency_change()
    return report
    
def create_jobs(config):
    processed = Set([])
    experiments_to_run = []
    for bm in config['bms']:
        for swoop_unroll, swoop_hoist, swoop_indir, scheduler, frequency_type in itertools.product(config['swoop_unroll'](bm), config['swoop_hoist_types'], config['swoop_indir'](bm), config['swoop_scheduler'], config['frequencies']):
            if scheduler != 'default':
                if not bm.endswith('orig'):
                    # for now: only allow instruction scheduling
                    # techniques for non-swoop and non-unroll
                    continue

            frequency = jobs.frequencies[frequency_type]
            benchname = os.path.splitext(bm)[0]
            input_counter = 1
            for inp in jobs.bm_input(benchname, config['input']):
                input_name = config['input'] + str(input_counter)
                jobname = jobs.make_jobname(bm, input_name, socket.gethostname(), frequency, swoop_hoist, swoop_unroll, swoop_indir, scheduler)
                if jobname in processed:
                    print jobname
                    # Already processed
                    continue

                # Add job to set of jobs that are already processed
                processed.add(jobname)

                binary = jobs.get_binary_name(bm, swoop_hoist, swoop_unroll, swoop_indir, scheduler)
                stdin_input = None
                if jobs.bm_has_stdin(benchname):
                    stdin_input = jobs.bm_get_stdin(benchname, config['input'], input_counter - 1)
                ref_output = jobs.get_bm_reference(benchname, config['input'], input_counter, inp, stdin_input)
                job = prepare_jobs(jobname, benchname, binary, inp, stdin_input, ref_output, frequency, config)
                
                if not job:
                    print "Something went wrong. Skipping job"
                    continue
                
                experiments_to_run.append(job)

                input_counter = input_counter + 1
            
    return experiments_to_run

if __name__ == '__main__':
    file_dir = os.path.dirname(os.path.realpath(__file__))
    config = {}
    config['swoop_unroll'] = lambda bm:jobs.bm2unroll.get(bm)
    config['swoop_indir'] = lambda bm:jobs.bm2indir.get(bm)
    config['sources_path'] = os.path.join(file_dir, '../sources')
    config['runs_path'] = file_dir
    config['swoop_hoist_types'] = jobs.swoop_hoist_types
    config['swoop_scheduler'] = jobs.swoop_schedulers
    config['frequencies'] = ('default',)

    bms = jobs.bms()
    repeat = 1
    pretend = False
    input_type = 'test'
    mail_notification = False

    try:
        opts, args = getopt.getopt(sys.argv[1:], "shni:mr:", ['help','benchmarks=','mail','input=','repeat='])
    except getopt.GetoptError, e:
        print e
        usage()
        sys.exit()
    for o, a in opts:
        print 'opts', o, a
        if o == '-h' or o == '--help':
            usage()
            sys.exit()
        if o == '--benchmarks' or o == '-b':
            bms = tuple(a.split(','))
        if o == '-n':
            pretend = True
        if o == '--mail' or o == '-m':
            mail_notification = True
        if o == '--input' or o =='-i':
            input_type = a
        if o == '-s':
            bms = jobs.bms_orig()
        if o == '--repeat=' or o == '-r':
            repeat = int(a)

    if args:
        usage()
        sys.exit(-1)

    if input_type:
        if input_type in jobs.input_types:
            config['input'] = input_type
        else:
            print 'No such input type: ', input_type, ' use one of ', jobs.input_types
            sys.exit()

    config['bms'] = bms
    config['repeat'] = repeat
    config['pretend'] = pretend
    config['mail_notification'] = mail_notification

    experiments = create_jobs(config)
    report = submit_jobs(experiments, config['pretend'], config['mail_notification'])

    if mail_notification and not pretend:
        subject = 'Experiments finished'
        content = '\n'.join(report)
        print content
        send_mail(subject, content)
