OBJS_DIR=../build/

SOURCES_EXE = crtl_main.c

SOURCES_LIB = crtl_lib.c

SOURCES_CMN = crtl_file_io.c \
              crtl_common.c

OBJS_EXE=$(addprefix $(OBJS_DIR), $(addsuffix .o, $(basename $(SOURCES_EXE))))
OBJS_LIB=$(addprefix $(OBJS_DIR), $(addsuffix .o, $(basename $(SOURCES_LIB))))
OBJS_CMN=$(addprefix $(OBJS_DIR), $(addsuffix .o, $(basename $(SOURCES_CMN))))

CURTAIL_EXE=$(OBJS_DIR)curtail
CURTAIL_LIB_A=$(OBJS_DIR)libcurtail.a
CURTAIL_LIB_SO=$(OBJS_DIR)libcurtail.so

.phony: all clean

all: $(OBJS_DIR) $(CURTAIL_LIB_A) $(CURTAIL_LIB_SO) $(CURTAIL_EXE)

$(OBJS_DIR):
	if [ ! -d $(OBJS_DIR) ]; then \
		mkdir $(OBJS_DIR); \
	fi

$(OBJS_DIR)%.o: %.c
	@echo Compiling $<...
	$(CC) -c $< $(CFLAGS) -o $@

$(CURTAIL_LIB_A): $(OBJS_LIB) $(OBJS_CMN)
	@echo Archiving $^...
	$(AR) rsc $@ $^

$(CURTAIL_LIB_SO): $(OBJS_LIB) $(OBJS_CMN)
	@echo Creating shared library...
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(INCLUDE_LIBS) -shared -o $@

$(CURTAIL_EXE): $(OBJS_EXE) $(OBJS_CMN)
	@echo Linking $^...
	$(CXX) $(CFLAGS) $(LDFLAGS) $^ $(INCLUDE_LIBS) -o $(CURTAIL_EXE)

clean:
	@echo Cleaning...
	rm -rf $(OBJS_DIR)
