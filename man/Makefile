MAN_DOCBOOK_XSL = /usr/share/xml/docbook/stylesheet/docbook-xsl/manpages/docbook.xsl
HTML_DOCBOOK_XSL = /usr/share/xml/docbook/stylesheet/docbook-xsl/html/docbook.xsl
XSLTPROC = xsltproc --nonet
RM = rm -f

ifndef NAME
	NAME = iwscan
endif
ifndef DAEM
	DAEM = $(NAME)d
endif
ifndef FORMAT
	FORMAT=$(NAME)-xml-format
endif
ifndef MAN_SECT
	MAN_SECT = 1
endif

update-man: update-man-pages update-man-html

%.html: %.xml
	$(XSLTPROC) -o $@ $(HTML_DOCBOOK_XSL) $^

%.$(MAN_SECT): %.xml
	$(XSLTPROC) -o $(@:.xsl=.$(MAN_SECT)) $(MAN_DOCBOOK_XSL) $^

%.7: %.xml
	$(XSLTPROC) -o $(@:.xsl=.7) $(MAN_DOCBOOK_XSL) $^

update-man-html: $(NAME).html $(DAEM).html $(FORMAT).html

update-man-pages: $(NAME).$(MAN_SECT) $(DAEM).$(MAN_SECT) $(FORMAT).7

clean-man:
	$(RM) $(NAME).html $(DAEM).html $(FORMAT).html
	$(RM) $(NAME).$(MAN_SECT) $(DAEM).$(MAN_SECT) $(FORMAT).7