<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<!--
 -
 - Copyright (C) 2006, 2007 John Costigan.
 -
 - This file is part of Maemo Mapper.
 -
 - Maemo Mapper is free software: you can redistribute it and/or modify
 - it under the terms of the GNU General Public License as published by
 - the Free Software Foundation, either version 3 of the License, or
 - (at your option) any later version.
 -
 - Maemo Mapper is distributed in the hope that it will be useful,
 - but WITHOUT ANY WARRANTY; without even the implied warranty of
 - MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 - GNU General Public License for more details.
 -
 - You should have received a copy of the GNU General Public License
 - along with Maemo Mapper.  If not, see <http://www.gnu.org/licenses/>.
 -
-->

<xsl:output method="html"/>


<!-- Root element. -->

<xsl:template match="ossohelpsource">
  <html>
  <head><title><xsl:value-of select="folder/title"/></title></head>

  <!-- Everything else -->
  <body>
  <xsl:apply-templates />
  </body>

  </html>
</xsl:template>


<!-- Sections (folder, topic) -->

<xsl:template match="folder">
  <!-- Introduction -->
  <xsl:apply-templates select="title"/>
  <p>
    This page is an HTML version of the built-in documentation that is
    available within Maemo Mapper via the "Help" main menu item.
  </p>

  <!-- Table of Contents -->
  <h2 id="help_maemomapper_">Table of Contents</h2>
  <ul>
    <xsl:for-each select="topic">
      <li>
        <xsl:element name="a">
          <xsl:attribute name="href">
            <xsl:choose>
              <xsl:when test="context/@contextUID">
                #<xsl:value-of select="context/@contextUID"/>
              </xsl:when>
              <xsl:otherwise>
                #<xsl:value-of select="translate(topictitle, ' ', '_')"/>
                <xsl:text>_topictitle</xsl:text>
              </xsl:otherwise>
            </xsl:choose>
          </xsl:attribute>
          <xsl:value-of select="topictitle"/>
        </xsl:element>
        <ul>
          <xsl:for-each select="heading">
            <li>
            <xsl:element name="a">
              <xsl:attribute name="href">
                #<xsl:value-of select="translate(../context/@contextUID, ' ', '_')"/>
                <xsl:text>_</xsl:text>
                <xsl:value-of select="translate(., ' ', '_')"/>
                <xsl:text>_heading</xsl:text>
              </xsl:attribute>
              <xsl:value-of select="."/>
            </xsl:element>
            </li>
          </xsl:for-each>
        </ul>
      </li>
    </xsl:for-each>
  </ul>
  <xsl:apply-templates select="topic"/>
</xsl:template>

<xsl:template match="topic">
  <xsl:element name="div">
    <xsl:attribute name="id">
      <xsl:value-of select="context/@contextUID"/>
    </xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>


<!-- Headings (title = h1, topictitle = h2, heading = h3) -->

<xsl:template match="title">
  <h1><xsl:apply-templates /></h1>
</xsl:template>

<xsl:template match="topictitle">
  <xsl:element name="h2">
    <xsl:attribute name="id">
      <xsl:value-of select="translate(., ' ', '_')"/>
      <xsl:text>_topictitle</xsl:text>
    </xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>

<xsl:template match="heading">
  <xsl:element name="h3">
    <xsl:attribute name="id">
      <xsl:value-of select="translate(../context/@contextUID, ' ', '_')"/>_<xsl:value-of select="translate(., ' ', '_')"/>
      <xsl:text>_heading</xsl:text>
    </xsl:attribute>
    <xsl:apply-templates />
  </xsl:element>
</xsl:template>


<!-- Simple HTML elements. -->

<xsl:template match="para">
  <p><xsl:apply-templates /></p>
</xsl:template>

<xsl:template match="list">
  <ul><xsl:apply-templates /></ul>
</xsl:template>

<xsl:template match="listitem">
  <li><xsl:apply-templates /></li>
</xsl:template>

<xsl:template match="task_seq">
  <ul><xsl:apply-templates /></ul>
</xsl:template>

<xsl:template match="step">
  <li><xsl:apply-templates /></li>
</xsl:template>

<xsl:template match="display_text">
  <code><xsl:apply-templates /></code>
</xsl:template>

<xsl:template match="tip">
  <p><strong>Tip: <xsl:apply-templates /></strong></p>
</xsl:template>

<xsl:template match="note">
  <p><strong>Note: <xsl:apply-templates /></strong></p>
</xsl:template>

<xsl:template match="important">
  <p><strong>Important: <xsl:apply-templates /></strong></p>
</xsl:template>

<xsl:template match="example">
  <p>Example: <xsl:apply-templates /></p>
</xsl:template>

<xsl:template match="Warning">
  <p><strong>Warning: <xsl:apply-templates /></strong></p>
</xsl:template>

<xsl:template match="emphasis">
  <em><xsl:apply-templates /></em>
</xsl:template>

<xsl:template match="@*|node()">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()"/>
  </xsl:copy>
</xsl:template>


<!-- Images -->

<xsl:template match="graphic">
  <xsl:element name="img">
    <xsl:attribute name="src">
      <xsl:text>http://maemo.org/forrest-images/4-x/maemo_4-0_tutorial/images/</xsl:text>
      <xsl:choose>
        <xsl:when test="@filename = '2686KEY_full_screen'">
          <xsl:text>hkey_fullscreen.png</xsl:text>
        </xsl:when>
        <xsl:when test="@filename = '2686KEY_zoom_in'">
          <xsl:text>hkey_zoomIn.png</xsl:text>
        </xsl:when>
        <xsl:when test="@filename = '2686KEY_zoom_out'">
          <xsl:text>hkey_zoomOut.png</xsl:text>
        </xsl:when>
        <xsl:when test="@filename = '2686KEY_scroll_up'">
          <xsl:text>hkey_up.png</xsl:text>
        </xsl:when>
        <xsl:when test="@filename = '2686KEY_scroll_down'">
          <xsl:text>hkey_down.png</xsl:text>
        </xsl:when>
        <xsl:when test="@filename = '2686KEY_scroll_left'">
          <xsl:text>hkey_left.png</xsl:text>
        </xsl:when>
        <xsl:when test="@filename = '2686KEY_scroll_right'">
          <xsl:text>hkey_right.png</xsl:text>
        </xsl:when>
        <xsl:when test="@filename = '2686KEY_scroll_center'">
          <xsl:text>hkey_enter.png</xsl:text>
        </xsl:when>
        <xsl:when test="@filename = '2686KEY_esc'">
          <xsl:text>hkey_cancel.png</xsl:text>
        </xsl:when>
        <xsl:otherwise>
          <xsl:text>???.png</xsl:text>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:attribute>
    <xsl:attribute name="alt">
      <xsl:value-of select="substring-after(@filename, '2686KEY_')"/>
    </xsl:attribute>
    <xsl:attribute name="style">
      <xsl:text>height: 1.5em;</xsl:text>
    </xsl:attribute>
  </xsl:element>
</xsl:template>


<!-- Links and references. -->

<xsl:template match="ref">
  <xsl:element name="a">
    <xsl:attribute name="href">
      #<xsl:value-of select="@refid"/>
    </xsl:attribute>
    <xsl:value-of select="@refdoc"/>
  </xsl:element>
</xsl:template>

</xsl:stylesheet>

