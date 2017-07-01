<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="xml" version="1.0" indent="yes"/>
  <xsl:key name="atts-by-name" match="@*" use="name()"/>
  <xsl:template match="/">
    <xsl:copy>
      <xsl:call-template name="merge">
        <xsl:with-param name="elements" select="*"/>
      </xsl:call-template>
    </xsl:copy>
  </xsl:template>
  <xsl:template name="merge">
    <xsl:param name="elements"/>
    <xsl:for-each select="$elements">
      <!--<xsl:variable name="same-elements" select="$elements[name()=name(current()) and count(@*)=count(current()/@*) and count(@*[. = key('atts-by-name',name())[generate-id(..)=generate-id(current())]])=count(@*)]"/>-->
      <xsl:variable name="same-elements" select="$elements[name()=name(current()) and (count(*)>0 or string()=string(current())) and count(@*)=count(current()/@*) and count(@*[. = key('atts-by-name',name())[generate-id(..)=generate-id(current())]])=count(@*)]"/>
      <xsl:if test="generate-id($same-elements[1]) = generate-id()">
        <xsl:copy>
          <xsl:copy-of select="@*"/>
          <xsl:if test="text()">
            <xsl:copy-of select="normalize-space(text())"/>
          </xsl:if>
          <xsl:call-template name="merge">
            <xsl:with-param name="elements" select="$same-elements/*"/>
          </xsl:call-template>
        </xsl:copy>
      </xsl:if>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>