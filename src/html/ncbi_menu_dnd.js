/*
 * Menu 0.8 990602
 * by gary smith, July 1997
 * Copyright (c) 1997-1999 Netscape Communications Corp.
 *
 * Netscape grants you a royalty free license to use or modify this
 * software provided that this copyright notice appears on all copies.
 * This software is provided "AS IS," without a warranty of any kind.
 * -------------------------------------------------------------------
 *
 * Modified by Vladimir Ivanov, NIH, NCBI, February 2002
 *    Added support dynamic menu (all menus use one container).
 *    Added automaticaly menu adjustment in the browsers window.
 *    Fixed some errors.
 */

// By default dynamic menu is off
window.useDynamicMenu = false;

// NOTE:
// By default all menu use a one container if "useDynamicMenu == TRUE"
// Accordingly only one menu can be shown at a time.
//
// Dynamic menu work only in browsers that support property innerHTML
// (Internet Explorer > 4.x & Netscape Navigator > 6.x) 


function dynamiccontentNS6(el, content)
{
  if (document.getElementById){
    rng = document.createRange();
    rng.setStartBefore(el);
    htmlFrag = rng.createContextualFragment(content);
    while (el.hasChildNodes())
      el.removeChild(el.lastChild);
    el.appendChild(htmlFrag);
  }
}


function Menu(label) 
{
    this.version = "0.8d [menu.js; Menu; 020412]";
    this.type = "Menu";
    if (document.layers) {
       this.fontSize = 14;
    } else {
       this.fontSize = 12;
    }
    this.fontWeight = "plain";
    this.fontFamily = "arial,helvetica,espy,sans-serif";
    this.fontColor = "#000000";
    this.fontColorHilite = "#ffffff";
    this.bgColor = "#555555";
    this.menuBorder = 1;
    this.menuItemBorder = 1;
    this.menuItemBgColor = "#cccccc";
    this.menuLiteBgColor = "#ffffff";
    this.menuBorderBgColor = "#777777";
    this.menuHiliteBgColor = "#000084";
    this.menuContainerBgColor = "#cccccc";
    this.childMenuIcon = "images/arrows.gif";
    this.childMenuIconHilite = "images/arrows2.gif";
    this.items = new Array();
    this.actions = new Array();
    this.colors = new Array();
    this.mouseovers = new Array();
    this.mouseouts = new Array();
    this.childMenus = new Array();

    this.addMenuItem = addMenuItem;
    this.addMenuSeparator = addMenuSeparator;
    this.writeMenus = writeMenus;
    this.showMenu = showMenu;
    this.onMenuItemOver = onMenuItemOver;
    this.onMenuItemOut = onMenuItemOut;
    this.onMenuItemDown = onMenuItemDown;
    this.onMenuItemAction = onMenuItemAction;
    this.hideMenu = hideMenu;
    this.hideChildMenu = hideChildMenu;
    this.mouseTracker = mouseTracker;
    this.setMouseTracker = setMouseTracker;

    if (!window.menus) window.menus = new Array();
    this.label = label || "menuLabel" + window.menus.length;
    this.number = window.menus.length;
    window.menus[this.label] = this;
    window.menus[window.menus.length] = this;
    if (!window.activeMenus) window.activeMenus = new Array();
    if (!window.menuContainers) window.menuContainers = new Array();
    if (!window.mDrag) {
        window.mDrag    = new Object();
        mDrag.startMenuDrag = startMenuDrag;
        mDrag.doMenuDrag    = doMenuDrag;
        this.setMouseTracker();
    }
    // Disable drag: problemm with using touchpads on notebooks
    this.disableDrag = 1;
    if (window.MenuAPI) MenuAPI(this);
}


function addMenuItem(label, action, color, mouseover, mouseout) 
{
    this.items[this.items.length] = label;
    this.actions[this.actions.length] = action;
    this.colors[this.colors.length] = color;
    this.mouseovers[this.mouseovers.length] = mouseover;
    this.mouseouts[this.mouseouts.length] = mouseout;
}


function addMenuSeparator() 
{
    this.items[this.items.length] = "separator";
    this.actions[this.actions.length] = "";
    this.menuItemBorder = 0;
}

function getMenuItemID(menu_index, item_index) 
{
	return menu_index * 1000 + item_index + 1;
}


function getMenuContent(container, index) 
{
	menu = container.menus[index];
	if (!menu) return '';
    var proto = menu.prototypeStyles || this.prototypeStyles || menu;
    var mouseOut = '';
    if (!document.getElementById) mouseOut = ' onMouseOut="hideMenu(this);"';
	
    var content = ''+
    '<DIV ID="menuLayer'+ index +'" STYLE="position:absolute;left:0;top:0;visibility:hidden;cursor:pointer;cursor:hand;">'+
    '  <DIV ID="menuLite'+ index +'" STYLE="position:absolute;left:'+ proto.menuBorder +';top:'+ proto.menuBorder +';visibility:hide;"'+mouseOut+'>'+
    '    <DIV ID="menuFg'+ index +'" STYLE="position:absolute;left:1;top:1;visibility:hide;">';

	proto.menuWidth = 0;
    if (!document.layers) {
		proto.menuWidth = proto.menuWidth || proto.menuItemWidth + (proto.menuBorder * 2) + 2 || 200;
	}
    for (var i=0; i<menu.items.length; i++) {
        var item = menu.items[i];
        var childMenu = false;
        var defaultHeight = 20;
        var defaultIndent = 15;
		var id = getMenuItemID(index,i);
        if (item.label) {
            item = item.label;
            childMenu = true;
//        } else if (item.indexOf(".gif") != -1 && item.indexOf("<IMG") == -1) {
//            item = '<IMG SRC="' + item + '" NAME="menuItem'+ id +'Img">';
//            defaultIndent = 0;
//            if (document.layers) {
//                defaultHeight = null;
//            }
        }
   		proto.menuItemHeight = proto.menuItemHeight || defaultHeight;
		h4 = proto.menuItemHeight/4;
    	proto.menuItemIndent = proto.menuItemIndent || defaultIndent;
        var itemProps = 'visibility:hide;font-Family:' + proto.fontFamily +';font-Weight:' + proto.fontWeight + ';fontSize:' + proto.fontSize + ';';
        if (document.all || document.getElementById)
        	itemProps += 'font-size:' + proto.fontSize + ';" onMouseOver="onMenuItemOver(null,this);" onMouseOut="onMenuItemOut(null,this);" onClick="onMenuItemAction(null,this);';
        var dTag  = '<DIV ID="menuItem'+ id +'" STYLE="position:absolute;left:0;top:'+ (i * proto.menuItemHeight) +';'+ itemProps +'">';
        var dText = '<DIV ID="menuItemText'+ id +'" STYLE="position:absolute;left:' + proto.menuItemIndent + ';top:0;color:'+ proto.fontColor +';">'+ item +'</DIV><DIV ID="menuItemHilite'+ id +'" STYLE="position:absolute;left:' + proto.menuItemIndent + ';top:0;color:'+ proto.fontColorHilite +';visibility:hidden;">'+ item +'</DIV>';
        if (item == "separator") {
//            content += ( dTag + '<DIV ID="menuSeparator'+ id +'" STYLE="position:absolute;left:1;top:2;"></DIV><DIV ID="menuSeparatorLite'+ id +'" STYLE="position:absolute;left:1;top:3;"></DIV></DIV>');
            content += ( dTag + '<DIV ID="menuSeparator'+ id +'" STYLE="position:absolute;left:1;top:'+(h4-1)+';width:'+proto.menuWidth+';height:1;"></DIV>'+
								'<DIV ID="menuSeparatorLite'+ id +'" STYLE="position:absolute;left:1;top:'+(h4)+';width:'+proto.menuWidth+';height:1;"></DIV></DIV>');
        } else if (childMenu) {
            content += ( dTag + dText + '<DIV ID="childMenu'+ id +'" STYLE="position:absolute;left:0;top:3;'+ itemProps +'"><IMG SRC="'+ proto.childMenuIcon +'"></DIV></DIV>');
        } else {
            content += ( dTag + dText + '</DIV>');
        }
    }
    content += '<DIV ID="focusItem'+ index +'" STYLE="position:absolute;left:0;top:0;visibility:hide;" onClick="onMenuItemAction(null,this);">&nbsp;</DIV>';
    content += '</DIV></DIV></DIV>\n\n';
	return content;
}


function setMenuProperty(container, index) 
{
    var proto = null;
	var x = index;
    if (document.layers) {
        proto = container.menus[x].prototypeStyles || this.prototypeStyles || container.menus[x];
        var menu = container.document.layers[x];
        container.menus[x].menuLayer = menu;
        container.menus[x].menuLayer.Menu = container.menus[x];
        container.menus[x].menuLayer.Menu.container = container;
        var body = menu.document.layers[0].document.layers[0];
        body.clip.width = proto.menuWidth || body.clip.width;
        body.clip.height = proto.menuHeight || body.clip.height;
        for (var i=0; i<body.document.layers.length-1; i++) {
            var l = body.document.layers[i];
            l.Menu = container.menus[x];
            l.menuHiliteBgColor = proto.menuHiliteBgColor;
            l.document.bgColor = proto.menuItemBgColor;
            l.saveColor = proto.menuItemBgColor;
            l.mouseout  = l.Menu.mouseouts[i];
            l.mouseover = l.Menu.mouseovers[i];
            l.onmouseover = proto.onMenuItemOver;
            l.onclick = proto.onMenuItemAction;
            l.action = container.menus[x].actions[i];
            l.focusItem = body.document.layers[body.document.layers.length-1];
            l.clip.width = proto.menuItemWidth || body.clip.width + proto.menuItemIndent;
            l.clip.height = proto.menuItemHeight || l.clip.height;
            if ( i>0 ) l.top = body.document.layers[i-1].top + body.document.layers[i-1].clip.height + proto.menuItemBorder;
            l.hilite = l.document.layers[1];
            l.document.layers[1].isHilite = true;
            if (l.document.layers[0].id.indexOf("menuSeparator") != -1) {
                l.hilite = null;
                l.clip.height -= l.clip.height / 2;
                l.document.layers[0].document.bgColor = proto.bgColor;
                l.document.layers[0].clip.width = l.clip.width -2;
                l.document.layers[0].clip.height = 1;
                l.document.layers[1].document.bgColor = proto.menuLiteBgColor;
                l.document.layers[1].clip.width = l.clip.width -2;
                l.document.layers[1].clip.height = 1;
                l.document.layers[1].top = l.document.layers[0].top + 1;
            } else if (l.document.layers.length > 2) {
                l.childMenu = container.menus[x].items[i].menuLayer;
                l.icon = proto.childMenuIcon;
                l.iconHilite = proto.childMenuIconHilite;
                l.document.layers[2].left = l.clip.width -13;
                l.document.layers[2].top = (l.clip.height / 2) -4;
                l.document.layers[2].clip.left += 3;
                l.Menu.childMenus[l.Menu.childMenus.length] = l.childMenu;
            }
        }
        body.document.bgColor = proto.bgColor;
        body.clip.width  = l.clip.width +1;
        body.clip.height = l.top + l.clip.height +1;
        body.document.layers[i].clip.width = body.clip.width;
        body.document.layers[i].captureEvents(Event.MOUSEDOWN);
        body.document.layers[i].onmousedown = proto.onMenuItemDown;
        body.document.layers[i].onmouseout = proto.onMenuItemOut;
        body.document.layers[i].Menu = l.Menu;
        body.document.layers[i].top = -30;
        menu.document.bgColor = proto.menuBorderBgColor;
        menu.document.layers[0].document.bgColor = proto.menuLiteBgColor;
        menu.document.layers[0].clip.width = body.clip.width +1;
        menu.document.layers[0].clip.height = body.clip.height +1;
        menu.clip.width = body.clip.width + (proto.menuBorder * 2) +1;
        menu.clip.height = body.clip.height + (proto.menuBorder * 2) +1;
        if (menu.Menu.enableTracker) {
            menu.Menu.disableHide = true;
            setMenuTracker(menu.Menu);
        }
  
    } else if (document.all) {
        var menu = container.document.all("menuLayer" + x);
        container.menus[x].menuLayer = menu;
        container.menus[x].menuLayer.Menu = container.menus[x];
        container.menus[x].menuLayer.Menu.container = menu;
        proto = container.menus[x].prototypeStyles || this.prototypeStyles || container.menus[x];
        proto.menuItemWidth = proto.menuItemWidth || 200;
        menu.style.backgroundColor = proto.menuBorderBgColor;
        for (var i=0; i<container.menus[x].items.length; i++) {
			var id = getMenuItemID(x,i);
            var l  = container.document.all["menuItem" + id];
            l.Menu = container.menus[x];
            proto = container.menus[x].prototypeStyles || this.prototypeStyles || container.menus[x];
            l.style.pixelWidth = proto.menuItemWidth;
            l.style.pixelHeight = proto.menuItemHeight;
            if (i>0) l.style.pixelTop = container.document.all["menuItem" + (id-1)].style.pixelTop + container.document.all["menuItem" + (id-1)].style.pixelHeight + proto.menuItemBorder;
            l.style.fontSize = proto.fontSize;
            l.style.backgroundColor = proto.menuItemBgColor;
            l.style.visibility = "inherit";
            l.saveColor = proto.menuItemBgColor;
            l.menuHiliteBgColor = proto.menuHiliteBgColor;
            l.action = container.menus[x].actions[i];
            l.hilite = container.document.all["menuItemHilite" + id];
            l.focusItem = container.document.all["focusItem" + x];
            l.focusItem.style.pixelTop = -30;
            l.mouseover = l.Menu.mouseovers[x];
            l.mouseout  = l.Menu.mouseouts[x];
            var childItem = container.document.all["childMenu" + id];
            if (childItem) {
                l.childMenu = container.menus[x].items[i].menuLayer;
                childItem.style.pixelLeft = l.style.pixelWidth -11;
                childItem.style.pixelTop = (l.style.pixelHeight /2) -4;
                childItem.style.pixelWidth = 30 || 7;
                childItem.style.clip = "rect(0 7 7 3)";
                l.Menu.childMenus[l.Menu.childMenus.length] = l.childMenu;
            }
            var sep = container.document.all["menuSeparator" + id];
            if (sep) {
                sep.style.clip = "rect(0 " + (proto.menuItemWidth - 3) + " 1 0)";
                sep.style.backgroundColor = proto.bgColor;
                sep = container.document.all["menuSeparatorLite" + id];
                sep.style.clip = "rect(1 " + (proto.menuItemWidth - 3) + " 2 0)";
                sep.style.backgroundColor = proto.menuLiteBgColor;
                l.style.pixelHeight = proto.menuItemHeight/2;
                l.isSeparator = true
            }
        }
        proto.menuHeight = (l.style.pixelTop + l.style.pixelHeight);
        var lite = container.document.all["menuLite" + x];
        lite.style.pixelHeight = proto.menuHeight +2;
        lite.style.pixelWidth = proto.menuItemWidth + 2;
        lite.style.backgroundColor = proto.menuLiteBgColor;
        var body = container.document.all["menuFg" + x];
        body.style.pixelHeight = proto.menuHeight + 1;
        body.style.pixelWidth = proto.menuItemWidth + 1;
        body.style.backgroundColor = proto.bgColor;
        container.menus[x].menuLayer.style.pixelWidth  = proto.menuWidth || proto.menuItemWidth + (proto.menuBorder * 2) +2;
        container.menus[x].menuLayer.style.pixelHeight = proto.menuHeight + (proto.menuBorder * 2) +2;
        if (menu.Menu.enableTracker) {
            menu.Menu.disableHide = true;
            setMenuTracker(menu.Menu);
        }

    } else if (document.getElementById) {
        var menu = document.getElementById("menuLayer" + x);
        container.menus[x].menuLayer = menu;
        container.menus[x].menuLayer.Menu = container.menus[x];
        container.menus[x].menuLayer.Menu.container = menu;
        proto = container.menus[x].prototypeStyles || this.prototypeStyles || container.menus[x];
        proto.menuItemWidth = proto.menuItemWidth || 200;
        menu.style.backgroundColor = proto.menuBorderBgColor;
        for (var i=0; i<container.menus[x].items.length; i++) {
	        var id = getMenuItemID(x,i);
            var l = document.getElementById("menuItem" + id);
            l.Menu = container.menus[x];
            proto = container.menus[x].prototypeStyles || this.prototypeStyles || container.menus[x];
            l.style.width = proto.menuItemWidth;
            l.style.height = proto.menuItemHeight;
            if (i>0) l.style.top = document.getElementById("menuItem" + (id-1)).style.pixelTop + document.getElementById("menuItem" + (id-1)).style.height + proto.menuItemBorder;
            l.style.fontSize = proto.fontSize;
            l.style.backgroundColor = proto.menuItemBgColor;
            l.style.visibility = "inherit";
            l.saveColor = proto.menuItemBgColor;
            l.menuHiliteBgColor = proto.menuHiliteBgColor;
            l.action = container.menus[x].actions[i];
            l.hilite = document.getElementById("menuItemHilite" + id);
            l.focusItem = document.getElementById("focusItem" + x);
            l.focusItem.style.top = -30;
            l.mouseover = l.Menu.mouseovers[x];
            l.mouseout  = l.Menu.mouseouts[x];
            l.onmouseover = proto.onMenuItemOver;
            var childItem = document.getElementById("childMenu" + id);
            if (childItem) {
                l.childMenu = container.menus[x].items[i].menuLayer;
                childItem.style.left = l.style.width -11;
                childItem.style.top = (l.style.height /2) -4;
                childItem.style.width = 30 || 7;
                childItem.style.clip = "rect(0,7,7,3)";
                l.Menu.childMenus[l.Menu.childMenus.length] = l.childMenu;
            }
            var sep = document.getElementById("menuSeparator" + id);
            if (sep) {
                sep.style.clip = "rect(0," + (proto.menuItemWidth - 3) + ",1,0)";
                sep.style.backgroundColor = proto.bgColor;
                sep = document.getElementById("menuSeparatorLite" + id);
                sep.style.clip = "rect(1," + (proto.menuItemWidth - 3) + ",2,0)";
                sep.style.backgroundColor = proto.menuLiteBgColor;
//                l.style.height = proto.menuItemHeight/2;
                l.isSeparator = true
            }
        }
        proto.menuHeight = (parseInt(l.style.top) + parseInt(l.style.height));
        var lite = document.getElementById("menuLite" + x);
        lite.style.height = proto.menuHeight +2;
        lite.style.width = proto.menuItemWidth + 2;
        lite.style.backgroundColor = proto.menuLiteBgColor;
        var body = document.getElementById("menuFg" + x);
        body.style.height = proto.menuHeight + 1;
        body.style.width = proto.menuItemWidth + 1;
        body.style.backgroundColor = proto.bgColor;
        container.menus[x].menuLayer.style.width  = proto.menuWidth || proto.menuItemWidth + (proto.menuBorder * 2) +2;
        container.menus[x].menuLayer.style.height = proto.menuHeight + (proto.menuBorder * 2) +2;
        if (menu.Menu.enableTracker) {
            menu.Menu.disableHide = true;
            setMenuTracker(menu.Menu);
        }
    }
}


function setContainerProperty(container) 
{
    if (document.layers) {
        container.clip.width = window.innerWidth;
        container.clip.height = window.innerHeight;
        container.onmouseout = this.hideMenu;
        container.menuContainerBgColor = this.menuContainerBgColor;
	}
	if (!useDynamicMenu) {
        for (var i=0; i<container.menus.length; i++) {
			setMenuProperty(container, i);
		}
	}
    if (document.all) {
        container.document.all("menuContainer").style.backgroundColor = container.menus[0].menuContainerBgColor;
        container.document.saveBgColor = container.document.bgColor;
    } else if (document.getElementById) {
        container.style.backgroundColor = container.menus[0].menuContainerBgColor;
        container.saveBgColor = container.bgColor;
    }
}


function writeMenus(container) 
{
    if (!window.attemptCount)
        window.attemptCount = 1;
    if (!container && document.layers) {
        if (eval("document.width"))
            container = new Layer(1000);

    } else if (!container && document.all) {
        if  (!document.all["menuContainer"]) {
            container = document.createElement("DIV") 
            container.id = "menuContainer" 
	        document.getElementsByTagName("BODY").item(0).appendChild(container)
        }
        container = document.all["menuContainer"];

    } else if (!container && document.getElementById && document.getElementsByTagName("BODY")) {
        if (!document.getElementById("menuContainer")) {
	        container = document.createElement("DIV") 
	        container.id = "menuContainer" 
	        document.getElementsByTagName("BODY").item(0).appendChild(container)
	        container.style.backgroundColor = this.menuContainerBgColor
        } else {
	        container = document.getElementById("menuContainer")
        }
    }
    if (!container && window.attemptCount < 10) {
        window.delayWriteMenus = this.writeMenus;
        window.menuContainerBgColor = this.menuContainerBgColor;
        window.attemptCount++;
        setTimeout('delayWriteMenus()', 3000);
        return;
    }
    container.isContainer = "menuContainer" + menuContainers.length;
    menuContainers[menuContainers.length] = container;
    container.menus = new Array();
    for (var i=0; i<window.menus.length; i++) {
        container.menus[i] = window.menus[i];
	}
    window.menus.length = 0;

	// Get menus html-content
    var content = '';
	if (window.useDynamicMenu && document.layers) window.useDynamicMenu = false;
	if (!useDynamicMenu) {
	    for (var i=0; i<container.menus.length; i++) {
			content += getMenuContent(container,i);
	    }
	}

    if (container.innerHTML) {
        container.innerHTML=content;
    } else if (!document.all && document.getElementById) {
//	dynamiccontentNS6(container,content)
        container.innerHTML=content;
    } else {
        container.document.open("text/html");
        container.document.writeln(content);
        container.document.close();
    }
	// Set containers propertyes
	setContainerProperty(container);
    window.wroteMenu = true;
}


function onMenuItemOver(e, l, a) 
{
    l = l || this;
    a = a || window.ActiveMenuItem;
    if (document.layers) {
        if (a) {
            a.document.bgColor = a.saveColor;
            if (a.hilite) a.hilite.visibility = "hidden";
            if (a.childMenu) a.document.layers[1].document.images[0].src = a.icon;
        } else {
            a = new Object();
        }
        if (this.mouseover && this.id != a.id) {
            if (this.mouseover.length > 4) {
                var ext = this.mouseover.substring(this.mouseover.length-4);
                if (ext == ".gif" || ext == ".jpg") {
                    this.document.layers[1].document.images[0].src = this.mouseover;
                } else {
                    eval("" + this.mouseover);
                }
            }
        }
        if (l.hilite) {
            l.document.bgColor = l.menuHiliteBgColor;
            l.zIndex = 1;
            l.hilite.visibility = "inherit";
            l.hilite.zIndex = 2;
            l.document.layers[1].zIndex = 1;
            l.focusItem.zIndex = this.zIndex +2;
        }
        l.focusItem.top = this.top;
        l.Menu.hideChildMenu(l);
    } else if (l.style) {
        document.onmousedown=l.Menu.onMenuItemDown;
        if (a) {
            a.style.backgroundColor = a.saveColor;
            if (a.hilite) a.hilite.style.visibility = "hidden";
        } else {
            a = new Object();
		}
        if (l.mouseover && l.id != a.id) {
            if (l.mouseover.length > 4) {
                var ext = l.mouseover.substring(l.mouseover.length-4);
                if (ext == ".gif" || ext == ".jpg") {
                    l.document.images[l.id + "Img"].src = l.mouseover;
                } else {
                    eval("" + l.mouseover);
                }
            }
        }
		if (l.isSeparator) return;
        l.style.backgroundColor = l.menuHiliteBgColor;
        if (l.hilite) {
            l.style.backgroundColor = l.menuHiliteBgColor;
            l.hilite.style.visibility = "inherit";
        }
	if (l.style.pixelTop) {
          l.focusItem.style.pixelTop = l.style.pixelTop;
	} else {
          l.focusItem.style.top = l.style.top;
        }
        if (isNaN(l.zIndex)) {
           l.zIndex = 1;
        }
        l.focusItem.style.zIndex = l.zIndex +1;
        l.zIndex = 1;
        l.Menu.hideChildMenu(l);
    }
    window.ActiveMenuItem = l;
}


function onMenuItemOut(e, l, a) 
{
    l = l || this;
    a = a || window.ActiveMenuItem;

    if (l.id.indexOf("focusItem")) {
        if (a && l.top) {
            l.top = -30;
	        if (a.mouseout && a.id != l.id) {
	            if (a.mouseout.length > 4) {
	                var ext = a.mouseout.substring(a.mouseout.length-4);
	                if (ext == ".gif" || ext == ".jpg") {
			            a.document.layers[1].document.images[0].src = a.mouseout;
   	                } else {
		               eval("" + a.mouseout);
	                }
	            }
	        }
        } else if (a && l.style) {
            document.onmousedown=null;
            window.event.cancelBubble=true;
	        if (l.mouseout) {
	            if (l.mouseout.length > 4) {
	                var ext = l.mouseout.substring(l.mouseout.length-4);
 	                if (ext == ".gif" || ext == ".jpg") {
		                l.document.images[l.id + "Img"].src = l.mouseout;
	                } else {
		                eval("" + l.mouseout);
	                }
	            }
	        }
        }
    }
}


function onMenuItemAction(e, l) 
{
    l = window.ActiveMenuItem;
    if (!l) return;
    if (!ActiveMenu.Menu.disableHide) hideActiveMenus(ActiveMenu.menuLayer);
    if (l.action) {
        eval("" + l.action);
    }
}


function getMenuLayer(menu) 
{
    var l = menu.menuLayer || menu;
	var n_container = 0;
	var n_menu;

	if (typeof(menu) == "string") {
		if (document.all) {
	    	l = document.all[menu];
		} else if (document.getElementById) {
            l = document.getElementById(menu);
		}
        for (var n=0; n < menuContainers.length; n++) {
            l = menuContainers[n].menus[menu];
             for (var i=0; i<menuContainers[n].menus.length; i++) {
                 if (menu == menuContainers[n].menus[i].label) {
					if (useDynamicMenu) break;
					l = menuContainers[n].menus[i].menuLayer;
				 }
                 if (l) break;
             }
			 if (i<menuContainers[n].menus.length) break;
        }
		if (useDynamicMenu) {
			n_container = n;
			n_menu = i;
		}
    } else {
		if (useDynamicMenu) {
			n_menu = menu.number;
		}
	}
	if (useDynamicMenu) {
		var container = menuContainers[n_container];
		var content = getMenuContent(container, n_menu);
      		container.innerHTML = content;
		setMenuProperty(container, n_menu);
		l = menuContainers[n_container].menus[n_menu].menuLayer;
	}
	return l;
}


function showMenu(menu, x, y, child) 
{
    if (!window.wroteMenu) return;
    if (document.layers) {
        if (menu) {
            var l = menu.menuLayer || menu;
            if (typeof(menu) == "string") {
                for (var n=0; n < menuContainers.length; n++) {
                    l = menuContainers[n].menus[menu];
                    for (var i=0; i<menuContainers[n].menus.length; i++) {
                        if (menu == menuContainers[n].menus[i].label) l = menuContainers[n].menus[i].menuLayer;
                        if (l) break;
                    }
                }
				if (!l) return;
            }
            l.Menu.container.document.bgColor = null;
            l.left = 1;
            l.top = 1;
            hideActiveMenus(l);
            if (this.visibility) l = this;
            window.ActiveMenu = l;
            window.releaseEvents(Event.MOUSEMOVE|Event.MOUSEUP);
            setTimeout('if(window.ActiveMenu)window.ActiveMenu.Menu.setMouseTracker();', 300);
        } else {
            var l = child;
        }
        if (!l) return;
        for (var i=0; i<l.layers.length; i++) {
            if (!l.layers[i].isHilite)
                l.layers[i].visibility = "inherit";
            if (l.layers[i].document.layers.length > 0)
                showMenu(null, "relative", "relative", l.layers[i]);
        }
        if (l.parentLayer) {

  			offX = 14; offY = 0;
            if (x != "relative")
                l.parentLayer.left = x || window.pageX || 0;
            if (y != "relative")
                l.parentLayer.top = y || window.pageY || 0;
            if (l.parentLayer.left + l.clip.width + offX > window.pageXOffset + window.innerWidth) 
                l.parentLayer.left = (window.pageXOffset + window.innerWidth - l.clip.width - offX);
            if (l.parentLayer.top + l.clip.height + offY > window.pageYOffset + window.innerHeight)
                l.parentLayer.top = (window.pageYOffset + window.innerHeight - l.clip.height - offY);
            if (l.parentLayer.isContainer) {
                l.Menu.xOffset = window.pageXOffset;
                l.Menu.yOffset = window.pageYOffset;
                l.parentLayer.clip.width = window.ActiveMenu.clip.width +2;
                l.parentLayer.clip.height = window.ActiveMenu.clip.height +2;
                if (l.parentLayer.menuContainerBgColor) l.parentLayer.document.bgColor = l.parentLayer.menuContainerBgColor;
            }
        }
        l.visibility = "inherit";
        if (l.Menu) l.Menu.container.visibility = "inherit";

    } else if (document.all) {
        var l = menu.menuLayer || menu;
        hideActiveMenus(l);
		l = getMenuLayer(menu);
        window.ActiveMenu = l;
        l.style.visibility = "inherit";
        if (x != "relative")
            l.style.pixelLeft = x || (window.pageX + document.body.scrollLeft) || 0;
        if (y != "relative")
            l.style.pixelTop = y || (window.pageY + document.body.scrollTop) || 0;

		if ( l.style.pixelLeft + l.style.pixelWidth > document.body.scrollLeft + document.body.clientWidth) {
           l.style.pixelLeft = document.body.scrollLeft + document.body.clientWidth - l.style.pixelWidth;
		}
		if ( l.style.pixelTop + l.style.pixelHeight > document.body.scrollTop + document.body.clientHeight) {
		   l.style.pixelTop = document.body.scrollTop + document.body.clientHeight - l.style.pixelHeight;
		}

        l.Menu.xOffset = document.body.scrollLeft;
        l.Menu.yOffset = document.body.scrollTop;

    } else if (document.getElementById) {
        var l = menu.menuLayer || menu;
        hideActiveMenus(l);
		l = getMenuLayer(menu);
        window.ActiveMenu = l;
		offX = 14; offY = 0;
        l.style.visibility = "inherit";
        if (x != "relative")
            l.style.left = x || parseInt(window.pageX) || 0;
        if (y != "relative")
            l.style.top = y || parseInt(window.pageY) || 0;
		if ( parseInt(l.style.left) + parseInt(l.style.width) + offX > window.pageXOffset + window.innerWidth) {
           l.style.left = window.pageXOffset + window.innerWidth - parseInt(l.style.width) - offX;
		}
		if ( parseInt(l.style.top) + parseInt(l.style.height) + offY > window.pageYOffset + window.innerHeight) {
		   l.style.top = window.pageYOffset + window.innerHeight - parseInt(l.style.height) - offY;
		}

        l.Menu.xOffset = window.pageXOffset;
        l.Menu.yOffset = window.pageYOffset;
        l.Menu.container.style.background = l.Menu.menuContainerBgColor;
    }
    if (menu) {
        window.activeMenus[window.activeMenus.length] = l;
    }
}


function hideMenu(e) 
{
    var l = e || window.ActiveMenu;
    if (!l) return true;
    if (l.menuLayer) {
        l = l.menuLayer;
    } else if (this.visibility) {
        l = this;
    }
    if (l.menuLayer) {
        l = l.menuLayer;
    }
    var a = window.ActiveMenuItem;
    document.saveMousemove = document.onmousemove;
    document.onmousemove = mouseTracker;
    if (a && document.layers) {
        a.document.bgColor = a.saveColor;
        a.focusItem.top = -30;
        if (a.hilite) a.hilite.visibility = "hidden";
        if (a.childMenu) a.document.layers[1].document.images[0].src = a.icon;
        if (mDrag.oldX <= e.pageX+3 && mDrag.oldX >= e.pageX-3 && mDrag.oldY <= e.pageY+3 && mDrag.oldY >= e.pageY-3) {
            if (a.action && window.ActiveMenu) setTimeout('window.ActiveMenu.Menu.onMenuItemAction();', 2);
        } else if (document.saveMousemove == mDrag.doMenuDrag) {
            if (window.ActiveMenu) return true;
        }
    } else if (window.ActiveMenu && (document.all||document.getElementById)) {
        document.onmousedown=null;
        if (a) {
            a.style.backgroundColor = a.saveColor;
            if (a.hilite) a.hilite.style.visibility = "hidden";
        }
        if (document.saveMousemove == mDrag.doMenuDrag) {
            return true;
        }
	}
    if (window.ActiveMenu) {
        if (window.ActiveMenu.Menu) {
            if (window.ActiveMenu.Menu.disableHide) return true;
            e = window.event || e;
            if (!window.ActiveMenu.Menu.enableHideOnMouseOut && e.type == "mouseout") return true;
        }
    }
    hideActiveMenus(l);
    return true;
}


function hideChildMenu(menuLayer) 
{
    var l = menuLayer || this;
    for (var i=0; i < l.Menu.childMenus.length; i++) {
        if (document.layers) {
            l.Menu.childMenus[i].visibility = "hidden";
        } else if (document.all || document.getElementById) {
            l.Menu.childMenus[i].style.visibility = "hidden";
        }
        l.Menu.childMenus[i].Menu.hideChildMenu(l.Menu.childMenus[i]);
    }
    if (l.childMenu) {
        if (document.layers) {
            l.Menu.container.document.bgColor = null;
            l.Menu.showMenu(null,null,null,l.childMenu.layers[0]);
            l.childMenu.zIndex = l.parentLayer.zIndex +1;
            l.childMenu.top = l.top + l.parentLayer.top + l.Menu.menuLayer.top;
            if (l.childMenu.left + l.childMenu.clip.width > window.innerWidth) {
                l.childMenu.left = l.parentLayer.left - l.childMenu.clip.width + l.Menu.menuLayer.top + 15;
                l.Menu.container.clip.left -= l.childMenu.clip.width;
            } else if (l.Menu.childMenuDirection == "left") {
                l.childMenu.left = l.parentLayer.left - l.parentLayer.clip.width;
                l.Menu.container.clip.left -= l.childMenu.clip.width;
            } else {
                l.childMenu.left = l.parentLayer.left + l.parentLayer.clip.width  + l.Menu.menuLayer.left -5;
            }
            l.Menu.container.clip.width += l.childMenu.clip.width +100;
            l.Menu.container.clip.height += l.childMenu.clip.height;
            l.document.layers[1].zIndex = 0;
            l.document.layers[1].document.images[0].src = l.iconHilite;
            l.childMenu.visibility = "inherit";
        } else if (document.all) {
            l.childMenu.style.zIndex = l.Menu.menuLayer.style.zIndex +1;
            l.childMenu.style.pixelTop = l.style.pixelTop + l.Menu.menuLayer.style.pixelTop;
            if (l.childMenu.style.pixelLeft + l.childMenu.style.pixelWidth > document.width) {
                l.childMenu.style.pixelLeft = l.childMenu.style.pixelWidth + l.Menu.menuLayer.style.pixelTop + 15;
            } else if (l.Menu.childMenuDirection == "left") {
                //l.childMenu.style.pixelLeft = l.parentLayer.left - l.parentLayer.clip.width;
            } else {
                l.childMenu.style.pixelLeft = l.Menu.menuLayer.style.pixelWidth + l.Menu.menuLayer.style.pixelLeft -5;
            }
            l.childMenu.style.visibility = "inherit";
        } else if (document.getElementById) {
            l.childMenu.style.zIndex = l.Menu.menuLayer.style.zIndex +1;
            l.childMenu.style.top = l.style.top + l.Menu.menuLayer.style.top;
            if (l.childMenu.style.left + l.childMenu.style.width > document.width) {
                l.childMenu.style.left = l.childMenu.style.width + l.Menu.menuLayer.style.top + 15;
            } else if (l.Menu.childMenuDirection == "left") {
                //l.childMenu.style.pixelLeft = l.parentLayer.left - l.parentLayer.clip.width;
            } else {
                l.childMenu.style.left = l.Menu.menuLayer.style.width + l.Menu.menuLayer.style.left -5;
            }
            l.childMenu.style.visibility = "inherit";
        }
        if (!l.childMenu.disableHide)
            window.activeMenus[window.activeMenus.length] = l.childMenu;
    }
}


function hideActiveMenus(l) 
{
    if (!window.activeMenus) return;
    for (var i=0; i < window.activeMenus.length; i++) {
    if (!activeMenus[i]) return;
        if (activeMenus[i].visibility && activeMenus[i].Menu) {
            activeMenus[i].visibility = "hidden";
            activeMenus[i].Menu.container.visibility = "hidden";
	    if (document.getElementById) {
              activeMenus[i].Menu.container.left = 0;
            } else {
              activeMenus[i].Menu.container.clip.left = 0;
            }
        } else if (activeMenus[i].style) {
            activeMenus[i].style.visibility = "hidden";
        }
    }
    document.onmousemove = mouseTracker;
    window.activeMenus.length = 0;
}


function mouseTracker(e) 
{
    e = e || window.Event || window.event;
    window.pageX = e.pageX || e.clientX;
    window.pageY = e.pageY || e.clientY;
}


function setMouseTracker() 
{
    if (document.captureEvents) {
        document.captureEvents(Event.MOUSEMOVE|Event.MOUSEUP);
    }
    document.onmousemove = this.mouseTracker;
    document.onmouseup = this.hideMenu;
}


function setMenuTracker(menu) 
{
    if (!window.menuTrackers) window.menuTrackers = new Array();
    menuTrackers[menuTrackers.length] = menu;
    window.menuTrackerID = setInterval('menuTracker()',10);
}


function menuTracker() 
{
    for (var i=0; i < menuTrackers.length; i++) {
        if (!isNaN(menuTrackers[i].xOffset) && document.layers) {
            var off = parseInt((menuTrackers[i].xOffset - window.pageXOffset) / 10);
            if (isNaN(off)) off = 0;
            if (off != 0) {
                menuTrackers[i].container.left += -off;
                menuTrackers[i].xOffset += -off;
            }
        }
        if (!isNaN(menuTrackers[i].yOffset) && document.layers) {
            var off = parseInt((menuTrackers[i].yOffset - window.pageYOffset) / 10);
            if (isNaN(off)) off = 0;
            if (off != 0) {
                menuTrackers[i].container.top += -off;
                menuTrackers[i].yOffset += -off;
            }
        }
        if (!isNaN(menuTrackers[i].xOffset) && document.body) {
            var off = parseInt((menuTrackers[i].xOffset - document.body.scrollLeft) / 10);
            if (isNaN(off)) off = 0;
            if (off != 0) {
                menuTrackers[i].menuLayer.style.pixelLeft += -off;
                menuTrackers[i].xOffset += -off;
            }
        }
        if (!isNaN(menuTrackers[i].yOffset) && document.body) {
            var off = parseInt((menuTrackers[i].yOffset - document.body.scrollTop) / 10);
            if (isNaN(off)) off = 0;
            if (off != 0) {
                menuTrackers[i].menuLayer.style.pixelTop += -off;
                menuTrackers[i].yOffset += -off;
            }
        }
    }
}


function onMenuItemDown(e, l) 
{
    l = l || window.ActiveMenuItem || this;
    if (!l.Menu) {
    } else {
        if (document.layers) {
            mDrag.dragLayer = l.Menu.container;
            mDrag.startMenuDrag(e);
        } else {
            mDrag.dragLayer = l.Menu.container.style;
            mDrag.startMenuDrag(e);
            window.event.cancelBubble=true;
        }
    }
}


function startMenuDrag(e) 
{
    if (document.layers) {
        if (e.which > 1) {
            if (window.ActiveMenu) ActiveMenu.Menu.container.visibility = "hidden";
            window.ActiveMenu = null;
            return true;
        }
        document.captureEvents(Event.MOUSEMOVE);
        var x = e.pageX;
        var y = e.pageY;
    } else {
        var x = window.event.clientX;
        var y = window.event.clientY;
    }
    mDrag.offX = x;
    mDrag.offY = y;
    mDrag.oldX = x;
    mDrag.oldY = y;
    if (!ActiveMenu.Menu.disableDrag) document.onmousemove = mDrag.doMenuDrag;
    return false;
}


function doMenuDrag(e) 
{
    if (document.layers) {
        mDrag.dragLayer.moveBy(e.pageX-mDrag.offX,e.pageY-mDrag.offY);
        mDrag.offX = e.pageX;
        mDrag.offY = e.pageY;
    } else if (document.all) {
        mDrag.dragLayer.pixelLeft = window.event.offsetX;
        mDrag.dragLayer.pixelTop  = window.event.offsetY;
        return false; //for IE
    } else if (document.getElementById) {
        mDrag.dragLayer.left = window.event.offsetX;
        mDrag.dragLayer.top  = window.event.offsetY;
        return false; //for ns6
    }
}
