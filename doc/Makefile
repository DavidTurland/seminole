##---------------------------------------------------------------------------
##
## MODULE NAME:     doc/Makefile
##
## COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
##                  file within the Seminole distribution for terms of use.
##                  If your distribution is incomplete, the Seminole
##                  documentation and license are available on the World
##                  Wide Web at http://www.conman.org/software/seminole/
##
## DESCRIPTION:     Documentation build directives
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
##  002 |10/18/01| Support a more generalized build system. | AMH  |
## -----+--------+------------------------------------------+------+---------
##  003 |11/02/01| Update to new make system.               | MYG  |
## -----+--------+------------------------------------------+------+---------
##---------------------------------------------------------------------------

DEPTH         = ..

# The top-level system entity.
SGML_SOURCE   = $(DEPTH)/doc/semman.sgm

# Pull in our build meta-information
include $(DEPTH)/mkdefs

# Pull in our environment-tailored rules and vars
include $(DEPTH)/ports/$(PORT)

all:: html

html::
	- $(PREP_DIR_CMD) $(HTMLOUT)
	$(SGML_PROC_CMD)

clean:: html-clean

html-clean:: 
	- $(CLEAN_DIR_CMD) $(HTMLOUT)

