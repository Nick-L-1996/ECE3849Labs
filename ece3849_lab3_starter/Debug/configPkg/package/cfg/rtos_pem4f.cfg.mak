# invoke SourceDir generated makefile for rtos.pem4f
rtos.pem4f: .libraries,rtos.pem4f
.libraries,rtos.pem4f: package/cfg/rtos_pem4f.xdl
	$(MAKE) -f D:\ECE3849Labs\ece3849_lab3_starter/src/makefile.libs

clean::
	$(MAKE) -f D:\ECE3849Labs\ece3849_lab3_starter/src/makefile.libs clean

