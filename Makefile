all: distpsnotify remoteagent

distpsnotify: distpsnotify.cc
	g++ distpsnotify.cc -g -o distpsnotify

remoteagent: remoteagent.cc
	g++ remoteagent.cc -g -o remoteagent

clean:
	-rm -rf distpsnotify remoteagent remoteagent.dSYM distpsnotify.dSYM 


