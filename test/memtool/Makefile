# list of platforms which did not want this test case
EXCLUDE_LIST:=

LINK ?=$(CROSS_COMPILE)gcc
STRIP ?=$(CROSS_COMPILE)strip

OBJ = memtool.o \
      mx6dl_modules.o \
      mx6q_modules.o \
      mx6sl_modules.o \
      mx6sx_modules.o \
      mx6ul_modules.o \
      mx7d_modules.o

ifeq (,$(findstring $(PLATFORM), $(EXCLUDE_LIST)))
TARGET = $(OBJDIR)/memtool
else
TARGET =
endif

CFLAGS+= -Os

all : $(TARGET)

$(TARGET):$(OBJ)
	$(CC) -o $(TARGET) $(OBJ) -Os
	$(STRIP) $(TARGET)

.PHONY: clean
clean :
	rm -f $(OBJ)

#
# include the Rules
#
include ../make.rules

