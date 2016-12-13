#!/usr/bin/env python
# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
import os

###############################
# Description:
# This file determines which parameters to choose for
# each benchmark (unroll count, indirection count)
###############################

def bms():
    return ['999.specrand','403.gcc','433.milc','410.bwaves','458.sjeng','400.perlbench','436.cactusADM','444.namd','483.xalancbmk','437.leslie3d','453.povray','401.bzip2','462.libquantum','459.GemsFDTD','482.sphinx3','434.zeusmp','447.dealII','454.calculix','435.gromacs','473.astar','471.omnetpp','464.h264ref','998.specrand','450.soplex','470.lbm','456.hmmer','465.tonto','429.mcf','481.wrf','445.gobmk','416.gamess','331.art_l','CG','UA','LU','429.mcf.kernel']

###############################
# FOR ARTIFACT EVALUATION:
#
# BENCHMARK.sw = Clairvoyance
# BENCHMARK.orig = O3/LLVM schedulers
#
# Here you can change unroll & indirection count
# Note: this is not *building* the binaries, just
# trying to *run* these versions. If the binaries
# are not yet built, nothing will happen.
##############################

bm2unroll = {
    '429.mcf.kernel.sw': (8,),
    'UA.sw': (2,),
    'LU.sw': (4,),
    'CG.sw': (4,),
    '331.art_l.sw': (2,3),
    '403.gcc.sw': (2,),
    '400.perlbench.sw': (1,2,4),
    '444.namd.sw': (1,2,4),
    '483.xalancbmk.sw': (1,2,4),
    '453.povray.sw': (1,2,4),
    '447.dealII.sw': (1,2,4),
    '471.omnetpp.sw': (1,2,4),
    '445.gobmk.sw': (1,2,4),
    '401.bzip2.sw': (1,2,4),
    '429.mcf.sw': (1,2,42),
    '470.lbm.sw': (1,2,4),
    '433.milc.sw': (1,2,4),
    '450.soplex.sw': (1,2,4),
    '482.sphinx3.sw': (1,2,4),
    '473.astar.sw': (1,2,4),
    '462.libquantum.sw': (1,2,4),
    '458.sjeng.sw': (1,2,4),
    '464.h264ref.sw': (1,2,4),
    '456.hmmer.sw': (1,2,4),

    '429.mcf.kernel.orig': (None,),
    'UA.orig': (None,),
    'LU.orig': (None,),
    'CG.orig': (None,),
    '331.art_l.orig': (None,),
    '403.gcc.orig': (None,),
    '400.perlbench.orig': (None,),
    '444.namd.orig': (None,),
    '483.xalancbmk.orig': (None,),
    '453.povray.orig': (None,),
    '447.dealII.orig': (None,),
    '471.omnetpp.orig': (None,),
    '445.gobmk.orig': (None,),
    '401.bzip2.orig': (None,),
    '429.mcf.orig': (None,),
    '470.lbm.orig': (None,),
    '433.milc.orig': (None,),
    '450.soplex.orig': (None,),
    '482.sphinx3.orig': (None,),
    '473.astar.orig': (None,),
    '462.libquantum.orig': (None,),
    '458.sjeng.orig': (None,),
    '464.h264ref.orig': (None,),
    '456.hmmer.orig': (None,),
}
bm2indir = {
    '429.mcf.kernel.sw': (0,),
    'UA.sw': (0,),
    'LU.sw': (0,),
    'CG.sw': (1,),
    '331.art_l.sw': (3,),
    '403.gcc.sw': (2,),
    '400.perlbench.sw': (0,1,2,3),
    '444.namd.sw': (0,1,2,3),
    '483.xalancbmk.sw': (0,1,2,3),
    '453.povray.sw': (0,1,2,3),
    '447.dealII.sw': (0,1,2,3),
    '471.omnetpp.sw': (0,1,2,3),
    '445.gobmk.sw': (0,1,2,3),
    '401.bzip2.sw': (0,1,2,3),
    '429.mcf.sw': (0,1,2,3,6),
    '470.lbm.sw': (0,1),
    '433.milc.sw': (0,1),
    '450.soplex.sw': (0,1,2,3,4,5,6),
    '482.sphinx3.sw': (0,1,2,4),
    '473.astar.sw': (0,1,4,5),
    '462.libquantum.sw': (0,1,2),
    '458.sjeng.sw': (0,1,2,3),
    '464.h264ref.sw': (0,1,2),
    '456.hmmer.sw': (0,1,2),

    '429.mcf.kernel.orig': (None,),
    'UA.orig': (None,),
    'LU.orig': (None,),
    'CG.orig': (None,),
    '331.art_l.orig': (None,),
    '403.gcc.orig': (None,),
    '400.perlbench.orig': (None,),
    '444.namd.orig': (None,),
    '483.xalancbmk.orig': (None,),
    '453.povray.orig': (None,),
    '447.dealII.orig': (None,),
    '471.omnetpp.orig': (None,),
    '445.gobmk.orig': (None,),
    '401.bzip2.orig': (None,),
    '429.mcf.orig': (None,),
    '470.lbm.orig': (None,),
    '433.milc.orig': (None,),
    '450.soplex.orig': (None,),
    '482.sphinx3.orig': (None,),
    '473.astar.orig': (None,),
    '462.libquantum.orig': (None,),
    '458.sjeng.orig': (None,),
    '464.h264ref.orig': (None,),
    '456.hmmer.orig': (None,),
}

###############################
# END ARTIFACT EVALUATION
##############################

bm_stdin = {
    '433.milc': {'ref':['su3imp.in'], 'train':['su3imp.in'], 'test':['su3imp.in']},
    '445.gobmk': {'ref':['13x13.tst','nngs.tst','score2.tst','trevorc.tst','trevord.tst'], 'train': ['arb.tst', 'arend.tst', 'arion.tst', 'atari_atari.tst', 'blunder.tst', 'buzco.tst', 'nicklas2.tst', 'nicklas4.tst'],'test':['capture.tst', 'connect.tst', 'connect_rot.tst', 'connection.tst', 'connection_rot.tst', 'cutstone.tst', 'dniwog.tst']},
}

def bm_has_stdin(bm):
    if bm in bm_stdin:
        return True
    return False

bm_ref_input = {
    '429.mcf.kernel': [[]],
    'UA': [[]],
    'LU': [[]],
    'CG': [[]],
    '331.art_l': [['-scanfile', 'c756hel.in', '-trainfile1', 'a10.img', '-trainfile2', 'hc.img', '-stride', '2', '-startx', '120', '-starty', '70', '-endx', '520', '-endy', '270', '-objects', '2000']],
    '447.dealII': [['23']],
    '483.xalancbmk': [['-v', 't5.xml', 'xalanc.xsl']],
    '471.omnetpp': [['omnetpp.ini']],
    '453.povray': [['SPEC-benchmark-ref.ini']],
    '444.namd': [['--input', 'namd.input', '--iterations', '38', '--output', 'namd.out']],
    '445.gobmk': [['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp']],
    '403.gcc': [['166.i','-o', '166.s'], ['200.i','-o', '200.s'], ['c-typeck.i','-o', 'c-typeck.s'], ['cp-decl.i','-o', 'cp-decl.s'], ['expr.i','-o', 'expr.s'], ['expr2.i','-o', 'expr2.s'], ['g23.i','-o', 'g23.s'], ['s04.i','-o', 's04.s'], ['scilab.i','-o', 'scilab.s']],
    '400.perlbench': [['-I./lib', 'checkspam.pl', '2500', '5', '25', '11', '150', '1', '1', '1', '1'], ['-I./lib', 'diffmail.pl', '4', '800', '10', '17', '19', '300'], ['-I./lib', 'splitmail.pl', '1600', '12', '26', '16', '4500']],
    '401.': [[]],
    '401.bzip2': [['input.source', '280'], ['chicken.jpg', '30'], ['liberty.jpg', '30'], ['input.program', '280'], ['text.html', '280'], ['input.combined', '200']],
    '429.mcf': [['inp.in']],
    '470.lbm': [['3000', 'reference.dat', '0', '0', '100_100_130_ldc.of']],
    '433.milc': [[]],
    '450.soplex': [['-s1', '-e','-m45000', 'pds-50.mps'], ['-m3500', 'ref.mps']],
    '482.sphinx3': [['ctlfile', '.', 'args.an4']],
    '473.astar': [['BigLakes2048.cfg'], ['rivers.cfg']],
    '462.libquantum': [['1397', '8']],
    '458.sjeng': [['ref.txt']],
    '464.h264ref': [['-d','foreman_ref_encoder_baseline.cfg'],['-d','foreman_ref_encoder_main.cfg'],['-d','sss_encoder_main.cfg']],
    '456.hmmer': [['nph3.hmm', 'swiss41'],['--fixed', '0', '--mean', '500', '--num', '500000', '--sd', '350', '--seed', '0', 'retro.hmm']],
}

bm_train_input = {
    '429.mcf.kernel': [[]],
    'UA': [[]],
    'LU': [[]],
    'CG': [[]],
    '331.art_l': [['-scanfile', 'c756hel.in', '-trainfile1', 'a10.img', '-trainfile2', 'hc.img', '-stride', '2', '-startx', '110', '-starty', '200', '-endx', '160', '-endy', '240', '-objects', '10'],['-scanfile', 'c756hel.in', '-trainfile1', 'a10.img', '-trainfile2', 'hc.img', '-stride', '2', '-startx', '470', '-starty', '140', '-endx', '520', '-endy', '180', '-objects', '10']],
    '447.dealII': [['10']],
    '483.xalancbmk': [['-v', 'allbooks.xml', 'xalanc.xsl']],
    '471.omnetpp': [['omnetpp.ini']],
    '453.povray': [['SPEC-benchmark-train.ini']],
    '444.namd': [['--input', 'namd.input', '--iterations', '1', '--output', 'namd.out']],
    '445.gobmk': [['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp']],
    '403.gcc': [['integrate.i', '-o', 'integrate.s']],
    '400.perlbench': [['-I./lib', 'diffmail.pl', '2', '550', '15', '24', '23', '100'], ['-I./lib', 'perfect.pl', 'b', '3'], ['-I.', '-I./lib', 'scrabbl.pl']],
    '401.bzip2': [['input.program', '10'], ['byoudoin.jpg', '5'], ['input.combined', '80']],
    '429.mcf': bm_ref_input['429.mcf'],
    '470.lbm': [['300', 'reference.dat', '0', '1', '100_100_130_cf_b.of']],
    '433.milc': bm_ref_input['433.milc'],
    '450.soplex': [['-s1', '-e','-m5000', 'pds-20.mps'], ['-m1200', 'train.mps']],
    '482.sphinx3': bm_ref_input['482.sphinx3'],
    '473.astar': [['BigLakes1024.cfg'], ['rivers1.cfg']],
    '462.libquantum': [['143', '25']],
    '458.sjeng': [['train.txt']],
    '464.h264ref': [['-d','foreman_train_encoder_baseline.cfg']],
    '456.hmmer': [['--fixed', '0', '--mean', '325', '--num', '5000', '--sd', '200', '--seed', '0', 'leng100.hmm']], 
}


bm_test_input = {
    '429.mcf.kernel': [[]],
    'UA': [[]],
    'LU': [[]],
    'CG': [[]],
    '331.art_l': [['-scanfile', 'c756hel.in', '-trainfile1', 'a10.img', '-trainfile2', 'hc.img', '-stride', '2', '-startx', '134', '-starty', '220', '-endx', '184', '-endy', '240', '-objects', '3']],
    '447.dealII': [['8']],
    '483.xalancbmk': [['-v', 'test.xml', 'xalanc.xsl']],
    '471.omnetpp': [['omnetpp.ini']],
    '453.povray': [['SPEC-benchmark-test.ini']],
    '444.namd': [['--input', 'namd.input', '--iterations', '1', '--output', 'namd.out']],
    '445.gobmk': [['--quiet', '--mode', 'gtp'], ['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp'],['--quiet', '--mode', 'gtp']],
    '403.gcc': [['cccp.i', '-o', 'cccp.s']],
    '400.perlbench': [['-I.', '-I./lib', 'attrs.pl'], ['-I.', '-I./lib', 'gv.pl'], ['-I.', '-I./lib', 'makerand.pl'], ['-I.', '-I./lib', 'pack.pl'], ['-I.', '-I./lib', 'redef'], ['-I.', '-I./lib', 'ref.pl'], ['-I.', '-I./lib', 'regmesg.pl'], ['-I.', '-I./lib', 'test.pl']],
    '401.bzip2': [['input.program', '5'],['dryer.jpg', '2']],
    '429.mcf': bm_ref_input['429.mcf'],
    '470.lbm': [['20', 'reference.dat', '0', '1', '100_100_130_cf_a.of']],
    '433.milc': bm_ref_input['433.milc'],
    '450.soplex': [['-m10000', 'test.mps']],
    '482.sphinx3': bm_ref_input['482.sphinx3'],
    '473.astar': [['lake.cfg']],
    '462.libquantum': [['33', '5']],
    '458.sjeng': [['test.txt']],
    '464.h264ref': [['-d','foreman_test_encoder_baseline.cfg']],
    '456.hmmer': [['--fixed', '0', '--mean', '325', '--num', '5000', '--sd', '200', '--seed', '0', 'bombesin.hmm']],
}

bm_reference = {
    '429.mcf': 'inp.out',
    '470.lbm': 'lbm.out',
    '433.milc': 'su3imp.out',
    '482.sphinx3': 'an4.log',
}

bm_input_dir = {
    'test': 'test',
    'train': 'train',
    'ref': 'ref',
    'all': 'all',
}

def bm_get_cmp_file(benchname, stdout, input):
    if benchname == '403.gcc':
        return os.path.join(os.path.dirname(stdout), input[-1])
    if benchname == '444.namd':
        return os.path.join(os.path.dirname(stdout), input[-1])

    return stdout

functions = {
    'CG': ('conj_grad',),
    '447.dealII': ('_ZNK13LaplaceSolver6SolverILi3EE15assemble_matrixERNS1_12LinearSystemERK18TriaActiveIteratorILi3E15DoFCellAccessorILi3EEES9_RN7Threads16DummyThreadMutexE',),
    '444.namd': ('_ZN20ComputeNonbondedUtil26calc_pair_energy_fullelectEP9nonbonded','_ZN20ComputeNonbondedUtil16calc_pair_energyEP9nonbonded','_ZN20ComputeNonbondedUtil32calc_pair_energy_merge_fullelectEP9nonbonded','_ZN20ComputeNonbondedUtil19calc_pair_fullelectEP9nonbonded'),
    '473.xalancbmk': ('_ZN11xercesc_2_510ValueStore13isDuplicateOfEPNS_17DatatypeValidatorEPKtS2_S4_',),
    '471.omnetpp': ('_ZN12cMessageHeap7shiftupEi',),
    '445.gobmk': ('fastlib','do_play_move','do_dfa_matchpad','dfa_matchpat_loop','incremental_order_moves'),
    '403.gcc': ('reg_is_remote_constant_p',),
    '331.art_l': ('compute_train_match','compute_values_match'),
    '429.mcf': ('primal_bea_mpp',),
    '401.bzip2': ('BZ2_compressBlock','BZ2_decompress', 'mainGtU'),
    '433.milc': ('mult_su3_na',),
    '450.soplex': ('_ZN6soplex8SSVector20assign2product4setupERKNS_5SVSetERKS0_',
                   '_ZN6soplex10SPxSteepPR9entered4XENS_5SPxIdEiiiii',
                   '_ZN6soplex8SSVector5setupEv'),
    '456.hmmer': ('P7Viterbi',),
    '458.sjeng': ('std_eval',),
    '462.libquantum': ('quantum_toffoli',),
    '470.lbm' : ('LBM_performStreamCollide',),
    '464.h264ref' : ('SetupFastFullPelSearch',),
    '473.astar': ('_ZN7way2obj12releaseboundEv',
                  '_ZN6wayobj10makebound2EPiiS0_'),
    '482.sphinx3' : ('mgau_eval',),
}

input_types = ('test', 'train', 'ref')

def get_bm_reference(benchname, input_type, input_counter, inp, stdin_input):
  if benchname == '450.soplex':
    input_file = inp[-1]
    basename = input_file.split('.')[0]
    if basename in input_types:
      return basename + '.out'
    else:
      return input_file + '.out'
  if benchname == '473.astar':
    input_file = inp[-1]
    basename = input_file.split('.')[0]
    return basename + '.out'
  if benchname == '464.h264ref':
    input_file = inp[-1]
    input_split = input_file.split('.')[0].split('encoder_')
    return '_'.join([input_split[0] + input_split[1], 'encodelog.out'])
  if benchname == '462.libquantum':
    return input_type + '.out'
  if benchname == '458.sjeng':
    return input_type + '.out'
  if benchname == '456.hmmer':
    if len(inp) > 3:
        input_file = inp[-1]
    else:
        input_file = inp[0]

    basename = input_file.split('.')[0]
    return basename + '.out'
  if benchname == '401.bzip2':
    input_file = inp[0]
    return input_file + '.out'
  if benchname == '400.perlbench':
    input_file = inp[0]
    basename = input_file.split('.')[0]
    rest_input = inp[1:]
    return '.'.join([basename] + rest_input) + '.out'
  if benchname == '445.gobmk':
    input_file = stdin_input.split('.')[0]
    return input_file + '.out'
  if benchname == '403.gcc':
    input_file = inp[0]
    basename = input_file.split('.')[0]
    return basename + '.s'
  if benchname == '444.namd':
    return 'namd.out'
  if benchname == '453.povray':
    input_file = inp[0]
    basename = input_file.split('.')[0]
    return basename + '.out'
  if benchname == '471.omnetpp':
    input_file = inp[0]
    basename = input_file.split('.')[0]
    return basename + '.log'
  if benchname == '483.xalancbmk':
    return input_type + '.out'
  if benchname == '447.dealII':
    return 'log'
  if benchname == '331.art_l':
      return '.'.join([input_type, str(input_counter), 'out'])
  if benchname == 'CG' or benchname == 'LU' or benchname == 'UA' or '429.mcf.kernel':
      return '.'.join([input_type, 'out']) # no real reference output
  else:
    return bm_reference[benchname]
