#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wut/share/wut_rules

#-------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#-------------------------------------------------------------------------------
TARGET		:=	00_mocha
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include

#-------------------------------------------------------------------------------
# options for code generation
#-------------------------------------------------------------------------------
CFLAGS	:=	-g -Wall -O2 -ffunction-sections \
			$(MACHDEP)

CFLAGS	+=	$(INCLUDE) -D__WIIU__ -D__WUT__

CXXFLAGS	:= $(CFLAGS)

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-g $(ARCH) $(RPXSPECS) --entry=_start -Wl,-Map,$(notdir $*.map)

LIBS	:= -lwut -lmocha

#-------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level
# containing include and lib
#-------------------------------------------------------------------------------
LIBDIRS	:= $(PORTLIBS) $(WUT_ROOT) $(WUT_ROOT)/usr

#-------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#-------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#-------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#-------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#-------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#-------------------------------------------------------------------------------
	export LD	:=	$(CC)
#-------------------------------------------------------------------------------
else
#-------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

#-------------------------------------------------------------------------------
$(BUILD): $(CURDIR)/source/ios_kernel/ios_kernel.bin.h
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) -j1 --no-print-directory -C $(CURDIR)/source/ios_fs -f $(CURDIR)/source/ios_fs/Makefile
	@$(MAKE) -j1 --no-print-directory -C $(CURDIR)/source/ios_mcp -f $(CURDIR)/source/ios_mcp/Makefile
	@$(MAKE) -j1 --no-print-directory -C $(CURDIR)/source/ios_usb -f $(CURDIR)/source/ios_usb/Makefile    
	@$(MAKE) -j1 --no-print-directory -C $(CURDIR)/source/ios_kernel -f $(CURDIR)/source/ios_kernel/Makefile
	@$(MAKE) -j1 --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(CURDIR)/source/ios_kernel/ios_kernel.bin.h: $(CURDIR)/source/ios_usb/ios_usb.bin.h  $(CURDIR)/source/ios_mcp/ios_mcp.bin.h $(CURDIR)/source/ios_fs/ios_fs.bin.h
	@$(MAKE) -j1 --no-print-directory -C $(CURDIR)/source/ios_kernel -f $(CURDIR)/source/ios_kernel/Makefile

$(CURDIR)/source/ios_usb/ios_usb.bin.h: 
	@$(MAKE) -j1 --no-print-directory -C $(CURDIR)/source/ios_usb -f $(CURDIR)/source/ios_usb/Makefile
    
$(CURDIR)/source/ios_mcp/ios_mcp.bin.h: 
	@$(MAKE) -j1 --no-print-directory -C $(CURDIR)/source/ios_mcp -f $(CURDIR)/source/ios_mcp/Makefile

$(CURDIR)/source/ios_fs/ios_fs.bin.h: 
	@$(MAKE) -j1 --no-print-directory -C $(CURDIR)/source/ios_fs -f $(CURDIR)/source/ios_fs/Makefile
#-------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).rpx $(TARGET).elf
	@$(MAKE) --no-print-directory -C $(CURDIR)/source/ios_kernel -f  $(CURDIR)/source/ios_kernel/Makefile clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/source/ios_usb -f  $(CURDIR)/source/ios_usb/Makefile clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/source/ios_mcp -f  $(CURDIR)/source/ios_mcp/Makefile clean	
	@$(MAKE) --no-print-directory -C $(CURDIR)/source/ios_fs -f  $(CURDIR)/source/ios_fs/Makefile clean	

#-------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#-------------------------------------------------------------------------------
# main targets
#-------------------------------------------------------------------------------
all	:	$(OUTPUT).rpx

$(OUTPUT).rpx	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SRC)	: $(HFILES_BIN)

#-------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#-------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#-------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)
	
#---------------------------------------------------------------------------------
%.o: %.s
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d -x assembler-with-cpp $(ASFLAGS) -c $< -o $@ $(ERROR_FILTER)

-include $(DEPENDS)

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------
