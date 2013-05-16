#!/usr/bin/python

import shlex, sys
from subprocess import Popen, PIPE

if len(sys.argv) < 2:
    print 'usage %s [start|stop|ps|init]' % (sys.argv[0])
    sys.exit(-1)

START = '"./fedora-bin/xia-core/experiments/planetlab/test_infrastructure.py ./fedora-bin/xia-core/experiments/planetlab/tunneling.ini"'
STOP = '"sudo killall sh; sudo ~/fedora-bin/xia-core/bin/xianet stop; sudo killall mapper_client.py"'
PS = '"ps -ae"'
INIT = '"wget http://www.cs.cmu.edu/~mmukerje/planetlab/init.sh && chmod 755 ./init.sh && ./init.sh && %s"' % (START.replace('"', ''))

if sys.argv[1] == 'start':
    cmd = START
elif sys.argv[1] == 'stop':
    cmd = STOP
elif sys.argv[1] == 'ps':
    cmd = PS
elif sys.argv[1] == 'init':
    cmd = INIT

machines = open('machines','r').read().split('\n')
for machine in machines:
    try:
        f = open('/tmp/%s-log' % (machine),'w')
        c = 'ssh cmu_xia@%s %s' % (machine, cmd)
        print c
        process = Popen(shlex.split(c),stdout=f,stderr=f)
    except:
        pass
