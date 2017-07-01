VERSION = 0.1
MAN_SECT=1

ifndef PREFIX
	PREFIX = /usr/local
endif

INSTALL_BIN = $(PREFIX)/bin
INSTALL_SBIN = $(PREFIX)/sbin
INSTALL_LIB = $(PREFIX)/lib
INSTALL_HDR = $(PREFIX)/include
INSTALL_SHR = $(PREFIX)/share
ifeq ("$(abspath $(INSTALL_HDR))", "/include") 
	INSTALL_HDR = /usr/include
endif
ifeq ("$(abspath $(INSTALL_SHR))", "/share") 
	INSTALL_SHR = /usr/share
endif

CC = gcc
CFLAGS = -Wall -Wextra -Wstrict-prototypes -I.
DFLAGS = -DVERSION='"$(VERSION)"'
LDFLAGS = -liw -lc
SOFLAGS = -fPIC
RM = rm -f
NAME = iwscan
DAEM = $(NAME)d
LIB_SO = $(NAME)lib.so
LIB_LINK = lib$(NAME).so
LIB = $(LIB_LINK).$(VERSION)
HDR = $(NAME).h
FORMAT = $(NAME)-xml-format
OBJ = *.o

export NAME DAEM FORMAT MAN_SECT

INSTALL_BIN_TGT = install-bin
SETCAP_BIN = $(shell which setcap)
ifneq ("$(realpath $(SETCAP_BIN))", "")
	SETCAP = $(SETCAP_BIN) cap_net_admin+ep
	INSTALL_BIN_TGT = install-bin-setcap
endif

.PHONY: lib bin clean

bin: $(NAME) $(DAEM)

lib: $(LIB)

%: %.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c $(HDR)
	$(CC) $(CFLAGS) $(DFLAGS) -c -o $@ $<

%.so: %.c $(HDR)
	$(CC) $(CFLAGS) $(SOFLAGS) -c -o $@ $<

$(NAME): $(LIB) $(NAME).o
    
$(DAEM): $(LIB) $(DAEM).o


$(LIB): $(LIB_SO)
	$(CC) -shared -Wl,-soname,$(LIB) -o $@ $^

all: bin

man/$(NAME).$(MAN_SECT) man/$(DAEM).$(MAN_SECT) man/$(FORMAT).7: update-man-pages

update-man update-man-pages update-man-html clean-man:
	$(MAKE) -C man $@

install: install-lib install-hdr install-shr install-man $(INSTALL_BIN_TGT)

install-lib: lib
	install -m 755 -d $(INSTALL_LIB)
	install -m 755 $(LIB) $(INSTALL_LIB)
	ln -sfn $(LIB) $(INSTALL_LIB)/$(LIB_LINK)

install-hdr: $(HDR) install-lib
	install -m 755 -d $(INSTALL_HDR)
	install -m 755 $(HDR) $(INSTALL_HDR)

install-shr:
	install -m 755 -d $(INSTALL_SHR)/$(NAME)
	install -m 755 stuff/* $(INSTALL_SHR)/$(NAME)/

install-man: man/$(NAME).$(MAN_SECT) man/$(DAEM).$(MAN_SECT) man/$(FORMAT).7
	install -m 755 -d $(INSTALL_SHR)/man/man$(MAN_SECT)
	install -m 755 man/$(NAME).$(MAN_SECT) $(INSTALL_SHR)/man/man$(MAN_SECT)/
	install -m 755 man/$(DAEM).$(MAN_SECT) $(INSTALL_SHR)/man/man$(MAN_SECT)/
	install -m 755 -d $(INSTALL_SHR)/man/man7
	install -m 755 man/$(FORMAT).7 $(INSTALL_SHR)/man/man7/

install-bin: bin
	@echo "*** setcap not found. You can run $(NAME) and $(DAEM) only as root. ***"
	@echo "*** Installing binaries into $(INSTALL_SBIN) ***"
	install -m 755 -d $(INSTALL_SBIN)
	install -m 755 $(NAME) $(INSTALL_SBIN)
	install -m 755 $(DAEM) $(INSTALL_SBIN)

install-bin-setcap: bin
	@echo "*** setcap found. You probably can run $(NAME) and $(DAEM) as user. ***"
	@echo "*** Installing binaries into $(INSTALL_BIN) ***"
	install -m 755 -d $(INSTALL_BIN)
	install -m 755 $(NAME) $(INSTALL_BIN)
	install -m 755 $(DAEM) $(INSTALL_BIN)
	$(SETCAP) $(INSTALL_BIN)/$(NAME)
	$(SETCAP) $(INSTALL_BIN)/$(DAEM)

uninstall:
	-$(RM) $(INSTALL_LIB)/$(LIB) $(INSTALL_HDR)/$(HDR) \
		$(INSTALL_SBIN)/$(NAME) $(INSTALL_BIN)/$(NAME)
	-$(RM) $(INSTALL_SHR)/man/man$(MAN_SECT)/$(NAME).$(MAN_SECT)
	-$(RM) $(INSTALL_SHR)/man/man$(MAN_SECT)/$(DAEM).$(MAN_SECT)
	-$(RM) $(INSTALL_SHR)/man/man$(MAN_SECT)/$(FORMAT).7
	-$(RM) -r $(INSTALL_SHR)/$(NAME)

clean:
	-$(RM) $(LIB) $(LIB_SO) $(OBJ) $(NAME) $(DAEM)
