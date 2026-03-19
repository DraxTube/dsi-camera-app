#---------------------------------------------------------------------------------
# Makefile per DSi Homebrew – devkitARM / libnds
# Compatibile con devkitPro docker image usata in GitHub Actions
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
  $(error "DEVKITPRO non è impostato. Installa devkitPro o esegui via GitHub Actions.")
endif

include $(DEVKITPRO)/libnds/ds_rules

#---------------------------------------------------------------------------------
# Nome e tipo di output
#---------------------------------------------------------------------------------
TARGET      := dsi-video-cam
BUILD       := build
SOURCES     := source
INCLUDES    := include
DATA        :=

#---------------------------------------------------------------------------------
# Opzioni per il compilatore
#---------------------------------------------------------------------------------
ARCH        := -march=armv5te -mtune=arm946e-s

CFLAGS      := -g -Wall -O2 \
               $(ARCH) \
               -fomit-frame-pointer \
               -ffast-math \
               $(INCLUDE)

CFLAGS      += $(DEFINES)

CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS     := -g $(ARCH)

# Linker flags
LDFLAGS     = -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# Librerie necessarie
#---------------------------------------------------------------------------------
LIBS        := -lnds9

LIBDIRS     := $(LIBNDS)

#---------------------------------------------------------------------------------
# Non modificare da qui in giù
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)

export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES    :=

export LD       := $(CC)
export OFILES   := $(BINFILES:.bin=.o) $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE  := $(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo Pulizia...
	@rm -fr $(BUILD) $(TARGET).nds $(TARGET).elf *.map

else

DEPENDS     := $(OFILES:.o=.d)

$(OUTPUT).nds: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
