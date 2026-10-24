##---------------------------------------------------------------------------
##
## MODULE NAME:     VxWorks/Makefile.gen
##
## COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
##                  file within the Seminole distribution for terms of use.
##                  If your distribution is incomplete, the Seminole
##                  documentation and license are available on the World
##                  Wide Web at http://www.conman.org/software/seminole/
##
## DESCRIPTION:     Compilation directives for Seminole targets
##
## AUTHOR:          M. Grosberg
##
## CREATION DATE:   Oct 30, 2001
##
##---------------------------------------------------------------------------
##
## HISTORY OF CHANGE
##
## -----+--------+------------------------------------------+------+---------
## VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
## =====+========+==========================================+======+=========
##  001 |10/30/01| File creation.                           | MYG  |
## -----+--------+------------------------------------------+------+---------
##---------------------------------------------------------------------------

####

DEPTH = ../../..


# Pull in our build meta-information
include $(DEPTH)/mkdefs

# Pull in our environment-tailored rules and vars
include $(DEPTH)/ports/$(PORT)


OUTDIRS = $(LIBOUT) $(INCOUT)
LIBRARY = $(SEMTGTLIB)


PUBINCS= \
   $(INCOUT)/sem_sys.h

APIFILES= \
   sem_sys.h

OBJS= \
    $(BUILT_DIR)/lib/sem_opsys.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_sock.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_thread.$(OBJSFX)

$(BUILT_DIR)/lib/sem_opsys.$(OBJSFX): sem_opsys.cpp sem_sys.h
	$(CXX) $(CFLAGS) -o $@ -c sem_opsys.cpp
$(BUILT_DIR)/lib/sem_sock.$(OBJSFX): sem_sock.cpp sem_sys.h
	$(CXX) $(CFLAGS) -o $@ -c sem_sock.cpp
$(BUILT_DIR)/lib/sem_thread.$(OBJSFX): sem_thread.cpp sem_sys.h
	$(CXX) $(CFLAGS) -o $@ -c sem_thread.cpp


all  : libs makeBins makeContent
libs : makePrep makeIncs makeLibs 

makePrep:: $(OUTDIRS)
	@ $(RECURSE_CMD)

makeLibs: $(LIBRARY)
	@ $(RECURSE_CMD)

makeIncs: $(PUBINCS)
	@ $(RECURSE_CMD)

makeBins: $(BINARY)
	@ $(RECURSE_CMD)

makeContent: $(CONTENT)
	@ $(RECURSE_CMD)


$(LIBRARY): $(OBJS)
	$(MK_LIB_CMD)
	- $(RAN_LIB_CMD)


$(PUBINCS) : $(APIFILES)
	$(COPY_HDRS_CMD)


$(OUTDIRS):
	- $(PREP_DIR_CMD) $@

clean:
	@ $(RECURSE_CMD)
	- $(CLEAN_DIR_CMD) $(OUTDIRS)

