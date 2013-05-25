#!/usr/bin/python

import sys
from subprocess import Popen, PIPE, check_call
from os.path import splitext

if len(sys.argv) < 3:
    print 'usage %s [topo_file] [start|stop]' % (sys.argv[0])
    sys.exit(-1)

cmd_output = open(splitext(sys.argv[1])[0]+'.ini', 'w')
check_call('./generate_commands.py %s' % (sys.argv[1]), stdout=cmd_output)

STOP = '"sudo killall sh; sudo killall init.sh; sudo killall rsync; sudo ~/fedora-bin/xia-core/bin/xianet stop; sudo killall mapper_client.py; sudo killall local_server.py"'
START = '"curl https://raw.github.com/XIA-Project/xia-core/develop/experiments/planetlab/init.sh > ./init.sh && chmod 755 ./init.sh && ./init.sh && ./fedora-bin/xia-core/experiments/planetlab/test_infrastructure.py ./fedora-bin/xia-core/experiments/planetlab/tunneling.ini"'
LS = '"ls"'
RM = '"rm -rf ~/*; rm -rf ~/.*"'

if sys.argv[2] == 'start':
    cmd = START
elif sys.argv[2] == 'stop':
    cmd = STOP
elif sys.argv[2] == 'ls':
    cmd = LS
elif sys.argv[2] == 'rm':
    cmd = RM

machines = open('machines','r').read().split('\n')
processes = []
for machine in machines:
    try:
        name = machine.split('#')[1].strip()
        if(len(sys.argv) == 4 and sys.argv[3] != name):
            continue
        machine = machine.split('#')[0].strip()
        f = open('/tmp/%s-log' % (name),'w')
        c = 'ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 cmu_xia@%s %s' % (machine, cmd)
        print c
        processes.append((name,Popen(c,shell=True,stdout=f,stderr=f)))
    except:
        pass

while True:
     for process in processes:
         process[1].communiate()
         if process[1].poll() is not None:
             if process[1].returncode is not 0:
                 # process[0] is the name of the machine
                 # need to re generate topo and such
                 rpyc.connect("localhost", 43278).root.error(process[0])
                 print process[1].returncode
             processes.remove(process)
     time.sleep(1)
     if len(processes) is 0:
         break
