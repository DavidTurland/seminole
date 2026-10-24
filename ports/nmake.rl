COMPILE $(CXX) $(CFLAGS) /Fo$@ /c __SOURCES__
HOST_COMPILE $(HOSTCXX) $(HOSTCFLAGS) /Fe__TGT__ __SRC__
PERL_MKTOOL @$(PERL) $(MKTOOL) -t $(TOOLOUT) -p $(PERL) __SRC__ __TGT__

DEFINE STD_RULES

all  : libs makeBins makeContent
libs : makePrep makeIncs makeLibs 

makePrep:: $(OUTDIRS)
	-@ $(RECURSE_CMD)

makeLibs: $(LIBRARY)
	-@ $(RECURSE_CMD)

makeIncs: $(PUBINCS)
	-@ $(RECURSE_CMD)

makeBins: $(BINARY)
	-@ $(RECURSE_CMD)

makeContent: $(CONTENT)
	-@ $(RECURSE_CMD)

DEFINE MAKE_INCS

$(PUBINCS) : $(APIFILES)
	-@ DEL /Q $(INCOUT)\$? >NUL
	@  $(CP) $? $(INCOUT)

DEFINE MAKE_LIBRARY

$(LIBRARY): $(OBJS)
	$(MK_LIB_CMD)

DEFINE MAKE_BIN

$(BINARY): $(OBJS) $(LIBS)
	$(LD_BINARY_CMD)

DEFINE MAKE_PREP

$(OUTDIRS):
	- $(PREP_DIR_CMD) $@

clean:
	- @ $(RECURSE_CMD)
	- $(CLEAN_DIR_CMD) $(OUTDIRS)

DEFINE MAKE_CONTENT

$(CONTENT) : content.cfg 
	@ $(PERL) $(TOOLOUT)\scpg.pl -v -o $(CONTENT)

DEFINE STANDARD_ENV

# Pull in our build meta-information
include <$(DEPTH)\mkdefs>

# Pull in our environment-tailored rules and vars
include <$(DEPTH)\ports\$(PORT)>

DEFINE TOOL_RULES

all   : makeBins

tools : makeBins

makePrep::
	-@ $(RECURSE_CMD)

makeLibs:
	-@ $(RECURSE_CMD)

makeIncs:
	-@ $(RECURSE_CMD)

makeBins: $(TOOLOUT) $(PERLOBJ) $(TOOLOBJ)
	-@ $(RECURSE_CMD)

makeContent:
	-@ $(RECURSE_CMD)


$(TOOLOUT):
	- $(PREP_DIR_CMD) $@
 
clean :
	-@ $(RECURSE_CMD)
	- $(RM) $(PERLOBJ) $(TOOLOBJ)

