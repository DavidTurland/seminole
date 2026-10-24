##---------------------------------------------------------------------------
##
## MODULE NAME:     tools/Makefile.gen
##
## COPYRIGHT:       Copyright 2002, Mark Grosberg.  Please see the LICENSE
##                  file within the Seminole distribution for terms of use.
##                  If your distribution is incomplete, the Seminole
##                  documentation and license are available on the World
##                  Wide Web at http://www.conman.org/software/seminole/
##
## DESCRIPTION:     Tool compilation and installation.
##
## AUTHOR:          M. Grosberg
##
## CREATION DATE:   Jan 22, 2002
##
##---------------------------------------------------------------------------
##
## HISTORY OF CHANGE
##
## -----+--------+------------------------------------------+------+---------
## VERS | DATE   | DESCRIPTION OF CHANGE                    | INIT | PR#
## =====+========+==========================================+======+=========
##  001 |01/22/02| File creation.                           | MYG  |
## -----+--------+------------------------------------------+------+---------
##  002 |06/23/02| Add header dependency.                   | MYG  | M1021
## -----+--------+------------------------------------------+------+---------
##---------------------------------------------------------------------------

TOOLHDRS = compress-util.h

DEPTH = ../..

# Pull in our build meta-information
include $(DEPTH)/mkdefs

# Pull in our environment-tailored rules and vars
include $(DEPTH)/ports/$(PORT)

PERLOBJ = $(TOOLOUT)/bin2c$(PERLEXT) $(TOOLOUT)/mimegen$(PERLEXT) $(TOOLOUT)/scpg$(PERLEXT) 

$(TOOLOUT)/bin2c$(PERLEXT) : bin2c.ipl
	@$(PERL) $(MKTOOL) -t $(TOOLOUT) -p $(PERL) bin2c.ipl $(TOOLOUT)/bin2c$(PERLEXT)

$(TOOLOUT)/mimegen$(PERLEXT) : mimegen.ipl
	@$(PERL) $(MKTOOL) -t $(TOOLOUT) -p $(PERL) mimegen.ipl $(TOOLOUT)/mimegen$(PERLEXT)

$(TOOLOUT)/scpg$(PERLEXT) : scpg.ipl
	@$(PERL) $(MKTOOL) -t $(TOOLOUT) -p $(PERL) scpg.ipl $(TOOLOUT)/scpg$(PERLEXT)


TOOLOBJ = $(TOOLOUT)/lzari_compress$(TOOLEXT) $(TOOLOUT)/lzrw1kh_compress$(TOOLEXT) 

$(TOOLOUT)/lzari_compress$(TOOLEXT) : lzari_compress.c $(TOOLHDRS)
	$(HOSTCXX) $(HOSTCFLAGS) -o $(TOOLOUT)/lzari_compress$(TOOLEXT) lzari_compress.c

$(TOOLOUT)/lzrw1kh_compress$(TOOLEXT) : lzrw1kh_compress.c $(TOOLHDRS)
	$(HOSTCXX) $(HOSTCFLAGS) -o $(TOOLOUT)/lzrw1kh_compress$(TOOLEXT) lzrw1kh_compress.c



all   : makeBins

tools : makeBins

makePrep::
	@ $(RECURSE_CMD)

makeLibs: 
	@ $(RECURSE_CMD)

makeIncs: 
	@ $(RECURSE_CMD)

makeBins: $(TOOLOUT) $(PERLOBJ) $(TOOLOBJ)
	@ $(RECURSE_CMD)

makeContent:
	@ $(RECURSE_CMD)

$(TOOLOUT):
	- $(PREP_DIR_CMD) $@

clean :
	@ $(RECURSE_CMD)
	- $(RM) $(PERLOBJ) $(TOOLOBJ)

