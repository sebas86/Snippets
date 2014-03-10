// ==UserScript==
// @author      Sebastian Śledź
// @name        Towers Of Mystoria
// @description Allows to use of an older Flash plugin (for all Linux user which stuck with 11.2 Flash plugin).
// @include     https://tom-mox.mintcoregames.com/*
// @version     0.2
// ==/UserScript==

var regex = /swfobject.embedSWF\s*\(\s*"(.*?)"\s*,/;
var url = regex.exec (document.getElementsByTagName ('html') [0].innerHTML) [1];

swfobject.embedSWF (url, "MOXFlash", "100%", "600", "11.2.0", false, flashvars, params, attributes);
