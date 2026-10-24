##---------------------------------------------------------------------------
##
## MODULE NAME:     Makefile
##
## COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
##                  file within the Seminole distribution for terms of use.
##                  If your distribution is incomplete, the Seminole
##                  documentation and license are available on the World
##                  Wide Web at http://www.conman.org/software/seminole/
##
## DESCRIPTION:     Top-level build directives
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
##  003 |11/06/01| Don't build docs unless specified.       | MYG  |
## -----+--------+------------------------------------------+------+---------
##  004 |04/01/02| Use $(PERL) to call perl scripts.        | RMO  |
## -----+--------+------------------------------------------+------+---------
##---------------------------------------------------------------------------

## You shouldn't have to change this file, except to add new subdirectories.
## (unless you are not on a POSIX system at all)
## Look in ports/ and mkdefs for the interesting stuff.

DEPTH = .

# Pull in our build meta-information
include $(DEPTH)/mkdefs

# Pull in our environment-tailored rules and vars
include $(DEPTH)/ports/$(PORT)

SUBDIRS  = doc src

# This 'all' is really everything but docs.
all::
	cd src && $(MAKE) $(MFLAGS) $@

docs::
	cd doc && $(MAKE) $(MFLAGS)

clean::
	for dir in $(SUBDIRS); do (cd $$dir && $(MAKE) $(MFLAGS) $@); done

deps::
	$(PERL) ./makedeps.pl -r ports/$(MAKERULES) -d src

depclean::
	$(PERL) ./makedeps.pl -c -d src


