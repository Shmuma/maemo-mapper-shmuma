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

  <p>
    If you use Maemo Mapper and would like to show your appreciation for
    it, or if you have a feature that you would like added to Maemo
    Mapper, please consider donating money via PayPal.
  </p>

  <form action="https://www.paypal.com/cgi-bin/webscr" method="post">
    <p>
      <input type="hidden" name="cmd" value="_s-xclick"/>
      <input type="image" src="https://www.paypal.com/en_US/i/btn/x-click-but21.gif" name="submit" alt="Make payments with PayPal - it's fast, free and secure!"/>
      <img alt="" src="https://www.paypal.com/en_US/i/scr/pixel.gif" width="1" height="1"/>
      <input type="hidden" name="encrypted" value="-----BEGIN PKCS7-----MIIHRwYJKoZIhvcNAQcEoIIHODCCBzQCAQExggEwMIIBLAIBADCBlDCBjjELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MRQwEgYDVQQKEwtQYXlQYWwgSW5jLjETMBEGA1UECxQKbGl2ZV9jZXJ0czERMA8GA1UEAxQIbGl2ZV9hcGkxHDAaBgkqhkiG9w0BCQEWDXJlQHBheXBhbC5jb20CAQAwDQYJKoZIhvcNAQEBBQAEgYCsFCpmuyu2NI6HUNWLUHtbzY48FfkOynlmUSRWqeAlDeCsC1BIhnva0YyvHAWijj+qk00t/z3qX93o+vrcTvr3pXFE1UJINDml7Y8CKPbCBfVm/2Z/qapJPlMfLij/d8hATXIj8YXpP36/fYiph5Mc0CeENwX+f/JTrzz2gRlxbjELMAkGBSsOAwIaBQAwgcQGCSqGSIb3DQEHATAUBggqhkiG9w0DBwQIMwxPqx5T2F2AgaA9xN5b+cRnkhDXF5UvguSejj4i4tA01VHz0DKYTP9KAwLswZT/IO4i4y3jnHpX7eyGkxECmJM8uUWBgCtxBwYS/Xd2krkLZhok93+CvZitxmKJmHZCGBip9pBFTz36gYM6c/6YzOth5AE3kUNdHLKfctEl1bQW8DQ1LOH3bO3Oczzmpf9qidRDQRizLXrKNmtRyFH6wig5hjeh4n+Md12GoIIDhzCCA4MwggLsoAMCAQICAQAwDQYJKoZIhvcNAQEFBQAwgY4xCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEWMBQGA1UEBxMNTW91bnRhaW4gVmlldzEUMBIGA1UEChMLUGF5UGFsIEluYy4xEzARBgNVBAsUCmxpdmVfY2VydHMxETAPBgNVBAMUCGxpdmVfYXBpMRwwGgYJKoZIhvcNAQkBFg1yZUBwYXlwYWwuY29tMB4XDTA0MDIxMzEwMTMxNVoXDTM1MDIxMzEwMTMxNVowgY4xCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEWMBQGA1UEBxMNTW91bnRhaW4gVmlldzEUMBIGA1UEChMLUGF5UGFsIEluYy4xEzARBgNVBAsUCmxpdmVfY2VydHMxETAPBgNVBAMUCGxpdmVfYXBpMRwwGgYJKoZIhvcNAQkBFg1yZUBwYXlwYWwuY29tMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDBR07d/ETMS1ycjtkpkvjXZe9k+6CieLuLsPumsJ7QC1odNz3sJiCbs2wC0nLE0uLGaEtXynIgRqIddYCHx88pb5HTXv4SZeuv0Rqq4+axW9PLAAATU8w04qqjaSXgbGLP3NmohqM6bV9kZZwZLR/klDaQGo1u9uDb9lr4Yn+rBQIDAQABo4HuMIHrMB0GA1UdDgQWBBSWn3y7xm8XvVk/UtcKG+wQ1mSUazCBuwYDVR0jBIGzMIGwgBSWn3y7xm8XvVk/UtcKG+wQ1mSUa6GBlKSBkTCBjjELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MRQwEgYDVQQKEwtQYXlQYWwgSW5jLjETMBEGA1UECxQKbGl2ZV9jZXJ0czERMA8GA1UEAxQIbGl2ZV9hcGkxHDAaBgkqhkiG9w0BCQEWDXJlQHBheXBhbC5jb22CAQAwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQUFAAOBgQCBXzpWmoBa5e9fo6ujionW1hUhPkOBakTr3YCDjbYfvJEiv/2P+IobhOGJr85+XHhN0v4gUkEDI8r2/rNk1m0GA8HKddvTjyGw/XqXa+LSTlDYkqI8OwR8GEYj4efEtcRpRYBxV8KxAW93YDWzFGvruKnnLbDAF6VR5w/cCMn5hzGCAZowggGWAgEBMIGUMIGOMQswCQYDVQQGEwJVUzELMAkGA1UECBMCQ0ExFjAUBgNVBAcTDU1vdW50YWluIFZpZXcxFDASBgNVBAoTC1BheVBhbCBJbmMuMRMwEQYDVQQLFApsaXZlX2NlcnRzMREwDwYDVQQDFAhsaXZlX2FwaTEcMBoGCSqGSIb3DQEJARYNcmVAcGF5cGFsLmNvbQIBADAJBgUrDgMCGgUAoF0wGAYJKoZIhvcNAQkDMQsGCSqGSIb3DQEHATAcBgkqhkiG9w0BCQUxDxcNMDYwNTEwMjAwMzI0WjAjBgkqhkiG9w0BCQQxFgQUS8mmmoO/csf0wOXxgMeHd/y6/HowDQYJKoZIhvcNAQEBBQAEgYB3UYQqHvJPPZ62FQlxW33p/7q4RAMffzUmXZlAMKIuX5ycakbq4FEJUGxX1HW03tl3Pj3En1EiUg5T+MGbsEDV49pV32QX3yvSjUKke26BIq0vrKYxxfTbwryPeqPHGLizIaCBfZOlffeKq0UDVDvS6iZ/fi7h1+r1qRC4UNvQ4A==-----END PKCS7-----"/>
    </p>
  </form>

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

<xsl:template match="context"></xsl:template>

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

