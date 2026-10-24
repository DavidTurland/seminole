##---------------------------------------------------------------------------
##
## MODULE NAME:     example/Makefile.gen
##
## COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
##                  file within the Seminole distribution for terms of use.
##                  If your distribution is incomplete, the Seminole
##                  documentation and license are available on the World
##                  Wide Web at http://www.conman.org/software/seminole/
##
## DESCRIPTION:     Compilation directives for Seminole itself
##
## AUTHOR:          A. Hoerter
##
## CREATION DATE:   Aug 21, 2001
##
##---------------------------------------------------------------------------
##
## HISTORY OF CHANGE
##
## -----+--------+------------------------------------------+------+---------
## VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
## =====+========+==========================================+======+=========
##  001 |08/21/01| File creation.                           | AMH  |
## -----+--------+------------------------------------------+------+---------
##  002 |10/13/01| A more generalized build system.         | AMH  |
## -----+--------+------------------------------------------+------+---------
##  003 |10/27/01| Recurse with top-level rules.            | MYG  |
## -----+--------+------------------------------------------+------+---------
##  004 |06/24/02| Build default content pack.              | MYG  |
## -----+--------+------------------------------------------+------+---------
##---------------------------------------------------------------------------

####

DEPTH = ../..


# Pull in our build meta-information
include $(DEPTH)/mkdefs

# Pull in our environment-tailored rules and vars
include $(DEPTH)/ports/$(PORT)


OUTDIRS = $(BINOUT)
BINARY  = $(SEMBIN)
LIBS    = $(SEMLIB) $(SEMTGTLIB)
CONTENT = $(BINOUT)/semweb.pkg 


OBJS= \
    $(BUILT_DIR)/lib/main.$(OBJSFX)

$(BUILT_DIR)/lib/main.$(OBJSFX): main.cpp $(SEMINCLUDE)
	$(CXX) $(CFLAGS) -o $@ -c main.cpp


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


$(BINARY): $(OBJS) $(LIBS)
	$(LD_BINARY_CMD)


$(OUTDIRS):
	- $(PREP_DIR_CMD) $@

clean:
	@ $(RECURSE_CMD)
	- $(CLEAN_DIR_CMD) $(OUTDIRS)


$(CONTENT) : content.cfg 
	@ $(TOOLOUT)/scpg -v -o $(CONTENT)

