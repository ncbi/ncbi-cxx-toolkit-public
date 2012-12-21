( function() {
    // Beginning of anon function that holds all jig code. jig code has:
    // 1) jQuery core lib
    // 2) Shortcuts to jQuery and noConflict statement to prevent people from calling "$" from outside this script
    // 3) Concatenated jQuery plugins
    // 4) Core jQuery UI code
    // 5) jQuery.ui.jig utility methods for use with ncbi-written UI widgets
    // 6) jig.decl which figures out what widget scripts and CSS files to write into head of doc


    // check to see if we already have jig
    // if we do, just exit and don't evaluate
    if ( typeof jQuery !== "undefined" && typeof jQuery.ui !== "undefined" && typeof jQuery.ui.jig !== "undefined") {
        return;
    } 
    
    
    // add a shortcut to jQuery once it's defined
    var $ = jQuery;

    // don't allow calls to $ from outside this function
    $.noConflict();
/**
 * Cookie plugin
 *
 * Copyright (c) 2006 Klaus Hartl (stilbuero.de)
 * Dual licensed under the MIT and GPL licenses:
 * http://www.opensource.org/licenses/mit-license.php
 * http://www.gnu.org/licenses/gpl.html
 *
 */

/**
 * Create a cookie with the given name and value and other optional parameters.
 *
 * @example $.cookie('the_cookie', 'the_value');
 * @desc Set the value of a cookie.
 * @example $.cookie('the_cookie', 'the_value', { expires: 7, path: '/', domain: 'jquery.com', secure: true });
 * @desc Create a cookie with all available options.
 * @example $.cookie('the_cookie', 'the_value');
 * @desc Create a session cookie.
 * @example $.cookie('the_cookie', null);
 * @desc Delete a cookie by passing null as value. Keep in mind that you have to use the same path and domain
 *       used when the cookie was set.
 *
 * @param String name The name of the cookie.
 * @param String value The value of the cookie.
 * @param Object options An object literal containing key/value pairs to provide optional cookie attributes.
 * @option Number|Date expires Either an integer specifying the expiration date from now on in days or a Date object.
 *                             If a negative value is specified (e.g. a date in the past), the cookie will be deleted.
 *                             If set to null or omitted, the cookie will be a session cookie and will not be retained
 *                             when the the browser exits.
 * @option String path The value of the path atribute of the cookie (default: path of page that created the cookie).
 * @option String domain The value of the domain attribute of the cookie (default: domain of page that created the cookie).
 * @option Boolean secure If true, the secure attribute of the cookie will be set and the cookie transmission will
 *                        require a secure protocol (like HTTPS).
 * @type undefined
 *
 * @name $.cookie
 * @cat Plugins/Cookie
 * @author Klaus Hartl/klaus.hartl@stilbuero.de
 */

/**
 * Get the value of a cookie with the given name.
 *
 * @example $.cookie('the_cookie');
 * @desc Get the value of a cookie.
 *
 * @param String name The name of the cookie.
 * @return The value of the cookie.
 * @type String
 *
 * @name $.cookie
 * @cat Plugins/Cookie
 * @author Klaus Hartl/klaus.hartl@stilbuero.de
 */
jQuery.cookie = function(name, value, options) {
    if (typeof value != 'undefined') { // name and value given, set cookie
        options = options || {};
        if (value === null) {
            value = '';
            options.expires = -1;
        }
        var expires = '';
        if (options.expires && (typeof options.expires == 'number' || options.expires.toUTCString)) {
            var date;
            if (typeof options.expires == 'number') {
                date = new Date();
                date.setTime(date.getTime() + (options.expires * 24 * 60 * 60 * 1000));
            } else {
                date = options.expires;
            }
            expires = '; expires=' + date.toUTCString(); // use expires attribute, max-age is not supported by IE
        }
        // CAUTION: Needed to parenthesize options.path and options.domain
        // in the following expressions, otherwise they evaluate to undefined
        // in the packed version for some reason...
        var path = options.path ? '; path=' + (options.path) : '';
        var domain = options.domain ? '; domain=' + (options.domain) : '';
        var secure = options.secure ? '; secure' : '';
        document.cookie = [name, '=', encodeURIComponent(value), expires, path, domain, secure].join('');
    } else { // only name given, get cookie
        var cookieValue = null;
        if (document.cookie && document.cookie != '') {
            var cookies = document.cookie.split(';');
            for (var i = 0; i < cookies.length; i++) {
                var cookie = jQuery.trim(cookies[i]);
                // Does this cookie string begin with the name we want?
                if ((cookie.substring(0, name.length + 1) == (name + '=')) && cookie.match('=') ) {
                    cookieValue = decodeURIComponent(cookie.substring(name.length + 1));
                    break;
		//handles IE case where empty cookie is returned without '='
                } else if ( !cookie.match('=') && (cookie.substring(0, name.length) == name)) {
		    cookieValue = '';
		    break;
		}
            }
        }
        return cookieValue;
    }
};
jQuery.fn.extend({

    elementFrame: function () { 

        var element = jQuery(this);

        if (arguments.length === 1) {

            var points = arguments[0];

            var tlPoint = points.topleft;
            var brPoint = points.bottomright;

            var top = tlPoint.y;
            var left = tlPoint.x;
            var width = points.width || brPoint.x - tlPoint.x;
            var height = points.height || brPoint.y - tlPoint.y;

            element.css("top", top + "px");
            element.css("left", left + "px");
            if (!points.ignoreDimensions) {
                element.width(width);
                element.height(height);
            }

            element.css("zIndex", points.zIndex);

            return this;
        } else {

            var element = jQuery(this);
            var position = element.offset();

            width = element.width();
            height = element.height();

            var outerWidth = element.outerWidth() || width;
            var outerHeight = element.outerHeight() || height;
            
            var xLeft = position.left;
            var xRight = xLeft + outerWidth;
            var yTop = position.top;
            var yBottom = yTop + outerHeight;

            var zIndex = element.css("z-index");

            return {
                "width": width,
                "height": height,
                "outerWidth": outerWidth,
                "outerHeight": outerHeight,
                "zIndex": zIndex,
                "topleft": {
                    "x": xLeft,
                    "y": yTop
                },
                "topright": {
                    "x": xRight,
                    "y": yTop
                },
                "bottomleft": {
                    "x": xLeft,
                    "y": yBottom
                },
                "bottomright": {
                    "x": xRight,
                    "y": yBottom
                }
            };
        }
    },

    //probably should not be part of fn?, just extend jQuery?
    windowFrame: function () {

        var bod = jQuery(document.body);
        var windowHeight = 0; 
        var windowWidth = jQuery(document.body).width(); 
        if (self.innerHeight) {
            windowHeight = self.innerHeight;
        } else if (document.documentElement && document.documentElement.clientHeight) {
            windowHeight = document.documentElement.clientHeight;
        } else if (document.body) {
            windowHeight = document.body.clientHeight;
        }
		
        var ml = parseInt(bod.css("margin-left"), 10);
        var mr = parseInt(bod.css("margin-right"), 10);

        var windowTop = self.pageYOffset || jQuery.boxModel && document.documentElement.scrollTop || document.body.scrollTop;
        var windowBottom = windowTop + windowHeight;
        var windowLeft = self.pageXOffset || jQuery.boxModel && document.documentElement.scrollLeft || document.body.scrollLeft;
        var windowRight = windowLeft + windowWidth + mr + ml;

        var scrollHeight = scrollWidth = 0;
        if(document.documentElement && document.documentElement.scrollHeight > scrollHeight){
            scrollHeight = document.documentElement.scrollHeight;
        }
        else if (document.body.scrollHeight > windowHeight){      
            scrollHeight = document.body.scrollHeight;
        }
        else { 
            scrollHeight = document.body.offsetHeight;
        }

        if(document.documentElement && document.documentElement.scrollWidth > scrollWidth){
            scrollWidth = document.documentElement.scrollWidth;
        }
        else if (document.body.scrollWidth > windowWidth){      
            scrollWidth = document.body.scrollWidth;
        }
        else { 
            scrollWidth = document.body.offsetWidth;
        }

        return {
            "width": windowWidth,
            "height": windowHeight,
            "scrollWidth": scrollWidth,
            "scrollHeight": scrollHeight,
            "topleft": {
                "x": windowLeft,
                "y": windowTop
            },
            "topright": {
                "x": windowRight,
                "y": windowTop
            },
            "bottomleft": {
                "x": windowLeft,
                "y": windowBottom
            },
            "bottomright": {
                "x": windowRight,
                "y": windowBottom
            }
        };

    },

    _availableFrameSizesHelper: function (w, h, t, r, b, l) {

        if (w === 0 || h === 0) {
            return null;
        } else {
            return {
                "width": w,
                "height": h,
                "topleft": {
                    "x": l,
                    "y": t
                },
                "topright": {
                    "x": r,
                    "y": t
                },
                "bottomleft": {
                    "x": l,
                    "y": b
                },
                "bottomright": {
                    "x": r,
                    "y": b
                }
            }
        }
    },

    availableFrameSizes: function () {

        var element = jQuery(this);
        var ef = element.elementFrame();
        var wf = element.windowFrame();

        var tEdge = wf.topleft.y;
        var lEdge = wf.topleft.x;
        var rEdge = wf.topright.x;
        var bEdge = wf.bottomright.y;

        var elemTop = ef.topleft.y;
        var elemBottom = ef.bottomleft.y;
        var elemLeft = ef.topleft.x;
        var elemRight = ef.topright.x;
        var elemWidth = ef.width;
        var elemHeight = ef.height;

        var topBlockHeight = elemTop - tEdge;
        if (topBlockHeight < 0) topBlockHeight = 0;

        var bottomBlockHeight = bEdge - elemBottom;
        if (bottomBlockHeight < 0) bottomBlockHeight = 0;

        var leftBlockWidth = elemLeft - lEdge;
        if (leftBlockWidth < 0) leftBlockWidth = 0;

        var rightBlockWidth = rEdge - elemRight;
        if (rightBlockWidth < 0) rightBlockWidth = 0;

        var topBlockObj = this._availableFrameSizesHelper(wf.width, topBlockHeight, tEdge, rEdge, elemTop, lEdge);
        var bottomBlockObj = this._availableFrameSizesHelper(wf.width, bottomBlockHeight, elemBottom, rEdge, bEdge, lEdge);

        var leftBlockObj = this._availableFrameSizesHelper(leftBlockWidth, wf.height, tEdge, elemRight, bEdge, lEdge);
        var rightBlockObj = this._availableFrameSizesHelper(rightBlockWidth, wf.height, tEdge, rEdge, bEdge, elemRight);

        var topleftBlockObj = this._availableFrameSizesHelper(leftBlockWidth, topBlockHeight, tEdge, elemLeft, elemTop, lEdge);
        var topcenterBlockObj = this._availableFrameSizesHelper(elemWidth, topBlockHeight, tEdge, elemRight, elemTop, elemLeft);
        var toprightBlockObj = this._availableFrameSizesHelper(rightBlockWidth, topBlockHeight, tEdge, rEdge, elemTop, elemRight);

        var bottomleftBlockObj = this._availableFrameSizesHelper(leftBlockWidth, bottomBlockHeight, elemBottom, elemLeft, bEdge, lEdge);
        var bottomcenterBlockObj = this._availableFrameSizesHelper(elemWidth, bottomBlockHeight, elemBottom, elemRight, bEdge, elemLeft);
        var bottomrightBlockObj = this._availableFrameSizesHelper(rightBlockWidth, bottomBlockHeight, elemBottom, rEdge, bEdge, elemRight);

        var middleLeftBlockObj = this._availableFrameSizesHelper(leftBlockWidth, elemHeight, elemTop, elemLeft, elemBottom, lEdge);
        var middleRightBlockObj = this._availableFrameSizesHelper(rightBlockWidth, elemHeight, elemTop, rEdge, elemBottom, elemRight);

        return {
            "top": topBlockObj,
            "bottom": bottomBlockObj,
            "left": leftBlockObj,
            "right": rightBlockObj,
            "topleft": topleftBlockObj,
            "topcenter": topcenterBlockObj,
            "topright": toprightBlockObj,
            "bottomleft": bottomleftBlockObj,
            "bottomcenter": bottomcenterBlockObj,
            "bottomright": bottomrightBlockObj,
            "middleleft": middleLeftBlockObj,
            "middleright": middleRightBlockObj
        }

    },

    elementFramePercentHidden: function () {

        var element = jQuery(this);
        var wf = element.windowFrame();
        var ef = element.elementFrame();

        var tEdge = wf.topleft.y;
        var lEdge = wf.topleft.x;
        var rEdge = wf.bottomright.x;
        var bEdge = wf.bottomright.y;

        var elemTop = ef.topleft.y;
        var elemBottom = ef.bottomleft.y;
        var elemLeft = ef.topleft.x;
        var elemRight = ef.topright.x;
        var elemWidth = ef.outerWidth;
        var elemHeight = ef.outerHeight;

        var lPixel = rPixel = tPixel = bPixel = 0;
        var lPercent = rPercent = tPercent = bPercent = 0;
        var isHidden = false;

        var calc = elemLeft - lEdge;
        if (calc < 0) {
            isHidden = true;
            lPixel = calc * -1;
            lPercent = (elemWidth + calc) / elemWidth * 100;
        }

        var calc = rEdge - elemRight;
        if (calc < 0) {
            isHidden = true;
            rPixel = calc * -1;
            rPercent = 100 - (elemWidth + calc) / elemWidth * 100;
        }

        var calc = elemTop - tEdge;
        if (calc < 0) {
            isHidden = true;
            tPixel = calc * -1;
            tPercent = (elemHeight + calc) / elemHeight * 100;
        }
        
        var scrollWidthAdjust = (!jQuery.browser.msie  && wf.scrollWidth > wf.width + 16)?-16:0;
        
        var calc = bEdge - elemBottom + scrollWidthAdjust;
        if (calc < 0) {
            isHidden = true;
            bPixel = calc * -1;
            bPercent = 100 - (elemHeight + calc) / elemHeight * 100;
        }

        return {

            "isHidden": isHidden,
            "left": lPercent,
            "right": rPercent,
            "top": tPercent,
            "bottom": bPercent,
            "pixels": {
                "left": lPixel,
                "right": rPixel,
                "top": tPixel,
                "bottom": bPixel
            }

        }

    },

    moveFrameIntoView: function () {

        var elem = jQuery(this);
        var percent = elem.elementFramePercentHidden();

        if (percent.isHidden) {

            var top = percent.pixels.top;
            var bot = percent.pixels.bottom;
            var lef = percent.pixels.left;
            var rig = percent.pixels.right;

            var adjY = top || -bot || 0;
            if (adjY !== 0) {
                var posY = parseInt(elem.css("top"), 10) + parseInt(adjY, 10);
                elem.css("top", posY + "px");
            }

            var adjX = lef || -rig || 0;
            if (adjX !== 0) {
                var posX = parseInt(elem.css("left"), 10) + parseInt(adjX, 10);
                elem.css("left", posX + "px");
            }
        }

    },

    /*
        setFramePosition - Positions element relative to another in the specified manner
            source : { - Element that has the data you want to position
                vertical - 'top', 'middle', 'bottom', 'XXX%', 'XXpx'
                hortizontal - 'right', 'center', 'left', 'XXX%', 'XXpx'
                adjustFit - 'none','autoAdjust'
            }
            target : { Element where you want to position the data relative too
                element - Element that we are getting position data from
                vertical - 'top', 'middle', 'bottom', 'XXX%', 'XXpx'
                hortizontal - 'right', 'center', 'lft', 'XXX%', 'XXpx'
            }
    */
    setFramePosition: function (source, target) {

        var sourceElement = jQuery(this).css("top", -1000).css("display", "block");
        var targetElement = jQuery(target.element);
        var elemFrame = sourceElement.elementFrame();
        var tarElemFrame = targetElement.elementFrame();
        var winFrame = sourceElement.windowFrame();

        //var parents = jQuery(elem).parents().get();
        var parents = sourceElement.parents().get();

        var isRelative = false;
        for (var i = 0; i < parents.length; i++) {
            var elemPosition = jQuery(parents[i]).css("position");
            if (elemPosition == "relative") {
                isRelative = true;
                break;
            }
        }

        if (isRelative) {
            var mountingPointX = 0;
            var mountingPointY = 0;
        } else {
            var mountingPointX = tarElemFrame.topleft.x;
            var mountingPointY = tarElemFrame.topleft.y;
        }

        var targetOffset = this._getFrameOffsetPosition(targetElement, tarElemFrame.height, tarElemFrame.width, target.vertical, target.hortizontal);
        mountingPointX += targetOffset.offsetX;
        mountingPointY += targetOffset.offsetY;
        
        var sourceOffset = this._getFrameOffsetPosition(sourceElement, elemFrame.height, elemFrame.width, source.vertical, source.hortizontal);

        var custOffsetX = source.customOffsetX || 0;
        var custOffsetY = source.customOffsetY || 0;

        var newLocationPointX = mountingPointX + custOffsetX - sourceOffset.offsetX;
        var newLocationPointY = mountingPointY + custOffsetY - sourceOffset.offsetY;

        if (isRelative) {
            newLocationPointX -= mountingPointX;
            newLocationPointY -= mountingPointY;
        }
        
        var newLocation = {
            "topleft": {
                "x": newLocationPointX,
                "y": newLocationPointY
            },
            "width": elemFrame.width,
            "height": elemFrame.height,
            "ignoreDimensions": true
        };
        
        var opac1 = sourceElement.css("opacity");
        var opac2 = sourceElement.css("filter");
        var opac3 = sourceElement.css("-ms-filter");

        sourceElement.css("opacity", 0).css("filter", "alpha(opacity=0)").css("-ms-filter", "progid:DXImageTransform.Microsoft.Alpha(Opacity=0)").elementFrame(newLocation);

        if (!source.adjustFit || source.adjustFit.toLowerCase() === "autoadjust" || source.adjustFit.toLowerCase() === "slide") {
            sourceElement.moveFrameIntoView();
        }

        sourceElement.css("display", "none").css("opacity", opac1).css("filter", opac2).css("-ms-filter", opac3);

        return this;

    },

    _getFrameOffsetPosition: function (elem, height, width, vert, hort) {

        function _calcNumber(str, widHei) {
            if (str.indexOf("%") > 0) {
                var per = parseFloat(str) / 100;
                return Math.floor(widHei * per);
            } else if (str.length > 0) {
                return parseInt(str);
            } else {
                return 0;
            }

        }
        
        var t = 0;
        var l = 0;

        switch (vert.toLowerCase()) {

        case 'top':
        case 't':
            t = parseInt(elem.css("margin-top"), 10) || 0;
            break;

        case 'middle':
        case 'm':
        case 'center':
        case 'c':        
            t = Math.floor(height / 2);
            break;

        case 'bottom':
        case 'b':
            t = height; //+ (parseInt(elem.css("margin-bottom"),10) || 0);
            break;
        default:
            t = _calcNumber(vert, height);
            if (isNaN(t)) t = 0;
        }

        switch (hort.toLowerCase()) {

        case 'left':
        case 'l':
            l = parseInt(elem.css("margin-left"), 10) || 0;
            break;

        case 'center':
        case 'c': 
            l = Math.floor(width / 2);
            break;

        case 'right':
        case 'r':
            l = width; // - ( parseInt(elem.css("margin-right"),10) || 0);
            break;
        default:
            l = _calcNumber(hort, width);
            if (isNaN(l)) l = 0;
        }
        
        return {
            "offsetX": l,
            "offsetY": t
        };
    },

    centerFrameInWindow: function () {

        var element = jQuery(this);

        var isHidden = false;

        if (element.is(":visible").length === 0) {
            isHidden = true;
            element.css("top", "-10000px").css("display", "block");
        }

        var ef = element.elementFrame();
        var wf = element.windowFrame();

        if (isHidden) {
            element.css("display", "none");
        }

        var x = Math.floor((wf.width - ef.width) / 2) + wf.topleft.x;
        var y = Math.floor((wf.height - ef.height) / 2) + wf.topleft.y;

        var newPosition = {
            "topleft": {
                "x": x,
                "y": y
            },
            "width": ef.width,
            "height": ef.height,
            "ignoreDimensions": true
        }

        element.elementFrame(newPosition);

        return this;

    },

    createIE6LayerFix: function (isAppendable) {

        var isIE6 = (jQuery.browser.msie && parseInt(jQuery.browser.version, 10) < 7);
        if (isIE6) {

            /*
        iframe.ui-ncbi-iframe-fix
        {
            position: absolute;
            top:0px;
            left:0px;       
            height: 200px;
            z-index:1000; 
            filter:alpha(opacity=1);
        }
        */

            var elem = jQuery("<iframe src='javascript:\"\";' class='ui-ncbi-iframe-fix' marginwidth='0' marginheight='0' align='bottom' scrolling='no' frameborder='0'></iframe>");

            var dims = jQuery(this).elementFrame();
            if (dims.zIndex !== "auto" && typeof parseInt(dims.zIndex, 10) === "number") {
                dims.zIndex = dims.zIndex - 1;
            }
            elem.elementFrame(dims)

            if (isAppendable) {
                elem.appendTo(document.body);
            }

            return elem;

        }

        return null;

    }

});

/**
* hoverIntent is similar to jQuery's built-in "hover" function except that
* instead of firing the onMouseOver event immediately, hoverIntent checks
* to see if the user's mouse has slowed down (beneath the sensitivity
* threshold) before firing the onMouseOver event.
* 
* hoverIntent r5 // 2007.03.27 // jQuery 1.1.2+
* <http://cherne.net/brian/resources/jquery.hoverIntent.html>
* 
* hoverIntent is currently available for use in all personal or commercial 
* projects under both MIT and GPL licenses. This means that you can choose 
* the license that best suits your project, and use it accordingly.
* 
* // basic usage (just like .hover) receives onMouseOver and onMouseOut functions
* $("ul li").hoverIntent( showNav , hideNav );
* 
* // advanced usage receives configuration object only
* $("ul li").hoverIntent({
*	sensitivity: 7, // number = sensitivity threshold (must be 1 or higher)
*	interval: 100,   // number = milliseconds of polling interval
*	over: showNav,  // function = onMouseOver callback (required)
*	timeout: 0,   // number = milliseconds delay before onMouseOut function call
*	out: hideNav    // function = onMouseOut callback (required)
* });
* 
* @param  f  onMouseOver function || An object with configuration options
* @param  g  onMouseOut function  || Nothing (use configuration options object)
* @author    Brian Cherne <brian@cherne.net>
*/
(function($) {
	$.fn.hoverIntent = function(f,g) {
		// default configuration options
		var cfg = {
			sensitivity: 7,
			interval: 100,
			timeout: 0
		};
		// override configuration options with user supplied object
		cfg = $.extend(cfg, g ? { over: f, out: g } : f );

		// instantiate variables
		// cX, cY = current X and Y position of mouse, updated by mousemove event
		// pX, pY = previous X and Y position of mouse, set by mouseover and polling interval
		var cX, cY, pX, pY;

		// A private function for getting mouse position
		var track = function(ev) {
			cX = ev.pageX;
			cY = ev.pageY;
		};

		// A private function for comparing current and previous mouse position
		var compare = function(ev,ob) {
			ob.hoverIntent_t = clearTimeout(ob.hoverIntent_t);
			// compare mouse positions to see if they've crossed the threshold
			if ( ( Math.abs(pX-cX) + Math.abs(pY-cY) ) < cfg.sensitivity ) {
				$(ob).unbind("mousemove",track);
				// set hoverIntent state to true (so mouseOut can be called)
				ob.hoverIntent_s = 1;
				return cfg.over.apply(ob,[ev]);
			} else {
				// set previous coordinates for next time
				pX = cX; pY = cY;
				// use self-calling timeout, guarantees intervals are spaced out properly (avoids JavaScript timer bugs)
				ob.hoverIntent_t = setTimeout( function(){compare(ev, ob);} , cfg.interval );
			}
		};

		// A private function for delaying the mouseOut function
		var delay = function(ev,ob) {
			ob.hoverIntent_t = clearTimeout(ob.hoverIntent_t);
			ob.hoverIntent_s = 0;
			return cfg.out.apply(ob,[ev]);
		};

		// A private function for handling mouse 'hovering'
		var handleHover = function(e) {
			// next three lines copied from jQuery.hover, ignore children onMouseOver/onMouseOut
			var p = (e.type == "mouseover" ? e.fromElement : e.toElement) || e.relatedTarget;
			while ( p && p != this ) { try { p = p.parentNode; } catch(e) { p = this; } }
			if ( p == this ) { return false; }

			// copy objects to be passed into t (required for event object to be passed in IE)
			var ev = jQuery.extend({},e);
			var ob = this;

			// cancel hoverIntent timer if it exists
			if (ob.hoverIntent_t) { ob.hoverIntent_t = clearTimeout(ob.hoverIntent_t); }

			// else e.type == "onmouseover"
			if (e.type == "mouseover") {
				// set "previous" X and Y position based on initial entry point
				pX = ev.pageX; pY = ev.pageY;
				// update "current" X and Y position based on mousemove
				$(ob).bind("mousemove",track);
				// start polling interval (self-calling timeout) to compare mouse coordinates over time
				if (ob.hoverIntent_s != 1) { ob.hoverIntent_t = setTimeout( function(){compare(ev,ob);} , cfg.interval );}

			// else e.type == "onmouseout"
			} else {
				// unbind expensive mousemove event
				$(ob).unbind("mousemove",track);
				// if hoverIntent state is true, then call the mouseOut function after the specified delay
				if (ob.hoverIntent_s == 1) { ob.hoverIntent_t = setTimeout( function(){delay(ev,ob);} , cfg.timeout );}
			}
		};

		// bind the function to the two event listeners
		return this.mouseover(handleHover).mouseout(handleHover);
	};
})(jQuery);
/* minimized file - orginal : http://www.JSON.org/json2.js */

if(!this.JSON){this.JSON={};}
(function(){function f(n){return n<10?'0'+n:n;}
if(typeof Date.prototype.toJSON!=='function'){Date.prototype.toJSON=function(key){return isFinite(this.valueOf())?this.getUTCFullYear()+'-'+
f(this.getUTCMonth()+1)+'-'+
f(this.getUTCDate())+'T'+
f(this.getUTCHours())+':'+
f(this.getUTCMinutes())+':'+
f(this.getUTCSeconds())+'Z':null;};String.prototype.toJSON=Number.prototype.toJSON=Boolean.prototype.toJSON=function(key){return this.valueOf();};}
var cx=/[\u0000\u00ad\u0600-\u0604\u070f\u17b4\u17b5\u200c-\u200f\u2028-\u202f\u2060-\u206f\ufeff\ufff0-\uffff]/g,escapable=/[\\\"\x00-\x1f\x7f-\x9f\u00ad\u0600-\u0604\u070f\u17b4\u17b5\u200c-\u200f\u2028-\u202f\u2060-\u206f\ufeff\ufff0-\uffff]/g,gap,indent,meta={'\b':'\\b','\t':'\\t','\n':'\\n','\f':'\\f','\r':'\\r','"':'\\"','\\':'\\\\'},rep;function quote(string){escapable.lastIndex=0;return escapable.test(string)?'"'+string.replace(escapable,function(a){var c=meta[a];return typeof c==='string'?c:'\\u'+('0000'+a.charCodeAt(0).toString(16)).slice(-4);})+'"':'"'+string+'"';}
function str(key,holder){var i,k,v,length,mind=gap,partial,value=holder[key];if(value&&typeof value==='object'&&typeof value.toJSON==='function'){value=value.toJSON(key);}
if(typeof rep==='function'){value=rep.call(holder,key,value);}
switch(typeof value){case'string':return quote(value);case'number':return isFinite(value)?String(value):'null';case'boolean':case'null':return String(value);case'object':if(!value){return'null';}
gap+=indent;partial=[];if(Object.prototype.toString.apply(value)==='[object Array]'){length=value.length;for(i=0;i<length;i+=1){partial[i]=str(i,value)||'null';}
v=partial.length===0?'[]':gap?'[\n'+gap+
partial.join(',\n'+gap)+'\n'+
mind+']':'['+partial.join(',')+']';gap=mind;return v;}
if(rep&&typeof rep==='object'){length=rep.length;for(i=0;i<length;i+=1){k=rep[i];if(typeof k==='string'){v=str(k,value);if(v){partial.push(quote(k)+(gap?': ':':')+v);}}}}else{for(k in value){if(Object.hasOwnProperty.call(value,k)){v=str(k,value);if(v){partial.push(quote(k)+(gap?': ':':')+v);}}}}
v=partial.length===0?'{}':gap?'{\n'+gap+partial.join(',\n'+gap)+'\n'+
mind+'}':'{'+partial.join(',')+'}';gap=mind;return v;}}
if(typeof JSON.stringify!=='function'){JSON.stringify=function(value,replacer,space){var i;gap='';indent='';if(typeof space==='number'){for(i=0;i<space;i+=1){indent+=' ';}}else if(typeof space==='string'){indent=space;}
rep=replacer;if(replacer&&typeof replacer!=='function'&&(typeof replacer!=='object'||typeof replacer.length!=='number')){throw new Error('JSON.stringify');}
return str('',{'':value});};}
if(typeof JSON.parse!=='function'){JSON.parse=function(text,reviver){var j;function walk(holder,key){var k,v,value=holder[key];if(value&&typeof value==='object'){for(k in value){if(Object.hasOwnProperty.call(value,k)){v=walk(value,k);if(v!==undefined){value[k]=v;}else{delete value[k];}}}}
return reviver.call(holder,key,value);}
cx.lastIndex=0;if(cx.test(text)){text=text.replace(cx,function(a){return'\\u'+
('0000'+a.charCodeAt(0).toString(16)).slice(-4);});}
if(/^[\],:{}\s]*$/.test(text.replace(/\\(?:["\\\/bfnrt]|u[0-9a-fA-F]{4})/g,'@').replace(/"[^"\\\n\r]*"|true|false|null|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?/g,']').replace(/(?:^|:|,)(?:\s*\[)+/g,''))){j=eval('('+text+')');return typeof reviver==='function'?walk({'':j},''):j;}
throw new SyntaxError('JSON.parse');};}}());/*!
 * jQuery UI 1.8
 *
 * Copyright (c) 2010 AUTHORS.txt (http://jqueryui.com/about)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * http://docs.jquery.com/UI
 */
;jQuery.ui || (function($) {

//Helper functions and ui object
$.ui = {
	version: "1.8",

	// $.ui.plugin is deprecated.  Use the proxy pattern instead.
	plugin: {
		add: function(module, option, set) {
			var proto = $.ui[module].prototype;
			for(var i in set) {
				proto.plugins[i] = proto.plugins[i] || [];
				proto.plugins[i].push([option, set[i]]);
			}
		},
		call: function(instance, name, args) {
			var set = instance.plugins[name];
			if(!set || !instance.element[0].parentNode) { return; }

			for (var i = 0; i < set.length; i++) {
				if (instance.options[set[i][0]]) {
					set[i][1].apply(instance.element, args);
				}
			}
		}
	},

	contains: function(a, b) {
		return document.compareDocumentPosition
			? a.compareDocumentPosition(b) & 16
			: a !== b && a.contains(b);
	},

	hasScroll: function(el, a) {

		//If overflow is hidden, the element might have extra content, but the user wants to hide it
		if ($(el).css('overflow') == 'hidden') { return false; }

		var scroll = (a && a == 'left') ? 'scrollLeft' : 'scrollTop',
			has = false;

		if (el[scroll] > 0) { return true; }

		// TODO: determine which cases actually cause this to happen
		// if the element doesn't have the scroll set, see if it's possible to
		// set the scroll
		el[scroll] = 1;
		has = (el[scroll] > 0);
		el[scroll] = 0;
		return has;
	},

	isOverAxis: function(x, reference, size) {
		//Determines when x coordinate is over "b" element axis
		return (x > reference) && (x < (reference + size));
	},

	isOver: function(y, x, top, left, height, width) {
		//Determines when x, y coordinates is over "b" element
		return $.ui.isOverAxis(y, top, height) && $.ui.isOverAxis(x, left, width);
	},

	keyCode: {
		BACKSPACE: 8,
		CAPS_LOCK: 20,
		COMMA: 188,
		CONTROL: 17,
		DELETE: 46,
		DOWN: 40,
		END: 35,
		ENTER: 13,
		ESCAPE: 27,
		HOME: 36,
		INSERT: 45,
		LEFT: 37,
		NUMPAD_ADD: 107,
		NUMPAD_DECIMAL: 110,
		NUMPAD_DIVIDE: 111,
		NUMPAD_ENTER: 108,
		NUMPAD_MULTIPLY: 106,
		NUMPAD_SUBTRACT: 109,
		PAGE_DOWN: 34,
		PAGE_UP: 33,
		PERIOD: 190,
		RIGHT: 39,
		SHIFT: 16,
		SPACE: 32,
		TAB: 9,
		UP: 38
	}
};

//jQuery plugins
$.fn.extend({
	_focus: $.fn.focus,
	focus: function(delay, fn) {
		return typeof delay === 'number'
			? this.each(function() {
				var elem = this;
				setTimeout(function() {
					$(elem).focus();
					(fn && fn.call(elem));
				}, delay);
			})
			: this._focus.apply(this, arguments);
	},
	
	enableSelection: function() {
		return this
			.attr('unselectable', 'off')
			.css('MozUserSelect', '')
			.unbind('selectstart.ui');
	},

	disableSelection: function() {
		return this
			.attr('unselectable', 'on')
			.css('MozUserSelect', 'none')
			.bind('selectstart.ui', function() { return false; });
	},

	scrollParent: function() {
		var scrollParent;
		if(($.browser.msie && (/(static|relative)/).test(this.css('position'))) || (/absolute/).test(this.css('position'))) {
			scrollParent = this.parents().filter(function() {
				return (/(relative|absolute|fixed)/).test($.curCSS(this,'position',1)) && (/(auto|scroll)/).test($.curCSS(this,'overflow',1)+$.curCSS(this,'overflow-y',1)+$.curCSS(this,'overflow-x',1));
			}).eq(0);
		} else {
			scrollParent = this.parents().filter(function() {
				return (/(auto|scroll)/).test($.curCSS(this,'overflow',1)+$.curCSS(this,'overflow-y',1)+$.curCSS(this,'overflow-x',1));
			}).eq(0);
		}

		return (/fixed/).test(this.css('position')) || !scrollParent.length ? $(document) : scrollParent;
	},

	zIndex: function(zIndex) {
		if (zIndex !== undefined) {
			return this.css('zIndex', zIndex);
		}
		
		if (this.length) {
			var elem = $(this[0]), position, value;
			while (elem.length && elem[0] !== document) {
				// Ignore z-index if position is set to a value where z-index is ignored by the browser
				// This makes behavior of this function consistent across browsers
				// WebKit always returns auto if the element is positioned
				position = elem.css('position');
				if (position == 'absolute' || position == 'relative' || position == 'fixed')
				{
					// IE returns 0 when zIndex is not specified
					// other browsers return a string
					// we ignore the case of nested elements with an explicit value of 0
					// <div style="z-index: -10;"><div style="z-index: 0;"></div></div>
					value = parseInt(elem.css('zIndex'));
					if (!isNaN(value) && value != 0) {
						return value;
					}
				}
				elem = elem.parent();
			}
		}

		return 0;
	}
});


//Additional selectors
$.extend($.expr[':'], {
	data: function(elem, i, match) {
		return !!$.data(elem, match[3]);
	},

	focusable: function(element) {
		var nodeName = element.nodeName.toLowerCase(),
			tabIndex = $.attr(element, 'tabindex');
		return (/input|select|textarea|button|object/.test(nodeName)
			? !element.disabled
			: 'a' == nodeName || 'area' == nodeName
				? element.href || !isNaN(tabIndex)
				: !isNaN(tabIndex))
			// the element and all of its ancestors must be visible
			// the browser may report that the area is hidden
			&& !$(element)['area' == nodeName ? 'parents' : 'closest'](':hidden').length;
	},

	tabbable: function(element) {
		var tabIndex = $.attr(element, 'tabindex');
		return (isNaN(tabIndex) || tabIndex >= 0) && $(element).is(':focusable');
	}
});

})(jQuery);
/*!
 * jQuery UI Widget 1.8
 *
 * Copyright (c) 2010 AUTHORS.txt (http://jqueryui.com/about)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * http://docs.jquery.com/UI/Widget
 */
(function( $ ) {

var _remove = $.fn.remove;

$.fn.remove = function( selector, keepData ) {
	return this.each(function() {
		if ( !keepData ) {
			if ( !selector || $.filter( selector, [ this ] ).length ) {
				$( "*", this ).add( this ).each(function() {
					$( this ).triggerHandler( "remove" );
				});
			}
		}
		return _remove.call( $(this), selector, keepData );
	});
};

$.widget = function( name, base, prototype ) {
	var namespace = name.split( "." )[ 0 ],
		fullName;
	name = name.split( "." )[ 1 ];
	fullName = namespace + "-" + name;

	if ( !prototype ) {
		prototype = base;
		base = $.Widget;
	}

	// create selector for plugin
	$.expr[ ":" ][ fullName ] = function( elem ) {
		return !!$.data( elem, name );
	};

	$[ namespace ] = $[ namespace ] || {};
	$[ namespace ][ name ] = function( options, element ) {
		// allow instantiation without initializing for simple inheritance
		if ( arguments.length ) {
			this._createWidget( options, element );
		}
	};

	var basePrototype = new base();
	// we need to make the options hash a property directly on the new instance
	// otherwise we'll modify the options hash on the prototype that we're
	// inheriting from
//	$.each( basePrototype, function( key, val ) {
//		if ( $.isPlainObject(val) ) {
//			basePrototype[ key ] = $.extend( {}, val );
//		}
//	});
	basePrototype.options = $.extend( {}, basePrototype.options );
	$[ namespace ][ name ].prototype = $.extend( true, basePrototype, {
		namespace: namespace,
		widgetName: name,
		widgetEventPrefix: $[ namespace ][ name ].prototype.widgetEventPrefix || name,
		widgetBaseClass: fullName
	}, prototype );

	$.widget.bridge( name, $[ namespace ][ name ] );
};

$.widget.bridge = function( name, object ) {
	$.fn[ name ] = function( options ) {
		var isMethodCall = typeof options === "string",
			args = Array.prototype.slice.call( arguments, 1 ),
			returnValue = this;

		// allow multiple hashes to be passed on init
		options = !isMethodCall && args.length ?
			$.extend.apply( null, [ true, options ].concat(args) ) :
			options;

		// prevent calls to internal methods
		if ( isMethodCall && options.substring( 0, 1 ) === "_" ) {
			return returnValue;
		}

		if ( isMethodCall ) {
			this.each(function() {
				var instance = $.data( this, name ),
					methodValue = instance && $.isFunction( instance[options] ) ?
						instance[ options ].apply( instance, args ) :
						instance;
				if ( methodValue !== instance && methodValue !== undefined ) {
					returnValue = methodValue;
					return false;
				}
			});
		} else {
			this.each(function() {
				var instance = $.data( this, name );
				if ( instance ) {
					if ( options ) {
						instance.option( options );
					}
					instance._init();
				} else {
					$.data( this, name, new object( options, this ) );
				}
			});
		}

		return returnValue;
	};
};

$.Widget = function( options, element ) {
	// allow instantiation without initializing for simple inheritance
	if ( arguments.length ) {
		this._createWidget( options, element );
	}
};

$.Widget.prototype = {
	widgetName: "widget",
	widgetEventPrefix: "",
	options: {
		disabled: false
	},
	_createWidget: function( options, element ) {
		// $.widget.bridge stores the plugin instance, but we do it anyway
		// so that it's stored even before the _create function runs
		this.element = $( element ).data( this.widgetName, this );
		this.options = $.extend( true, {},
			this.options,
			$.metadata && $.metadata.get( element )[ this.widgetName ],
			options );

		var self = this;
		this.element.bind( "remove." + this.widgetName, function() {
			self.destroy();
		});

		this._create();
		this._init();
	},
	_create: function() {},
	_init: function() {},

	destroy: function() {
		this.element
			.unbind( "." + this.widgetName )
			.removeData( this.widgetName );
		this.widget()
			.unbind( "." + this.widgetName )
			.removeAttr( "aria-disabled" )
			.removeClass(
				this.widgetBaseClass + "-disabled " +
				this.namespace + "-state-disabled" );
	},

	widget: function() {
		return this.element;
	},

	option: function( key, value ) {
		var options = key,
			self = this;

		if ( arguments.length === 0 ) {
			// don't return a reference to the internal hash
			return $.extend( {}, self.options );
		}

		if  (typeof key === "string" ) {
			if ( value === undefined ) {
				return this.options[ key ];
			}
			options = {};
			options[ key ] = value;
		}

		$.each( options, function( key, value ) {
			self._setOption( key, value );
		});

		return self;
	},
	_setOption: function( key, value ) {
		this.options[ key ] = value;

		if ( key === "disabled" ) {
			this.widget()
				[ value ? "addClass" : "removeClass"](
					this.widgetBaseClass + "-disabled" + " " +
					this.namespace + "-state-disabled" )
				.attr( "aria-disabled", value );
		}

		return this;
	},

	enable: function() {
		return this._setOption( "disabled", false );
	},
	disable: function() {
		return this._setOption( "disabled", true );
	},

	_trigger: function( type, event, data ) {
		var callback = this.options[ type ];

		event = $.Event( event );
		event.type = ( type === this.widgetEventPrefix ?
			type :
			this.widgetEventPrefix + type ).toLowerCase();
		data = data || {};

		// copy original event properties over to the new event
		// this would happen if we could call $.event.fix instead of $.Event
		// but we don't have a way to force an event to be fixed multiple times
		if ( event.originalEvent ) {
			for ( var i = $.event.props.length, prop; i; ) {
				prop = $.event.props[ --i ];
				event[ prop ] = event.originalEvent[ prop ];
			}
		}

		this.element.trigger( event, data );

		return !( $.isFunction(callback) &&
			callback.call( this.element[0], event, data ) === false ||
			event.isDefaultPrevented() );
	}
};

})( jQuery );
/*!
 * jQuery UI Mouse 1.8
 *
 * Copyright (c) 2010 AUTHORS.txt (http://jqueryui.com/about)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * http://docs.jquery.com/UI/Mouse
 *
 * Depends:
 *	jquery.ui.widget.js
 */
(function($) {

$.widget("ui.mouse", {
	options: {
		cancel: ':input,option',
		distance: 1,
		delay: 0
	},
	_mouseInit: function() {
		var self = this;

		this.element
			.bind('mousedown.'+this.widgetName, function(event) {
				return self._mouseDown(event);
			})
			.bind('click.'+this.widgetName, function(event) {
				if(self._preventClickEvent) {
					self._preventClickEvent = false;
					event.stopImmediatePropagation();
					return false;
				}
			});

		this.started = false;
	},

	// TODO: make sure destroying one instance of mouse doesn't mess with
	// other instances of mouse
	_mouseDestroy: function() {
		this.element.unbind('.'+this.widgetName);
	},

	_mouseDown: function(event) {
		// don't let more than one widget handle mouseStart
		// TODO: figure out why we have to use originalEvent
		event.originalEvent = event.originalEvent || {};
		if (event.originalEvent.mouseHandled) { return; }

		// we may have missed mouseup (out of window)
		(this._mouseStarted && this._mouseUp(event));

		this._mouseDownEvent = event;

		var self = this,
			btnIsLeft = (event.which == 1),
			elIsCancel = (typeof this.options.cancel == "string" ? $(event.target).parents().add(event.target).filter(this.options.cancel).length : false);
		if (!btnIsLeft || elIsCancel || !this._mouseCapture(event)) {
			return true;
		}

		this.mouseDelayMet = !this.options.delay;
		if (!this.mouseDelayMet) {
			this._mouseDelayTimer = setTimeout(function() {
				self.mouseDelayMet = true;
			}, this.options.delay);
		}

		if (this._mouseDistanceMet(event) && this._mouseDelayMet(event)) {
			this._mouseStarted = (this._mouseStart(event) !== false);
			if (!this._mouseStarted) {
				event.preventDefault();
				return true;
			}
		}

		// these delegates are required to keep context
		this._mouseMoveDelegate = function(event) {
			return self._mouseMove(event);
		};
		this._mouseUpDelegate = function(event) {
			return self._mouseUp(event);
		};
		$(document)
			.bind('mousemove.'+this.widgetName, this._mouseMoveDelegate)
			.bind('mouseup.'+this.widgetName, this._mouseUpDelegate);

		// preventDefault() is used to prevent the selection of text here -
		// however, in Safari, this causes select boxes not to be selectable
		// anymore, so this fix is needed
		($.browser.safari || event.preventDefault());

		event.originalEvent.mouseHandled = true;
		return true;
	},

	_mouseMove: function(event) {
		// IE mouseup check - mouseup happened when mouse was out of window
		if ($.browser.msie && !event.button) {
			return this._mouseUp(event);
		}

		if (this._mouseStarted) {
			this._mouseDrag(event);
			return event.preventDefault();
		}

		if (this._mouseDistanceMet(event) && this._mouseDelayMet(event)) {
			this._mouseStarted =
				(this._mouseStart(this._mouseDownEvent, event) !== false);
			(this._mouseStarted ? this._mouseDrag(event) : this._mouseUp(event));
		}

		return !this._mouseStarted;
	},

	_mouseUp: function(event) {
		$(document)
			.unbind('mousemove.'+this.widgetName, this._mouseMoveDelegate)
			.unbind('mouseup.'+this.widgetName, this._mouseUpDelegate);

		if (this._mouseStarted) {
			this._mouseStarted = false;
			this._preventClickEvent = (event.target == this._mouseDownEvent.target);
			this._mouseStop(event);
		}

		return false;
	},

	_mouseDistanceMet: function(event) {
		return (Math.max(
				Math.abs(this._mouseDownEvent.pageX - event.pageX),
				Math.abs(this._mouseDownEvent.pageY - event.pageY)
			) >= this.options.distance
		);
	},

	_mouseDelayMet: function(event) {
		return this.mouseDelayMet;
	},

	// These are placeholder methods, to be overriden by extending plugin
	_mouseStart: function(event) {},
	_mouseDrag: function(event) {},
	_mouseStop: function(event) {},
	_mouseCapture: function(event) { return true; }
});

})(jQuery);
/*
 * jQuery UI Position 1.8
 *
 * Copyright (c) 2010 AUTHORS.txt (http://jqueryui.com/about)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * http://docs.jquery.com/UI/Position
 */
(function( $ ) {

$.ui = $.ui || {};

var horizontalPositions = /left|center|right/,
	horizontalDefault = "center",
	verticalPositions = /top|center|bottom/,
	verticalDefault = "center",
	_position = $.fn.position,
	_offset = $.fn.offset;

$.fn.position = function( options ) {
	if ( !options || !options.of ) {
		return _position.apply( this, arguments );
	}

	// make a copy, we don't want to modify arguments
	options = $.extend( {}, options );

	var target = $( options.of ),
		collision = ( options.collision || "flip" ).split( " " ),
		offset = options.offset ? options.offset.split( " " ) : [ 0, 0 ],
		targetWidth,
		targetHeight,
		basePosition;

	if ( options.of.nodeType === 9 ) {
		targetWidth = target.width();
		targetHeight = target.height();
		basePosition = { top: 0, left: 0 };
	} else if ( options.of.scrollTo && options.of.document ) {
		targetWidth = target.width();
		targetHeight = target.height();
		basePosition = { top: target.scrollTop(), left: target.scrollLeft() };
	} else if ( options.of.preventDefault ) {
		// force left top to allow flipping
		options.at = "left top";
		targetWidth = targetHeight = 0;
		basePosition = { top: options.of.pageY, left: options.of.pageX };
	} else {
		targetWidth = target.outerWidth();
		targetHeight = target.outerHeight();
		basePosition = target.offset();
	}

	// force my and at to have valid horizontal and veritcal positions
	// if a value is missing or invalid, it will be converted to center 
	$.each( [ "my", "at" ], function() {
		var pos = ( options[this] || "" ).split( " " );
		if ( pos.length === 1) {
			pos = horizontalPositions.test( pos[0] ) ?
				pos.concat( [verticalDefault] ) :
				verticalPositions.test( pos[0] ) ?
					[ horizontalDefault ].concat( pos ) :
					[ horizontalDefault, verticalDefault ];
		}
		pos[ 0 ] = horizontalPositions.test( pos[0] ) ? pos[ 0 ] : horizontalDefault;
		pos[ 1 ] = verticalPositions.test( pos[1] ) ? pos[ 1 ] : verticalDefault;
		options[ this ] = pos;
	});

	// normalize collision option
	if ( collision.length === 1 ) {
		collision[ 1 ] = collision[ 0 ];
	}

	// normalize offset option
	offset[ 0 ] = parseInt( offset[0], 10 ) || 0;
	if ( offset.length === 1 ) {
		offset[ 1 ] = offset[ 0 ];
	}
	offset[ 1 ] = parseInt( offset[1], 10 ) || 0;

	if ( options.at[0] === "right" ) {
		basePosition.left += targetWidth;
	} else if (options.at[0] === horizontalDefault ) {
		basePosition.left += targetWidth / 2;
	}

	if ( options.at[1] === "bottom" ) {
		basePosition.top += targetHeight;
	} else if ( options.at[1] === verticalDefault ) {
		basePosition.top += targetHeight / 2;
	}

	basePosition.left += offset[ 0 ];
	basePosition.top += offset[ 1 ];

	return this.each(function() {
		var elem = $( this ),
			elemWidth = elem.outerWidth(),
			elemHeight = elem.outerHeight(),
			position = $.extend( {}, basePosition );

		if ( options.my[0] === "right" ) {
			position.left -= elemWidth;
		} else if ( options.my[0] === horizontalDefault ) {
			position.left -= elemWidth / 2;
		}

		if ( options.my[1] === "bottom" ) {
			position.top -= elemHeight;
		} else if ( options.my[1] === verticalDefault ) {
			position.top -= elemHeight / 2;
		}

		$.each( [ "left", "top" ], function( i, dir ) {
			if ( $.ui.position[ collision[i] ] ) {
				$.ui.position[ collision[i] ][ dir ]( position, {
					targetWidth: targetWidth,
					targetHeight: targetHeight,
					elemWidth: elemWidth,
					elemHeight: elemHeight,
					offset: offset,
					my: options.my,
					at: options.at
				});
			}
		});

		if ( $.fn.bgiframe ) {
			elem.bgiframe();
		}
		elem.offset( $.extend( position, { using: options.using } ) );
	});
};

$.ui.position = {
	fit: {
		left: function( position, data ) {
			var win = $( window ),
				over = position.left + data.elemWidth - win.width() - win.scrollLeft();
			position.left = over > 0 ? position.left - over : Math.max( 0, position.left );
		},
		top: function( position, data ) {
			var win = $( window ),
				over = position.top + data.elemHeight - win.height() - win.scrollTop();
			position.top = over > 0 ? position.top - over : Math.max( 0, position.top );
		}
	},

	flip: {
		left: function( position, data ) {
			if ( data.at[0] === "center" ) {
				return;
			}
			var win = $( window ),
				over = position.left + data.elemWidth - win.width() - win.scrollLeft(),
				myOffset = data.my[ 0 ] === "left" ?
					-data.elemWidth :
					data.my[ 0 ] === "right" ?
						data.elemWidth :
						0,
				offset = -2 * data.offset[ 0 ];
			position.left += position.left < 0 ?
				myOffset + data.targetWidth + offset :
				over > 0 ?
					myOffset - data.targetWidth + offset :
					0;
		},
		top: function( position, data ) {
			if ( data.at[1] === "center" ) {
				return;
			}
			var win = $( window ),
				over = position.top + data.elemHeight - win.height() - win.scrollTop(),
				myOffset = data.my[ 1 ] === "top" ?
					-data.elemHeight :
					data.my[ 1 ] === "bottom" ?
						data.elemHeight :
						0,
				atOffset = data.at[ 1 ] === "top" ?
					data.targetHeight :
					-data.targetHeight,
				offset = -2 * data.offset[ 1 ];
			position.top += position.top < 0 ?
				myOffset + data.targetHeight + offset :
				over > 0 ?
					myOffset + atOffset + offset :
					0;
		}
	}
};

// offset setter from jQuery 1.4
if ( !$.offset.setOffset ) {
	$.offset.setOffset = function( elem, options ) {
		// set position first, in-case top/left are set even on static elem
		if ( /static/.test( $.curCSS( elem, "position" ) ) ) {
			elem.style.position = "relative";
		}
		var curElem   = $( elem ),
			curOffset = curElem.offset(),
			curTop    = parseInt( $.curCSS( elem, "top",  true ), 10 ) || 0,
			curLeft   = parseInt( $.curCSS( elem, "left", true ), 10)  || 0,
			props     = {
				top:  (options.top  - curOffset.top)  + curTop,
				left: (options.left - curOffset.left) + curLeft
			};
		
		if ( 'using' in options ) {
			options.using.call( elem, props );
		} else {
			curElem.css( props );
		}
	};

	$.fn.offset = function( options ) {
		var elem = this[ 0 ];
		if ( !elem || !elem.ownerDocument ) { return null; }
		if ( options ) { 
			return this.each(function() {
				$.offset.setOffset( this, options );
			});
		}
		return _offset.call( this );
	};
}

}( jQuery ));
    // jig-util.js
    // provides utility methods for all ui widgets intended for jig framework
    
    if ( typeof jQuery.ui.jig === 'undefined') {
        jQuery.ui.jig = {};
    } 
    
    jQuery.ui.jig._decodeJSON = function(contentAttr, nameAttr) {
    // decodes a string to a JSON object
        try {
            return eval('({' + contentAttr + '})');
        } catch(e) {
            if ( jQuery.ui.jig._isConsole('info')  ) {
                console.error("Error parsing parameter metatag: '" + nameAttr + "'. "  + e.message);
            }
        }
    };
    
    jQuery.ui.jig._setLocalConfig = function(element, options) {
        // gets config string from a $ element, decodes to JSON, extends options, returns options
        var configAttr = element.attr('config');
        if (configAttr) {
            var localConfig = jQuery.ui.jig._decodeJSON(configAttr);
            jQuery.extend(options, localConfig);
        }
        return options;
    };
    
    jQuery.ui.jig._generateId = function(strWidgetName, strModifier) {
        // generates a "unique id" for a jig widget. Can be used to generated class names too of course.
        // returns string
        // strWidgetName is the name of the widget, for example "ncbimenu".
        // for example,
        // $.ui.jig._generateId("ncbimenu"); // returns 'ui-ncbimenu-1'
        // use optional strModifier to further refine id. For example,
        // $.ui.jig._generateId("ncbimenu", "foo"); // returns 'ui-ncbimenu-foo-1'
        var strModifier = (typeof strModifier !== 'undefined') ? '-' + strModifier : '';
        arguments.callee.i++;
        var strGeneratedId = 'ui-' + strWidgetName + strModifier + '-' + arguments.callee.i;
    
        // check to make sure this id isn't already on page, if it is up the index #
        while (document.getElementById(strGeneratedId)){
            arguments.callee.i++;
            strGeneratedId = 'ui-' + strWidgetName + strModifier + arguments.callee.i;
        }
        return strGeneratedId;
        
    }
    jQuery.ui.jig._generateId.i = 0;

    /**
    * Returns whether a console object and one of its methods
    * is defined.
    *
    * @param method String. The method to check for.
    *
    * @return boolean Whether console or a method is defined.
    */
    jQuery.ui.jig._isConsole = function(method) {
        var consoleDefined = typeof window.console !== 'undefined' || false;
        if ( consoleDefined ) {
            var methodDefined = typeof window.console[ method ] !== 'undefined' || false;
        }
        if ( consoleDefined && methodDefined ) {
            return true;
        } else {
            return false;
        } 
    };
	
    jQuery.ui.jig._getFncFromStr = function( fnc ){	
        var newFunc = null;
        var typeOfFnc = typeof fnc;
        if(typeOfFnc === "function"){
            newFunc = fnc;
        }
        else if(typeOfFnc === "string" /*&& fnc.toLowerCase().indexOf("fnc:")===0*/ ){	
            var parts = fnc.replace(/^[^:]*:\s*/, "").replace(/\s*$/,"").split(".");            
            newFunc = window[ parts[0] ];
            if(newFunc){
                for(var i=1;i<parts.length;i++){               
                    newFunc = newFunc[ parts[i] ];
                    if(!newFunc){
                        newFunc = null;
                        break;
                    }
                }
            }
            else{
                newFunc = null;
            }
        }
        return newFunc;		
    };

    jQuery.ui.jig.registerPageHeightWatcher = function(){
        var body = jQuery(document.body);
        if( body.data("hasHeightWatcher") ){
            return;
        }
        body.data("_lastHeight", null);
        body.data("hasHeightWatcher", true);     
        body.data("heightWatcher", window.setInterval( function(){ jQuery.ui.jig.watchPageHeight(); }, 250) );
    };

    jQuery.ui.jig.watchPageHeight = function(){
        var body = jQuery(document.body);
        var currentHeight = body.height();
        var lastHeight = body.data("_lastHeight");
        if(lastHeight!==currentHeight){
            if(lastHeight){
                body.trigger("ncbijigpageheightchanged");
            }
            body.data("_lastHeight", currentHeight);
        }
    };
    /********************
     * @file jig.decl.js:

     * jig framework code that reads meta tags for declarative rendering of jQuery UI plugins.
     * See: http://iwebdev.ncbi.nlm.nih.gov/core/jig/ for docs.
     * jig.decl is not used alone. It is concatenated into jig.js. Above this code should be
     * jQuery, ui.core, and jig utility functions

    ********************/

    /***
    * NAMESPACE DECLARATIONS
    ***/

    // Shortcut to UI object, which is gotten from ui.core.js which is above
    var ui = $.ui;

    // define widgets ns, if it doesn't already exist
    if (typeof ui.jig === 'undefined'  ) {
        ui.jig = {};
    }

    var jig = ui.jig;

    // Allow jig debug mode for clients without a console. Cancel calls to console
    // and output to top of page IF jig_debug=true is in query string and console is undefined
    if ( document.location.search.search(/jig_debug=true/i) !== -1 && typeof window.console === 'undefined') {
        $( function() {
            $('body').prepend('<div style="border: 2px solid #ccc; height: 150px; overflow:auto;"><h2 style="color:red">jig Console</h2><ul id="jig-cons"></ul></div>');
            console = {};
            var methods = ['log', 'info', 'warn', 'group', 'groupEnd'];
            var f = function() {
                $('#jig-cons').append('<li>' + arguments[0] + '</li>');
            };
            for (var i = 0; i < methods.length; i++) {
                var mStr = methods[i];
                var mObj = console[mStr];
                if ( typeof mObj === 'undefined') {
                    console[mStr] = f;
                }
            }
        });
    }

    // check for quirks mode. Warn if true, since some widgets won't behave properly
    if ( document.compatMode === 'BackCompat' && jig._isConsole('warn') ) {
        console.warn('Document is in quirks mode. jig widgets only work properly when in standards mode. Please add or correct your DOCTYPE definition in your page.');
    }


    /***
     * INITIALIZE PRIVATE AND PUBLIC JIG PROPERTIES
    ***/

    // Tells whether page was scanned for jig widget markup
    // this is set at the end of scanjig() function
    jig.scanned = false;
jig.version = '1.7';

    // 'min.js' or '.js'
    jig._jsExt = null;
    jig._cssExt = null;



    function JigWidget (options) {
      this.selector         = '';
      this.name             = '';
      this.onPage           = false;
      this.dependsOn        = [];
      this.interactions     = [];
      this.overrideDefaults = {};
      this.addCss = function() {
        document.write('<link type="text/css" rel="stylesheet" href="' + baseURL + 'css/jquery.ui.' + this.name + cssExt +'"/>');
      };
      this.addJs = function() {
        document.write('<script type="text/javascript" src="' + baseURL + 'js/jquery.ui.' + this.name + jsExt + '"></script>');
      };

      this.addDependent = function () {
        for (var i = 0; i < this.dependsOn.length; i++) {
          widgDict[this.dependsOn[i]].addToPage();
        }
      };
      this.addInteractions = function () {
        for (var i = 0; i < this.interactions.length; i++) {
          document.write('<script type="text/javascript" src="' + baseURL + 'js/jquery.ui.' + this.interactions[i] + jsExt + '"></script>');
        }
      };
      this.addToPage = function () {
        if (!this.onPage) {
          this.addInteractions();
          this.addDependent();
          this.addJs();
          this.addCss();
          this.onPage = true;
        }
      };
      $.extend(this,options);
    }

    /***
    * WIDGET AND UI INTERACTIONS DICTIONARY AND ARRAY
    * This dictionary stores each widget's selector, its other widget dependencies, and interaction dependencies
    * You can get the selector from this dictionary from jQuery.ui.widget.getSelector();

    It is an informal private method. Don't access it directly. I've made it accessible for testing purposes
    ***/

    jig._widgDict = {
        'ncbiaccordion': new JigWidget ({
            name: 'ncbiaccordion',
            selector: 'div.jig-ncbiaccordion, div.jig-accordion',
            overrideDefaults: {
                autoHeight: false,
                header: ':header'
            }
        })/*,
        'button': new JigWidget ({
        	name: 'button',
        	selector: 'input[type="button"].jig-button, input[type="checkbox"].jig-button, button.jig-button, a.jig-button'
        }),
        'buttonset': new JigWidget ({
        	name: 'buttonset',
        	selector: 'div.jig-buttonset'
        }),
        'ncbichart_pie': new JigWidget({
        	name: 'ncbichart_pie',
        	selector: 'table.jig-ncbichart-pie'
        }),
        'ncbichart_bar': new JigWidget({
        	name: 'ncbichart_bar',
        	selector: 'table.jig-ncbichart-bar'
        }),
        'ncbisortable':  new JigWidget ({
            name: 'ncbisortable',
            selector: 'ul.jig-ncbisortable, ol.jig-ncbisortable'
        })*/,
        'ncbidatepicker':  new JigWidget ({
            name: 'ncbidatepicker',
            selector: 'input.jig-ncbidatepicker'
        }),
        'ncbielastictextarea': new JigWidget ({
            name: 'ncbielastictextarea',
            selector: 'textarea.jig-ncbielastictextarea'
        }),
        'ncbidialog': new JigWidget({
            name: 'ncbidialog',
            selector: 'a.jig-ncbidialog, button.jig-ncbidialog, input.jig-ncbidialog, textarea.jig-ncbidialog',
            interactions: ['draggable', 'resizable'],
            overrideDefaults: {
                autoOpen: false
            }
        }),
        'ncbiautocomplete': new JigWidget({
            name: 'ncbiautocomplete',
            selector: 'input[type="text"].jig-ncbiautocomplete'
        }),
        'ncbigrid': new JigWidget({
            name: 'ncbigrid',
            selector: 'div.jig-ncbigrid table,div.jig-ncbigrid-scroll table,table.jig-ncbigrid,table.jig-ncbigrid-scroll'
        }),
        'ncbiservergrid': new JigWidget({
            name: 'ncbiservergrid',
            selector: 'div.jig-ncbiservergrid table,div.jig-ncbiservergrid-scroll table,table.jig-ncbiservergrid,table.jig-ncbiservergrid-scroll',
            dependsOn: ['ncbigrid']
        }),  
        'ncbilinkedselects': new JigWidget ({
        	name: 'ncbilinkedselects',
        	selector: 'select.jig-linkedselects'
        }),      
        'ncbimenu': new JigWidget({
            name: 'ncbimenu',
            selector: 'ul.jig-ncbimenu'
        }),
        'ncbipopper': new JigWidget({
            name: 'ncbipopper',
            selector: '.jig-ncbipopper'
        }),
        'ncbislideshow': new JigWidget({
            name: 'ncbislideshow',
            selector: 'div.jig-ncbislideshow'
        }),
        'ncbitabpopper': new JigWidget({
            name: 'ncbitabpopper',
            selector: '.jig-ncbitabpopper'
        }),
        'ncbitoggler':new JigWidget({
            name: 'ncbitoggler',
            selector: 'a.jig-ncbitoggler,a.jig-ncbitoggler-open,a.ui-ncbitoggler,a.ui-ncbitoggler-open'
        }),
        'ncbitree_base': new JigWidget({
            name: 'ncbitree_base',
            selector: 'ul.jig-ncbitree_base'
        }),
        'ncbitree': new JigWidget ({
            name: 'ncbitree',
            selector: 'ul.jig-ncbitree',
            dependsOn: ['ncbitree_base']
        }),
        'ncbitabs': new JigWidget({
            name: 'ncbitabs',
            selector: 'div.jig-ncbitabs, div.jig-tabs, '
        }),
        'ncbilinksmenu': new JigWidget({
            name: 'ncbilinksmenu',
            selector: '.jig-ncbilinksmenu',
            dependsOn: ['ncbipopper']
        })
    };

    var widgDict = jig._widgDict;

    // extract out just widget names as array
    var jigWidgs = [];

    for (var i in widgDict) {
        jigWidgs.push(i);
    }

    // these are the UI interactions that get loaded into page if a widget needs it and its config is set to true
    var interactions = ['draggable', 'resizable', 'sortable'];

    jig.scanjig = function(contextNode, configParam) {
        // "Class method" to scan the page for jig widget markup
        // this fnc is called when the page loads and can be called after page load
        // either from overidden jQuery append(), html(), and prepend(),
        // Or it can be called directly on the page (usually in an AJAX callback method
        // the configs passed in if calling scanjig directly, always override param (global) meta tags

        // helper functions for overriding widget _inits
        var cfs;

        // Will hold the widget strings we will loop thru to call. These can be either
        // the default names in jig, or an array passed in direct to scanjig() in the
        // config parameter configParam
        var widgetNames;

        // if called without a second param
        if ( !configParam ) {
            cfs = {};
            // use the default names in jig
            widgetNames = jigWidgs;
        }
        else {
            // otherwise, use the names passed in via configParam, or if that is null,
            // use the default names from the widget dictionary
            // names should be an array, if non specified, its a reference to all jig widgets
            widgetNames = configParam.widgets || jigWidgs;

            // use the configs passed in via parameter, or nothing
            cfs = configParam.configs || {};
        }


        // now loop thru widgets we need to call
        for (var i = 0; i < widgetNames.length; i++) {

            // use the context node passed in via parameter, or body
            contextNode = contextNode || $(document.body);
            var widgetName = widgetNames[i];

            // If user is calling jig.scan() after page load manually, make sure
            // we only scan for widgets specified in the meta tag. TO DO: do test case for this.
            if (jigMeta && $.inArray(widgetName, widgNames) === -1) {
                continue;
            }

            // try to get the widget function in ui namespace.
            // only warn about wrong name if the document hasn't already been
            // scanned, otherwise we are probably scanning after the page has
            // loaded and we don't want to tell the page author
            // about all the widgets if they call scan jig in default mode
            // TO DO: add test case for this

            var oWidget = ui[widgetName];
            if ( typeof oWidget === 'undefined' && !jig.scanned ) {

                if ( jig._isConsole( 'warn' ) ) {
                    console.warn('jig: widget "' + widgetName + '" does not exist. Check documentation (http://iwebdev/core/jig/ for correct name');
                }

                // if we can't get the widget, try the next widget
                continue;
            }

            // Override each widget's init method, so we can get local config from framework code,
            // not widget code. We only should do this once, if the document has not been scanned
            // yet and if the widget has a property called _customLocalConfig.
            // Whether the doc has been scanned is stored as a private property of jig.

            if ( !jig.scanned && typeof oWidget._customLocalConfig === 'undefined') {

                var oldCreate = oWidget.prototype._create;

                // because of the closure, we have to call a function, passing in the explicit widget
                // function and oldInit method. This function is called imediately after it is defined,
                // and all it does is set the widget's _init function to the new one
                (function(oWidget, oldCreate, widgetName) {
                    oWidget.prototype._create =  function() {
                        // get the local config from the root node's config attribute and extend this.options
                        var that = this;
                        var configAttr = this.element.attr('config') || this.element.attr('data-jigconfig');
                        if (configAttr) {
                            var localConfig = ui.jig._decodeJSON(configAttr);
                            $.extend(this.options, localConfig);
                        }

                        // selectively override ui built widgets' _inits
                        if ( widgetName === 'droppable' ) {
                            var dragEls = jQuery( this.options.dragEls);

                            // get callback
                            var cbName = this.options.dropCallback;
                            var cb = widgDict[ 'droppable' ].callbacks[ cbName ];

                            if ( typeof cb !== 'undefined' ) {
                                this._setData('drop', function(event, ui) {
                                    console.log( event);
                                    console.log( ui );
                                    cb();
                                });
                            }

                        }
                        if (widgetName === 'tabs') {

                            var el = this.element;
                            this.element.children('ul').siblings('div').attr('role', 'tabpanel').end().
                                    attr('role', 'tablist').children('li').
                                        attr('role', 'presentation').find('a').
                                            attr('role', 'tab').each( function(i) { // here set all tabs but first out of taborder
                                                if ( i > 0 ) {
                                                    this.tabIndex = -1;
                                                } else {
                                                    this.tabIndex = 0;
                                                }
                                            });
                        }

                        // call in interactions scripts into a page if necessary
                        // TO DO: the test here is magic, it should be a function
                        var widgetInteractions = widgDict[ widgetName ].interactions || null;

                        if ( widgetInteractions && widgetInteractions.length > 0 ) {
                          // add interactions only if option is set in widget and the script is not already in page
                          var inc = '';
                          if ( this.options.draggable && !ui.draggable ) {
                            inc += '<script type="text/javascript" src="/core/ui/' + ui.version + '/development-bundle/ui/jquery.ui.draggable.js"></script>';
                          }
                          if (this.options.resizable && !ui.resizable ) {
                            inc += '<script type="text/javascript" src="/core/ui/' + ui.version + '/development-bundle/ui/jquery.ui.resizable.js"></script>';
                          }
                          $('head').append(inc);
                        }

                        // now do everything in the old init
                        oldCreate.apply(this, []);
                    };
                    if (widgetName === 'tabs') {
                        var oldTabify = oWidget.prototype._tabify;
                        oWidget.prototype._tabify = function(init) {
                            var that = this;
                            oldTabify.apply( this, [init] );

                            this.getCurrentTabIndex = function() {
                                var index = null;
                                that.element.find('ul.ui-tabs-nav').children('li').each( function(i) {
                                    if ( $(this).hasClass('ui-tabs-selected')) {
                                        index = i;
                                        return false;
                                    }

                                });
                                return index;

                            };

                            // override select so that it returns the tab selected
                            var oldSelect = oWidget.prototype.select;
                            oWidget.prototype.select = function(index) {
                                oldSelect.apply( this, [index]);
                                return this.anchors.eq(index);
                            };

                            this.getNextTabIndex = function() {
                                var anchors = this.anchors;
                                var curIndex = this.getCurrentTabIndex();

                                if ( curIndex === anchors.length - 1) { // last one
                                    return 0;
                                } else {
                                    return curIndex + 1;
                                }

                            };

                            this.getPreviousTabIndex = function() {
                                var anchors = this.anchors;
                                var curIndex = this.getCurrentTabIndex();

                                if ( curIndex === 0 ) { // first one
                                    return anchors.length - 1;
                                } else {
                                    return curIndex -1;
                                }
                            };

                            // I know, it won't work if you add a tab dynamically, but I can't hook onto
                            this.element.children('ul.ui-tabs-nav').keydown( function(e) {
                                var t = $( e.target);
                                var pressed = e.keyCode;
                                var keyD = $.ui.keyCode;

                                // private function to take other tabs out of tabindex
                                var activate = function(n, collection) {
                                    n = jQuery(n);
                                    nDom = n[0];
                                    nDom.focus();
                                    nDom.tabIndex = 0;
                                    if (collection) {
                                        $(collection).each( function() {
                                            if ( this !== nDom) {
                                                this.tabIndex = -1;
                                            }
                                        });
                                    }
                                };

                                if ( pressed === keyD.RIGHT) {
                                    // we overrode select to return tab instead of undefined
                                    var tab = that.select( that.getNextTabIndex() )[0];
                                    activate(tab, that.anchors);
                                }

                                if ( pressed === keyD.LEFT ) {
                                    var tab = that.select( that.getPreviousTabIndex() )[0];
                                    activate( tab, that.anchors);
                                }

                            });
                        };

                    } // end if widget is tabs


                })(oWidget, oldCreate, widgetName);

            }

            // add public getSelector function to each widget's prototype so we
            // don't have to add functions to widget code. It really belongs in jig code
            // since it is not part of UI, we need to create the function using an
            // anon func, passing it the widget name because of the closure

            (function( name ) {
                oWidget.getSelector = function() {
                    return widgDict[name].selector;
                };
            })(widgetName);

            // extract each widget's config by looping thru configs obj
            // the matching config passed in directly for this widget
            var configParam = {};
            for (var j in cfs) {
                if ( j === widgetName) {
                    //console.log(cfs[j]);
                    configParam = cfs[j];
                }
            }

            // is there a global config for this widget?
            var globalConfig = parseGlobalConfig( getParamMeta( widgetName ) ) || {};

            // if there's a global config with contextNode set, use that instead of one passed in
            if ( typeof globalConfig.contextNode !== 'undefined' ) {
                contextNode = $( globalConfig.contextNode );
            }
            var mergedGlobalConfig = jQuery.extend( globalConfig, configParam );
            //console.log('mergedGlobal: ', mergedGlobalConfig);
            //console.log('now contextNode: ', contextNode);

            // specical case for accordion. We have to pass a header config of :header, to force header markup

            var overrideDefaults =  widgDict[ widgetName ].overrideDefaults;
            if ( typeof overrideDefaults !== 'undefined' ) {
                $.extend( mergedGlobalConfig, overrideDefaults );
            }

            var selector = ui[widgetName].getSelector();

            $(selector, contextNode)[widgetName](mergedGlobalConfig);
        }

        jig.scanned = true;
    }; // end scan jig

    // jig.scanjig should really be called jig.scan, but let's not break the API
    jig.scan = jig.scanjig;

    // hijack jQuery's manipulation methods to wake jig up so that we can inject jig markup into page after page-load
    var manipulationMethodStrings = [ 'after', 'before', 'append', 'html', 'prepend' ];

    for ( var i = 0; i < manipulationMethodStrings.length; i++ ) {
        var methodStr = manipulationMethodStrings[ i ];
        origMethod = $.fn[ methodStr ];

        // redefine the original method.
        // Because of closure, need to pass in method string and original method as parameters
        ( function( origMethod, methodStr ) {
            $.fn[ methodStr ] = function( content, configParam) {

                // the new function will have an extra parameter which is either true, or a config param.
                // If this parameter is present, then we have jig scan for the markup calling any of these
                // methods returns the reference node(s) to the content we are adding in the cases where the
                // reference node is the parent of the inserted content, we want to remember the node so that
                // we can scan just that node
                var referenceNode = origMethod.apply( this, [ content ] );
                if ( configParam ) {

                    // for methods inserting inside node
                    if ( methodStr !== 'after' && methodStr !== 'before' ) {
                        $.ui.jig.scan( referenceNode, configParam );
                    } else { // methods that insert outside, we scan the parent
                        $.ui.jig.scan( referenceNode.parent(), configParam );

                    }
                }

                return referenceNode;
            };
        }) ( origMethod, methodStr);
    }

    // Loop through script tags to get jig.js include. From the src attribute we can get the
    // base URL and isMin property. We use the baseURL to figure out whether other *dynamic*
    // dependency includes should go to most recent major version, or specific version. We
    // use the isMin property to determine whether dynamic dependency includes should be
    // debug or minimized versions
    var jigRe = /(.*\/)?js\/jig(?:\.nojquery)?(\.min)?\.js$/;
    $('script').each( function() {
        var s = $(this);
        var srcAttr = s.attr('src');

        // if not an inline script
        if (srcAttr) {
            var srcMatch = srcAttr.match(jigRe);

            // if the src attribute points to some form of jig.js
            if (srcMatch) {

                // In most cases, the base url should be the part of the src attr
                // before the 'js/jig.js' string. we set all dynamically
                // included files relative to that string.
                // In some cases we will want to be able to have a test
                // page at the root of the jig directory, and then point
                // to the jig.js file under test. In that case there will be no 
                // baseURL, so set it to empty string ('').
                jig._baseURL = srcMatch[1] || '';
                if (srcMatch[2] === '.min') {
                    jig._isMin = true;
                    jig._jsExt = '.min.js';
                    jig._cssExt = '.min.css';
                } else {
                    jig._isMin = false;
                    jig._jsExt = '.js';
                    jig._cssExt = '.css';
                }
            }
        }
    });

    var baseURL = jig._baseURL;

    // shortcuts to some jig private properties we will use
    var baseURL = jig._baseURL;
    var isMin = jig._isMin;
    var jsExt = jig._jsExt;
    var cssExt = jig._cssExt;

    // Some helper functions for dispatcher code, can't be accessed public

    // returns matching parameter meta tag, given a widget name
    // for example I can give it "toggler" and it should return the node: <meta name="ui.toggler" content="foo:bar"/>
    // paramMetas is an array we get later in the script which holds all possible parameter meta tags
    var getParamMeta = function(widgName) {
        var paramMetasLen = paramMetas.length;
        if (paramMetasLen > 0) {
            for (var i = 0; i < paramMetasLen; i++) {
                var paramMeta = paramMetas[i];
                var paramMetaName = $.trim(paramMeta.getAttribute('name'));
                if (widgName === paramMetaName) {
                    return paramMeta;
                }
            }
        }
    };

    // helper function that takes a meta tag and creates a JSON object from the content attribute
    var parseGlobalConfig = function(meta) {
        meta = $(meta);
        var contentAttr = meta.attr('content');
        var n = meta.attr('name');
        if ( contentAttr) {
            return ui.jig._decodeJSON(contentAttr, name);
        }
    };

    var addCss = function(widgName) {
      document.write('<link type="text/css" rel="stylesheet" href="' + baseURL + 'css/jquery.ui.' + widgName + cssExt +'"/>');
    };

    var addJs = function(widgName) {
      document.write('<script type="text/javascript" src="' + baseURL + 'js/jquery.ui.' + widgName + jsExt + '"></script>');
    };

    var runAll = function() {
        // getConfigs: boolean true: get global config for widget, false, don't get global config
        // writes concatenated widget js and css files, loops through dependency dictionary and
        // calls all widgets. This is called when there are no meta tags on the page, or there
        // only parameter meta tags, since in these 2 cases we call all widgets in jig

        // first write in the proper concatenated jig widget js file
        //console.log('write in concatenated files');
        var widgTag = (isMin) ? 'jquery.ui.widgets.min.js' : 'jquery.ui.widgets.js';
        addJs('widgets');

        // now the proper concatenated jig widget css file
        var widgCSSTag = (isMin) ? 'jig.min.css' : 'jig.css';
        addCss('widgets');

        // on document ready, call all jig widgets
        //console.log('on document ready...');
        $(document).ready( function() {
            jig.scan();
        });
    };
    // end helper functions

    // always write in ui theme css files, debug or minimized
    document.write('<link rel="stylesheet" type="text/css" href="' + baseURL + 'css/jig.core.theme' + cssExt + '"/>');

    /**
    3 options for how widgets load:

   a) no metas, jig scans page for all widgets
   b) Set name = widget, content= config ex,
    <meta name="toggler" content="rememberState: false"/>
    <meta name="grid" content="isZebra:false"/>

    Loads everything, only toggle and grid set specific configs

    c)
    <meta name="jig" content="toggler grid"/>
    <meta name="toggle" content="rembeState:true"/>
    loads only toggler and grid, sets toggler config
    */

    // parse meta tags
    // get meta tags. jigMeta is the meta tag that restricts widgets to load,
    // paramMeta is meta tag that sets global parameters
    // interactMeta is meta tag of interactions to load if restricting loaded widgets
    var jigMeta = null, paramMetas = [], interactMeta = null;

    // get meta tags when doc is ready, should reorganzize code...
    var metas = $('head').find('meta');

    metas.each(function() {
      var name = $.trim($(this).attr('name'));
      if (name.search(/^jig$/) !== -1 ) {
        jigMeta = this;
      }

      // if parameter meta tag name attr is in the form "ncbi[widgetname]"
      if ($.inArray(name, jigWidgs) !== -1) {
        paramMetas.push(this);
      }

    });


    // fork code depending on presence of various meta tags

    // if no meta tags at all, call all widgets
    var paramMetasLen = paramMetas.length;

    if (jigMeta) {
        //console.log('There is a jig meta. There may or may not be parameter metas. Selectively call only those widgets set in ui meta and possibly set configs from parameter metas');
        var widgNames = jigMeta.getAttribute('content').split(/ +|, ?|;/);

        // Not everything in the jig meta tag is a widget, so need to create a
        // new list of widgets to scan.
        var scanlist = [];

        for (var i = 0; i < widgNames.length; i++) {
            // access dict to insert dependencies, To Do: change first letter to caps
            var widgName = widgNames[i];
            var isWidget = $.inArray(widgName, jigWidgs) !== -1 ? true: false;
            var isInteraction = $.inArray(widgName, interactions) !== -1 ? true: false;
            if (!isWidget && !isInteraction) {
                var m = 'jig error: No widget with the name "' + widgName + '". Check documentation at http://iwebdev/core/jig for proper name, and set meta tag accordingly';
                if ( typeof window.console !== 'undefined' && typeof console.warn !== 'undefined' ) {
                    console.warn(m);
                } else {
                    alert(m);
                }
                continue; // try next widget
            }
            if (isWidget) {
                scanlist.push(widgName);
                // if the widget depends on another widget, we have to write them in here
                widgDict[widgName].addToPage();
            } // end if widget
            if (isInteraction){
                addJs(widgName);
            }

        } // end for loop

        $(function() {
            jig.scan(null, { widgets: scanlist} );
        });
    } else {
        //console.log('no jig meta, run all');
        runAll();
    }
})();
