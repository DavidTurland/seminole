##---------------------------------------------------------------------------
##
## MODULE NAME:     src/Makefile.gen
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
##  004 |06/24/02| NT4 build problems (only built on XP     | MYG  |
##      |        | originally).                             |      |
## -----+--------+------------------------------------------+------+---------
##---------------------------------------------------------------------------

####

DEPTH = ..


# Pull in our build meta-information
include $(DEPTH)/mkdefs

# Pull in our environment-tailored rules and vars
include $(DEPTH)/ports/$(PORT)


SUBDIRS = tools example targets/$(TARGET)
OUTDIRS = $(LIBOUT) $(INCOUT)
LIBRARY = $(SEMLIB)


PUBINCS= \
   $(INCOUT)/seminole.h

APIFILES= \
   seminole.h

OBJS= \
    $(BUILT_DIR)/lib/sem_cgi.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_cmprf.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_cntntsink.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_datasrc.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_file.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_fs.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_hand.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_httpd.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_lzarirf.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_lzrw1rf.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_memdsrc.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_rawrf.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_redr.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_req.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_response.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_romdir.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_romfs.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_time.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_ut.$(OBJSFX) \
    $(BUILT_DIR)/lib/sem_write.$(OBJSFX)

$(BUILT_DIR)/lib/sem_cgi.$(OBJSFX): sem_cgi.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_cgi.cpp
$(BUILT_DIR)/lib/sem_cmprf.$(OBJSFX): sem_cmprf.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_cmprf.cpp
$(BUILT_DIR)/lib/sem_cntntsink.$(OBJSFX): sem_cntntsink.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_cntntsink.cpp
$(BUILT_DIR)/lib/sem_datasrc.$(OBJSFX): sem_datasrc.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_datasrc.cpp
$(BUILT_DIR)/lib/sem_file.$(OBJSFX): sem_file.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_file.cpp
$(BUILT_DIR)/lib/sem_fs.$(OBJSFX): sem_fs.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_fs.cpp
$(BUILT_DIR)/lib/sem_hand.$(OBJSFX): sem_hand.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_hand.cpp
$(BUILT_DIR)/lib/sem_httpd.$(OBJSFX): sem_httpd.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_httpd.cpp
$(BUILT_DIR)/lib/sem_lzarirf.$(OBJSFX): sem_lzarirf.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_lzarirf.cpp
$(BUILT_DIR)/lib/sem_lzrw1rf.$(OBJSFX): sem_lzrw1rf.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_lzrw1rf.cpp
$(BUILT_DIR)/lib/sem_memdsrc.$(OBJSFX): sem_memdsrc.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_memdsrc.cpp
$(BUILT_DIR)/lib/sem_rawrf.$(OBJSFX): sem_rawrf.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_rawrf.cpp
$(BUILT_DIR)/lib/sem_redr.$(OBJSFX): sem_redr.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_redr.cpp
$(BUILT_DIR)/lib/sem_req.$(OBJSFX): sem_req.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_req.cpp
$(BUILT_DIR)/lib/sem_response.$(OBJSFX): sem_response.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_response.cpp
$(BUILT_DIR)/lib/sem_romdir.$(OBJSFX): sem_romdir.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_romdir.cpp
$(BUILT_DIR)/lib/sem_romfs.$(OBJSFX): sem_romfs.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_romfs.cpp
$(BUILT_DIR)/lib/sem_time.$(OBJSFX): sem_time.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_time.cpp
$(BUILT_DIR)/lib/sem_ut.$(OBJSFX): sem_ut.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_ut.cpp
$(BUILT_DIR)/lib/sem_write.$(OBJSFX): sem_write.cpp sem_romfs.h seminole.h
	$(CXX) $(CFLAGS) -o $@ -c sem_write.cpp


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

