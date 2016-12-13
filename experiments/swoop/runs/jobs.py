#!/usr/bin/env python

###############################
# Description:
# This file determines which benchmarks to run.
# Each benchmark may have different parameters, see benchmarks.py
###############################

import glob, os, itertools, sys
import benchmarks
from os.path import expanduser

###############################
# FOR ARTIFACT EVALUATION:
#
# Below you can find parameters to change what to run
##############################

# Specify which speculative versions you want to run (here: spec and multi-spec-safe)
swoop_hoist_types = ('spec','multispecsafe')
#swoop_hoist_types = ('specsafe','spec', 'multispec','multispecsafe','consv') # Run all Clairvoyance version

# Specify which LLVM schedulers you want to run (here: only O3).
swoop_schedulers = ('default',)
#swoop_schedulers = ('default','list-ilp', 'list-hybrid', 'list-burr') # Run all (O3 + schedulers)

# Specify which benchmarks you want to run
def bms_current():
  bms = ['429.mcf.kernel','CG','UA','LU']
  return  bms

# Specify which "type" you want to run ('orig' or 'sw' = Clairvoyance)
def bms():
  bms = bms_current() 
  bms_orig = [i + '.orig' for i in bms] #original
  bms_sw = [i + '.sw' for i in bms] #clairvoyance
  return  bms_orig + bms_sw

###############################
# END ARTIFACT EVALUATION
##############################

mail_address='' # example: 'abc@hotmail.de'

LLVM_BIN=os.path.join('/var/www/clairvoyance/compiler/build/llvm-build/')

def get_lib_dir(): 
  return os.path.join(LLVM_BIN, 'lib')

def jobname(bm, inputsize, ncores, coretype):
  return '%s-%s-%s-%s' % (coretype, bm, inputsize, ncores)

def input(bm):
  bm_orig = bm.split('.')[0]
  if bm_orig in rodinia_bms():
    input = 'large'
  elif bm_orig in cpu2006_bms():
    input = 'ref'
  elif bm_orig in polybench_bms():
    input = 'large'
  else:
    input = 'ref'

  return input

frequencies = {'min': 1600000, 'max':3401000, 'ondemand':'ondemand', 'default':'default'}

def merge_two_dicts(x, y):
  z = x.copy()
  z.update(y)
  return z

bm2unroll = benchmarks.bm2unroll
bm2indir = benchmarks.bm2indir

bm_ref_input = benchmarks.bm_ref_input
bm_train_input = benchmarks.bm_train_input
bm_test_input = benchmarks.bm_test_input
bm_input = lambda bm, inp:globals()['bm_' + inp + '_input'][bm]

bm_stdin = benchmarks.bm_stdin
bm_has_stdin = benchmarks.bm_has_stdin
bm_get_stdin = lambda bm, inp_type, index:bm_stdin[bm][inp_type][index]
bm_reference = benchmarks.bm_reference
bm_input_dir = benchmarks.bm_input_dir
bm_get_cmp_file = benchmarks.bm_get_cmp_file

def get_tool(repeat):
  return ['perf', 'stat', '-r', str(repeat), '-e', 'cycles,instructions']

def get_cmd(benchmark, binary, repeat, inp):
  return ['taskset', '-c', '0'] + get_tool(repeat) + ['./' + binary] +  inp


swoop_hoists = lambda bm:bm2hoists[bm] or (None,)
swoop_unroll = lambda bm:bm2unroll[bm] or (None,)
swoop_indir = lambda bm:bm2indir[bm] or (None,)
swoop_unroll_counts = (4, 8)
input_types = benchmarks.input_types
functions = benchmarks.functions

def get_binary_name(bm, swoop_hoist, swoop_unroll, indir = 1000, scheduler = 'default'):
  unroll = bm.endswith('.unr')
  sw_pref = bm.endswith('.pref')
  sw_swoop = bm.endswith('.sw')
  name = os.path.splitext(bm)[0]
  if unroll:
      name += '.unr' + str(swoop_unroll) + '.' + 'cae'
  elif sw_swoop:
      name += '.unr' + str(swoop_unroll) + '.' + 'indir' + str(indir) + '.' + swoop_hoist
  elif scheduler != 'default':
      name += '.sched' + scheduler + '.sched'
  elif sw_pref:
    name += '.pref'
  else:
    name += '.original'
  return name

def get_bm_input_dir(benchmark_src_dir, input_type):
  return os.path.join(benchmark_src_dir, 'data', bm_input_dir[input_type], 'input')

def get_bm_output_dir(benchmark_src_dir, input_type):
  return os.path.join(benchmark_src_dir, 'data', bm_input_dir[input_type], 'output')

def get_bm_bin_dir(benchmark_src_dir):
  return os.path.join(benchmark_src_dir, 'bin')

def get_bm_reference(benchname, input_type, input_counter, inp, stdin_input):
  if benchname in benchmarks.bms():
    return benchmarks.get_bm_reference(benchname, input_type, input_counter, inp, stdin_input)
  else:
    return None

def parse_IR_file_name(file_path):
  job_info = dict.fromkeys(['unroll', 'benchmark', 'file', 'type', 'hoist', 'indirection'],None)
  basename = os.path.basename(file_path)
  directory = os.path.dirname(file_path)
  config = basename.split('.')
  job_info.update({'file': config[0], 'benchmark': directory.split('/')[-2]})

  for info in config:
    if info in swoop_hoist_types:
      if info == 'dae':
        job_info.update({'type': 'dae', 'hoist': info})
      else:
        job_info.update({'type': 'swoop', 'hoist': info})
    elif info == 'cae':
        job_info.update({'type': 'unroll', 'hoist': None})
    elif info == 'pref':
        job_info.update({'type': 'pref', 'hoist': None})
    elif info == 'marked':
        job_info.update({'type': 'original', 'hoist': None})
    elif info.startswith('unr'):
        job_info.update({'unroll': int(info[3:])})
    elif info.startswith('indir'):
        job_info.update({'indirection': int(info[5:])})

  return job_info

def parse_jobname(jobname):
  job_info = dict.fromkeys(['unroll', 'benchmark', 'type', 'host', 'sched', 'hoist', 'frequency', 'indirection', 'input'],None)
  job_info.update({'sched':'default'})
  hw_sw_config = jobname.split('_', 1)

  # It might be that the jobname contains an underscore "_".
  # In that case: merge index 1 and 2
  hw_config = hw_sw_config[0]
  sw_config = hw_sw_config[1]

  # parse hw config
  print hw_config
  frequency = hw_config.split('-')[-1]
  host = ''.join(hw_config.split('-')[:-1])
  job_info.update({'host': host, 'frequency':frequency})

  sw_config = sw_config.split('-')
  job_info.update({'binary':sw_config[0]})
  job_info.update({'benchmark':os.path.splitext(sw_config[0])[0]})

  
  specs = sw_config[1:]
  for token in specs:
    if token.startswith('u'):
      unroll_count = int(token.split('u')[1])
      job_info.update({'unroll':unroll_count})
    if token.startswith(input_types):
      job_info.update({'input':token})
    if token.startswith('h'):
      hoist_type = token[1:]
      job_info.update({'hoist':hoist_type})
    if token.startswith('s'):
      sched_type = token[1:].replace('.','-')
      print sched_type
      job_info.update({'sched':sched_type})
    if token.startswith('i'):
      indirection = int(token[1:])
      job_info.update({'indirection':indirection})

  if sw_config[0].endswith('sw'):
    if hoist_type == 'dae':
      type = 'dae'
    else:
      type = 'swoop'
  elif sw_config[0].endswith('unr'):
    type ='unroll'
  elif sw_config[0].endswith('orig'):
    type = 'original'
  elif sw_config[0].endswith('pref'):
    type = 'pref'

  print jobname
  job_info.update({'type':type})

  return job_info

def make_jobname(bm, inputsize, host_type = None, frequency = 'max', swoop_hoist = None, swoop_unroll = None, indir = 1000, scheduler = 'default'):
  swoop = bm.endswith('.sw')
  original = bm.endswith('.orig')

  hw_config_name = host_type + '-' + str(frequency)
  
  sw_config_name = bm
  if swoop:
    sw_config_name += '-i'+ str(indir)
  if swoop and swoop_hoist != None:
    sw_config_name += '-h'+str(swoop_hoist)
  if swoop_unroll != None:
    sw_config_name += '-u'+str(swoop_unroll)

  if original:
    if scheduler != 'default':
      sw_config_name += '-s' + scheduler.replace('-','.')

  return '%s_%s-%s' % (hw_config_name, sw_config_name, inputsize)

def get_host():
   return socket.gethostname()
