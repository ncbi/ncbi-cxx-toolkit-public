/*
 * jQuery UI Accordion 1.8
 *
 * Copyright (c) 2010 AUTHORS.txt (http://jqueryui.com/about)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * http://docs.jquery.com/UI/Accordion
 *
 * Depends:
 *	jquery.ui.core.js
 *	jquery.ui.widget.js
 */
(function($) {

$.widget("ui.accordion", {
	options: {
		active: 0,
		animated: 'slide',
		autoHeight: true,
		clearStyle: false,
		collapsible: false,
		event: "click",
		fillSpace: false,
		header: "> li > :first-child,> :not(li):even",
		icons: {
			header: "ui-icon-triangle-1-e",
			headerSelected: "ui-icon-triangle-1-s"
		},
		navigation: false,
		navigationFilter: function() {
			return this.href.toLowerCase() == location.href.toLowerCase();
		}
	},
	_create: function() {

		var o = this.options, self = this;
		this.running = 0;

		this.element.addClass("ui-accordion ui-widget ui-helper-reset");
		
		// in lack of child-selectors in CSS we need to mark top-LIs in a UL-accordion for some IE-fix
		if (this.element[0].nodeName == "UL") {
			this.element.children("li").addClass("ui-accordion-li-fix");
		}

		this.headers = this.element.find(o.header).addClass("ui-accordion-header ui-helper-reset ui-state-default ui-corner-all")
			.bind("mouseenter.accordion", function(){ $(this).addClass('ui-state-hover'); })
			.bind("mouseleave.accordion", function(){ $(this).removeClass('ui-state-hover'); })
			.bind("focus.accordion", function(){ $(this).addClass('ui-state-focus'); })
			.bind("blur.accordion", function(){ $(this).removeClass('ui-state-focus'); });

		this.headers
			.next()
				.addClass("ui-accordion-content ui-helper-reset ui-widget-content ui-corner-bottom");

		if ( o.navigation ) {
			var current = this.element.find("a").filter(o.navigationFilter);
			if ( current.length ) {
				var header = current.closest(".ui-accordion-header");
				if ( header.length ) {
					// anchor within header
					this.active = header;
				} else {
					// anchor within content
					this.active = current.closest(".ui-accordion-content").prev();
				}
			}
		}

		this.active = this._findActive(this.active || o.active).toggleClass("ui-state-default").toggleClass("ui-state-active").toggleClass("ui-corner-all").toggleClass("ui-corner-top");
		this.active.next().addClass('ui-accordion-content-active');

		//Append icon elements
		this._createIcons();

		// IE7-/Win - Extra vertical space in lists fixed
		if ($.browser.msie) {
			this.element.find('a').css('zoom', '1');
		}

		this.resize();

		//ARIA
		this.element.attr('role','tablist');

		this.headers
			.attr('role','tab')
			.bind('keydown', function(event) { return self._keydown(event); })
			.next()
			.attr('role','tabpanel');

		this.headers
			.not(this.active || "")
			.attr('aria-expanded','false')
			.attr("tabIndex", "-1")
			.next()
			.hide();

		// make sure at least one header is in the tab order
		if (!this.active.length) {
			this.headers.eq(0).attr('tabIndex','0');
		} else {
			this.active
				.attr('aria-expanded','true')
				.attr('tabIndex', '0');
		}

		// only need links in taborder for Safari
		if (!$.browser.safari)
			this.headers.find('a').attr('tabIndex','-1');

		if (o.event) {
			this.headers.bind((o.event) + ".accordion", function(event) {
				self._clickHandler.call(self, event, this);
				event.preventDefault();
			});
		}

	},
	
	_createIcons: function() {
		var o = this.options;
		if (o.icons) {
			$("<span/>").addClass("ui-icon " + o.icons.header).prependTo(this.headers);
			this.active.find(".ui-icon").toggleClass(o.icons.header).toggleClass(o.icons.headerSelected);
			this.element.addClass("ui-accordion-icons");
		}
	},
	
	_destroyIcons: function() {
		this.headers.children(".ui-icon").remove();
		this.element.removeClass("ui-accordion-icons");
	},

	destroy: function() {
		var o = this.options;

		this.element
			.removeClass("ui-accordion ui-widget ui-helper-reset")
			.removeAttr("role")
			.unbind('.accordion')
			.removeData('accordion');

		this.headers
			.unbind(".accordion")
			.removeClass("ui-accordion-header ui-helper-reset ui-state-default ui-corner-all ui-state-active ui-corner-top")
			.removeAttr("role").removeAttr("aria-expanded").removeAttr("tabindex");

		this.headers.find("a").removeAttr("tabindex");
		this._destroyIcons();
		var contents = this.headers.next().css("display", "").removeAttr("role").removeClass("ui-helper-reset ui-widget-content ui-corner-bottom ui-accordion-content ui-accordion-content-active");
		if (o.autoHeight || o.fillHeight) {
			contents.css("height", "");
		}

		return this;
	},
	
	_setOption: function(key, value) {
		$.Widget.prototype._setOption.apply(this, arguments);
			
		if (key == "active") {
			this.activate(value);
		}
		if (key == "icons") {
			this._destroyIcons();
			if (value) {
				this._createIcons();
			}
		}
		
	},

	_keydown: function(event) {

		var o = this.options, keyCode = $.ui.keyCode;

		if (o.disabled || event.altKey || event.ctrlKey)
			return;

		var length = this.headers.length;
		var currentIndex = this.headers.index(event.target);
		var toFocus = false;

		switch(event.keyCode) {
			case keyCode.RIGHT:
			case keyCode.DOWN:
				toFocus = this.headers[(currentIndex + 1) % length];
				break;
			case keyCode.LEFT:
			case keyCode.UP:
				toFocus = this.headers[(currentIndex - 1 + length) % length];
				break;
			case keyCode.SPACE:
			case keyCode.ENTER:
				this._clickHandler({ target: event.target }, event.target);
				event.preventDefault();
		}

		if (toFocus) {
			$(event.target).attr('tabIndex','-1');
			$(toFocus).attr('tabIndex','0');
			toFocus.focus();
			return false;
		}

		return true;

	},

	resize: function() {

		var o = this.options, maxHeight;

		if (o.fillSpace) {
			
			if($.browser.msie) { var defOverflow = this.element.parent().css('overflow'); this.element.parent().css('overflow', 'hidden'); }
			maxHeight = this.element.parent().height();
			if($.browser.msie) { this.element.parent().css('overflow', defOverflow); }
	
			this.headers.each(function() {
				maxHeight -= $(this).outerHeight(true);
			});

			this.headers.next().each(function() {
    		   $(this).height(Math.max(0, maxHeight - $(this).innerHeight() + $(this).height()));
			}).css('overflow', 'auto');

		} else if ( o.autoHeight ) {
			maxHeight = 0;
			this.headers.next().each(function() {
				maxHeight = Math.max(maxHeight, $(this).height());
			}).height(maxHeight);
		}

		return this;
	},

	activate: function(index) {
		// TODO this gets called on init, changing the option without an explicit call for that
		this.options.active = index;
		// call clickHandler with custom event
		var active = this._findActive(index)[0];
		this._clickHandler({ target: active }, active);

		return this;
	},

	_findActive: function(selector) {
		return selector
			? typeof selector == "number"
				? this.headers.filter(":eq(" + selector + ")")
				: this.headers.not(this.headers.not(selector))
			: selector === false
				? $([])
				: this.headers.filter(":eq(0)");
	},

	// TODO isn't event.target enough? why the seperate target argument?
	_clickHandler: function(event, target) {

		var o = this.options;
		if (o.disabled)
			return;

		// called only when using activate(false) to close all parts programmatically
		if (!event.target) {
			if (!o.collapsible)
				return;
			this.active.removeClass("ui-state-active ui-corner-top").addClass("ui-state-default ui-corner-all")
				.find(".ui-icon").removeClass(o.icons.headerSelected).addClass(o.icons.header);
			this.active.next().addClass('ui-accordion-content-active');
			var toHide = this.active.next(),
				data = {
					options: o,
					newHeader: $([]),
					oldHeader: o.active,
					newContent: $([]),
					oldContent: toHide
				},
				toShow = (this.active = $([]));
			this._toggle(toShow, toHide, data);
			return;
		}

		// get the click target
		var clicked = $(event.currentTarget || target);
		var clickedIsActive = clicked[0] == this.active[0];
		
		// TODO the option is changed, is that correct?
		// TODO if it is correct, shouldn't that happen after determining that the click is valid?
		o.active = o.collapsible && clickedIsActive ? false : $('.ui-accordion-header', this.element).index(clicked);

		// if animations are still active, or the active header is the target, ignore click
		if (this.running || (!o.collapsible && clickedIsActive)) {
			return;
		}

		// switch classes
		this.active.removeClass("ui-state-active ui-corner-top").addClass("ui-state-default ui-corner-all")
			.find(".ui-icon").removeClass(o.icons.headerSelected).addClass(o.icons.header);
		if (!clickedIsActive) {
			clicked.removeClass("ui-state-default ui-corner-all").addClass("ui-state-active ui-corner-top")
				.find(".ui-icon").removeClass(o.icons.header).addClass(o.icons.headerSelected);
			clicked.next().addClass('ui-accordion-content-active');
		}

		// find elements to show and hide
		var toShow = clicked.next(),
			toHide = this.active.next(),
			data = {
				options: o,
				newHeader: clickedIsActive && o.collapsible ? $([]) : clicked,
				oldHeader: this.active,
				newContent: clickedIsActive && o.collapsible ? $([]) : toShow,
				oldContent: toHide
			},
			down = this.headers.index( this.active[0] ) > this.headers.index( clicked[0] );

		this.active = clickedIsActive ? $([]) : clicked;
		this._toggle(toShow, toHide, data, clickedIsActive, down);

		return;

	},

	_toggle: function(toShow, toHide, data, clickedIsActive, down) {

		var o = this.options, self = this;

		this.toShow = toShow;
		this.toHide = toHide;
		this.data = data;

		var complete = function() { if(!self) return; return self._completed.apply(self, arguments); };

		// trigger changestart event
		this._trigger("changestart", null, this.data);

		// count elements to animate
		this.running = toHide.size() === 0 ? toShow.size() : toHide.size();

		if (o.animated) {

			var animOptions = {};

			if ( o.collapsible && clickedIsActive ) {
				animOptions = {
					toShow: $([]),
					toHide: toHide,
					complete: complete,
					down: down,
					autoHeight: o.autoHeight || o.fillSpace
				};
			} else {
				animOptions = {
					toShow: toShow,
					toHide: toHide,
					complete: complete,
					down: down,
					autoHeight: o.autoHeight || o.fillSpace
				};
			}

			if (!o.proxied) {
				o.proxied = o.animated;
			}

			if (!o.proxiedDuration) {
				o.proxiedDuration = o.duration;
			}

			o.animated = $.isFunction(o.proxied) ?
				o.proxied(animOptions) : o.proxied;

			o.duration = $.isFunction(o.proxiedDuration) ?
				o.proxiedDuration(animOptions) : o.proxiedDuration;

			var animations = $.ui.accordion.animations,
				duration = o.duration,
				easing = o.animated;

			if (easing && !animations[easing] && !$.easing[easing]) {
				easing = 'slide';
			}
			if (!animations[easing]) {
				animations[easing] = function(options) {
					this.slide(options, {
						easing: easing,
						duration: duration || 700
					});
				};
			}

			animations[easing](animOptions);

		} else {

			if (o.collapsible && clickedIsActive) {
				toShow.toggle();
			} else {
				toHide.hide();
				toShow.show();
			}

			complete(true);

		}

		// TODO assert that the blur and focus triggers are really necessary, remove otherwise
		toHide.prev().attr('aria-expanded','false').attr("tabIndex", "-1").blur();
		toShow.prev().attr('aria-expanded','true').attr("tabIndex", "0").focus();

	},

	_completed: function(cancel) {

		var o = this.options;

		this.running = cancel ? 0 : --this.running;
		if (this.running) return;

		if (o.clearStyle) {
			this.toShow.add(this.toHide).css({
				height: "",
				overflow: ""
			});
		}
		
		// other classes are removed before the animation; this one needs to stay until completed
		this.toHide.removeClass("ui-accordion-content-active");

		this._trigger('change', null, this.data);
	}

});


$.extend($.ui.accordion, {
	version: "1.8",
	animations: {
		slide: function(options, additions) {
			options = $.extend({
				easing: "swing",
				duration: 300
			}, options, additions);
			if ( !options.toHide.size() ) {
				options.toShow.animate({height: "show"}, options);
				return;
			}
			if ( !options.toShow.size() ) {
				options.toHide.animate({height: "hide"}, options);
				return;
			}
			var overflow = options.toShow.css('overflow'),
				percentDone = 0,
				showProps = {},
				hideProps = {},
				fxAttrs = [ "height", "paddingTop", "paddingBottom" ],
				originalWidth;
			// fix width before calculating height of hidden element
			var s = options.toShow;
			originalWidth = s[0].style.width;
			s.width( parseInt(s.parent().width(),10) - parseInt(s.css("paddingLeft"),10) - parseInt(s.css("paddingRight"),10) - (parseInt(s.css("borderLeftWidth"),10) || 0) - (parseInt(s.css("borderRightWidth"),10) || 0) );
			
			$.each(fxAttrs, function(i, prop) {
				hideProps[prop] = 'hide';
				
				var parts = ('' + $.css(options.toShow[0], prop)).match(/^([\d+-.]+)(.*)$/);
				showProps[prop] = {
					value: parts[1],
					unit: parts[2] || 'px'
				};
			});
			options.toShow.css({ height: 0, overflow: 'hidden' }).show();
			options.toHide.filter(":hidden").each(options.complete).end().filter(":visible").animate(hideProps,{
				step: function(now, settings) {
					// only calculate the percent when animating height
					// IE gets very inconsistent results when animating elements
					// with small values, which is common for padding
					if (settings.prop == 'height') {
						percentDone = ( settings.end - settings.start === 0 ) ? 0 :
							(settings.now - settings.start) / (settings.end - settings.start);
					}
					
					options.toShow[0].style[settings.prop] =
						(percentDone * showProps[settings.prop].value) + showProps[settings.prop].unit;
				},
				duration: options.duration,
				easing: options.easing,
				complete: function() {
					if ( !options.autoHeight ) {
						options.toShow.css("height", "");
					}
					options.toShow.css("width", originalWidth);
					options.toShow.css({overflow: overflow});
					options.complete();
				}
			});
		},
		bounceslide: function(options) {
			this.slide(options, {
				easing: options.down ? "easeOutBounce" : "swing",
				duration: options.down ? 1000 : 200
			});
		}
	}
});

})(jQuery);
/*
 * jQuery UI Tabs 1.8
 *
 * Copyright (c) 2010 AUTHORS.txt (http://jqueryui.com/about)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * http://docs.jquery.com/UI/Tabs
 *
 * Depends:
 *	jquery.ui.core.js
 *	jquery.ui.widget.js
 */
(function($) {

var tabId = 0,
	listId = 0;

$.widget("ui.tabs", {
	options: {
		add: null,
		ajaxOptions: null,
		cache: false,
		cookie: null, // e.g. { expires: 7, path: '/', domain: 'jquery.com', secure: true }
		collapsible: false,
		disable: null,
		disabled: [],
		enable: null,
		event: 'click',
		fx: null, // e.g. { height: 'toggle', opacity: 'toggle', duration: 200 }
		idPrefix: 'ui-tabs-',
		load: null,
		panelTemplate: '<div></div>',
		remove: null,
		select: null,
		show: null,
		spinner: '<em>Loading&#8230;</em>',
		tabTemplate: '<li><a href="#{href}"><span>#{label}</span></a></li>'
	},
	_create: function() {
		this._tabify(true);
	},

	_setOption: function(key, value) {
		if (key == 'selected') {
			if (this.options.collapsible && value == this.options.selected) {
				return;
			}
			this.select(value);
		}
		else {
			this.options[key] = value;
			this._tabify();
		}
	},

	_tabId: function(a) {
		return a.title && a.title.replace(/\s/g, '_').replace(/[^A-Za-z0-9\-_:\.]/g, '') ||
			this.options.idPrefix + (++tabId);
	},

	_sanitizeSelector: function(hash) {
		return hash.replace(/:/g, '\\:'); // we need this because an id may contain a ":"
	},

	_cookie: function() {
		var cookie = this.cookie || (this.cookie = this.options.cookie.name || 'ui-tabs-' + (++listId));
		return $.cookie.apply(null, [cookie].concat($.makeArray(arguments)));
	},

	_ui: function(tab, panel) {
		return {
			tab: tab,
			panel: panel,
			index: this.anchors.index(tab)
		};
	},

	_cleanup: function() {
		// restore all former loading tabs labels
		this.lis.filter('.ui-state-processing').removeClass('ui-state-processing')
				.find('span:data(label.tabs)')
				.each(function() {
					var el = $(this);
					el.html(el.data('label.tabs')).removeData('label.tabs');
				});
	},

	_tabify: function(init) {

		this.list = this.element.find('ol,ul').eq(0);
		this.lis = $('li:has(a[href])', this.list);
		this.anchors = this.lis.map(function() { return $('a', this)[0]; });
		this.panels = $([]);

		var self = this, o = this.options;

		var fragmentId = /^#.+/; // Safari 2 reports '#' for an empty hash
		this.anchors.each(function(i, a) {
			var href = $(a).attr('href');

			// For dynamically created HTML that contains a hash as href IE < 8 expands
			// such href to the full page url with hash and then misinterprets tab as ajax.
			// Same consideration applies for an added tab with a fragment identifier
			// since a[href=#fragment-identifier] does unexpectedly not match.
			// Thus normalize href attribute...
			var hrefBase = href.split('#')[0], baseEl;
			if (hrefBase && (hrefBase === location.toString().split('#')[0] ||
					(baseEl = $('base')[0]) && hrefBase === baseEl.href)) {
				href = a.hash;
				a.href = href;
			}

			// inline tab
			if (fragmentId.test(href)) {
				self.panels = self.panels.add(self._sanitizeSelector(href));
			}

			// remote tab
			else if (href != '#') { // prevent loading the page itself if href is just "#"
				$.data(a, 'href.tabs', href); // required for restore on destroy

				// TODO until #3808 is fixed strip fragment identifier from url
				// (IE fails to load from such url)
				$.data(a, 'load.tabs', href.replace(/#.*$/, '')); // mutable data

				var id = self._tabId(a);
				a.href = '#' + id;
				var $panel = $('#' + id);
				if (!$panel.length) {
					$panel = $(o.panelTemplate).attr('id', id).addClass('ui-tabs-panel ui-widget-content ui-corner-bottom')
						.insertAfter(self.panels[i - 1] || self.list);
					$panel.data('destroy.tabs', true);
				}
				self.panels = self.panels.add($panel);
			}

			// invalid tab href
			else {
				o.disabled.push(i);
			}
		});

		// initialization from scratch
		if (init) {

			// attach necessary classes for styling
			this.element.addClass('ui-tabs ui-widget ui-widget-content ui-corner-all');
			this.list.addClass('ui-tabs-nav ui-helper-reset ui-helper-clearfix ui-widget-header ui-corner-all');
			this.lis.addClass('ui-state-default ui-corner-top');
			this.panels.addClass('ui-tabs-panel ui-widget-content ui-corner-bottom');

			// Selected tab
			// use "selected" option or try to retrieve:
			// 1. from fragment identifier in url
			// 2. from cookie
			// 3. from selected class attribute on <li>
			if (o.selected === undefined) {
				if (location.hash) {
					this.anchors.each(function(i, a) {
						if (a.hash == location.hash) {
							o.selected = i;
							return false; // break
						}
					});
				}
				if (typeof o.selected != 'number' && o.cookie) {
					o.selected = parseInt(self._cookie(), 10);
				}
				if (typeof o.selected != 'number' && this.lis.filter('.ui-tabs-selected').length) {
					o.selected = this.lis.index(this.lis.filter('.ui-tabs-selected'));
				}
				o.selected = o.selected || (this.lis.length ? 0 : -1);
			}
			else if (o.selected === null) { // usage of null is deprecated, TODO remove in next release
				o.selected = -1;
			}

			// sanity check - default to first tab...
			o.selected = ((o.selected >= 0 && this.anchors[o.selected]) || o.selected < 0) ? o.selected : 0;

			// Take disabling tabs via class attribute from HTML
			// into account and update option properly.
			// A selected tab cannot become disabled.
			o.disabled = $.unique(o.disabled.concat(
				$.map(this.lis.filter('.ui-state-disabled'),
					function(n, i) { return self.lis.index(n); } )
			)).sort();

			if ($.inArray(o.selected, o.disabled) != -1) {
				o.disabled.splice($.inArray(o.selected, o.disabled), 1);
			}

			// highlight selected tab
			this.panels.addClass('ui-tabs-hide');
			this.lis.removeClass('ui-tabs-selected ui-state-active');
			if (o.selected >= 0 && this.anchors.length) { // check for length avoids error when initializing empty list
				this.panels.eq(o.selected).removeClass('ui-tabs-hide');
				this.lis.eq(o.selected).addClass('ui-tabs-selected ui-state-active');

				// seems to be expected behavior that the show callback is fired
				self.element.queue("tabs", function() {
					self._trigger('show', null, self._ui(self.anchors[o.selected], self.panels[o.selected]));
				});
				
				this.load(o.selected);
			}

			// clean up to avoid memory leaks in certain versions of IE 6
			$(window).bind('unload', function() {
				self.lis.add(self.anchors).unbind('.tabs');
				self.lis = self.anchors = self.panels = null;
			});

		}
		// update selected after add/remove
		else {
			o.selected = this.lis.index(this.lis.filter('.ui-tabs-selected'));
		}

		// update collapsible
		this.element[o.collapsible ? 'addClass' : 'removeClass']('ui-tabs-collapsible');

		// set or update cookie after init and add/remove respectively
		if (o.cookie) {
			this._cookie(o.selected, o.cookie);
		}

		// disable tabs
		for (var i = 0, li; (li = this.lis[i]); i++) {
			$(li)[$.inArray(i, o.disabled) != -1 &&
				!$(li).hasClass('ui-tabs-selected') ? 'addClass' : 'removeClass']('ui-state-disabled');
		}

		// reset cache if switching from cached to not cached
		if (o.cache === false) {
			this.anchors.removeData('cache.tabs');
		}

		// remove all handlers before, tabify may run on existing tabs after add or option change
		this.lis.add(this.anchors).unbind('.tabs');

		if (o.event != 'mouseover') {
			var addState = function(state, el) {
				if (el.is(':not(.ui-state-disabled)')) {
					el.addClass('ui-state-' + state);
				}
			};
			var removeState = function(state, el) {
				el.removeClass('ui-state-' + state);
			};
			this.lis.bind('mouseover.tabs', function() {
				addState('hover', $(this));
			});
			this.lis.bind('mouseout.tabs', function() {
				removeState('hover', $(this));
			});
			this.anchors.bind('focus.tabs', function() {
				addState('focus', $(this).closest('li'));
			});
			this.anchors.bind('blur.tabs', function() {
				removeState('focus', $(this).closest('li'));
			});
		}

		// set up animations
		var hideFx, showFx;
		if (o.fx) {
			if ($.isArray(o.fx)) {
				hideFx = o.fx[0];
				showFx = o.fx[1];
			}
			else {
				hideFx = showFx = o.fx;
			}
		}

		// Reset certain styles left over from animation
		// and prevent IE's ClearType bug...
		function resetStyle($el, fx) {
			$el.css({ display: '' });
			if (!$.support.opacity && fx.opacity) {
				$el[0].style.removeAttribute('filter');
			}
		}

		// Show a tab...
		var showTab = showFx ?
			function(clicked, $show) {
				$(clicked).closest('li').addClass('ui-tabs-selected ui-state-active');
				$show.hide().removeClass('ui-tabs-hide') // avoid flicker that way
					.animate(showFx, showFx.duration || 'normal', function() {
						resetStyle($show, showFx);
						self._trigger('show', null, self._ui(clicked, $show[0]));
					});
			} :
			function(clicked, $show) {
				$(clicked).closest('li').addClass('ui-tabs-selected ui-state-active');
				$show.removeClass('ui-tabs-hide');
				self._trigger('show', null, self._ui(clicked, $show[0]));
			};

		// Hide a tab, $show is optional...
		var hideTab = hideFx ?
			function(clicked, $hide) {
				$hide.animate(hideFx, hideFx.duration || 'normal', function() {
					self.lis.removeClass('ui-tabs-selected ui-state-active');
					$hide.addClass('ui-tabs-hide');
					resetStyle($hide, hideFx);
					self.element.dequeue("tabs");
				});
			} :
			function(clicked, $hide, $show) {
				self.lis.removeClass('ui-tabs-selected ui-state-active');
				$hide.addClass('ui-tabs-hide');
				self.element.dequeue("tabs");
			};

		// attach tab event handler, unbind to avoid duplicates from former tabifying...
		this.anchors.bind(o.event + '.tabs', function() {
			var el = this, $li = $(this).closest('li'), $hide = self.panels.filter(':not(.ui-tabs-hide)'),
					$show = $(self._sanitizeSelector(this.hash));

			// If tab is already selected and not collapsible or tab disabled or
			// or is already loading or click callback returns false stop here.
			// Check if click handler returns false last so that it is not executed
			// for a disabled or loading tab!
			if (($li.hasClass('ui-tabs-selected') && !o.collapsible) ||
				$li.hasClass('ui-state-disabled') ||
				$li.hasClass('ui-state-processing') ||
				self._trigger('select', null, self._ui(this, $show[0])) === false) {
				this.blur();
				return false;
			}

			o.selected = self.anchors.index(this);

			self.abort();

			// if tab may be closed
			if (o.collapsible) {
				if ($li.hasClass('ui-tabs-selected')) {
					o.selected = -1;

					if (o.cookie) {
						self._cookie(o.selected, o.cookie);
					}

					self.element.queue("tabs", function() {
						hideTab(el, $hide);
					}).dequeue("tabs");
					
					this.blur();
					return false;
				}
				else if (!$hide.length) {
					if (o.cookie) {
						self._cookie(o.selected, o.cookie);
					}
					
					self.element.queue("tabs", function() {
						showTab(el, $show);
					});

					self.load(self.anchors.index(this)); // TODO make passing in node possible, see also http://dev.jqueryui.com/ticket/3171
					
					this.blur();
					return false;
				}
			}

			if (o.cookie) {
				self._cookie(o.selected, o.cookie);
			}

			// show new tab
			if ($show.length) {
				if ($hide.length) {
					self.element.queue("tabs", function() {
						hideTab(el, $hide);
					});
				}
				self.element.queue("tabs", function() {
					showTab(el, $show);
				});
				
				self.load(self.anchors.index(this));
			}
			else {
				throw 'jQuery UI Tabs: Mismatching fragment identifier.';
			}

			// Prevent IE from keeping other link focussed when using the back button
			// and remove dotted border from clicked link. This is controlled via CSS
			// in modern browsers; blur() removes focus from address bar in Firefox
			// which can become a usability and annoying problem with tabs('rotate').
			if ($.browser.msie) {
				this.blur();
			}

		});

		// disable click in any case
		this.anchors.bind('click.tabs', function(){return false;});

	},

	destroy: function() {
		var o = this.options;

		this.abort();
		
		this.element.unbind('.tabs')
			.removeClass('ui-tabs ui-widget ui-widget-content ui-corner-all ui-tabs-collapsible')
			.removeData('tabs');

		this.list.removeClass('ui-tabs-nav ui-helper-reset ui-helper-clearfix ui-widget-header ui-corner-all');

		this.anchors.each(function() {
			var href = $.data(this, 'href.tabs');
			if (href) {
				this.href = href;
			}
			var $this = $(this).unbind('.tabs');
			$.each(['href', 'load', 'cache'], function(i, prefix) {
				$this.removeData(prefix + '.tabs');
			});
		});

		this.lis.unbind('.tabs').add(this.panels).each(function() {
			if ($.data(this, 'destroy.tabs')) {
				$(this).remove();
			}
			else {
				$(this).removeClass([
					'ui-state-default',
					'ui-corner-top',
					'ui-tabs-selected',
					'ui-state-active',
					'ui-state-hover',
					'ui-state-focus',
					'ui-state-disabled',
					'ui-tabs-panel',
					'ui-widget-content',
					'ui-corner-bottom',
					'ui-tabs-hide'
				].join(' '));
			}
		});

		if (o.cookie) {
			this._cookie(null, o.cookie);
		}

		return this;
	},

	add: function(url, label, index) {
		if (index === undefined) {
			index = this.anchors.length; // append by default
		}

		var self = this, o = this.options,
			$li = $(o.tabTemplate.replace(/#\{href\}/g, url).replace(/#\{label\}/g, label)),
			id = !url.indexOf('#') ? url.replace('#', '') : this._tabId($('a', $li)[0]);

		$li.addClass('ui-state-default ui-corner-top').data('destroy.tabs', true);

		// try to find an existing element before creating a new one
		var $panel = $('#' + id);
		if (!$panel.length) {
			$panel = $(o.panelTemplate).attr('id', id).data('destroy.tabs', true);
		}
		$panel.addClass('ui-tabs-panel ui-widget-content ui-corner-bottom ui-tabs-hide');

		if (index >= this.lis.length) {
			$li.appendTo(this.list);
			$panel.appendTo(this.list[0].parentNode);
		}
		else {
			$li.insertBefore(this.lis[index]);
			$panel.insertBefore(this.panels[index]);
		}

		o.disabled = $.map(o.disabled,
			function(n, i) { return n >= index ? ++n : n; });

		this._tabify();

		if (this.anchors.length == 1) { // after tabify
			o.selected = 0;
			$li.addClass('ui-tabs-selected ui-state-active');
			$panel.removeClass('ui-tabs-hide');
			this.element.queue("tabs", function() {
				self._trigger('show', null, self._ui(self.anchors[0], self.panels[0]));
			});
				
			this.load(0);
		}

		// callback
		this._trigger('add', null, this._ui(this.anchors[index], this.panels[index]));
		return this;
	},

	remove: function(index) {
		var o = this.options, $li = this.lis.eq(index).remove(),
			$panel = this.panels.eq(index).remove();

		// If selected tab was removed focus tab to the right or
		// in case the last tab was removed the tab to the left.
		if ($li.hasClass('ui-tabs-selected') && this.anchors.length > 1) {
			this.select(index + (index + 1 < this.anchors.length ? 1 : -1));
		}

		o.disabled = $.map($.grep(o.disabled, function(n, i) { return n != index; }),
			function(n, i) { return n >= index ? --n : n; });

		this._tabify();

		// callback
		this._trigger('remove', null, this._ui($li.find('a')[0], $panel[0]));
		return this;
	},

	enable: function(index) {
		var o = this.options;
		if ($.inArray(index, o.disabled) == -1) {
			return;
		}

		this.lis.eq(index).removeClass('ui-state-disabled');
		o.disabled = $.grep(o.disabled, function(n, i) { return n != index; });

		// callback
		this._trigger('enable', null, this._ui(this.anchors[index], this.panels[index]));
		return this;
	},

	disable: function(index) {
		var self = this, o = this.options;
		if (index != o.selected) { // cannot disable already selected tab
			this.lis.eq(index).addClass('ui-state-disabled');

			o.disabled.push(index);
			o.disabled.sort();

			// callback
			this._trigger('disable', null, this._ui(this.anchors[index], this.panels[index]));
		}

		return this;
	},

	select: function(index) {
		if (typeof index == 'string') {
			index = this.anchors.index(this.anchors.filter('[href$=' + index + ']'));
		}
		else if (index === null) { // usage of null is deprecated, TODO remove in next release
			index = -1;
		}
		if (index == -1 && this.options.collapsible) {
			index = this.options.selected;
		}

		this.anchors.eq(index).trigger(this.options.event + '.tabs');
		return this;
	},

	load: function(index) {
		var self = this, o = this.options, a = this.anchors.eq(index)[0], url = $.data(a, 'load.tabs');

		this.abort();

		// not remote or from cache
		if (!url || this.element.queue("tabs").length !== 0 && $.data(a, 'cache.tabs')) {
			this.element.dequeue("tabs");
			return;
		}

		// load remote from here on
		this.lis.eq(index).addClass('ui-state-processing');

		if (o.spinner) {
			var span = $('span', a);
			span.data('label.tabs', span.html()).html(o.spinner);
		}

		this.xhr = $.ajax($.extend({}, o.ajaxOptions, {
			url: url,
			success: function(r, s) {
				$(self._sanitizeSelector(a.hash)).html(r);

				// take care of tab labels
				self._cleanup();

				if (o.cache) {
					$.data(a, 'cache.tabs', true); // if loaded once do not load them again
				}

				// callbacks
				self._trigger('load', null, self._ui(self.anchors[index], self.panels[index]));
				try {
					o.ajaxOptions.success(r, s);
				}
				catch (e) {}
			},
			error: function(xhr, s, e) {
				// take care of tab labels
				self._cleanup();

				// callbacks
				self._trigger('load', null, self._ui(self.anchors[index], self.panels[index]));
				try {
					// Passing index avoid a race condition when this method is
					// called after the user has selected another tab.
					// Pass the anchor that initiated this request allows
					// loadError to manipulate the tab content panel via $(a.hash)
					o.ajaxOptions.error(xhr, s, index, a);
				}
				catch (e) {}
			}
		}));

		// last, so that load event is fired before show...
		self.element.dequeue("tabs");

		return this;
	},

	abort: function() {
		// stop possibly running animations
		this.element.queue([]);
		this.panels.stop(false, true);

		// "tabs" queue must not contain more than two elements,
		// which are the callbacks for the latest clicked tab...
		this.element.queue("tabs", this.element.queue("tabs").splice(-2, 2));

		// terminate pending requests from other tabs
		if (this.xhr) {
			this.xhr.abort();
			delete this.xhr;
		}

		// take care of tab labels
		this._cleanup();
		return this;
	},

	url: function(index, url) {
		this.anchors.eq(index).removeData('cache.tabs').data('load.tabs', url);
		return this;
	},

	length: function() {
		return this.anchors.length;
	}

});

$.extend($.ui.tabs, {
	version: '1.8'
});

/*
 * Tabs Extensions
 */

/*
 * Rotate
 */
$.extend($.ui.tabs.prototype, {
	rotation: null,
	rotate: function(ms, continuing) {

		var self = this, o = this.options;
		
		var rotate = self._rotate || (self._rotate = function(e) {
			clearTimeout(self.rotation);
			self.rotation = setTimeout(function() {
				var t = o.selected;
				self.select( ++t < self.anchors.length ? t : 0 );
			}, ms);
			
			if (e) {
				e.stopPropagation();
			}
		});
		
		var stop = self._unrotate || (self._unrotate = !continuing ?
			function(e) {
				if (e.clientX) { // in case of a true click
					self.rotate(null);
				}
			} :
			function(e) {
				t = o.selected;
				rotate();
			});

		// start rotation
		if (ms) {
			this.element.bind('tabsshow', rotate);
			this.anchors.bind(o.event + '.tabs', stop);
			rotate();
		}
		// stop rotation
		else {
			clearTimeout(self.rotation);
			this.element.unbind('tabsshow', rotate);
			this.anchors.unbind(o.event + '.tabs', stop);
			delete this._rotate;
			delete this._unrotate;
		}

		return this;
	}
});

})(jQuery);
/*
 * jQuery UI Resizable 1.8
 *
 * Copyright (c) 2010 AUTHORS.txt (http://jqueryui.com/about)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * http://docs.jquery.com/UI/Resizables
 *
 * Depends:
 *	jquery.ui.core.js
 *	jquery.ui.mouse.js
 *	jquery.ui.widget.js
 */
(function($) {

$.widget("ui.resizable", $.ui.mouse, {
	widgetEventPrefix: "resize",
	options: {
		alsoResize: false,
		animate: false,
		animateDuration: "slow",
		animateEasing: "swing",
		aspectRatio: false,
		autoHide: false,
		containment: false,
		ghost: false,
		grid: false,
		handles: "e,s,se",
		helper: false,
		maxHeight: null,
		maxWidth: null,
		minHeight: 10,
		minWidth: 10,
		zIndex: 1000
	},
	_create: function() {

		var self = this, o = this.options;
		this.element.addClass("ui-resizable");

		$.extend(this, {
			_aspectRatio: !!(o.aspectRatio),
			aspectRatio: o.aspectRatio,
			originalElement: this.element,
			_proportionallyResizeElements: [],
			_helper: o.helper || o.ghost || o.animate ? o.helper || 'ui-resizable-helper' : null
		});

		//Wrap the element if it cannot hold child nodes
		if(this.element[0].nodeName.match(/canvas|textarea|input|select|button|img/i)) {

			//Opera fix for relative positioning
			if (/relative/.test(this.element.css('position')) && $.browser.opera)
				this.element.css({ position: 'relative', top: 'auto', left: 'auto' });

			//Create a wrapper element and set the wrapper to the new current internal element
			this.element.wrap(
				$('<div class="ui-wrapper" style="overflow: hidden;"></div>').css({
					position: this.element.css('position'),
					width: this.element.outerWidth(),
					height: this.element.outerHeight(),
					top: this.element.css('top'),
					left: this.element.css('left')
				})
			);

			//Overwrite the original this.element
			this.element = this.element.parent().data(
				"resizable", this.element.data('resizable')
			);

			this.elementIsWrapper = true;

			//Move margins to the wrapper
			this.element.css({ marginLeft: this.originalElement.css("marginLeft"), marginTop: this.originalElement.css("marginTop"), marginRight: this.originalElement.css("marginRight"), marginBottom: this.originalElement.css("marginBottom") });
			this.originalElement.css({ marginLeft: 0, marginTop: 0, marginRight: 0, marginBottom: 0});

			//Prevent Safari textarea resize
			this.originalResizeStyle = this.originalElement.css('resize');
			this.originalElement.css('resize', 'none');

			//Push the actual element to our proportionallyResize internal array
			this._proportionallyResizeElements.push(this.originalElement.css({ position: 'static', zoom: 1, display: 'block' }));

			// avoid IE jump (hard set the margin)
			this.originalElement.css({ margin: this.originalElement.css('margin') });

			// fix handlers offset
			this._proportionallyResize();

		}

		this.handles = o.handles || (!$('.ui-resizable-handle', this.element).length ? "e,s,se" : { n: '.ui-resizable-n', e: '.ui-resizable-e', s: '.ui-resizable-s', w: '.ui-resizable-w', se: '.ui-resizable-se', sw: '.ui-resizable-sw', ne: '.ui-resizable-ne', nw: '.ui-resizable-nw' });
		if(this.handles.constructor == String) {

			if(this.handles == 'all') this.handles = 'n,e,s,w,se,sw,ne,nw';
			var n = this.handles.split(","); this.handles = {};

			for(var i = 0; i < n.length; i++) {

				var handle = $.trim(n[i]), hname = 'ui-resizable-'+handle;
				var axis = $('<div class="ui-resizable-handle ' + hname + '"></div>');

				// increase zIndex of sw, se, ne, nw axis
				//TODO : this modifies original option
				if(/sw|se|ne|nw/.test(handle)) axis.css({ zIndex: ++o.zIndex });

				//TODO : What's going on here?
				if ('se' == handle) {
					axis.addClass('ui-icon ui-icon-gripsmall-diagonal-se');
				};

				//Insert into internal handles object and append to element
				this.handles[handle] = '.ui-resizable-'+handle;
				this.element.append(axis);
			}

		}

		this._renderAxis = function(target) {

			target = target || this.element;

			for(var i in this.handles) {

				if(this.handles[i].constructor == String)
					this.handles[i] = $(this.handles[i], this.element).show();

				//Apply pad to wrapper element, needed to fix axis position (textarea, inputs, scrolls)
				if (this.elementIsWrapper && this.originalElement[0].nodeName.match(/textarea|input|select|button/i)) {

					var axis = $(this.handles[i], this.element), padWrapper = 0;

					//Checking the correct pad and border
					padWrapper = /sw|ne|nw|se|n|s/.test(i) ? axis.outerHeight() : axis.outerWidth();

					//The padding type i have to apply...
					var padPos = [ 'padding',
						/ne|nw|n/.test(i) ? 'Top' :
						/se|sw|s/.test(i) ? 'Bottom' :
						/^e$/.test(i) ? 'Right' : 'Left' ].join("");

					target.css(padPos, padWrapper);

					this._proportionallyResize();

				}

				//TODO: What's that good for? There's not anything to be executed left
				if(!$(this.handles[i]).length)
					continue;

			}
		};

		//TODO: make renderAxis a prototype function
		this._renderAxis(this.element);

		this._handles = $('.ui-resizable-handle', this.element)
			.disableSelection();

		//Matching axis name
		this._handles.mouseover(function() {
			if (!self.resizing) {
				if (this.className)
					var axis = this.className.match(/ui-resizable-(se|sw|ne|nw|n|e|s|w)/i);
				//Axis, default = se
				self.axis = axis && axis[1] ? axis[1] : 'se';
			}
		});

		//If we want to auto hide the elements
		if (o.autoHide) {
			this._handles.hide();
			$(this.element)
				.addClass("ui-resizable-autohide")
				.hover(function() {
					$(this).removeClass("ui-resizable-autohide");
					self._handles.show();
				},
				function(){
					if (!self.resizing) {
						$(this).addClass("ui-resizable-autohide");
						self._handles.hide();
					}
				});
		}

		//Initialize the mouse interaction
		this._mouseInit();

	},

	destroy: function() {

		this._mouseDestroy();

		var _destroy = function(exp) {
			$(exp).removeClass("ui-resizable ui-resizable-disabled ui-resizable-resizing")
				.removeData("resizable").unbind(".resizable").find('.ui-resizable-handle').remove();
		};

		//TODO: Unwrap at same DOM position
		if (this.elementIsWrapper) {
			_destroy(this.element);
			var wrapper = this.element;
			wrapper.after(
				this.originalElement.css({
					position: wrapper.css('position'),
					width: wrapper.outerWidth(),
					height: wrapper.outerHeight(),
					top: wrapper.css('top'),
					left: wrapper.css('left')
				})
			).remove();
		}

		this.originalElement.css('resize', this.originalResizeStyle);
		_destroy(this.originalElement);

		return this;
	},

	_mouseCapture: function(event) {
		var handle = false;
		for (var i in this.handles) {
			if ($(this.handles[i])[0] == event.target) {
				handle = true;
			}
		}

		return !this.options.disabled && handle;
	},

	_mouseStart: function(event) {

		var o = this.options, iniPos = this.element.position(), el = this.element;

		this.resizing = true;
		this.documentScroll = { top: $(document).scrollTop(), left: $(document).scrollLeft() };

		// bugfix for http://dev.jquery.com/ticket/1749
		if (el.is('.ui-draggable') || (/absolute/).test(el.css('position'))) {
			el.css({ position: 'absolute', top: iniPos.top, left: iniPos.left });
		}

		//Opera fixing relative position
		if ($.browser.opera && (/relative/).test(el.css('position')))
			el.css({ position: 'relative', top: 'auto', left: 'auto' });

		this._renderProxy();

		var curleft = num(this.helper.css('left')), curtop = num(this.helper.css('top'));

		if (o.containment) {
			curleft += $(o.containment).scrollLeft() || 0;
			curtop += $(o.containment).scrollTop() || 0;
		}

		//Store needed variables
		this.offset = this.helper.offset();
		this.position = { left: curleft, top: curtop };
		this.size = this._helper ? { width: el.outerWidth(), height: el.outerHeight() } : { width: el.width(), height: el.height() };
		this.originalSize = this._helper ? { width: el.outerWidth(), height: el.outerHeight() } : { width: el.width(), height: el.height() };
		this.originalPosition = { left: curleft, top: curtop };
		this.sizeDiff = { width: el.outerWidth() - el.width(), height: el.outerHeight() - el.height() };
		this.originalMousePosition = { left: event.pageX, top: event.pageY };

		//Aspect Ratio
		this.aspectRatio = (typeof o.aspectRatio == 'number') ? o.aspectRatio : ((this.originalSize.width / this.originalSize.height) || 1);

	    var cursor = $('.ui-resizable-' + this.axis).css('cursor');
	    $('body').css('cursor', cursor == 'auto' ? this.axis + '-resize' : cursor);

		el.addClass("ui-resizable-resizing");
		this._propagate("start", event);
		return true;
	},

	_mouseDrag: function(event) {

		//Increase performance, avoid regex
		var el = this.helper, o = this.options, props = {},
			self = this, smp = this.originalMousePosition, a = this.axis;

		var dx = (event.pageX-smp.left)||0, dy = (event.pageY-smp.top)||0;
		var trigger = this._change[a];
		if (!trigger) return false;

		// Calculate the attrs that will be change
		var data = trigger.apply(this, [event, dx, dy]), ie6 = $.browser.msie && $.browser.version < 7, csdif = this.sizeDiff;

		if (this._aspectRatio || event.shiftKey)
			data = this._updateRatio(data, event);

		data = this._respectSize(data, event);

		// plugins callbacks need to be called first
		this._propagate("resize", event);

		el.css({
			top: this.position.top + "px", left: this.position.left + "px",
			width: this.size.width + "px", height: this.size.height + "px"
		});

		if (!this._helper && this._proportionallyResizeElements.length)
			this._proportionallyResize();

		this._updateCache(data);

		// calling the user callback at the end
		this._trigger('resize', event, this.ui());

		return false;
	},

	_mouseStop: function(event) {

		this.resizing = false;
		var o = this.options, self = this;

		if(this._helper) {
			var pr = this._proportionallyResizeElements, ista = pr.length && (/textarea/i).test(pr[0].nodeName),
						soffseth = ista && $.ui.hasScroll(pr[0], 'left') /* TODO - jump height */ ? 0 : self.sizeDiff.height,
							soffsetw = ista ? 0 : self.sizeDiff.width;

			var s = { width: (self.size.width - soffsetw), height: (self.size.height - soffseth) },
				left = (parseInt(self.element.css('left'), 10) + (self.position.left - self.originalPosition.left)) || null,
				top = (parseInt(self.element.css('top'), 10) + (self.position.top - self.originalPosition.top)) || null;

			if (!o.animate)
				this.element.css($.extend(s, { top: top, left: left }));

			self.helper.height(self.size.height);
			self.helper.width(self.size.width);

			if (this._helper && !o.animate) this._proportionallyResize();
		}

		$('body').css('cursor', 'auto');

		this.element.removeClass("ui-resizable-resizing");

		this._propagate("stop", event);

		if (this._helper) this.helper.remove();
		return false;

	},

	_updateCache: function(data) {
		var o = this.options;
		this.offset = this.helper.offset();
		if (isNumber(data.left)) this.position.left = data.left;
		if (isNumber(data.top)) this.position.top = data.top;
		if (isNumber(data.height)) this.size.height = data.height;
		if (isNumber(data.width)) this.size.width = data.width;
	},

	_updateRatio: function(data, event) {

		var o = this.options, cpos = this.position, csize = this.size, a = this.axis;

		if (data.height) data.width = (csize.height * this.aspectRatio);
		else if (data.width) data.height = (csize.width / this.aspectRatio);

		if (a == 'sw') {
			data.left = cpos.left + (csize.width - data.width);
			data.top = null;
		}
		if (a == 'nw') {
			data.top = cpos.top + (csize.height - data.height);
			data.left = cpos.left + (csize.width - data.width);
		}

		return data;
	},

	_respectSize: function(data, event) {

		var el = this.helper, o = this.options, pRatio = this._aspectRatio || event.shiftKey, a = this.axis,
				ismaxw = isNumber(data.width) && o.maxWidth && (o.maxWidth < data.width), ismaxh = isNumber(data.height) && o.maxHeight && (o.maxHeight < data.height),
					isminw = isNumber(data.width) && o.minWidth && (o.minWidth > data.width), isminh = isNumber(data.height) && o.minHeight && (o.minHeight > data.height);

		if (isminw) data.width = o.minWidth;
		if (isminh) data.height = o.minHeight;
		if (ismaxw) data.width = o.maxWidth;
		if (ismaxh) data.height = o.maxHeight;

		var dw = this.originalPosition.left + this.originalSize.width, dh = this.position.top + this.size.height;
		var cw = /sw|nw|w/.test(a), ch = /nw|ne|n/.test(a);

		if (isminw && cw) data.left = dw - o.minWidth;
		if (ismaxw && cw) data.left = dw - o.maxWidth;
		if (isminh && ch)	data.top = dh - o.minHeight;
		if (ismaxh && ch)	data.top = dh - o.maxHeight;

		// fixing jump error on top/left - bug #2330
		var isNotwh = !data.width && !data.height;
		if (isNotwh && !data.left && data.top) data.top = null;
		else if (isNotwh && !data.top && data.left) data.left = null;

		return data;
	},

	_proportionallyResize: function() {

		var o = this.options;
		if (!this._proportionallyResizeElements.length) return;
		var element = this.helper || this.element;

		for (var i=0; i < this._proportionallyResizeElements.length; i++) {

			var prel = this._proportionallyResizeElements[i];

			if (!this.borderDif) {
				var b = [prel.css('borderTopWidth'), prel.css('borderRightWidth'), prel.css('borderBottomWidth'), prel.css('borderLeftWidth')],
					p = [prel.css('paddingTop'), prel.css('paddingRight'), prel.css('paddingBottom'), prel.css('paddingLeft')];

				this.borderDif = $.map(b, function(v, i) {
					var border = parseInt(v,10)||0, padding = parseInt(p[i],10)||0;
					return border + padding;
				});
			}

			if ($.browser.msie && !(!($(element).is(':hidden') || $(element).parents(':hidden').length)))
				continue;

			prel.css({
				height: (element.height() - this.borderDif[0] - this.borderDif[2]) || 0,
				width: (element.width() - this.borderDif[1] - this.borderDif[3]) || 0
			});

		};

	},

	_renderProxy: function() {

		var el = this.element, o = this.options;
		this.elementOffset = el.offset();

		if(this._helper) {

			this.helper = this.helper || $('<div style="overflow:hidden;"></div>');

			// fix ie6 offset TODO: This seems broken
			var ie6 = $.browser.msie && $.browser.version < 7, ie6offset = (ie6 ? 1 : 0),
			pxyoffset = ( ie6 ? 2 : -1 );

			this.helper.addClass(this._helper).css({
				width: this.element.outerWidth() + pxyoffset,
				height: this.element.outerHeight() + pxyoffset,
				position: 'absolute',
				left: this.elementOffset.left - ie6offset +'px',
				top: this.elementOffset.top - ie6offset +'px',
				zIndex: ++o.zIndex //TODO: Don't modify option
			});

			this.helper
				.appendTo("body")
				.disableSelection();

		} else {
			this.helper = this.element;
		}

	},

	_change: {
		e: function(event, dx, dy) {
			return { width: this.originalSize.width + dx };
		},
		w: function(event, dx, dy) {
			var o = this.options, cs = this.originalSize, sp = this.originalPosition;
			return { left: sp.left + dx, width: cs.width - dx };
		},
		n: function(event, dx, dy) {
			var o = this.options, cs = this.originalSize, sp = this.originalPosition;
			return { top: sp.top + dy, height: cs.height - dy };
		},
		s: function(event, dx, dy) {
			return { height: this.originalSize.height + dy };
		},
		se: function(event, dx, dy) {
			return $.extend(this._change.s.apply(this, arguments), this._change.e.apply(this, [event, dx, dy]));
		},
		sw: function(event, dx, dy) {
			return $.extend(this._change.s.apply(this, arguments), this._change.w.apply(this, [event, dx, dy]));
		},
		ne: function(event, dx, dy) {
			return $.extend(this._change.n.apply(this, arguments), this._change.e.apply(this, [event, dx, dy]));
		},
		nw: function(event, dx, dy) {
			return $.extend(this._change.n.apply(this, arguments), this._change.w.apply(this, [event, dx, dy]));
		}
	},

	_propagate: function(n, event) {
		$.ui.plugin.call(this, n, [event, this.ui()]);
		(n != "resize" && this._trigger(n, event, this.ui()));
	},

	plugins: {},

	ui: function() {
		return {
			originalElement: this.originalElement,
			element: this.element,
			helper: this.helper,
			position: this.position,
			size: this.size,
			originalSize: this.originalSize,
			originalPosition: this.originalPosition
		};
	}

});

$.extend($.ui.resizable, {
	version: "1.8"
});

/*
 * Resizable Extensions
 */

$.ui.plugin.add("resizable", "alsoResize", {

	start: function(event, ui) {

		var self = $(this).data("resizable"), o = self.options;

		var _store = function(exp) {
			$(exp).each(function() {
				$(this).data("resizable-alsoresize", {
					width: parseInt($(this).width(), 10), height: parseInt($(this).height(), 10),
					left: parseInt($(this).css('left'), 10), top: parseInt($(this).css('top'), 10)
				});
			});
		};

		if (typeof(o.alsoResize) == 'object' && !o.alsoResize.parentNode) {
			if (o.alsoResize.length) { o.alsoResize = o.alsoResize[0];	_store(o.alsoResize); }
			else { $.each(o.alsoResize, function(exp, c) { _store(exp); }); }
		}else{
			_store(o.alsoResize);
		}
	},

	resize: function(event, ui){
		var self = $(this).data("resizable"), o = self.options, os = self.originalSize, op = self.originalPosition;

		var delta = {
			height: (self.size.height - os.height) || 0, width: (self.size.width - os.width) || 0,
			top: (self.position.top - op.top) || 0, left: (self.position.left - op.left) || 0
		},

		_alsoResize = function(exp, c) {
			$(exp).each(function() {
				var el = $(this), start = $(this).data("resizable-alsoresize"), style = {}, css = c && c.length ? c : ['width', 'height', 'top', 'left'];

				$.each(css || ['width', 'height', 'top', 'left'], function(i, prop) {
					var sum = (start[prop]||0) + (delta[prop]||0);
					if (sum && sum >= 0)
						style[prop] = sum || null;
				});

				//Opera fixing relative position
				if (/relative/.test(el.css('position')) && $.browser.opera) {
					self._revertToRelativePosition = true;
					el.css({ position: 'absolute', top: 'auto', left: 'auto' });
				}

				el.css(style);
			});
		};

		if (typeof(o.alsoResize) == 'object' && !o.alsoResize.nodeType) {
			$.each(o.alsoResize, function(exp, c) { _alsoResize(exp, c); });
		}else{
			_alsoResize(o.alsoResize);
		}
	},

	stop: function(event, ui){
		var self = $(this).data("resizable");

		//Opera fixing relative position
		if (self._revertToRelativePosition && $.browser.opera) {
			self._revertToRelativePosition = false;
			el.css({ position: 'relative' });
		}

		$(this).removeData("resizable-alsoresize-start");
	}
});

$.ui.plugin.add("resizable", "animate", {

	stop: function(event, ui) {
		var self = $(this).data("resizable"), o = self.options;

		var pr = self._proportionallyResizeElements, ista = pr.length && (/textarea/i).test(pr[0].nodeName),
					soffseth = ista && $.ui.hasScroll(pr[0], 'left') /* TODO - jump height */ ? 0 : self.sizeDiff.height,
						soffsetw = ista ? 0 : self.sizeDiff.width;

		var style = { width: (self.size.width - soffsetw), height: (self.size.height - soffseth) },
					left = (parseInt(self.element.css('left'), 10) + (self.position.left - self.originalPosition.left)) || null,
						top = (parseInt(self.element.css('top'), 10) + (self.position.top - self.originalPosition.top)) || null;

		self.element.animate(
			$.extend(style, top && left ? { top: top, left: left } : {}), {
				duration: o.animateDuration,
				easing: o.animateEasing,
				step: function() {

					var data = {
						width: parseInt(self.element.css('width'), 10),
						height: parseInt(self.element.css('height'), 10),
						top: parseInt(self.element.css('top'), 10),
						left: parseInt(self.element.css('left'), 10)
					};

					if (pr && pr.length) $(pr[0]).css({ width: data.width, height: data.height });

					// propagating resize, and updating values for each animation step
					self._updateCache(data);
					self._propagate("resize", event);

				}
			}
		);
	}

});

$.ui.plugin.add("resizable", "containment", {

	start: function(event, ui) {
		var self = $(this).data("resizable"), o = self.options, el = self.element;
		var oc = o.containment,	ce = (oc instanceof $) ? oc.get(0) : (/parent/.test(oc)) ? el.parent().get(0) : oc;
		if (!ce) return;

		self.containerElement = $(ce);

		if (/document/.test(oc) || oc == document) {
			self.containerOffset = { left: 0, top: 0 };
			self.containerPosition = { left: 0, top: 0 };

			self.parentData = {
				element: $(document), left: 0, top: 0,
				width: $(document).width(), height: $(document).height() || document.body.parentNode.scrollHeight
			};
		}

		// i'm a node, so compute top, left, right, bottom
		else {
			var element = $(ce), p = [];
			$([ "Top", "Right", "Left", "Bottom" ]).each(function(i, name) { p[i] = num(element.css("padding" + name)); });

			self.containerOffset = element.offset();
			self.containerPosition = element.position();
			self.containerSize = { height: (element.innerHeight() - p[3]), width: (element.innerWidth() - p[1]) };

			var co = self.containerOffset, ch = self.containerSize.height,	cw = self.containerSize.width,
						width = ($.ui.hasScroll(ce, "left") ? ce.scrollWidth : cw ), height = ($.ui.hasScroll(ce) ? ce.scrollHeight : ch);

			self.parentData = {
				element: ce, left: co.left, top: co.top, width: width, height: height
			};
		}
	},

	resize: function(event, ui) {
		var self = $(this).data("resizable"), o = self.options,
				ps = self.containerSize, co = self.containerOffset, cs = self.size, cp = self.position,
				pRatio = self._aspectRatio || event.shiftKey, cop = { top:0, left:0 }, ce = self.containerElement;

		if (ce[0] != document && (/static/).test(ce.css('position'))) cop = co;

		if (cp.left < (self._helper ? co.left : 0)) {
			self.size.width = self.size.width + (self._helper ? (self.position.left - co.left) : (self.position.left - cop.left));
			if (pRatio) self.size.height = self.size.width / o.aspectRatio;
			self.position.left = o.helper ? co.left : 0;
		}

		if (cp.top < (self._helper ? co.top : 0)) {
			self.size.height = self.size.height + (self._helper ? (self.position.top - co.top) : self.position.top);
			if (pRatio) self.size.width = self.size.height * o.aspectRatio;
			self.position.top = self._helper ? co.top : 0;
		}

		self.offset.left = self.parentData.left+self.position.left;
		self.offset.top = self.parentData.top+self.position.top;

		var woset = Math.abs( (self._helper ? self.offset.left - cop.left : (self.offset.left - cop.left)) + self.sizeDiff.width ),
					hoset = Math.abs( (self._helper ? self.offset.top - cop.top : (self.offset.top - co.top)) + self.sizeDiff.height );

		var isParent = self.containerElement.get(0) == self.element.parent().get(0),
		    isOffsetRelative = /relative|absolute/.test(self.containerElement.css('position'));

		if(isParent && isOffsetRelative) woset -= self.parentData.left;

		if (woset + self.size.width >= self.parentData.width) {
			self.size.width = self.parentData.width - woset;
			if (pRatio) self.size.height = self.size.width / self.aspectRatio;
		}

		if (hoset + self.size.height >= self.parentData.height) {
			self.size.height = self.parentData.height - hoset;
			if (pRatio) self.size.width = self.size.height * self.aspectRatio;
		}
	},

	stop: function(event, ui){
		var self = $(this).data("resizable"), o = self.options, cp = self.position,
				co = self.containerOffset, cop = self.containerPosition, ce = self.containerElement;

		var helper = $(self.helper), ho = helper.offset(), w = helper.outerWidth() - self.sizeDiff.width, h = helper.outerHeight() - self.sizeDiff.height;

		if (self._helper && !o.animate && (/relative/).test(ce.css('position')))
			$(this).css({ left: ho.left - cop.left - co.left, width: w, height: h });

		if (self._helper && !o.animate && (/static/).test(ce.css('position')))
			$(this).css({ left: ho.left - cop.left - co.left, width: w, height: h });

	}
});

$.ui.plugin.add("resizable", "ghost", {

	start: function(event, ui) {

		var self = $(this).data("resizable"), o = self.options, cs = self.size;

		self.ghost = self.originalElement.clone();
		self.ghost
			.css({ opacity: .25, display: 'block', position: 'relative', height: cs.height, width: cs.width, margin: 0, left: 0, top: 0 })
			.addClass('ui-resizable-ghost')
			.addClass(typeof o.ghost == 'string' ? o.ghost : '');

		self.ghost.appendTo(self.helper);

	},

	resize: function(event, ui){
		var self = $(this).data("resizable"), o = self.options;
		if (self.ghost) self.ghost.css({ position: 'relative', height: self.size.height, width: self.size.width });
	},

	stop: function(event, ui){
		var self = $(this).data("resizable"), o = self.options;
		if (self.ghost && self.helper) self.helper.get(0).removeChild(self.ghost.get(0));
	}

});

$.ui.plugin.add("resizable", "grid", {

	resize: function(event, ui) {
		var self = $(this).data("resizable"), o = self.options, cs = self.size, os = self.originalSize, op = self.originalPosition, a = self.axis, ratio = o._aspectRatio || event.shiftKey;
		o.grid = typeof o.grid == "number" ? [o.grid, o.grid] : o.grid;
		var ox = Math.round((cs.width - os.width) / (o.grid[0]||1)) * (o.grid[0]||1), oy = Math.round((cs.height - os.height) / (o.grid[1]||1)) * (o.grid[1]||1);

		if (/^(se|s|e)$/.test(a)) {
			self.size.width = os.width + ox;
			self.size.height = os.height + oy;
		}
		else if (/^(ne)$/.test(a)) {
			self.size.width = os.width + ox;
			self.size.height = os.height + oy;
			self.position.top = op.top - oy;
		}
		else if (/^(sw)$/.test(a)) {
			self.size.width = os.width + ox;
			self.size.height = os.height + oy;
			self.position.left = op.left - ox;
		}
		else {
			self.size.width = os.width + ox;
			self.size.height = os.height + oy;
			self.position.top = op.top - oy;
			self.position.left = op.left - ox;
		}
	}

});

var num = function(v) {
	return parseInt(v, 10) || 0;
};

var isNumber = function(value) {
	return !isNaN(parseInt(value, 10));
};

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
 * jQuery UI Draggable 1.8
 *
 * Copyright (c) 2010 AUTHORS.txt (http://jqueryui.com/about)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * http://docs.jquery.com/UI/Draggables
 *
 * Depends:
 *	jquery.ui.core.js
 *	jquery.ui.mouse.js
 *	jquery.ui.widget.js
 */
(function($) {

$.widget("ui.draggable", $.ui.mouse, {
	widgetEventPrefix: "drag",
	options: {
		addClasses: true,
		appendTo: "parent",
		axis: false,
		connectToSortable: false,
		containment: false,
		cursor: "auto",
		cursorAt: false,
		grid: false,
		handle: false,
		helper: "original",
		iframeFix: false,
		opacity: false,
		refreshPositions: false,
		revert: false,
		revertDuration: 500,
		scope: "default",
		scroll: true,
		scrollSensitivity: 20,
		scrollSpeed: 20,
		snap: false,
		snapMode: "both",
		snapTolerance: 20,
		stack: false,
		zIndex: false
	},
	_create: function() {

		if (this.options.helper == 'original' && !(/^(?:r|a|f)/).test(this.element.css("position")))
			this.element[0].style.position = 'relative';

		(this.options.addClasses && this.element.addClass("ui-draggable"));
		(this.options.disabled && this.element.addClass("ui-draggable-disabled"));

		this._mouseInit();

	},

	destroy: function() {
		if(!this.element.data('draggable')) return;
		this.element
			.removeData("draggable")
			.unbind(".draggable")
			.removeClass("ui-draggable"
				+ " ui-draggable-dragging"
				+ " ui-draggable-disabled");
		this._mouseDestroy();

		return this;
	},

	_mouseCapture: function(event) {

		var o = this.options;

		// among others, prevent a drag on a resizable-handle
		if (this.helper || o.disabled || $(event.target).is('.ui-resizable-handle'))
			return false;

		//Quit if we're not on a valid handle
		this.handle = this._getHandle(event);
		if (!this.handle)
			return false;

		return true;

	},

	_mouseStart: function(event) {

		var o = this.options;

		//Create and append the visible helper
		this.helper = this._createHelper(event);

		//Cache the helper size
		this._cacheHelperProportions();

		//If ddmanager is used for droppables, set the global draggable
		if($.ui.ddmanager)
			$.ui.ddmanager.current = this;

		/*
		 * - Position generation -
		 * This block generates everything position related - it's the core of draggables.
		 */

		//Cache the margins of the original element
		this._cacheMargins();

		//Store the helper's css position
		this.cssPosition = this.helper.css("position");
		this.scrollParent = this.helper.scrollParent();

		//The element's absolute position on the page minus margins
		this.offset = this.positionAbs = this.element.offset();
		this.offset = {
			top: this.offset.top - this.margins.top,
			left: this.offset.left - this.margins.left
		};

		$.extend(this.offset, {
			click: { //Where the click happened, relative to the element
				left: event.pageX - this.offset.left,
				top: event.pageY - this.offset.top
			},
			parent: this._getParentOffset(),
			relative: this._getRelativeOffset() //This is a relative to absolute position minus the actual position calculation - only used for relative positioned helper
		});

		//Generate the original position
		this.originalPosition = this.position = this._generatePosition(event);
		this.originalPageX = event.pageX;
		this.originalPageY = event.pageY;

		//Adjust the mouse offset relative to the helper if 'cursorAt' is supplied
		(o.cursorAt && this._adjustOffsetFromHelper(o.cursorAt));

		//Set a containment if given in the options
		if(o.containment)
			this._setContainment();

		//Trigger event + callbacks
		if(this._trigger("start", event) === false) {
			this._clear();
			return false;
		}

		//Recache the helper size
		this._cacheHelperProportions();

		//Prepare the droppable offsets
		if ($.ui.ddmanager && !o.dropBehaviour)
			$.ui.ddmanager.prepareOffsets(this, event);

		this.helper.addClass("ui-draggable-dragging");
		this._mouseDrag(event, true); //Execute the drag once - this causes the helper not to be visible before getting its correct position
		return true;
	},

	_mouseDrag: function(event, noPropagation) {

		//Compute the helpers position
		this.position = this._generatePosition(event);
		this.positionAbs = this._convertPositionTo("absolute");

		//Call plugins and callbacks and use the resulting position if something is returned
		if (!noPropagation) {
			var ui = this._uiHash();
			if(this._trigger('drag', event, ui) === false) {
				this._mouseUp({});
				return false;
			}
			this.position = ui.position;
		}

		if(!this.options.axis || this.options.axis != "y") this.helper[0].style.left = this.position.left+'px';
		if(!this.options.axis || this.options.axis != "x") this.helper[0].style.top = this.position.top+'px';
		if($.ui.ddmanager) $.ui.ddmanager.drag(this, event);

		return false;
	},

	_mouseStop: function(event) {

		//If we are using droppables, inform the manager about the drop
		var dropped = false;
		if ($.ui.ddmanager && !this.options.dropBehaviour)
			dropped = $.ui.ddmanager.drop(this, event);

		//if a drop comes from outside (a sortable)
		if(this.dropped) {
			dropped = this.dropped;
			this.dropped = false;
		}
		
		//if the original element is removed, don't bother to continue
		if(!this.element[0] || !this.element[0].parentNode)
			return false;

		if((this.options.revert == "invalid" && !dropped) || (this.options.revert == "valid" && dropped) || this.options.revert === true || ($.isFunction(this.options.revert) && this.options.revert.call(this.element, dropped))) {
			var self = this;
			$(this.helper).animate(this.originalPosition, parseInt(this.options.revertDuration, 10), function() {
				if(self._trigger("stop", event) !== false) {
					self._clear();
				}
			});
		} else {
			if(this._trigger("stop", event) !== false) {
				this._clear();
			}
		}

		return false;
	},
	
	cancel: function() {
		
		if(this.helper.is(".ui-draggable-dragging")) {
			this._mouseUp({});
		} else {
			this._clear();
		}
		
		return this;
		
	},

	_getHandle: function(event) {

		var handle = !this.options.handle || !$(this.options.handle, this.element).length ? true : false;
		$(this.options.handle, this.element)
			.find("*")
			.andSelf()
			.each(function() {
				if(this == event.target) handle = true;
			});

		return handle;

	},

	_createHelper: function(event) {

		var o = this.options;
		var helper = $.isFunction(o.helper) ? $(o.helper.apply(this.element[0], [event])) : (o.helper == 'clone' ? this.element.clone() : this.element);

		if(!helper.parents('body').length)
			helper.appendTo((o.appendTo == 'parent' ? this.element[0].parentNode : o.appendTo));

		if(helper[0] != this.element[0] && !(/(fixed|absolute)/).test(helper.css("position")))
			helper.css("position", "absolute");

		return helper;

	},

	_adjustOffsetFromHelper: function(obj) {
		if (typeof obj == 'string') {
			obj = obj.split(' ');
		}
		if ($.isArray(obj)) {
			obj = {left: +obj[0], top: +obj[1] || 0};
		}
		if ('left' in obj) {
			this.offset.click.left = obj.left + this.margins.left;
		}
		if ('right' in obj) {
			this.offset.click.left = this.helperProportions.width - obj.right + this.margins.left;
		}
		if ('top' in obj) {
			this.offset.click.top = obj.top + this.margins.top;
		}
		if ('bottom' in obj) {
			this.offset.click.top = this.helperProportions.height - obj.bottom + this.margins.top;
		}
	},

	_getParentOffset: function() {

		//Get the offsetParent and cache its position
		this.offsetParent = this.helper.offsetParent();
		var po = this.offsetParent.offset();

		// This is a special case where we need to modify a offset calculated on start, since the following happened:
		// 1. The position of the helper is absolute, so it's position is calculated based on the next positioned parent
		// 2. The actual offset parent is a child of the scroll parent, and the scroll parent isn't the document, which means that
		//    the scroll is included in the initial calculation of the offset of the parent, and never recalculated upon drag
		if(this.cssPosition == 'absolute' && this.scrollParent[0] != document && $.ui.contains(this.scrollParent[0], this.offsetParent[0])) {
			po.left += this.scrollParent.scrollLeft();
			po.top += this.scrollParent.scrollTop();
		}

		if((this.offsetParent[0] == document.body) //This needs to be actually done for all browsers, since pageX/pageY includes this information
		|| (this.offsetParent[0].tagName && this.offsetParent[0].tagName.toLowerCase() == 'html' && $.browser.msie)) //Ugly IE fix
			po = { top: 0, left: 0 };

		return {
			top: po.top + (parseInt(this.offsetParent.css("borderTopWidth"),10) || 0),
			left: po.left + (parseInt(this.offsetParent.css("borderLeftWidth"),10) || 0)
		};

	},

	_getRelativeOffset: function() {

		if(this.cssPosition == "relative") {
			var p = this.element.position();
			return {
				top: p.top - (parseInt(this.helper.css("top"),10) || 0) + this.scrollParent.scrollTop(),
				left: p.left - (parseInt(this.helper.css("left"),10) || 0) + this.scrollParent.scrollLeft()
			};
		} else {
			return { top: 0, left: 0 };
		}

	},

	_cacheMargins: function() {
		this.margins = {
			left: (parseInt(this.element.css("marginLeft"),10) || 0),
			top: (parseInt(this.element.css("marginTop"),10) || 0)
		};
	},

	_cacheHelperProportions: function() {
		this.helperProportions = {
			width: this.helper.outerWidth(),
			height: this.helper.outerHeight()
		};
	},

	_setContainment: function() {

		var o = this.options;
		if(o.containment == 'parent') o.containment = this.helper[0].parentNode;
		if(o.containment == 'document' || o.containment == 'window') this.containment = [
			0 - this.offset.relative.left - this.offset.parent.left,
			0 - this.offset.relative.top - this.offset.parent.top,
			$(o.containment == 'document' ? document : window).width() - this.helperProportions.width - this.margins.left,
			($(o.containment == 'document' ? document : window).height() || document.body.parentNode.scrollHeight) - this.helperProportions.height - this.margins.top
		];

		if(!(/^(document|window|parent)$/).test(o.containment) && o.containment.constructor != Array) {
			var ce = $(o.containment)[0]; if(!ce) return;
			var co = $(o.containment).offset();
			var over = ($(ce).css("overflow") != 'hidden');

			this.containment = [
				co.left + (parseInt($(ce).css("borderLeftWidth"),10) || 0) + (parseInt($(ce).css("paddingLeft"),10) || 0) - this.margins.left,
				co.top + (parseInt($(ce).css("borderTopWidth"),10) || 0) + (parseInt($(ce).css("paddingTop"),10) || 0) - this.margins.top,
				co.left+(over ? Math.max(ce.scrollWidth,ce.offsetWidth) : ce.offsetWidth) - (parseInt($(ce).css("borderLeftWidth"),10) || 0) - (parseInt($(ce).css("paddingRight"),10) || 0) - this.helperProportions.width - this.margins.left,
				co.top+(over ? Math.max(ce.scrollHeight,ce.offsetHeight) : ce.offsetHeight) - (parseInt($(ce).css("borderTopWidth"),10) || 0) - (parseInt($(ce).css("paddingBottom"),10) || 0) - this.helperProportions.height - this.margins.top
			];
		} else if(o.containment.constructor == Array) {
			this.containment = o.containment;
		}

	},

	_convertPositionTo: function(d, pos) {

		if(!pos) pos = this.position;
		var mod = d == "absolute" ? 1 : -1;
		var o = this.options, scroll = this.cssPosition == 'absolute' && !(this.scrollParent[0] != document && $.ui.contains(this.scrollParent[0], this.offsetParent[0])) ? this.offsetParent : this.scrollParent, scrollIsRootNode = (/(html|body)/i).test(scroll[0].tagName);

		return {
			top: (
				pos.top																	// The absolute mouse position
				+ this.offset.relative.top * mod										// Only for relative positioned nodes: Relative offset from element to offset parent
				+ this.offset.parent.top * mod											// The offsetParent's offset without borders (offset + border)
				- ($.browser.safari && $.browser.version < 526 && this.cssPosition == 'fixed' ? 0 : ( this.cssPosition == 'fixed' ? -this.scrollParent.scrollTop() : ( scrollIsRootNode ? 0 : scroll.scrollTop() ) ) * mod)
			),
			left: (
				pos.left																// The absolute mouse position
				+ this.offset.relative.left * mod										// Only for relative positioned nodes: Relative offset from element to offset parent
				+ this.offset.parent.left * mod											// The offsetParent's offset without borders (offset + border)
				- ($.browser.safari && $.browser.version < 526 && this.cssPosition == 'fixed' ? 0 : ( this.cssPosition == 'fixed' ? -this.scrollParent.scrollLeft() : scrollIsRootNode ? 0 : scroll.scrollLeft() ) * mod)
			)
		};

	},

	_generatePosition: function(event) {

		var o = this.options, scroll = this.cssPosition == 'absolute' && !(this.scrollParent[0] != document && $.ui.contains(this.scrollParent[0], this.offsetParent[0])) ? this.offsetParent : this.scrollParent, scrollIsRootNode = (/(html|body)/i).test(scroll[0].tagName);
		var pageX = event.pageX;
		var pageY = event.pageY;

		/*
		 * - Position constraining -
		 * Constrain the position to a mix of grid, containment.
		 */

		if(this.originalPosition) { //If we are not dragging yet, we won't check for options

			if(this.containment) {
				if(event.pageX - this.offset.click.left < this.containment[0]) pageX = this.containment[0] + this.offset.click.left;
				if(event.pageY - this.offset.click.top < this.containment[1]) pageY = this.containment[1] + this.offset.click.top;
				if(event.pageX - this.offset.click.left > this.containment[2]) pageX = this.containment[2] + this.offset.click.left;
				if(event.pageY - this.offset.click.top > this.containment[3]) pageY = this.containment[3] + this.offset.click.top;
			}

			if(o.grid) {
				var top = this.originalPageY + Math.round((pageY - this.originalPageY) / o.grid[1]) * o.grid[1];
				pageY = this.containment ? (!(top - this.offset.click.top < this.containment[1] || top - this.offset.click.top > this.containment[3]) ? top : (!(top - this.offset.click.top < this.containment[1]) ? top - o.grid[1] : top + o.grid[1])) : top;

				var left = this.originalPageX + Math.round((pageX - this.originalPageX) / o.grid[0]) * o.grid[0];
				pageX = this.containment ? (!(left - this.offset.click.left < this.containment[0] || left - this.offset.click.left > this.containment[2]) ? left : (!(left - this.offset.click.left < this.containment[0]) ? left - o.grid[0] : left + o.grid[0])) : left;
			}

		}

		return {
			top: (
				pageY																// The absolute mouse position
				- this.offset.click.top													// Click offset (relative to the element)
				- this.offset.relative.top												// Only for relative positioned nodes: Relative offset from element to offset parent
				- this.offset.parent.top												// The offsetParent's offset without borders (offset + border)
				+ ($.browser.safari && $.browser.version < 526 && this.cssPosition == 'fixed' ? 0 : ( this.cssPosition == 'fixed' ? -this.scrollParent.scrollTop() : ( scrollIsRootNode ? 0 : scroll.scrollTop() ) ))
			),
			left: (
				pageX																// The absolute mouse position
				- this.offset.click.left												// Click offset (relative to the element)
				- this.offset.relative.left												// Only for relative positioned nodes: Relative offset from element to offset parent
				- this.offset.parent.left												// The offsetParent's offset without borders (offset + border)
				+ ($.browser.safari && $.browser.version < 526 && this.cssPosition == 'fixed' ? 0 : ( this.cssPosition == 'fixed' ? -this.scrollParent.scrollLeft() : scrollIsRootNode ? 0 : scroll.scrollLeft() ))
			)
		};

	},

	_clear: function() {
		this.helper.removeClass("ui-draggable-dragging");
		if(this.helper[0] != this.element[0] && !this.cancelHelperRemoval) this.helper.remove();
		//if($.ui.ddmanager) $.ui.ddmanager.current = null;
		this.helper = null;
		this.cancelHelperRemoval = false;
	},

	// From now on bulk stuff - mainly helpers

	_trigger: function(type, event, ui) {
		ui = ui || this._uiHash();
		$.ui.plugin.call(this, type, [event, ui]);
		if(type == "drag") this.positionAbs = this._convertPositionTo("absolute"); //The absolute position has to be recalculated after plugins
		return $.Widget.prototype._trigger.call(this, type, event, ui);
	},

	plugins: {},

	_uiHash: function(event) {
		return {
			helper: this.helper,
			position: this.position,
			originalPosition: this.originalPosition,
			offset: this.positionAbs
		};
	}

});

$.extend($.ui.draggable, {
	version: "1.8"
});

$.ui.plugin.add("draggable", "connectToSortable", {
	start: function(event, ui) {

		var inst = $(this).data("draggable"), o = inst.options,
			uiSortable = $.extend({}, ui, { item: inst.element });
		inst.sortables = [];
		$(o.connectToSortable).each(function() {
			var sortable = $.data(this, 'sortable');
			if (sortable && !sortable.options.disabled) {
				inst.sortables.push({
					instance: sortable,
					shouldRevert: sortable.options.revert
				});
				sortable._refreshItems();	//Do a one-time refresh at start to refresh the containerCache
				sortable._trigger("activate", event, uiSortable);
			}
		});

	},
	stop: function(event, ui) {

		//If we are still over the sortable, we fake the stop event of the sortable, but also remove helper
		var inst = $(this).data("draggable"),
			uiSortable = $.extend({}, ui, { item: inst.element });

		$.each(inst.sortables, function() {
			if(this.instance.isOver) {

				this.instance.isOver = 0;

				inst.cancelHelperRemoval = true; //Don't remove the helper in the draggable instance
				this.instance.cancelHelperRemoval = false; //Remove it in the sortable instance (so sortable plugins like revert still work)

				//The sortable revert is supported, and we have to set a temporary dropped variable on the draggable to support revert: 'valid/invalid'
				if(this.shouldRevert) this.instance.options.revert = true;

				//Trigger the stop of the sortable
				this.instance._mouseStop(event);

				this.instance.options.helper = this.instance.options._helper;

				//If the helper has been the original item, restore properties in the sortable
				if(inst.options.helper == 'original')
					this.instance.currentItem.css({ top: 'auto', left: 'auto' });

			} else {
				this.instance.cancelHelperRemoval = false; //Remove the helper in the sortable instance
				this.instance._trigger("deactivate", event, uiSortable);
			}

		});

	},
	drag: function(event, ui) {

		var inst = $(this).data("draggable"), self = this;

		var checkPos = function(o) {
			var dyClick = this.offset.click.top, dxClick = this.offset.click.left;
			var helperTop = this.positionAbs.top, helperLeft = this.positionAbs.left;
			var itemHeight = o.height, itemWidth = o.width;
			var itemTop = o.top, itemLeft = o.left;

			return $.ui.isOver(helperTop + dyClick, helperLeft + dxClick, itemTop, itemLeft, itemHeight, itemWidth);
		};

		$.each(inst.sortables, function(i) {
			
			//Copy over some variables to allow calling the sortable's native _intersectsWith
			this.instance.positionAbs = inst.positionAbs;
			this.instance.helperProportions = inst.helperProportions;
			this.instance.offset.click = inst.offset.click;
			
			if(this.instance._intersectsWith(this.instance.containerCache)) {

				//If it intersects, we use a little isOver variable and set it once, so our move-in stuff gets fired only once
				if(!this.instance.isOver) {

					this.instance.isOver = 1;
					//Now we fake the start of dragging for the sortable instance,
					//by cloning the list group item, appending it to the sortable and using it as inst.currentItem
					//We can then fire the start event of the sortable with our passed browser event, and our own helper (so it doesn't create a new one)
					this.instance.currentItem = $(self).clone().appendTo(this.instance.element).data("sortable-item", true);
					this.instance.options._helper = this.instance.options.helper; //Store helper option to later restore it
					this.instance.options.helper = function() { return ui.helper[0]; };

					event.target = this.instance.currentItem[0];
					this.instance._mouseCapture(event, true);
					this.instance._mouseStart(event, true, true);

					//Because the browser event is way off the new appended portlet, we modify a couple of variables to reflect the changes
					this.instance.offset.click.top = inst.offset.click.top;
					this.instance.offset.click.left = inst.offset.click.left;
					this.instance.offset.parent.left -= inst.offset.parent.left - this.instance.offset.parent.left;
					this.instance.offset.parent.top -= inst.offset.parent.top - this.instance.offset.parent.top;

					inst._trigger("toSortable", event);
					inst.dropped = this.instance.element; //draggable revert needs that
					//hack so receive/update callbacks work (mostly)
					inst.currentItem = inst.element;
					this.instance.fromOutside = inst;

				}

				//Provided we did all the previous steps, we can fire the drag event of the sortable on every draggable drag, when it intersects with the sortable
				if(this.instance.currentItem) this.instance._mouseDrag(event);

			} else {

				//If it doesn't intersect with the sortable, and it intersected before,
				//we fake the drag stop of the sortable, but make sure it doesn't remove the helper by using cancelHelperRemoval
				if(this.instance.isOver) {

					this.instance.isOver = 0;
					this.instance.cancelHelperRemoval = true;
					
					//Prevent reverting on this forced stop
					this.instance.options.revert = false;
					
					// The out event needs to be triggered independently
					this.instance._trigger('out', event, this.instance._uiHash(this.instance));
					
					this.instance._mouseStop(event, true);
					this.instance.options.helper = this.instance.options._helper;

					//Now we remove our currentItem, the list group clone again, and the placeholder, and animate the helper back to it's original size
					this.instance.currentItem.remove();
					if(this.instance.placeholder) this.instance.placeholder.remove();

					inst._trigger("fromSortable", event);
					inst.dropped = false; //draggable revert needs that
				}

			};

		});

	}
});

$.ui.plugin.add("draggable", "cursor", {
	start: function(event, ui) {
		var t = $('body'), o = $(this).data('draggable').options;
		if (t.css("cursor")) o._cursor = t.css("cursor");
		t.css("cursor", o.cursor);
	},
	stop: function(event, ui) {
		var o = $(this).data('draggable').options;
		if (o._cursor) $('body').css("cursor", o._cursor);
	}
});

$.ui.plugin.add("draggable", "iframeFix", {
	start: function(event, ui) {
		var o = $(this).data('draggable').options;
		$(o.iframeFix === true ? "iframe" : o.iframeFix).each(function() {
			$('<div class="ui-draggable-iframeFix" style="background: #fff;"></div>')
			.css({
				width: this.offsetWidth+"px", height: this.offsetHeight+"px",
				position: "absolute", opacity: "0.001", zIndex: 1000
			})
			.css($(this).offset())
			.appendTo("body");
		});
	},
	stop: function(event, ui) {
		$("div.ui-draggable-iframeFix").each(function() { this.parentNode.removeChild(this); }); //Remove frame helpers
	}
});

$.ui.plugin.add("draggable", "opacity", {
	start: function(event, ui) {
		var t = $(ui.helper), o = $(this).data('draggable').options;
		if(t.css("opacity")) o._opacity = t.css("opacity");
		t.css('opacity', o.opacity);
	},
	stop: function(event, ui) {
		var o = $(this).data('draggable').options;
		if(o._opacity) $(ui.helper).css('opacity', o._opacity);
	}
});

$.ui.plugin.add("draggable", "scroll", {
	start: function(event, ui) {
		var i = $(this).data("draggable");
		if(i.scrollParent[0] != document && i.scrollParent[0].tagName != 'HTML') i.overflowOffset = i.scrollParent.offset();
	},
	drag: function(event, ui) {

		var i = $(this).data("draggable"), o = i.options, scrolled = false;

		if(i.scrollParent[0] != document && i.scrollParent[0].tagName != 'HTML') {

			if(!o.axis || o.axis != 'x') {
				if((i.overflowOffset.top + i.scrollParent[0].offsetHeight) - event.pageY < o.scrollSensitivity)
					i.scrollParent[0].scrollTop = scrolled = i.scrollParent[0].scrollTop + o.scrollSpeed;
				else if(event.pageY - i.overflowOffset.top < o.scrollSensitivity)
					i.scrollParent[0].scrollTop = scrolled = i.scrollParent[0].scrollTop - o.scrollSpeed;
			}

			if(!o.axis || o.axis != 'y') {
				if((i.overflowOffset.left + i.scrollParent[0].offsetWidth) - event.pageX < o.scrollSensitivity)
					i.scrollParent[0].scrollLeft = scrolled = i.scrollParent[0].scrollLeft + o.scrollSpeed;
				else if(event.pageX - i.overflowOffset.left < o.scrollSensitivity)
					i.scrollParent[0].scrollLeft = scrolled = i.scrollParent[0].scrollLeft - o.scrollSpeed;
			}

		} else {

			if(!o.axis || o.axis != 'x') {
				if(event.pageY - $(document).scrollTop() < o.scrollSensitivity)
					scrolled = $(document).scrollTop($(document).scrollTop() - o.scrollSpeed);
				else if($(window).height() - (event.pageY - $(document).scrollTop()) < o.scrollSensitivity)
					scrolled = $(document).scrollTop($(document).scrollTop() + o.scrollSpeed);
			}

			if(!o.axis || o.axis != 'y') {
				if(event.pageX - $(document).scrollLeft() < o.scrollSensitivity)
					scrolled = $(document).scrollLeft($(document).scrollLeft() - o.scrollSpeed);
				else if($(window).width() - (event.pageX - $(document).scrollLeft()) < o.scrollSensitivity)
					scrolled = $(document).scrollLeft($(document).scrollLeft() + o.scrollSpeed);
			}

		}

		if(scrolled !== false && $.ui.ddmanager && !o.dropBehaviour)
			$.ui.ddmanager.prepareOffsets(i, event);

	}
});

$.ui.plugin.add("draggable", "snap", {
	start: function(event, ui) {

		var i = $(this).data("draggable"), o = i.options;
		i.snapElements = [];

		$(o.snap.constructor != String ? ( o.snap.items || ':data(draggable)' ) : o.snap).each(function() {
			var $t = $(this); var $o = $t.offset();
			if(this != i.element[0]) i.snapElements.push({
				item: this,
				width: $t.outerWidth(), height: $t.outerHeight(),
				top: $o.top, left: $o.left
			});
		});

	},
	drag: function(event, ui) {

		var inst = $(this).data("draggable"), o = inst.options;
		var d = o.snapTolerance;

		var x1 = ui.offset.left, x2 = x1 + inst.helperProportions.width,
			y1 = ui.offset.top, y2 = y1 + inst.helperProportions.height;

		for (var i = inst.snapElements.length - 1; i >= 0; i--){

			var l = inst.snapElements[i].left, r = l + inst.snapElements[i].width,
				t = inst.snapElements[i].top, b = t + inst.snapElements[i].height;

			//Yes, I know, this is insane ;)
			if(!((l-d < x1 && x1 < r+d && t-d < y1 && y1 < b+d) || (l-d < x1 && x1 < r+d && t-d < y2 && y2 < b+d) || (l-d < x2 && x2 < r+d && t-d < y1 && y1 < b+d) || (l-d < x2 && x2 < r+d && t-d < y2 && y2 < b+d))) {
				if(inst.snapElements[i].snapping) (inst.options.snap.release && inst.options.snap.release.call(inst.element, event, $.extend(inst._uiHash(), { snapItem: inst.snapElements[i].item })));
				inst.snapElements[i].snapping = false;
				continue;
			}

			if(o.snapMode != 'inner') {
				var ts = Math.abs(t - y2) <= d;
				var bs = Math.abs(b - y1) <= d;
				var ls = Math.abs(l - x2) <= d;
				var rs = Math.abs(r - x1) <= d;
				if(ts) ui.position.top = inst._convertPositionTo("relative", { top: t - inst.helperProportions.height, left: 0 }).top - inst.margins.top;
				if(bs) ui.position.top = inst._convertPositionTo("relative", { top: b, left: 0 }).top - inst.margins.top;
				if(ls) ui.position.left = inst._convertPositionTo("relative", { top: 0, left: l - inst.helperProportions.width }).left - inst.margins.left;
				if(rs) ui.position.left = inst._convertPositionTo("relative", { top: 0, left: r }).left - inst.margins.left;
			}

			var first = (ts || bs || ls || rs);

			if(o.snapMode != 'outer') {
				var ts = Math.abs(t - y1) <= d;
				var bs = Math.abs(b - y2) <= d;
				var ls = Math.abs(l - x1) <= d;
				var rs = Math.abs(r - x2) <= d;
				if(ts) ui.position.top = inst._convertPositionTo("relative", { top: t, left: 0 }).top - inst.margins.top;
				if(bs) ui.position.top = inst._convertPositionTo("relative", { top: b - inst.helperProportions.height, left: 0 }).top - inst.margins.top;
				if(ls) ui.position.left = inst._convertPositionTo("relative", { top: 0, left: l }).left - inst.margins.left;
				if(rs) ui.position.left = inst._convertPositionTo("relative", { top: 0, left: r - inst.helperProportions.width }).left - inst.margins.left;
			}

			if(!inst.snapElements[i].snapping && (ts || bs || ls || rs || first))
				(inst.options.snap.snap && inst.options.snap.snap.call(inst.element, event, $.extend(inst._uiHash(), { snapItem: inst.snapElements[i].item })));
			inst.snapElements[i].snapping = (ts || bs || ls || rs || first);

		};

	}
});

$.ui.plugin.add("draggable", "stack", {
	start: function(event, ui) {

		var o = $(this).data("draggable").options;

		var group = $.makeArray($(o.stack)).sort(function(a,b) {
			return (parseInt($(a).css("zIndex"),10) || 0) - (parseInt($(b).css("zIndex"),10) || 0);
		});
		if (!group.length) { return; }
		
		var min = parseInt(group[0].style.zIndex) || 0;
		$(group).each(function(i) {
			this.style.zIndex = min + i;
		});

		this[0].style.zIndex = min + group.length;

	}
});

$.ui.plugin.add("draggable", "zIndex", {
	start: function(event, ui) {
		var t = $(ui.helper), o = $(this).data("draggable").options;
		if(t.css("zIndex")) o._zIndex = t.css("zIndex");
		t.css('zIndex', o.zIndex);
	},
	stop: function(event, ui) {
		var o = $(this).data("draggable").options;
		if(o._zIndex) $(ui.helper).css('zIndex', o._zIndex);
	}
});

})(jQuery);
/*!
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
/*
 * jQuery UI Sortable 1.8
 *
 * Copyright (c) 2010 AUTHORS.txt (http://jqueryui.com/about)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * http://docs.jquery.com/UI/Sortables
 *
 * Depends:
 *	jquery.ui.core.js
 *	jquery.ui.mouse.js
 *	jquery.ui.widget.js
 */
(function($) {

$.widget("ui.sortable", $.ui.mouse, {
	widgetEventPrefix: "sort",
	options: {
		appendTo: "parent",
		axis: false,
		connectWith: false,
		containment: false,
		cursor: 'auto',
		cursorAt: false,
		dropOnEmpty: true,
		forcePlaceholderSize: false,
		forceHelperSize: false,
		grid: false,
		handle: false,
		helper: "original",
		items: '> *',
		opacity: false,
		placeholder: false,
		revert: false,
		scroll: true,
		scrollSensitivity: 20,
		scrollSpeed: 20,
		scope: "default",
		tolerance: "intersect",
		zIndex: 1000
	},
	_create: function() {

		var o = this.options;
		this.containerCache = {};
		this.element.addClass("ui-sortable");

		//Get the items
		this.refresh();

		//Let's determine if the items are floating
		this.floating = this.items.length ? (/left|right/).test(this.items[0].item.css('float')) : false;

		//Let's determine the parent's offset
		this.offset = this.element.offset();

		//Initialize mouse events for interaction
		this._mouseInit();

	},

	destroy: function() {
		this.element
			.removeClass("ui-sortable ui-sortable-disabled")
			.removeData("sortable")
			.unbind(".sortable");
		this._mouseDestroy();

		for ( var i = this.items.length - 1; i >= 0; i-- )
			this.items[i].item.removeData("sortable-item");

		return this;
	},

	_mouseCapture: function(event, overrideHandle) {

		if (this.reverting) {
			return false;
		}

		if(this.options.disabled || this.options.type == 'static') return false;

		//We have to refresh the items data once first
		this._refreshItems(event);

		//Find out if the clicked node (or one of its parents) is a actual item in this.items
		var currentItem = null, self = this, nodes = $(event.target).parents().each(function() {
			if($.data(this, 'sortable-item') == self) {
				currentItem = $(this);
				return false;
			}
		});
		if($.data(event.target, 'sortable-item') == self) currentItem = $(event.target);

		if(!currentItem) return false;
		if(this.options.handle && !overrideHandle) {
			var validHandle = false;

			$(this.options.handle, currentItem).find("*").andSelf().each(function() { if(this == event.target) validHandle = true; });
			if(!validHandle) return false;
		}

		this.currentItem = currentItem;
		this._removeCurrentsFromItems();
		return true;

	},

	_mouseStart: function(event, overrideHandle, noActivation) {

		var o = this.options, self = this;
		this.currentContainer = this;

		//We only need to call refreshPositions, because the refreshItems call has been moved to mouseCapture
		this.refreshPositions();

		//Create and append the visible helper
		this.helper = this._createHelper(event);

		//Cache the helper size
		this._cacheHelperProportions();

		/*
		 * - Position generation -
		 * This block generates everything position related - it's the core of draggables.
		 */

		//Cache the margins of the original element
		this._cacheMargins();

		//Get the next scrolling parent
		this.scrollParent = this.helper.scrollParent();

		//The element's absolute position on the page minus margins
		this.offset = this.currentItem.offset();
		this.offset = {
			top: this.offset.top - this.margins.top,
			left: this.offset.left - this.margins.left
		};

		// Only after we got the offset, we can change the helper's position to absolute
		// TODO: Still need to figure out a way to make relative sorting possible
		this.helper.css("position", "absolute");
		this.cssPosition = this.helper.css("position");

		$.extend(this.offset, {
			click: { //Where the click happened, relative to the element
				left: event.pageX - this.offset.left,
				top: event.pageY - this.offset.top
			},
			parent: this._getParentOffset(),
			relative: this._getRelativeOffset() //This is a relative to absolute position minus the actual position calculation - only used for relative positioned helper
		});

		//Generate the original position
		this.originalPosition = this._generatePosition(event);
		this.originalPageX = event.pageX;
		this.originalPageY = event.pageY;

		//Adjust the mouse offset relative to the helper if 'cursorAt' is supplied
		(o.cursorAt && this._adjustOffsetFromHelper(o.cursorAt));

		//Cache the former DOM position
		this.domPosition = { prev: this.currentItem.prev()[0], parent: this.currentItem.parent()[0] };

		//If the helper is not the original, hide the original so it's not playing any role during the drag, won't cause anything bad this way
		if(this.helper[0] != this.currentItem[0]) {
			this.currentItem.hide();
		}

		//Create the placeholder
		this._createPlaceholder();

		//Set a containment if given in the options
		if(o.containment)
			this._setContainment();

		if(o.cursor) { // cursor option
			if ($('body').css("cursor")) this._storedCursor = $('body').css("cursor");
			$('body').css("cursor", o.cursor);
		}

		if(o.opacity) { // opacity option
			if (this.helper.css("opacity")) this._storedOpacity = this.helper.css("opacity");
			this.helper.css("opacity", o.opacity);
		}

		if(o.zIndex) { // zIndex option
			if (this.helper.css("zIndex")) this._storedZIndex = this.helper.css("zIndex");
			this.helper.css("zIndex", o.zIndex);
		}

		//Prepare scrolling
		if(this.scrollParent[0] != document && this.scrollParent[0].tagName != 'HTML')
			this.overflowOffset = this.scrollParent.offset();

		//Call callbacks
		this._trigger("start", event, this._uiHash());

		//Recache the helper size
		if(!this._preserveHelperProportions)
			this._cacheHelperProportions();


		//Post 'activate' events to possible containers
		if(!noActivation) {
			 for (var i = this.containers.length - 1; i >= 0; i--) { this.containers[i]._trigger("activate", event, self._uiHash(this)); }
		}

		//Prepare possible droppables
		if($.ui.ddmanager)
			$.ui.ddmanager.current = this;

		if ($.ui.ddmanager && !o.dropBehaviour)
			$.ui.ddmanager.prepareOffsets(this, event);

		this.dragging = true;

		this.helper.addClass("ui-sortable-helper");
		this._mouseDrag(event); //Execute the drag once - this causes the helper not to be visible before getting its correct position
		return true;

	},

	_mouseDrag: function(event) {

		//Compute the helpers position
		this.position = this._generatePosition(event);
		this.positionAbs = this._convertPositionTo("absolute");

		if (!this.lastPositionAbs) {
			this.lastPositionAbs = this.positionAbs;
		}

		//Do scrolling
		if(this.options.scroll) {
			var o = this.options, scrolled = false;
			if(this.scrollParent[0] != document && this.scrollParent[0].tagName != 'HTML') {

				if((this.overflowOffset.top + this.scrollParent[0].offsetHeight) - event.pageY < o.scrollSensitivity)
					this.scrollParent[0].scrollTop = scrolled = this.scrollParent[0].scrollTop + o.scrollSpeed;
				else if(event.pageY - this.overflowOffset.top < o.scrollSensitivity)
					this.scrollParent[0].scrollTop = scrolled = this.scrollParent[0].scrollTop - o.scrollSpeed;

				if((this.overflowOffset.left + this.scrollParent[0].offsetWidth) - event.pageX < o.scrollSensitivity)
					this.scrollParent[0].scrollLeft = scrolled = this.scrollParent[0].scrollLeft + o.scrollSpeed;
				else if(event.pageX - this.overflowOffset.left < o.scrollSensitivity)
					this.scrollParent[0].scrollLeft = scrolled = this.scrollParent[0].scrollLeft - o.scrollSpeed;

			} else {

				if(event.pageY - $(document).scrollTop() < o.scrollSensitivity)
					scrolled = $(document).scrollTop($(document).scrollTop() - o.scrollSpeed);
				else if($(window).height() - (event.pageY - $(document).scrollTop()) < o.scrollSensitivity)
					scrolled = $(document).scrollTop($(document).scrollTop() + o.scrollSpeed);

				if(event.pageX - $(document).scrollLeft() < o.scrollSensitivity)
					scrolled = $(document).scrollLeft($(document).scrollLeft() - o.scrollSpeed);
				else if($(window).width() - (event.pageX - $(document).scrollLeft()) < o.scrollSensitivity)
					scrolled = $(document).scrollLeft($(document).scrollLeft() + o.scrollSpeed);

			}

			if(scrolled !== false && $.ui.ddmanager && !o.dropBehaviour)
				$.ui.ddmanager.prepareOffsets(this, event);
		}

		//Regenerate the absolute position used for position checks
		this.positionAbs = this._convertPositionTo("absolute");

		//Set the helper position
		if(!this.options.axis || this.options.axis != "y") this.helper[0].style.left = this.position.left+'px';
		if(!this.options.axis || this.options.axis != "x") this.helper[0].style.top = this.position.top+'px';

		//Rearrange
		for (var i = this.items.length - 1; i >= 0; i--) {

			//Cache variables and intersection, continue if no intersection
			var item = this.items[i], itemElement = item.item[0], intersection = this._intersectsWithPointer(item);
			if (!intersection) continue;

			if(itemElement != this.currentItem[0] //cannot intersect with itself
				&&	this.placeholder[intersection == 1 ? "next" : "prev"]()[0] != itemElement //no useless actions that have been done before
				&&	!$.ui.contains(this.placeholder[0], itemElement) //no action if the item moved is the parent of the item checked
				&& (this.options.type == 'semi-dynamic' ? !$.ui.contains(this.element[0], itemElement) : true)
				//&& itemElement.parentNode == this.placeholder[0].parentNode // only rearrange items within the same container
			) {

				this.direction = intersection == 1 ? "down" : "up";

				if (this.options.tolerance == "pointer" || this._intersectsWithSides(item)) {
					this._rearrange(event, item);
				} else {
					break;
				}

				this._trigger("change", event, this._uiHash());
				break;
			}
		}

		//Post events to containers
		this._contactContainers(event);

		//Interconnect with droppables
		if($.ui.ddmanager) $.ui.ddmanager.drag(this, event);

		//Call callbacks
		this._trigger('sort', event, this._uiHash());

		this.lastPositionAbs = this.positionAbs;
		return false;

	},

	_mouseStop: function(event, noPropagation) {

		if(!event) return;

		//If we are using droppables, inform the manager about the drop
		if ($.ui.ddmanager && !this.options.dropBehaviour)
			$.ui.ddmanager.drop(this, event);

		if(this.options.revert) {
			var self = this;
			var cur = self.placeholder.offset();

			self.reverting = true;

			$(this.helper).animate({
				left: cur.left - this.offset.parent.left - self.margins.left + (this.offsetParent[0] == document.body ? 0 : this.offsetParent[0].scrollLeft),
				top: cur.top - this.offset.parent.top - self.margins.top + (this.offsetParent[0] == document.body ? 0 : this.offsetParent[0].scrollTop)
			}, parseInt(this.options.revert, 10) || 500, function() {
				self._clear(event);
			});
		} else {
			this._clear(event, noPropagation);
		}

		return false;

	},

	cancel: function() {

		var self = this;

		if(this.dragging) {

			this._mouseUp();

			if(this.options.helper == "original")
				this.currentItem.css(this._storedCSS).removeClass("ui-sortable-helper");
			else
				this.currentItem.show();

			//Post deactivating events to containers
			for (var i = this.containers.length - 1; i >= 0; i--){
				this.containers[i]._trigger("deactivate", null, self._uiHash(this));
				if(this.containers[i].containerCache.over) {
					this.containers[i]._trigger("out", null, self._uiHash(this));
					this.containers[i].containerCache.over = 0;
				}
			}

		}

		//$(this.placeholder[0]).remove(); would have been the jQuery way - unfortunately, it unbinds ALL events from the original node!
		if(this.placeholder[0].parentNode) this.placeholder[0].parentNode.removeChild(this.placeholder[0]);
		if(this.options.helper != "original" && this.helper && this.helper[0].parentNode) this.helper.remove();

		$.extend(this, {
			helper: null,
			dragging: false,
			reverting: false,
			_noFinalSort: null
		});

		if(this.domPosition.prev) {
			$(this.domPosition.prev).after(this.currentItem);
		} else {
			$(this.domPosition.parent).prepend(this.currentItem);
		}

		return this;

	},

	serialize: function(o) {

		var items = this._getItemsAsjQuery(o && o.connected);
		var str = []; o = o || {};

		$(items).each(function() {
			var res = ($(o.item || this).attr(o.attribute || 'id') || '').match(o.expression || (/(.+)[-=_](.+)/));
			if(res) str.push((o.key || res[1]+'[]')+'='+(o.key && o.expression ? res[1] : res[2]));
		});

		return str.join('&');

	},

	toArray: function(o) {

		var items = this._getItemsAsjQuery(o && o.connected);
		var ret = []; o = o || {};

		items.each(function() { ret.push($(o.item || this).attr(o.attribute || 'id') || ''); });
		return ret;

	},

	/* Be careful with the following core functions */
	_intersectsWith: function(item) {

		var x1 = this.positionAbs.left,
			x2 = x1 + this.helperProportions.width,
			y1 = this.positionAbs.top,
			y2 = y1 + this.helperProportions.height;

		var l = item.left,
			r = l + item.width,
			t = item.top,
			b = t + item.height;

		var dyClick = this.offset.click.top,
			dxClick = this.offset.click.left;

		var isOverElement = (y1 + dyClick) > t && (y1 + dyClick) < b && (x1 + dxClick) > l && (x1 + dxClick) < r;

		if(	   this.options.tolerance == "pointer"
			|| this.options.forcePointerForContainers
			|| (this.options.tolerance != "pointer" && this.helperProportions[this.floating ? 'width' : 'height'] > item[this.floating ? 'width' : 'height'])
		) {
			return isOverElement;
		} else {

			return (l < x1 + (this.helperProportions.width / 2) // Right Half
				&& x2 - (this.helperProportions.width / 2) < r // Left Half
				&& t < y1 + (this.helperProportions.height / 2) // Bottom Half
				&& y2 - (this.helperProportions.height / 2) < b ); // Top Half

		}
	},

	_intersectsWithPointer: function(item) {

		var isOverElementHeight = $.ui.isOverAxis(this.positionAbs.top + this.offset.click.top, item.top, item.height),
			isOverElementWidth = $.ui.isOverAxis(this.positionAbs.left + this.offset.click.left, item.left, item.width),
			isOverElement = isOverElementHeight && isOverElementWidth,
			verticalDirection = this._getDragVerticalDirection(),
			horizontalDirection = this._getDragHorizontalDirection();

		if (!isOverElement)
			return false;

		return this.floating ?
			( ((horizontalDirection && horizontalDirection == "right") || verticalDirection == "down") ? 2 : 1 )
			: ( verticalDirection && (verticalDirection == "down" ? 2 : 1) );

	},

	_intersectsWithSides: function(item) {

		var isOverBottomHalf = $.ui.isOverAxis(this.positionAbs.top + this.offset.click.top, item.top + (item.height/2), item.height),
			isOverRightHalf = $.ui.isOverAxis(this.positionAbs.left + this.offset.click.left, item.left + (item.width/2), item.width),
			verticalDirection = this._getDragVerticalDirection(),
			horizontalDirection = this._getDragHorizontalDirection();

		if (this.floating && horizontalDirection) {
			return ((horizontalDirection == "right" && isOverRightHalf) || (horizontalDirection == "left" && !isOverRightHalf));
		} else {
			return verticalDirection && ((verticalDirection == "down" && isOverBottomHalf) || (verticalDirection == "up" && !isOverBottomHalf));
		}

	},

	_getDragVerticalDirection: function() {
		var delta = this.positionAbs.top - this.lastPositionAbs.top;
		return delta != 0 && (delta > 0 ? "down" : "up");
	},

	_getDragHorizontalDirection: function() {
		var delta = this.positionAbs.left - this.lastPositionAbs.left;
		return delta != 0 && (delta > 0 ? "right" : "left");
	},

	refresh: function(event) {
		this._refreshItems(event);
		this.refreshPositions();
		return this;
	},

	_connectWith: function() {
		var options = this.options;
		return options.connectWith.constructor == String
			? [options.connectWith]
			: options.connectWith;
	},
	
	_getItemsAsjQuery: function(connected) {

		var self = this;
		var items = [];
		var queries = [];
		var connectWith = this._connectWith();

		if(connectWith && connected) {
			for (var i = connectWith.length - 1; i >= 0; i--){
				var cur = $(connectWith[i]);
				for (var j = cur.length - 1; j >= 0; j--){
					var inst = $.data(cur[j], 'sortable');
					if(inst && inst != this && !inst.options.disabled) {
						queries.push([$.isFunction(inst.options.items) ? inst.options.items.call(inst.element) : $(inst.options.items, inst.element).not(".ui-sortable-helper").not('.ui-sortable-placeholder'), inst]);
					}
				};
			};
		}

		queries.push([$.isFunction(this.options.items) ? this.options.items.call(this.element, null, { options: this.options, item: this.currentItem }) : $(this.options.items, this.element).not(".ui-sortable-helper").not('.ui-sortable-placeholder'), this]);

		for (var i = queries.length - 1; i >= 0; i--){
			queries[i][0].each(function() {
				items.push(this);
			});
		};

		return $(items);

	},

	_removeCurrentsFromItems: function() {

		var list = this.currentItem.find(":data(sortable-item)");

		for (var i=0; i < this.items.length; i++) {

			for (var j=0; j < list.length; j++) {
				if(list[j] == this.items[i].item[0])
					this.items.splice(i,1);
			};

		};

	},

	_refreshItems: function(event) {

		this.items = [];
		this.containers = [this];
		var items = this.items;
		var self = this;
		var queries = [[$.isFunction(this.options.items) ? this.options.items.call(this.element[0], event, { item: this.currentItem }) : $(this.options.items, this.element), this]];
		var connectWith = this._connectWith();

		if(connectWith) {
			for (var i = connectWith.length - 1; i >= 0; i--){
				var cur = $(connectWith[i]);
				for (var j = cur.length - 1; j >= 0; j--){
					var inst = $.data(cur[j], 'sortable');
					if(inst && inst != this && !inst.options.disabled) {
						queries.push([$.isFunction(inst.options.items) ? inst.options.items.call(inst.element[0], event, { item: this.currentItem }) : $(inst.options.items, inst.element), inst]);
						this.containers.push(inst);
					}
				};
			};
		}

		for (var i = queries.length - 1; i >= 0; i--) {
			var targetData = queries[i][1];
			var _queries = queries[i][0];

			for (var j=0, queriesLength = _queries.length; j < queriesLength; j++) {
				var item = $(_queries[j]);

				item.data('sortable-item', targetData); // Data for target checking (mouse manager)

				items.push({
					item: item,
					instance: targetData,
					width: 0, height: 0,
					left: 0, top: 0
				});
			};
		};

	},

	refreshPositions: function(fast) {

		//This has to be redone because due to the item being moved out/into the offsetParent, the offsetParent's position will change
		if(this.offsetParent && this.helper) {
			this.offset.parent = this._getParentOffset();
		}

		for (var i = this.items.length - 1; i >= 0; i--){
			var item = this.items[i];

			var t = this.options.toleranceElement ? $(this.options.toleranceElement, item.item) : item.item;

			if (!fast) {
				item.width = t.outerWidth();
				item.height = t.outerHeight();
			}

			var p = t.offset();
			item.left = p.left;
			item.top = p.top;
		};

		if(this.options.custom && this.options.custom.refreshContainers) {
			this.options.custom.refreshContainers.call(this);
		} else {
			for (var i = this.containers.length - 1; i >= 0; i--){
				var p = this.containers[i].element.offset();
				this.containers[i].containerCache.left = p.left;
				this.containers[i].containerCache.top = p.top;
				this.containers[i].containerCache.width	= this.containers[i].element.outerWidth();
				this.containers[i].containerCache.height = this.containers[i].element.outerHeight();
			};
		}

		return this;
	},

	_createPlaceholder: function(that) {

		var self = that || this, o = self.options;

		if(!o.placeholder || o.placeholder.constructor == String) {
			var className = o.placeholder;
			o.placeholder = {
				element: function() {

					var el = $(document.createElement(self.currentItem[0].nodeName))
						.addClass(className || self.currentItem[0].className+" ui-sortable-placeholder")
						.removeClass("ui-sortable-helper")[0];

					if(!className)
						el.style.visibility = "hidden";

					return el;
				},
				update: function(container, p) {

					// 1. If a className is set as 'placeholder option, we don't force sizes - the class is responsible for that
					// 2. The option 'forcePlaceholderSize can be enabled to force it even if a class name is specified
					if(className && !o.forcePlaceholderSize) return;

					//If the element doesn't have a actual height by itself (without styles coming from a stylesheet), it receives the inline height from the dragged item
					if(!p.height()) { p.height(self.currentItem.innerHeight() - parseInt(self.currentItem.css('paddingTop')||0, 10) - parseInt(self.currentItem.css('paddingBottom')||0, 10)); };
					if(!p.width()) { p.width(self.currentItem.innerWidth() - parseInt(self.currentItem.css('paddingLeft')||0, 10) - parseInt(self.currentItem.css('paddingRight')||0, 10)); };
				}
			};
		}

		//Create the placeholder
		self.placeholder = $(o.placeholder.element.call(self.element, self.currentItem));

		//Append it after the actual current item
		self.currentItem.after(self.placeholder);

		//Update the size of the placeholder (TODO: Logic to fuzzy, see line 316/317)
		o.placeholder.update(self, self.placeholder);

	},

	_contactContainers: function(event) {
		
		// get innermost container that intersects with item 
		var innermostContainer = null, innermostIndex = null;		
		
		
		for (var i = this.containers.length - 1; i >= 0; i--){

			// never consider a container that's located within the item itself 
			if($.ui.contains(this.currentItem[0], this.containers[i].element[0]))
				continue;

			if(this._intersectsWith(this.containers[i].containerCache)) {

				// if we've already found a container and it's more "inner" than this, then continue 
				if(innermostContainer && $.ui.contains(this.containers[i].element[0], innermostContainer.element[0]))
					continue;

				innermostContainer = this.containers[i]; 
				innermostIndex = i;
					
			} else {
				// container doesn't intersect. trigger "out" event if necessary 
				if(this.containers[i].containerCache.over) {
					this.containers[i]._trigger("out", event, this._uiHash(this));
					this.containers[i].containerCache.over = 0;
				}
			}

		}
		
		// if no intersecting containers found, return 
		if(!innermostContainer) return; 

		// move the item into the container if it's not there already
		if(this.containers.length === 1) {
			this.containers[innermostIndex]._trigger("over", event, this._uiHash(this));
			this.containers[innermostIndex].containerCache.over = 1;
		} else if(this.currentContainer != this.containers[innermostIndex]) { 

			//When entering a new container, we will find the item with the least distance and append our item near it 
			var dist = 10000; var itemWithLeastDistance = null; var base = this.positionAbs[this.containers[innermostIndex].floating ? 'left' : 'top']; 
			for (var j = this.items.length - 1; j >= 0; j--) { 
				if(!$.ui.contains(this.containers[innermostIndex].element[0], this.items[j].item[0])) continue; 
				var cur = this.items[j][this.containers[innermostIndex].floating ? 'left' : 'top']; 
				if(Math.abs(cur - base) < dist) { 
					dist = Math.abs(cur - base); itemWithLeastDistance = this.items[j]; 
				} 
			} 

			if(!itemWithLeastDistance && !this.options.dropOnEmpty) //Check if dropOnEmpty is enabled 
				return; 

			this.currentContainer = this.containers[innermostIndex]; 
			itemWithLeastDistance ? this._rearrange(event, itemWithLeastDistance, null, true) : this._rearrange(event, null, this.containers[innermostIndex].element, true); 
			this._trigger("change", event, this._uiHash()); 
			this.containers[innermostIndex]._trigger("change", event, this._uiHash(this)); 

			//Update the placeholder 
			this.options.placeholder.update(this.currentContainer, this.placeholder); 
		
			this.containers[innermostIndex]._trigger("over", event, this._uiHash(this)); 
			this.containers[innermostIndex].containerCache.over = 1;
		} 
	
		
	},

	_createHelper: function(event) {

		var o = this.options;
		var helper = $.isFunction(o.helper) ? $(o.helper.apply(this.element[0], [event, this.currentItem])) : (o.helper == 'clone' ? this.currentItem.clone() : this.currentItem);

		if(!helper.parents('body').length) //Add the helper to the DOM if that didn't happen already
			$(o.appendTo != 'parent' ? o.appendTo : this.currentItem[0].parentNode)[0].appendChild(helper[0]);

		if(helper[0] == this.currentItem[0])
			this._storedCSS = { width: this.currentItem[0].style.width, height: this.currentItem[0].style.height, position: this.currentItem.css("position"), top: this.currentItem.css("top"), left: this.currentItem.css("left") };

		if(helper[0].style.width == '' || o.forceHelperSize) helper.width(this.currentItem.width());
		if(helper[0].style.height == '' || o.forceHelperSize) helper.height(this.currentItem.height());

		return helper;

	},

	_adjustOffsetFromHelper: function(obj) {
		if (typeof obj == 'string') {
			obj = obj.split(' ');
		}
		if ($.isArray(obj)) {
			obj = {left: +obj[0], top: +obj[1] || 0};
		}
		if ('left' in obj) {
			this.offset.click.left = obj.left + this.margins.left;
		}
		if ('right' in obj) {
			this.offset.click.left = this.helperProportions.width - obj.right + this.margins.left;
		}
		if ('top' in obj) {
			this.offset.click.top = obj.top + this.margins.top;
		}
		if ('bottom' in obj) {
			this.offset.click.top = this.helperProportions.height - obj.bottom + this.margins.top;
		}
	},

	_getParentOffset: function() {


		//Get the offsetParent and cache its position
		this.offsetParent = this.helper.offsetParent();
		var po = this.offsetParent.offset();

		// This is a special case where we need to modify a offset calculated on start, since the following happened:
		// 1. The position of the helper is absolute, so it's position is calculated based on the next positioned parent
		// 2. The actual offset parent is a child of the scroll parent, and the scroll parent isn't the document, which means that
		//    the scroll is included in the initial calculation of the offset of the parent, and never recalculated upon drag
		if(this.cssPosition == 'absolute' && this.scrollParent[0] != document && $.ui.contains(this.scrollParent[0], this.offsetParent[0])) {
			po.left += this.scrollParent.scrollLeft();
			po.top += this.scrollParent.scrollTop();
		}

		if((this.offsetParent[0] == document.body) //This needs to be actually done for all browsers, since pageX/pageY includes this information
		|| (this.offsetParent[0].tagName && this.offsetParent[0].tagName.toLowerCase() == 'html' && $.browser.msie)) //Ugly IE fix
			po = { top: 0, left: 0 };

		return {
			top: po.top + (parseInt(this.offsetParent.css("borderTopWidth"),10) || 0),
			left: po.left + (parseInt(this.offsetParent.css("borderLeftWidth"),10) || 0)
		};

	},

	_getRelativeOffset: function() {

		if(this.cssPosition == "relative") {
			var p = this.currentItem.position();
			return {
				top: p.top - (parseInt(this.helper.css("top"),10) || 0) + this.scrollParent.scrollTop(),
				left: p.left - (parseInt(this.helper.css("left"),10) || 0) + this.scrollParent.scrollLeft()
			};
		} else {
			return { top: 0, left: 0 };
		}

	},

	_cacheMargins: function() {
		this.margins = {
			left: (parseInt(this.currentItem.css("marginLeft"),10) || 0),
			top: (parseInt(this.currentItem.css("marginTop"),10) || 0)
		};
	},

	_cacheHelperProportions: function() {
		this.helperProportions = {
			width: this.helper.outerWidth(),
			height: this.helper.outerHeight()
		};
	},

	_setContainment: function() {

		var o = this.options;
		if(o.containment == 'parent') o.containment = this.helper[0].parentNode;
		if(o.containment == 'document' || o.containment == 'window') this.containment = [
			0 - this.offset.relative.left - this.offset.parent.left,
			0 - this.offset.relative.top - this.offset.parent.top,
			$(o.containment == 'document' ? document : window).width() - this.helperProportions.width - this.margins.left,
			($(o.containment == 'document' ? document : window).height() || document.body.parentNode.scrollHeight) - this.helperProportions.height - this.margins.top
		];

		if(!(/^(document|window|parent)$/).test(o.containment)) {
			var ce = $(o.containment)[0];
			var co = $(o.containment).offset();
			var over = ($(ce).css("overflow") != 'hidden');

			this.containment = [
				co.left + (parseInt($(ce).css("borderLeftWidth"),10) || 0) + (parseInt($(ce).css("paddingLeft"),10) || 0) - this.margins.left,
				co.top + (parseInt($(ce).css("borderTopWidth"),10) || 0) + (parseInt($(ce).css("paddingTop"),10) || 0) - this.margins.top,
				co.left+(over ? Math.max(ce.scrollWidth,ce.offsetWidth) : ce.offsetWidth) - (parseInt($(ce).css("borderLeftWidth"),10) || 0) - (parseInt($(ce).css("paddingRight"),10) || 0) - this.helperProportions.width - this.margins.left,
				co.top+(over ? Math.max(ce.scrollHeight,ce.offsetHeight) : ce.offsetHeight) - (parseInt($(ce).css("borderTopWidth"),10) || 0) - (parseInt($(ce).css("paddingBottom"),10) || 0) - this.helperProportions.height - this.margins.top
			];
		}

	},

	_convertPositionTo: function(d, pos) {

		if(!pos) pos = this.position;
		var mod = d == "absolute" ? 1 : -1;
		var o = this.options, scroll = this.cssPosition == 'absolute' && !(this.scrollParent[0] != document && $.ui.contains(this.scrollParent[0], this.offsetParent[0])) ? this.offsetParent : this.scrollParent, scrollIsRootNode = (/(html|body)/i).test(scroll[0].tagName);

		return {
			top: (
				pos.top																	// The absolute mouse position
				+ this.offset.relative.top * mod										// Only for relative positioned nodes: Relative offset from element to offset parent
				+ this.offset.parent.top * mod											// The offsetParent's offset without borders (offset + border)
				- ($.browser.safari && this.cssPosition == 'fixed' ? 0 : ( this.cssPosition == 'fixed' ? -this.scrollParent.scrollTop() : ( scrollIsRootNode ? 0 : scroll.scrollTop() ) ) * mod)
			),
			left: (
				pos.left																// The absolute mouse position
				+ this.offset.relative.left * mod										// Only for relative positioned nodes: Relative offset from element to offset parent
				+ this.offset.parent.left * mod											// The offsetParent's offset without borders (offset + border)
				- ($.browser.safari && this.cssPosition == 'fixed' ? 0 : ( this.cssPosition == 'fixed' ? -this.scrollParent.scrollLeft() : scrollIsRootNode ? 0 : scroll.scrollLeft() ) * mod)
			)
		};

	},

	_generatePosition: function(event) {

		var o = this.options, scroll = this.cssPosition == 'absolute' && !(this.scrollParent[0] != document && $.ui.contains(this.scrollParent[0], this.offsetParent[0])) ? this.offsetParent : this.scrollParent, scrollIsRootNode = (/(html|body)/i).test(scroll[0].tagName);

		// This is another very weird special case that only happens for relative elements:
		// 1. If the css position is relative
		// 2. and the scroll parent is the document or similar to the offset parent
		// we have to refresh the relative offset during the scroll so there are no jumps
		if(this.cssPosition == 'relative' && !(this.scrollParent[0] != document && this.scrollParent[0] != this.offsetParent[0])) {
			this.offset.relative = this._getRelativeOffset();
		}

		var pageX = event.pageX;
		var pageY = event.pageY;

		/*
		 * - Position constraining -
		 * Constrain the position to a mix of grid, containment.
		 */

		if(this.originalPosition) { //If we are not dragging yet, we won't check for options

			if(this.containment) {
				if(event.pageX - this.offset.click.left < this.containment[0]) pageX = this.containment[0] + this.offset.click.left;
				if(event.pageY - this.offset.click.top < this.containment[1]) pageY = this.containment[1] + this.offset.click.top;
				if(event.pageX - this.offset.click.left > this.containment[2]) pageX = this.containment[2] + this.offset.click.left;
				if(event.pageY - this.offset.click.top > this.containment[3]) pageY = this.containment[3] + this.offset.click.top;
			}

			if(o.grid) {
				var top = this.originalPageY + Math.round((pageY - this.originalPageY) / o.grid[1]) * o.grid[1];
				pageY = this.containment ? (!(top - this.offset.click.top < this.containment[1] || top - this.offset.click.top > this.containment[3]) ? top : (!(top - this.offset.click.top < this.containment[1]) ? top - o.grid[1] : top + o.grid[1])) : top;

				var left = this.originalPageX + Math.round((pageX - this.originalPageX) / o.grid[0]) * o.grid[0];
				pageX = this.containment ? (!(left - this.offset.click.left < this.containment[0] || left - this.offset.click.left > this.containment[2]) ? left : (!(left - this.offset.click.left < this.containment[0]) ? left - o.grid[0] : left + o.grid[0])) : left;
			}

		}

		return {
			top: (
				pageY																// The absolute mouse position
				- this.offset.click.top													// Click offset (relative to the element)
				- this.offset.relative.top												// Only for relative positioned nodes: Relative offset from element to offset parent
				- this.offset.parent.top												// The offsetParent's offset without borders (offset + border)
				+ ($.browser.safari && this.cssPosition == 'fixed' ? 0 : ( this.cssPosition == 'fixed' ? -this.scrollParent.scrollTop() : ( scrollIsRootNode ? 0 : scroll.scrollTop() ) ))
			),
			left: (
				pageX																// The absolute mouse position
				- this.offset.click.left												// Click offset (relative to the element)
				- this.offset.relative.left												// Only for relative positioned nodes: Relative offset from element to offset parent
				- this.offset.parent.left												// The offsetParent's offset without borders (offset + border)
				+ ($.browser.safari && this.cssPosition == 'fixed' ? 0 : ( this.cssPosition == 'fixed' ? -this.scrollParent.scrollLeft() : scrollIsRootNode ? 0 : scroll.scrollLeft() ))
			)
		};

	},

	_rearrange: function(event, i, a, hardRefresh) {

		a ? a[0].appendChild(this.placeholder[0]) : i.item[0].parentNode.insertBefore(this.placeholder[0], (this.direction == 'down' ? i.item[0] : i.item[0].nextSibling));

		//Various things done here to improve the performance:
		// 1. we create a setTimeout, that calls refreshPositions
		// 2. on the instance, we have a counter variable, that get's higher after every append
		// 3. on the local scope, we copy the counter variable, and check in the timeout, if it's still the same
		// 4. this lets only the last addition to the timeout stack through
		this.counter = this.counter ? ++this.counter : 1;
		var self = this, counter = this.counter;

		window.setTimeout(function() {
			if(counter == self.counter) self.refreshPositions(!hardRefresh); //Precompute after each DOM insertion, NOT on mousemove
		},0);

	},

	_clear: function(event, noPropagation) {

		this.reverting = false;
		// We delay all events that have to be triggered to after the point where the placeholder has been removed and
		// everything else normalized again
		var delayedTriggers = [], self = this;

		// We first have to update the dom position of the actual currentItem
		// Note: don't do it if the current item is already removed (by a user), or it gets reappended (see #4088)
		if(!this._noFinalSort && this.currentItem[0].parentNode) this.placeholder.before(this.currentItem);
		this._noFinalSort = null;

		if(this.helper[0] == this.currentItem[0]) {
			for(var i in this._storedCSS) {
				if(this._storedCSS[i] == 'auto' || this._storedCSS[i] == 'static') this._storedCSS[i] = '';
			}
			this.currentItem.css(this._storedCSS).removeClass("ui-sortable-helper");
		} else {
			this.currentItem.show();
		}

		if(this.fromOutside && !noPropagation) delayedTriggers.push(function(event) { this._trigger("receive", event, this._uiHash(this.fromOutside)); });
		if((this.fromOutside || this.domPosition.prev != this.currentItem.prev().not(".ui-sortable-helper")[0] || this.domPosition.parent != this.currentItem.parent()[0]) && !noPropagation) delayedTriggers.push(function(event) { this._trigger("update", event, this._uiHash()); }); //Trigger update callback if the DOM position has changed
		if(!$.ui.contains(this.element[0], this.currentItem[0])) { //Node was moved out of the current element
			if(!noPropagation) delayedTriggers.push(function(event) { this._trigger("remove", event, this._uiHash()); });
			for (var i = this.containers.length - 1; i >= 0; i--){
				if($.ui.contains(this.containers[i].element[0], this.currentItem[0]) && !noPropagation) {
					delayedTriggers.push((function(c) { return function(event) { c._trigger("receive", event, this._uiHash(this)); };  }).call(this, this.containers[i]));
					delayedTriggers.push((function(c) { return function(event) { c._trigger("update", event, this._uiHash(this));  }; }).call(this, this.containers[i]));
				}
			};
		};

		//Post events to containers
		for (var i = this.containers.length - 1; i >= 0; i--){
			if(!noPropagation) delayedTriggers.push((function(c) { return function(event) { c._trigger("deactivate", event, this._uiHash(this)); };  }).call(this, this.containers[i]));
			if(this.containers[i].containerCache.over) {
				delayedTriggers.push((function(c) { return function(event) { c._trigger("out", event, this._uiHash(this)); };  }).call(this, this.containers[i]));
				this.containers[i].containerCache.over = 0;
			}
		}

		//Do what was originally in plugins
		if(this._storedCursor) $('body').css("cursor", this._storedCursor); //Reset cursor
		if(this._storedOpacity) this.helper.css("opacity", this._storedOpacity); //Reset opacity
		if(this._storedZIndex) this.helper.css("zIndex", this._storedZIndex == 'auto' ? '' : this._storedZIndex); //Reset z-index

		this.dragging = false;
		if(this.cancelHelperRemoval) {
			if(!noPropagation) {
				this._trigger("beforeStop", event, this._uiHash());
				for (var i=0; i < delayedTriggers.length; i++) { delayedTriggers[i].call(this, event); }; //Trigger all delayed events
				this._trigger("stop", event, this._uiHash());
			}
			return false;
		}

		if(!noPropagation) this._trigger("beforeStop", event, this._uiHash());

		//$(this.placeholder[0]).remove(); would have been the jQuery way - unfortunately, it unbinds ALL events from the original node!
		this.placeholder[0].parentNode.removeChild(this.placeholder[0]);

		if(this.helper[0] != this.currentItem[0]) this.helper.remove(); this.helper = null;

		if(!noPropagation) {
			for (var i=0; i < delayedTriggers.length; i++) { delayedTriggers[i].call(this, event); }; //Trigger all delayed events
			this._trigger("stop", event, this._uiHash());
		}

		this.fromOutside = false;
		return true;

	},

	_trigger: function() {
		if ($.Widget.prototype._trigger.apply(this, arguments) === false) {
			this.cancel();
		}
	},

	_uiHash: function(inst) {
		var self = inst || this;
		return {
			helper: self.helper,
			placeholder: self.placeholder || $([]),
			position: self.position,
			originalPosition: self.originalPosition,
			offset: self.positionAbs,
			item: self.currentItem,
			sender: inst ? inst.element : null
		};
	}

});

$.extend($.ui.sortable, {
	version: "1.8"
});

})(jQuery);

jQuery.widget("ui.ncbiautocomplete", {

    options : {
        webserviceUrl: "/portal/utils/autocomp.fcgi",
        isUrlRelative : false,
        dictionary: "",
        responseFormat: "old",
        disableUrl: null,
        prefUrl: null,
        zIndex: 1001,
        isEnabled: true,
        hasRelatedMatches: false,
        minLength : 2,
        expandPauseTime : 400,
        maxListLimit : null,
        isCrossDomain : false
    },

    _create: function() {
        this._createOptionsBox();
        this._setHandlers();
        this._setWebServicePath();
        this._lastEnteredTerm = "";
        this._setSGData();
    },

    _activeRequest: null,

    _keys: {
        "up": 38,
        "down": 40,
        "enter": 13,
        "escape": 27,
        "tab": 9,
        "shift" : 16
    },

    _setSGData : function(){
        this.sgData = {
            "jsevent" : "autocomplete",
            "userTyped" : "",
            "hasScrolled" : false,
            "usedArrows" : false,
            "selectionAction" : "",
            "optionSelected" : "",
            "optionIndex" : -1,
            "optionsCount" : -1
        };
    },	
		
    _setHandlers: function() {

        var that = this;

        jQuery(this.element).focus(function() {
            that._focused();
        }).keypress(function(e) {
            that._keyPress(e);
        }).keydown(function(e) {
            that._keyPress(e);
        }).keyup(function(e) {
            that._keyUp(e);
        }).attr("AUTOCOMPLETE","OFF");

        //For firefox!
        var domElem = jQuery(this.element)[0];
		domElem.autocomplete="off";
        domElem.AUTOCOMPLETE="OFF";

        jQuery(window).resize(function() {
            if(that._isActive){
                that._positionOptionsBox();
            }
        })

        jQuery(document).click(function(e){
            that._checkClickEvent(e);
        });

        jQuery(this._gol.optionsBox).hover(function() {
            if(that._isActive){
                that._isOptionsBoxFocused = true;
            }
        },
        function() {
            if(that._isActive){
                that._isOptionsBoxFocused = false;
            }
        });

    },

    _isOptionsBoxFocused: false,
    _createOptionsBox: function() {

        //Check to see if we already created this!
        if(jQuery.ui.ncbiautocomplete._globalOptionsList===null){

            jQuery.ui.ncbiautocomplete._globalOptionsList = { };

            jQuery.ui.ncbiautocomplete._globalOptionsList.optionsBox = jQuery("<div class='ui-ncbiautocomplete-holder'></div>").appendTo(document.body);
            jQuery.ui.ncbiautocomplete._globalOptionsList.optionsList = jQuery("<ul class='ui-ncbiautocomplete-options' role='menu' aria-live='assertive'></ul>").appendTo(jQuery(jQuery.ui.ncbiautocomplete._globalOptionsList.optionsBox));

            jQuery.ui.ncbiautocomplete._globalOptionsList.optionsActions = jQuery("<div class='ui-ncbiautocomplete-actions'></div>").appendTo(jQuery(jQuery.ui.ncbiautocomplete._globalOptionsList.optionsBox));

            jQuery.ui.ncbiautocomplete._globalOptionsList.prefLink = jQuery("<a href='#' class='ui-ncbiautocomplete-link-pref'>Preferences</a>").appendTo(jQuery(jQuery.ui.ncbiautocomplete._globalOptionsList.optionsActions));
            jQuery.ui.ncbiautocomplete._globalOptionsList.turnOffLink = jQuery("<a href='#' class='ui-ncbiautocomplete-link-off'>Turn off</a>").appendTo(jQuery(jQuery.ui.ncbiautocomplete._globalOptionsList.optionsActions));

            jQuery.ui.ncbiautocomplete._globalOptionsList.isIE6 = (jQuery.browser.msie && parseInt(jQuery.browser.version,10)<7);
            if(jQuery.ui.ncbiautocomplete._globalOptionsList.isIE6){
                jQuery.ui.ncbiautocomplete._globalOptionsList.optionsBox.iframe = jQuery("<iframe src='javascript:\"\";' class='ui-ncbiautocomplete-iframe' marginwidth='0' marginheight='0' align='bottom' scrolling='no' frameborder='0'></iframe>").appendTo(document.body);
            }

        }

        this._gol = jQuery.ui.ncbiautocomplete._globalOptionsList;

    },

    _positionOptionsBox: function() {

        if(this._gol.activeElement){

            var thisElem = jQuery(this.element);
            var prevElement = jQuery(this._gol.activeElement);

            var id1 = thisElem.attr("id") || thisElem[0];
            var id2 = prevElement.attr("id") || prevElement[0];

            if(id1 !== id2){
                this._hideOptions();
            }
        }

		var that = this;

        if(this._gol.activeElement !== this.element){
            var prefDisplay = (this.options.prefUrl !== null)?"block":"none";
            var disableDisplay = (this.options.disableUrl !== null)?"block":"none";
            var optionsDisplay = (prefDisplay === "block" || disableDisplay === "block")?"block":"none";

            jQuery(this._gol.prefLink).css("display",prefDisplay).attr("href",this.options.prefUrl).unbind("click").click(function(e) {
                that._prefLinkClick();
            });

            jQuery(this._gol.turnOffLink).css("display",disableDisplay).unbind("click").click(function(e) {
                that.turnOff();
                e.preventDefault();
            });

            jQuery(this._gol.optionsActions).css("display",optionsDisplay);

        }
        else{
            this._gol.activeElement = this.element;
        }

        var tb = jQuery(this.element);
        var tbWidth = tb.outerWidth();
        var tbHeight = tb.outerHeight() - 2;
        var tbPosition = tb.offset();

        var box = jQuery(this._gol.optionsBox);

        if (jQuery.browser.msie) {
            var bl = 0;
            var br = 0;
            var adj = 0;
        }
        else{
            bl = parseInt(box.css("borderLeftWidth"), 10);
            br = parseInt(box.css("borderRightWidth"), 10);
            adj = -1;
        }

        box.css("top", (tbPosition.top + tbHeight) + "px").css("left", tbPosition.left + "px").width(tbWidth - bl - br + adj + "px");

        var zIndex = parseInt(this.options.zIndex,10);
        box.css("zIndex", zIndex);

		var optList = jQuery(this._gol.optionsList);
		optList[0].onscroll = function(){};
        optList.scrollTop(0);
		optList[0].onscroll = function(){ that.sgData.hasScrolled = true;};

        if(this._gol.isIE6){
            var ifr = jQuery(this._gol.optionsBox.iframe);
            ifr.css("top", (tbPosition.top + tbHeight) + "px").css("left", tbPosition.left + "px").width(tbWidth - bl - br + adj + "px");
            ifr.css("zIndex", zIndex-1);
        }

    },

    _isActive: false,

    _focused: function() {

        if(this._gol.activeElement !== this.element){
            this._hideOptions();
        }

        this._positionOptionsBox();

        if (this.options.isEnabled) {
            this._isActive = true;
            this._hasBeenEscaped = false;
        }

    },

    _lastEnteredTerm: null,
    _isCached: false,
    _hasBeenEscaped : false,

    _lastKeyPressDwnUpScroll : new Date(),

    _lastFoundMatch : null,
    _findMatchInCache : function( txt ){

        this._lastFoundMatch = null;
        if(typeof this._localCache[this.options.dictionary] === "undefined"){
			return false;
		}

        var limit = "l" + (this.options.maxListLimit || "n");
        if(typeof this._localCache[this.options.dictionary][limit] === "undefined"){
			return false;
		}

		var cnt = 0;
        while(txt.length > 1 || cnt<10){
            txt = txt.substr(0,txt.length-1);
			var cacheObj = this._localCache[this.options.dictionary][limit][txt];
            if( cacheObj ){
				if(!cacheObj.isCompleteList){
					break;
				}
				else{
					this._lastFoundMatch = txt;
					return true;
				}
            }
			cnt++;
        }

        return false;

    },

    _cloneMatchObject : function( orgData ){
        var obj = {};
        obj.matchedText = orgData.matchedText.toString();
        obj.matches = orgData.matches.join("~~**~~**~~").split("~~**~~**~~");
        obj.isCompleteList = ( orgData.isCompleteList === true);
        return obj;
    },

	_timerTriggerEnterEvent : null,
	_clearTriggerEnterEvent : function(){
	    this._timerTriggerEnterEvent = null;
	},
	_triggerEnterEvent : function(){
	    var that = this;
	    if(!this._timerTriggerEnterEvent){
		    this.element.trigger("ncbiautocompleteenter", this.sgData);
		    this.element.trigger("ncbiautocompletechange", this.sgData);
			this._sgSend();
			this._timerTriggerEnterEvent = window.setTimeout(function(){that._clearTriggerEnterEvent();},10);
		}
	},

    _keyPress : function(e){
        if (e.keyCode === this._keys.enter) {
            if( this._isActive && this._isOptionsBoxOpen()){
                if(this._currIndex===this.options.maxListLimit && jQuery("li:eq(" + this._currIndex + ")", this._gol.optionsList).hasClass("ui-ncbiautocomplete-show-more")){
                    this._hideOptions( true );
                    this._gotoShowAll();
                }
                else if( this.sgData.optionSelected === jQuery(this.element).val() ){
                    if(!this._timerTriggerEnterEvent){
                        this.sgData.selectionAction = "enter";
                        this._hideOptions( true );
                        this._triggerEnterEvent();
                    }
                    e.stopPropagation();
                    e.preventDefault();
                    return false;
                }
            }
        }
        else if (e.keyCode === this._keys.tab) {
			this.sgData.selectionAction = "tab";
			this._sgSend();
            this._isOptionsBoxFocused = false;
            this._isActive = false;
            this._hideOptions(true);
        }
        else if(jQuery(this.element).val().length === 0){
            this._hideOptions(true);
        }
        else if (e.keyCode === this._keys.up) {
            this._scrollUpDownRateLimiter( -1 );
        }
        else if (e.keyCode === this._keys.down && !e.shiftKey) {
            this._scrollUpDownRateLimiter( 1 );
        }
    },

    _scrollUpDownRateLimiter : function(dir){
        if(new Date() - this._lastKeyPressDwnUpScroll < 90) return;
        this._moveSelection( dir );
        this._lastKeyPressDwnUpScroll = new Date();
    },

    _keyUp: function(e) {

        if (!this.options.isEnabled || this._hasBeenEscaped) {
            return;
        }

        var txt = jQuery(this.element).val().toLowerCase().replace(/^\s+/,"").replace(/\s+$/," ");
        var limit = "l" + (this.options.maxListLimit || "n");

        if (e.keyCode === this._keys.up) {
            //this._moveSelection( - 1);
        }
        else if (e.keyCode === this._keys.down) {
            //this._moveSelection(1);
        }
        else if (e.keyCode === this._keys.enter) {

        }
        else if (e.keyCode === this._keys.tab) {
            this._isOptionsBoxFocused = false;
            this._isActive = false;
            this._hideOptions(true);
        }
        else if (e.keyCode === this._keys.escape) {
            this._isOptionsBoxFocused = false;
            this._isActive = false;
            this._hasBeenEscaped = true;
            this._hideOptions();
        }
        else if(this._lastEnteredTerm === txt || e.keyCode === this._keys.shift){//Shift is needed for shift tab
          //do nothing
        }
        else if( this._localCache[this.options.dictionary] && this._localCache[this.options.dictionary][limit] && this._localCache[this.options.dictionary][limit][txt] ){
            this._isActive = true;
            this._lastEnteredTerm = txt;
            this._displayOptions(this._localCache[this.options.dictionary][limit][txt]);
        }
        else if( this._isCached && this._findMatchInCache(txt)){
            this._isActive = true;
            this._localCache[this.options.dictionary][limit][txt] = this._cloneMatchObject(this._localCache[this.options.dictionary][limit][this._lastFoundMatch]);
            this._lastEnteredTerm = txt;
            this._filteredCache(txt);
        }
        else if (txt.length >= this.options.minLength ) {

            if (!this._isActive) {
                this._isActive = true;
                this._focused();
            }
            if (this._lastEnteredTerm === txt) {
                return;
            }
            this._lastEnteredTerm = txt;
            this._fecthOptions(txt);
        }
        else {
            this._hideOptions();
        }

    },

    _webSerivcePath : null,
    _setWebServicePath : function(){

        var userPath = this.options.webserviceUrl;

        var wsUrl = "";
        
        if(this.options.isCrossDomain){
            this._webSerivcePath = "http://www.ncbi.nlm.nih.gov/portal/utils/autocomp.fcgi";
        }
        else{
            if(!this.options.isUrlRelative){
                wsUrl = window.location.protocol + "//" + window.location.host;
                if(!userPath.charAt(0)==="/"){
                    userPath = "/" + userPath;
                }
            }
            this._webSerivcePath = wsUrl + userPath;                    
        }
    },

    _fecthOptions: function(txt) {

        if (this._activeRequest !== null) {
            this._activeRequest.abort();
        }

        var that = this;
        var format = (this.options.responseFormat === "old") ? "text": "json";
        var data =  {
                        dict: this.options.dictionary,
                        q: txt
                    }        
        if(this.options.isCrossDomain){
            this._addGlobalListener("NSuggest_CreateData");
            jQuery.ajax({
                  url: this._webSerivcePath,
                  dataType: 'script',
                  data: data,
                  //success: success,
                  cache: true
            }); 
        }
        else{
            this._activeRequest = jQuery.get(this._webSerivcePath, data,
            function(x, y) {
                that._handleResponse(x, y);
            },
            format);        
        }

    },

    _handleResponse: function(data, status) {

        this._activeRequest = null;
        if (status === "success") {
            if (this.options.responseFormat === "old") {
                this._oldFormat(data);
            }
            else {
                this._displayOptions(data);
            }
        }
        else{
            this.turnOff();
        }
    },
    
    _addGlobalListener: function( methName ){
        var that = this;
        window[ methName ] = function( txt, matches, cacheStatus){
            var dataObj = {
                "matchedText": txt,
                "matches": matches,
                "isCompleteList": (cacheStatus === 1)
            };
            that._displayOptions(dataObj);
        };    
    },

    _oldFormat: function(data) {

		if(data.indexOf("_dictionary_error") !== -1){
            this.turnOff(true);
        }
		else if(jQuery.trim(data).indexOf("NSuggest_CreateData") !== -1){

            var that = this;
            //Due to the chrome debugger I need to make this in global scope.
            //To prevent conflicts with old version, I changed fn name to new
            //and replaced it in the web service response - EZ-5180
			//function NSuggest_CreateData( txt, matches, cacheStatus){			
            this._addGlobalListener("NSuggest_CreateData_new");
			eval(data.replace("NSuggest_CreateData","NSuggest_CreateData_new"));

		}
		else{
			this._hideOptions();
		}

    },

    
    
    _localCache: {},
    
    _putInCache: function( dataObj ){   
        var limit = "l" + (this.options.maxListLimit || "n");  
        if(!this._localCache[this.options.dictionary]) this._localCache[this.options.dictionary] = {};
        if(!this._localCache[this.options.dictionary][limit]) this._localCache[this.options.dictionary][limit] = {}
        if(!this._localCache[this.options.dictionary][limit][this._lastEnteredTerm]) this._localCache[this.options.dictionary][limit][this._lastEnteredTerm] = dataObj;    
    },
    
    

    _displayOptions: function(dataObj) {

        this._putInCache( dataObj );
        
		if(dataObj.matchedText !== this._lastEnteredTerm ){
			dataObj.isCompleteList = true;
			this._filteredCache(this._lastEnteredTerm, dataObj);
			return;
		}

		this.sgData.userTyped = this._lastEnteredTerm;
		this.sgData.optionsCount = dataObj.matches.length;

        this._positionOptionsBox();

        var that = this;

        if(dataObj.matchedText === "_dictionary_error"){
            this.turnOff(true);
            return;
        }

        var matchedText = dataObj.matchedText;
        var matches = dataObj.matches;
        this._isCached = dataObj.isCompleteList;

        var strOut = dataObj.previousFormat;

        if(!strOut){

            var maxListLimit = this.options.maxListLimit;
            var remainingNumber = 0;
            if(maxListLimit===null || matches.length<=maxListLimit){
                var startPoint = matches.length;
            }
            else{
                startPoint = maxListLimit;
                remainingNumber = matches.length - startPoint
            }

            //Fix the text entered so it can form a valid regular expression
            var regChars = /([\^\$\\\?\(\)\[\]\*\+\{\}\|\/\.\,])/g;
            var termText = this._lastEnteredTerm || "";
            var regText = termText.replace(regChars,"\\$1");

            var lastTextRegExp = new RegExp("(" + regText + ")","i");
            var highMatches = new Array(matches.length);
            var hasRelatedMatches = this.options.hasRelatedMatches;
            for (var i = startPoint - 1; i >= 0; i--) {

                var optText = matches[i];
                var optVal = "";

                var indLoc = optText.indexOf("@")
                if(indLoc !== -1){
                    optVal = 'valueId="' + optText.substr(indLoc+1) + '"';
                    optText = optText.substr(0,indLoc);
                    hasRelatedMatches = true;
                }

                highMatches[i] = "<li role='menuitem' " + optVal + ">" + optText.replace(lastTextRegExp, "<span>$1</span>").replace(/\\"/g,'"') + "</li>";
            }

            if(remainingNumber>0){
                highMatches.push( "<li class='ui-ncbiautocomplete-show-more' role='menuitem' moreOption='true'>See all results</li>" );
            }

            strOut = highMatches.join("");

            if(highMatches.length === 0 || (!hasRelatedMatches && strOut.indexOf("<span>") === -1) ){
                this._hideOptions();
                return;
            }

            dataObj.previousFormat = strOut;

        }

        var box = jQuery(this._gol.optionsBox);
        var list = jQuery(this._gol.optionsList);

        list.scrollTop(0);
        list.html(strOut);

        if (box.css("display") !== "block" && this._isActive) {
            box.css("display","block");
            if(this._gol.isIE6){
                var ifr = jQuery(this._gol.optionsBox.iframe).css("display","block");
                ifr.height( ifr.height() + jQuery(this._gol.optionsActions).height());
            }
        }
        else if(!this._isActive){
            this._hideOptions();
        }

        var lis = jQuery(".ui-ncbiautocomplete-options li");
        var h = lis.eq(0).outerHeight() || 20;
        var sz = h * lis.length;
        var overflow = "hidden";
        if(!this.options.maxListLimit){
            sz=218;
            overflow = "auto";
        }
        lis.closest(".ui-ncbiautocomplete-holder").height(sz).css("min-height", sz + "px");
        lis.closest(".ui-ncbiautocomplete-options").height(sz).css("overflow-y", overflow);

        var boxWidth = box.width();

        jQuery("li", list).hover(function() {
            that._addHightlightMouse(this);
        },
        function() {
            that._removeHighlight( "mouse" );
        }).click(function() {
            that._optionClicked(this);return false;
        });

        jQuery(list).mouseout(
            function(){
                that._removeHighlight( "mouse" );
                that._resetCurrentIndex();
            }
        );

        this._resetCurrentIndex();

        var li = jQuery("li",box);

		var boxWidth = jQuery(this._gol.optionsBox).width();

		//Move this check out to another section so it does not have to be run EVERY TIME
		var supportsEllipse = (jQuery.browser.msie && !this._gol.isIE6) || jQuery.browser.safari || jQuery.browser.opera;

		var fLi = li.eq(0);
		var pLeft = parseFloat(fLi.css("padding-left"));
		var temp = fLi.html();

		fLi.html("<span>MMMMMMMMM.</span>");
		var avgCharWidth = (fLi.children("span:first").width())/10;
		fLi.html("<span>&hellip;<span>");
		var hellipWidth = fLi.children("span:first").width();
		fLi.html(temp);

		var listWidth = list.width();

		var maxChars = Math.floor( (boxWidth) / avgCharWidth);

        li.each( function(){
            var elem = jQuery(this);

			elem.css("white-space","nowrap");
            var txt = elem.html();

            elem.attr("fullText",txt)
				.attr("acValue",txt.replace(/<\/?span>/gi,""));

			var elmWidth = elem.width();
			var charWidth = (elmWidth / elem.attr("acValue").length);
			if( charWidth < 9){
				if(supportsEllipse){
                    elem.attr("partialText",elem.html())
						.attr("isIEExpand",true)
						.css("width", (listWidth - 28) + "px");
				}
				else{

					var spanOpen = txt.toLowerCase().indexOf("<span>");
					var spanClosed = txt.toLowerCase().indexOf("</span>");

					function spanCut( cutLoc ){

						var addCloseSpan = false;

						if(cutLoc>=spanOpen && cutLoc<spanOpen+6){
							var diff = cutLoc-spanOpen;
							cutLoc = spanOpen + diff;
							addCloseSpan = true;
						}

						if(cutLoc>=spanClosed && cutLoc<spanClosed+7){
							var diff = cutLoc - spanClosed;
							cutLoc = spanClosed + diff;
						}

						return { "cutLoc" : cutLoc, "addSpan" : addCloseSpan };

					}

					var upper = txt.length;

					function fitText( curLoc, min, max, lastText, lastGood ){

						var isItem = txt.indexOf("Journal of magnetic") === 0;

						var loc = spanCut( curLoc );
						var displayText = txt.substr(0,loc.cutLoc) + (loc.addCloseSpan?"</span>":"") + "&hellip;";
						elem.html(displayText);

						if(displayText === lastText){

							if(elem.height() !== newHeight && typeof lastGood !== "undefined"){
								displayText = lastGood;
							}

					        elem.attr("partialText", displayText)
							    .css("white-space", "nowrap");

						}
						else{

							if( elem.height() === newHeight ){
								var newLastGood = displayText.toString() + "";
								var x = Math.floor( (loc.cutLoc + max) / 2);
						        fitText(x, loc.cutLoc, max, displayText, newLastGood);
							}
							else{
								var xDwn = Math.floor( (parseInt(loc.cutLoc) + parseInt(min)) / 2);
								fitText(xDwn, min, loc.cutLoc, displayText, lastGood);
							}

						}

					}

					elem.css("white-space","normal").css("width", (listWidth - 22) + "px");
					var orgHeight = elem.height();
					elem.html(".");
					var newHeight = elem.height();
					elem.html(txt);

					if(orgHeight > newHeight){
					    fitText( maxChars, 0, txt.length );
					}
				}
			}

        });

        //Moved the scrollTop code here since FF3 - JSL-396
        list.scrollTop(0);
		this._removeHighlight( "mouse" );
        this._hideOptionsIfOffPage();

    },

    _filteredCache: function(txt, data) {

        var matchObj;
        var showOptions = true;
        var limit = "l" + (this.options.maxListLimit || "n");

		if(txt === null){
		    showOptions = false;
		}
		else{

            if(data){

			    matchObj = data;

			    if(typeof this._localCache[this.options.dictionary] === "undefined"){
			        this._localCache[this.options.dictionary] = {};
			    }
                else if(typeof this._localCache[this.options.dictionary][limit] === "undefined"){
                    this._localCache[this.options.dictionary][limit] = {};
                }

		    }
		    else{
                matchObj = this._localCache[this.options.dictionary][limit][txt];
		    }

		    matchObj.matchedText = txt;

            if(!this.options.hasRelatedMatches){

                var regChars = /([\^\$\\\?\(\)\[\]\*\+\{\}\|\/\.\,])/g;
                var regText = txt.replace(regChars,"\\$1");

                var matchRE = new RegExp(regText,"ig");
                //Make a new copy
                var orgMatches = matchObj.matches.join("!!!~~~!!!").split("!!!~~~!!!");
                for (var i = orgMatches.length - 1; i >= 0; i--){
                    if (orgMatches[i].match(matchRE) === null) {
                        var c = orgMatches.splice(i, 1);
                    }
                }
                matchObj.matches = orgMatches;

                this._localCache[this.options.dictionary][limit][txt] = matchObj;
                matchObj = null;

                showOptions = (orgMatches.length > 0);

            }
		}

        if (showOptions) {
            this._displayOptions(this._localCache[this.options.dictionary][limit][txt]);
        }
        else {
            this._hideOptions();
        }

    },

    _removeHighlight: function( evtName) {
        if(evtName === "mouse"  && new Date() - this._lastKeyPressDwnUpScroll < 100){
            return;
        }
        var that = this;
        jQuery("li", this._gol.optionsList).removeClass("ui-ncbiautocomplete-options-high").each( function(){ that._collapseOption(this); });
    },

    _addHightlightMouse: function(elem) {

        if(new Date() - this._lastKeyPressDwnUpScroll > 100){
            this._resetCurrentIndex();
            this._currIndex = jQuery(elem).prevAll("li").length;
            this._addHightlight(elem);
        }

    },

    _addHightlight: function(elem) {
        this._removeHighlight();
        jQuery(elem).addClass("ui-ncbiautocomplete-options-high");

        var that = this;
        if(this._expandTimer){
            window.clearTimeout(this._expandTimer);
        }
        this._expandTimer = window.setTimeout( function(){ that._expandOption(elem); }, this.options.expandPauseTime);
    },

    _expandOption: function( elem ){
        if(jQuery(elem).hasClass("ui-ncbiautocomplete-options-high")){
            var elem = jQuery(elem).attr("isExpanded",true);
            var fText = elem.attr("fullText");
            if(elem.html() !== fText || elem.attr("isIEExpand") ){
                elem.html(fText)
					.css("text-overflow","")
					.css("white-space","normal")
					.css("-o-text-overflow","");
                this._scrollIntoView();
            }
        }
    },

    _collapseOption: function(elem){
        var elem = jQuery(elem);

		if(elem.attr("isIEExpand")){
			elem.css("text-overflow","ellipsis")
				.css("white-space","nowrap")
				.css("-o-text-overflow","ellipsis");
		}
		else{
	        elem.css("white-space","nowrap")
				.html(elem.attr("partialText"));
		}

    },

    _optionClicked: function(elem) {
        elem = jQuery(elem);

        if(elem.hasClass("ui-ncbiautocomplete-show-more")){
            this._gotoShowAll();
        }
        else{
            var txt = this._cleanUpSelectionText(elem);
            var valueId = elem.attr("valueId") || null;
            this.sgData.optionSelected  = txt;
            this.sgData.optionIndex = this._currIndex;
            this.sgData.valueId = valueId;
            this.sgData.selectionAction = "click";
            this._sgSend();

            var txtBox = jQuery(this.element).val(txt);
            txtBox.attr("valueid", valueId);
            txtBox.trigger("ncbiautocompleteoptionclick", this.sgData);
            txtBox.trigger("ncbiautocompletechange", this.sgData);
        }
        this._isOptionsBoxFocused = false;
        this._hideOptions();
    },

    _isOptionsBoxOpen: function(){
        return jQuery(this._gol.optionsBox).css("display") === "block";
    },

    _hideOptions: function( isForceClose ) {

        this._gol.activeElement = null;

        if (this._isOptionsBoxFocused && !isForceClose) return;

        var box = jQuery(this._gol.optionsBox);
        if(isForceClose){
            jQuery(this._gol.optionsBox).css("display","none");
        }

        //might screw with annimation in IE.6
        if(this._gol.isIE6){
            jQuery(this._gol.optionsBox.iframe).css("display","none");
        }

        this._resetCurrentIndex();
        this._isActive = false;
        this._lastEnteredTerm = null;

        var box = jQuery(this._gol.optionsBox);
        var list = jQuery(this._gol.optionsList);

        box.css("display","none");
        list.html("");

    },

    _setTextCursorToEnd: function(txt) {
        var elem = this.element;
        jQuery(elem).val(txt);
        if (elem.createTextRange) {
            var text = elem.createTextRange();
            text.moveStart('character', elem.value.length);
            text.collapse();
            text.select();
        }
    },

    _resetCurrentIndex: function() {
        this._currIndex = -1;
    },

    _cleanUpSelectionText : function( li ){
        return jQuery.trim(li.attr("acValue")).replace(/&amp;/gi,"&").replace(/\\"/g,'\"');
    },

    _currIndex: -1,
    _moveSelection: function(dir) {

        if (!this._isActive) {
            return;
        }

		this.sgData.usedArrows = true;

        var options = jQuery("li", this._gol.optionsList);
        var cnt = options.length;
        this._currIndex += dir;
        this._removeHighlight();

        var txt = this._lastEnteredTerm;
        var optValue = "";

        if (txt === null || txt.length < 2) {
            return;
        }

        if (this._currIndex < 0 || this._currIndex >= cnt) {
            if (this._currIndex === -2) {
                this._currIndex = cnt - 1;
            }
            else {
                this._currIndex = -1;
            }
        }

        if (this._currIndex !== -1) {

            var li = jQuery("li:eq(" + this._currIndex + ")", this._gol.optionsList);
            this._addHightlight(li);

            if(this._currIndex===this.options.maxListLimit){
                this.sgData.optionSelected  = "";
                this.sgData.valueId = "";
            }
            else{
                txt = this._cleanUpSelectionText( li );
                optValue = li.attr("valueId") || null;
                this.sgData.optionSelected  = txt;
                this.sgData.valueId = optValue;
            }

        }
        else{
       		this.sgData.optionSelected  = "";
            this.sgData.valueId = "";
        }

        this._setTextCursorToEnd(txt);
        this._scrollIntoView();

        this.sgData.optionIndex = this._currIndex;

    },

    _scrollIntoView: function() {

        var li = jQuery("li:eq(" + this._currIndex + ")", this._gol.optionsList);
        if (li.length === 0) return;

        var box = jQuery(this._gol.optionsList)

        var boxHeight = box.height();
        var boxTop = box.scrollTop()
        var offset = li[0].offsetTop;
        var itemHeight = li.height();

        if (offset - boxHeight + itemHeight > boxTop) {
            var nextLi = jQuery("li:eq(" + (this._currIndex + 1) + ")", this._gol.optionsList);
            var offsetNext = (nextLi.length === 1) ? nextLi[0].offsetTop: offset + itemHeight + 10;
            box.scrollTop(offsetNext - boxHeight);
        }
        else if (offset < boxTop) {
            box.scrollTop(offset);
        }

    },

    turnOff: function(isError) {
        this._setOption("isEnabled", false);
        this._isOptionsBoxFocused = false;
        this._hideOptions();
        if(!isError){

            //Ping that it was turned off
            if(typeof ncbi !== "undefined" && ncbi.sg && ncbi.sg.ping){
                ncbi.sg.ping( this.element[0], "autocompleteoffclick", ("dictionary=" + this.options.dictionary) );
            }

            var dUrl = this.options.disableUrl;
            if (dUrl!==null && dUrl.length > 0) {                
                var fnc = jQuery.ui.jig._getFncFromStr(dUrl);                
                if(typeof fnc === "function"){
                    fnc();
                }
                else{
                    jQuery.get(dUrl);
                }
            }
        }
    },

    _prefLinkClick: function(){
        if(typeof ncbi !== "undefined" && ncbi.sg && ncbi.sg.ping){
            ncbi.sg.ping( this.element[0], "autocompleteprefclick", ("dictionary=" + this.options.dictionary) );
        }
    },

    _checkClickEvent : function(e){

        if(!this._isActive){
            return;
        }

        if(jQuery(this.element)[0] == e.target){
            return;
        }
        else{
            this._isOptionsBoxFocused = false;
            this._isActive = false;
            this._hideOptions();
        }
    },

	_sgSend : function(){
		if(typeof ncbi !== "undefined" && typeof ncbi.sg !== "undefined" && typeof ncbi.sg.ping !== "undefined"){
		    ncbi.sg.ping( this.sgData, false );
		}
	},

	getSgData : function(){
	    return this.sgData;
	},

    _gotoShowAll : function(){
        var txtBox = jQuery(this.element)
        var term = txtBox.val();
        var dict = this.options.dictionary;
        txtBox.trigger("ncbiautocompleteshowall", { "value" : term, "dictionary" : dict} );
    },

    _hideOptionsIfOffPage : function () {

        if(!this.options.maxListLimit) return;

        var that = this;
        var lis = jQuery(".ui-ncbiautocomplete-options li");
        var h = lis.eq(0).outerHeight() || 20;

        var holder = jQuery(this._gol.optionsBox);
        var winHeight = jQuery(window).height();
        var posTop = holder.position().top;
        var totlalH = posTop + holder.outerHeight() - (document.body.scrollTop || document.documentElement.scrollTop || window.pageYOffset || 0);

        if (lis.last().hasClass("ui-ncbiautocomplete-show-more")) {

            var lookupId = posTop + "-" + winHeight + "-" + totlalH;

            if (!this.choppedDetails) {
                this.choppedDetails = {};
            }

            var details = this.choppedDetails[lookupId];

            var cnt = 0;

            function checkHeight() {
                //debugger;
                totlalH = posTop + holder.outerHeight() - (document.body.scrollTop || document.documentElement.scrollTop || window.pageYOffset || 0);
                if (totlalH > winHeight) {
                    lis = jQuery(".ui-ncbiautocomplete-options li");
                    var ind = lis.length - 2;
                    if (ind >= 0) {
                        var li = lis.eq(ind);
                        li.remove();
                        sz = h * (lis.length - 1);
                        //debugger;
                        lis.closest(".ui-ncbiautocomplete-holder").height(sz).css("min-height", sz + "px");
                        lis.closest(".ui-ncbiautocomplete-options").height(sz).css("overflow-y", "hidden");
                        cnt++;
                        checkHeight();
                    }
                }
                else {
                    that.choppedDetails[lookupId].count = cnt;
                }
            }


            if (details) {

                var count = details.count;

                if (count > 0) {
                    var length = details.length;
                    var diff = (2 * length) - count - lis.length - 2;

                    var removeLis = lis.filter(":lt(" + (lis.length - 1) + ")");
                    removeLis.filter(":gt(" + diff + ")").remove();

                    lis = jQuery(".ui-ncbiautocomplete-options li");
                    sz = h * (lis.length);
                    lis.closest(".ui-ncbiautocomplete-holder").height(sz).css("min-height", sz + "px");
                    lis.closest(".ui-ncbiautocomplete-options").height(sz).css("overflow-y", "hidden");
                }

                cnt = count;
            }
            else {

                this.choppedDetails[lookupId] = {
                    count: 0,
                    length: lis.length
                }

            }
            checkHeight();
        }

        else {

            var totalH = Math.floor( (winHeight - posTop) / h) * h;

            var neededH = lis.length * h;
            if(totalH> neededH){
                totalH = neededH;
            }

            lis.closest(".ui-ncbiautocomplete-holder").height(totalH).css("min-height", totalH + "px");
            lis.closest(".ui-ncbiautocomplete-options").height(totalH).css("overflow-y", "auto");

        }

    }

});

//This is so we only have one element on the page and not one for each control.
jQuery.ui.ncbiautocomplete._globalOptionsList = null;

jQuery.widget("ui.ncbiaccordion", {
    _create: function() {
    
        if(this.element.hasClass("jig-accordion")){
            if(typeof console !== "undefined" && console.warn){
                console.warn("The classname widget identifier jig-accordion has been depreciated, please change it to jig-ncbiaccordion");
            }
        }
    
        var newObj = {};
        jQuery.extend(newObj, this.options, jQuery.ui.accordion.prototype.options );
        this.element.accordion( newObj );
    }
});
jQuery.widget("ui.ncbitabs", {
    _create: function() {
        // Delegate to the native tabs widget. 
        // Extend the native widget's options hash.
        
        if(this.element.hasClass("jig-tabs")){
            if(typeof console !== "undefined" && console.warn){
                console.warn("The classname widget identifier jig-tabs has been depreciated, please change it to jig-ncbitabs");
            }
        }
        
        var newObj = {};
        jQuery.extend(newObj, this.options, jQuery.ui.tabs.prototype.options );
        this.element.tabs( newObj );
    }
});

jQuery.widget("ui.ncbigrid", {

    options : {
        /*PAGING*/
        isPageable : false,
        pageSize : 5,
        currentPage : 1,
        isPageToolbarHideable : true,

        /*SORTING*/
        isSortable : false,
        columnTypes : [],
        sortRowIndex : 0,
        isPresorted : false,
        sortColumn : -1,
        sortColumnDir : 1,
        titleAscending : "sort ascending",
        titleDescending : "sort descending",

        /*SCROLLING*/
        isScrollable : false,
        width : null,
        height : "10em",
        usesPercentage : true,

        /* Filter Control */
        hasFilterControl : false,
        filterIsCaseInsensitive : false,
        filterColumnIndex : null,
        filterIsInverse : null,    
        filterAsUserTypes : false,

        /*Common Options*/
        isLoadingMessageShown : false,
        loadingText : "Loading",
        isZebra : true

    },

    _init : function(){

        /*PAGING*/

        /*SORTING*/

        /*SCROLLING*/

    },

    _create : function(){


        this._cleanUpLegacyMarkUp();

        /*PAGING*/
        if(this.options.isPageable){
            this.options.currentPage-=1;
            this.createPagingElements();
            this.addPagingEvents();
            this.addPagingListeners();
            this._gotoPage( this._getCurrentPage() );
        }

        /*SORTING*/
        if(this.options.isSortable){
            this.applySortClasses();
            this.createHeaderLinks();
            this.addHeaderEvents();
            this.sortDefaults();
        }

        /*SCROLLING*/

        //if(jQuery(this.element).hasClass("jig-ncbigrid-scroll")){
        jQuery(this.element).addClass("ui-ncbigrid");
        //}

        this.options.isScrollable = this.options.isScrollable || jQuery(this.element).hasClass("jig-ncbigrid-scroll") || this.element.parent().hasClass("jig-ncbigrid-scroll");

        if(this.options.isScrollable){
            this._setupScrollMarkup();

            /* I really hate this, but there is nothing I can do about it! */
            if(jQuery.browser.mozilla){
                this._setupScrollFirefox();
            }
            else if(jQuery.browser.safari){
                this._setupScrollSafariChrome();
            }
            else if((jQuery.browser.msie && parseInt(jQuery.browser.version) >=8) || jQuery.browser.opera){
                this._setupScrollIE8();
            }
            else if(jQuery.browser.msie && parseInt(jQuery.browser.version) <8){
                this._setupScrollIE6_7();
            }

            this._addResetScrollListener();
            if(this.options.isPageable){
                this.setPagingElementsWidth();
            }

            if(!jQuery.browser.mozilla){
                var that = this;
                jQuery.ui.jig.registerPageHeightWatcher();
                jQuery(document.body).bind("ncbijigpageheightchanged", function(){ that.forceRedraw(); } );
            }

        }
        else{
            jQuery(this.element).addClass("ui-ncbigrid-nonscroll");
        }

        
        
        /* Filter Control */
        if(this.options.hasFilterControl){
            this._addFilterControl();
        }
        
        
        
        /*Common Functionality*/

        /* Loading Bar Message */
        this._addLoadingBar();

        /*Zebra Striping*/
        this._makeZebraStripped();
        this._addZebraWatcher();

        this._addCaptionCurves();

        /*Click handling */
        this._addRowClickWatcher();

        this.element.trigger("ncbigridpreready");

    },

    destroy : function(){

        /*PAGING*/
        if(this.options.isPageable){
            this.removePagingEvents();
            this.removePagingElements();
            this.removePagingListeners();
        }

        /*SORTING*/
        if(this.options.isSortable){
            this.removeSortClasses();
            this.removeHeaderEvents();
            this.removeHeaderLinks();
        }

        /*SCROLLING*/
        if(this.options.isScrollable){
             /* I really hate this, but there is nothing I can do about it! */
            if(jQuery.browser.mozilla){
                this._destroyScrollFirefox();
            }
            else if(jQuery.browser.safari || jQuery.browser.opera){
                this._destroyScrollSafariChrome();
            }
            else if(jQuery.browser.msie && parseInt(jQuery.browser.version) >=8){
                this._destroyScrollIE8();
            }
            else if(jQuery.browser.msie && parseInt(jQuery.browser.version) <8){
                this._destroyScrollIE6_7();
            }
            this._removeResetScrollListener();
            this._destroyScrollMarkup();
        }
        
        if(this.options.hasFilterControl){
            this._destroyFilterControl();
        }

        jQuery.Widget.prototype.destroy.apply(this, arguments);

    },


    //With redesign of code, the markup has changed to use only one wrapper element.
    //So devs do not need to change their code this will make the changes for them.
    _cleanUpLegacyMarkUp : function(){

        var parElem = this.element.parent();

        if(parElem.hasClass("ui-ncbigrid-inner-div")){
            var wrapPar = parElem.parent();
            var clonedChildren = parElem.children().clone( true );
            var clonedTable = parElem.children().eq(0).unwrap();
            this.element = clonedTable;
        }

        parElem = this.element.parent();
        if(parElem.hasClass("jig-ncbigrid-scroll") || this.element.hasClass("jig-ncbigrid-scroll")){
            this.element.addClass("ui-ncbigrid-scroll");
        }
        if(parElem.hasClass("jig-ncbigrid")){
            this.element.addClass("ui-ncbigrid");
        }

        if(this.options.isPagable){
            this.options.isPageable = this.options.isPagable;
        }

    },



    /*PAGING*/

    controlsHTML : '\
                     \
                    <div class="ui-ncbigrid-paged-toolbar">\
                        \
                        <div class="ui-ncbigrid-paged-gotoControl">\
                        Go To Page <input type="text" value="1" size="2"/>\
                        </div>\
                        \
                        <div class="ui-ncbigrid-paged-countItems">\
                        Items <span class="ui-ncbigrid-paged-startRow">1</span> - <span class="ui-ncbigrid-paged-endRow">10</span>\
                        </div>\
                        \
                        <div class="ui-ncbigrid-paged-pageControl">\
                        <button>previous</button>\
                        Page <span class="ui-ncbigrid-paged-startPage">1</span> of <span class="ui-ncbigrid-paged-endPage">2</span>\
                        <button>next</button>\
                        </div>\
                        \
                    </div>',


    createPagingElements : function(){

        this.controlBar = jQuery( this.controlsHTML ).insertAfter( this.element );  //This will screw up in scrollable, need to do a check!
        this._showHidePageToolbar();
        
        //TODO: Destroy this

        this._addResizeListener();
        this.setPagingElementsWidth();

    },
    
    _showHidePageToolbar : function(){
        
        if(this.options.isPageToolbarHideable){
            if(this.getRowCount() <= this.options.pageSize){
                this.controlBar.hide();
            }
            else{
                this.controlBar.show();
            }
        }
    },

    _addResizeListener : function(){

        var that = this;
        jQuery(window).resize( function(){ that.setPagingElementsWidth(); });

        //Mozilla does not fire the onresize event with text resize [aka mousewheel]
        if(jQuery.browser.mozilla){

            jQuery(window).bind( 'DOMMouseScroll',
                function(e){
                    if(e.ctrlKey){
                        window.setTimeout( function(){that.setPagingElementsWidth() },5);
                    }
                }
            );

            jQuery(document).keypress(
                function(e){
                    if(e.ctrlKey && (e.which===48 || e.which===96) ){
                        window.setTimeout( function(){that.setPagingElementsWidth() },5);
                    }
                }
            );

        }


        var wonderfulResizeBug = function(){
                window.setTimeout( function(){that.setPagingElementsWidth(); },10);
                window.setTimeout( function(){that.setPagingElementsWidth(); },500);
        }

        this.element.bind("ncbigridpreready", wonderfulResizeBug)
                    .bind("ncbigridupdated", wonderfulResizeBug)
                    .bind("ncbigridupdated", function(){ that._showHidePageToolbar();});

    },

    setPagingElementsWidth : function(){

        if(!this.options.isPageable){
            return;
        }
    
        var isIE6 = jQuery.browser.msie && parseFloat(jQuery.browser.version)<7;
        if(isIE6){
            if(this.options.isScrollable){
                newWidth = this.element.parent().outerWidth();
            }
            else{
                newWidth = this.element.outerWidth();
            }
        }
        else{
            if(!jQuery.browser.mozilla && this.element.parent().hasClass("ui-ncbigrid-scroll")){
                var outWidth = this.element.parent().outerWidth();;
                var innWidth = this.element.parent().width();
            }
            else{
                outWidth = this.element.outerWidth();
                innWidth = this.element.width();
            }

            var diffTable = outWidth - innWidth;
            var diffControl = this.controlBar.outerWidth() - this.controlBar.width();
            var diff = diffControl - diffTable;

            newWidth = innWidth - diff;
        }

        this.controlBar.width( newWidth );

    },

    removePagingElements : function(){
        this.controlBar.remove();
        this.controlBar = null;
    },

    addPagingEvents : function(){

        var that = this;

        var buttons = this.controlBar.find("div.ui-ncbigrid-paged-pageControl button");
        buttons.eq(0).click(
            function(e){
                that.gotoPrevPage();
                e.preventDefault();
            }
        );
        buttons.eq(1).click(
            function(e){
                that.gotoNextPage();
                e.preventDefault();
            }
        );

        this.element.parent().find("div.ui-ncbigrid-paged-gotoControl input" ).keypress(
            function(e){
                if(e.keyCode === 13){
                    that.gotoPage(this.value);
                    e.preventDefault();
                }
            }
        );

    },

    removePagingEvents : function(){
         this.controlBar.find("div.ui-ncbigrid-paged-pageControl button").unbind("click");
         this.controlBar.find("div.ui-ncbigrid-paged-gotoControl input").unbind("keypress");
    },

    addPagingListeners : function(){
        var that = this;
        var gotoPage1 = function(){
            that.gotoPage(1);
        };
        this.element.bind("ncbigridcolumnsorted", gotoPage1 );
    },

    removePagingListeners : function(){
        this.element.unbind("ncbigridcolumnsorted");

    },

    setPage : function(){

        var currentPage = this._getCurrentPage();
        var maxPage = this.getMaxPage();

        this.updateRows(currentPage, maxPage);
        this._updatePagingMargin();
        this.updateButtons(currentPage, maxPage);
        this.updatePagingText(currentPage, maxPage);
        this.updateGoto(currentPage, maxPage);
        this.updateItemRange(currentPage, maxPage);
        this._notifyGridUpdated();

    },

    updateRows : function( currentPage ){

        /* this has to be reset first or IE6/7 positions header wrong */
        this.element.trigger("ncbigridresetscroll");

        var pageSize = this.options.pageSize;
        var rowCount = this.getRowCount();
        var start = currentPage * pageSize;
        var end = start + pageSize;

        var filterSelector = "";
        if(end<rowCount){
            filterSelector += ":lt(" + end + ")";
        }
        if(start>0){
            filterSelector += ":gt(" + (start - 1) + ")";
        }

        var trs = jQuery("tbody tr:not(.ncbigrid-row-filtered)", this.element);
        var filteredTrs = trs;
        //If there is no filterSelector, that means we have less rows than the pageSize
        if(filterSelector.length > 0){
            trs.not(".ui-ncbigrid-rowHidden").addClass("ui-ncbigrid-rowHidden");
            filteredTrs = trs.filter(filterSelector);
        }
        filteredTrs.removeClass("ui-ncbigrid-rowHidden");

        this._notifyGridUpdated();

    },

    updateButtons : function( currentPage, maxPage ){

        var prevDisabled = (currentPage===0);
        var nextDisabled = (currentPage===maxPage-1);

        var buttons = this.controlBar.find("div.ui-ncbigrid-paged-pageControl button");
        buttons.eq(0).attr( "disabled", prevDisabled );
        buttons.eq(1).attr( "disabled", nextDisabled );

    },

    updatePagingText : function( currentPage, maxPage ){

        var pageControl = this.controlBar.find("div.ui-ncbigrid-paged-pageControl");
        pageControl.find("span.ui-ncbigrid-paged-startPage").html( currentPage + 1 );
        pageControl.find("span.ui-ncbigrid-paged-endPage").html( maxPage );

    },

    updateGoto : function( currentPage ){

        this.controlBar.find("div.ui-ncbigrid-paged-gotoControl input").val( currentPage + 1 );

    },

    updateItemRange : function( currentPage, maxPage ){

        var pageSize = this.options.pageSize;
        var rowCount = this.getRowCount();

        var start = currentPage * pageSize;
        var end = start + pageSize;

        if(end > rowCount){
            end = rowCount;
        }

        var pageControl = this.controlBar.find("div.ui-ncbigrid-paged-countItems");
        pageControl.find("span.ui-ncbigrid-paged-startRow").html( start + 1 );
        pageControl.find("span.ui-ncbigrid-paged-endRow").html( end );

    },

    _getCurrentPage : function(){
        return this.options.currentPage;
    },
    getCurrentPage : function(){
        return this._getCurrentPage() + 1;
    },    
    getMaxPage : function(){
        return Math.ceil(this.getRowCount() / this.options.pageSize );
    },
    getRowCount: function(){
        //TODO: Account for filtered rows aka tr:not(td....filtered...)
        return jQuery(this.element).find("tbody tr:not(.ncbigrid-row-filtered)").length;
    },

    gotoFirstPage : function(){
        this._gotoPage( 0 );
    },
    gotoLastPage : function(){
        this._gotoPage( this.getMaxPage() );
    },
    gotoNextPage: function(){
        this._gotoPage( this._getCurrentPage() + 1 );
    },
    gotoPrevPage: function(){
        this._gotoPage( this._getCurrentPage() - 1 );
    },
    gotoPage: function( pageNumber_IndexStartsAtOne ){
        this._gotoPage( parseInt(pageNumber_IndexStartsAtOne, 10) - 1 );
    },
    _gotoPage : function( pageNum ){

        if( isNaN(pageNum) || pageNum < 0 ){
            pageNum = 0;
        }
        else if(pageNum >= this.getMaxPage()){
            pageNum = this.getMaxPage()-1;
        }

        this.options.currentPage = pageNum;

        this.setPage();

    },

    _stndHeightTbody : null,
    _hasSpacerSet : false,
    _updatePagingMargin : function(){

        if(this._stndHeightTbody===null){
           this._stndHeightTable = this.element.outerHeight();
           this._stndHeightTbody = this.element.find("tbody:eq(0)").outerHeight();
           this._stndScrollHeight = this.element.find("tbody:eq(0)")[0].scrollHeight;
        }

        if(jQuery.browser.mozilla){
            if(this.options.isScrollable){
                this._updatePagingMarginFirefox_Scroll();
            }
            else{
                this._updatePagingMarginFirefox_NonScroll();
            }
        }
        else{
            if(this.options.isScrollable){
                this._updatePagingMarginIE6_7_Scroll();
            }
            else{
                this._updatePagingMarginIE6_7_NonScroll();
            }
        }


    },
    _updatePagingMarginFirefox_Scroll : function(){

        var stndOffsetHeight = this._stndScrollHeight;
        var tbody = this.element.find("tbody:eq(0)");
        var spacer = tbody.find("tr.ui-ncbigrid-spacerElement");

        var currHeight = tbody.height();
        var scrollHeight = tbody[0].scrollHeight;

        var spacerDiff = this._stndHeightTbody-currHeight;

        if(currHeight===scrollHeight && this.getMaxPage()==this._getCurrentPage()+1 && this._getCurrentPage()!==0){
            if(spacer.length===0){
                var tdCnt = this.getColumnCount( true );
                var cell = "<td>&nbsp;</td>";
                var cellString = cell;
                for(var i = tdCnt;i>1;i--){
                    cellString += cell;
                }
                spacer = jQuery("<tr class='ui-ncbigrid-spacerElement'>" + cellString + "</tr>");
                tbody.append(spacer);
            }
            else{
                spacer.show();
            }
            spacer.height(spacerDiff);
            tbody.css("overflow-y","hidden");
        }
        else if(spacer.length===1){
           tbody.css("overflow-y","scroll");
           spacer.hide().remove();
        }

    },

    _updatePagingMarginFirefox_NonScroll : function(){

        if(this.options.isScrollable) return;

        var stndHeight = this._stndHeightTbody;
        if(!stndHeight) return;

        var table = this.element;
        var tbody = table.find("tbody:eq(0)");
        var tfootH = table.find("tfoot th:eq(0)").outerHeight() + 3;

        var currHeight = table.outerHeight();
        var spacerDiff = stndHeight-currHeight;
        var hasSpacer = this._hasSpacerSet;

    },


    _updatePagingMarginIE6_7_Scroll : function(){

        var stndOffsetHeight = this._stndScrollHeight;
        var tbody = this.element.find("tbody:eq(0)");
        var spacer = tbody.find("tr.ui-ncbigrid-spacerElement");

        var currHeight = tbody.height();
        var wrapperHeight = this.element.parent().outerHeight();
        var scrollHeight = this.element.parent()[0].scrollHeight;

        var theadH = this.element.find("thead:eq(0)").outerHeight() || 0;
        var tfootH = this.element.find("thead:eq(0)").outerHeight() || 0;
        var captH = this.element.find("caption:eq(0)").outerHeight() || 0;

        var spacerDiff = wrapperHeight-currHeight - theadH - tfootH - captH - 8;

        if(spacerDiff>0 && this.getMaxPage()==this._getCurrentPage()+1 && this._getCurrentPage()!==0){
            if(spacer.length===0){
                var tdCnt = this.getColumnCount( true );
                var cell = "<td>&nbsp;</td>";
                var cellString = cell;
                for(var i = tdCnt;i>1;i--){
                    cellString += cell;
                }
                spacer = jQuery("<tr class='ui-ncbigrid-spacerElement'>" + cellString + "</tr>");
                tbody.append(spacer);
            }
            else{
                spacer.show();
            }
            spacer.height(spacerDiff);
            tbody.css("overflow-y","hidden");
        }
        else if(spacer.length===1){
           tbody.css("overflow-y","scroll");
           spacer.hide().remove();
        }

    },

    _updatePagingMarginIE6_7_NonScroll : function(){

        if(this.options.isScrollable) return;

        //debugger;
        var stndHeight = this._stndHeightTbody; //this._stndHeightTable;
        if(!stndHeight) return;

        var table = this.element;
        var tbody = table.find("tbody:eq(0)");
        var currHeight = tbody.outerHeight();

        var theadH = table.find("thead:eq(0)").outerHeight() || 0;
        var captH = table.find("caption:eq(0)").outerHeight() || 0;

        var tRowH = 0;
        if(parseFloat(jQuery.browser.version)<7){
            tRowH = tbody.find("tr:has(td:visible)").height() || 0;
        }

        var spacerDiff = stndHeight-currHeight-theadH-captH-3+tRowH;

        var hasSpacer = this._hasSpacerSet;

        if(spacerDiff> 0 && !hasSpacer && this.getMaxPage()==this._getCurrentPage()+1 && this._getCurrentPage()!==0){
            table.css("margin-bottom", spacerDiff );
            this._hasSpacerSet = true;
        }
        else if(hasSpacer){
             table.css("margin-bottom", 0);
             this._hasSpacerSet = false;
        }
        else{
             this._stndHeightTbody = currHeight;
        }

    },

    /*SORTING*/
    applySortClasses : function(){

        var columnTypes = this.options.columnTypes;

        if(columnTypes.length===0){
            jQuery("th", this.element).addClass("sortNone");
        }
        else{

            var addTHClasses = function(ind){
                if(columnTypes[ind] !== "none"){
                    jQuery(this).addClass("sortNone");
                }
            }

            jQuery("thead th", this.element).each( addTHClasses );
            jQuery("tfoot th", this.element).each( addTHClasses );

        }

    },

    removeSortClasses : function(){
        jQuery("th", this.table).removeClass("sortNone sortAsc sortDsc");
    },

    createHeaderLinks : function(){

        var titleText = this.options.titleAscending;
        var colType = this.options.columnTypes;
        var hasType =  colType.length>0;

        var formatCell = function(ind){
            if(!hasType ||  colType[ind] !== "none"){
                var cell = jQuery(this);
                cell.data("colIndex",ind);
                var cellHTML = cell.html() || cell.text();
                cell.html("<a href='#'>" + cellHTML + "</a>").find("a").attr("title", titleText );
            }
        }

        /* Need to process seperately so indexs are right */
        jQuery(this.element).find("thead tr").eq( this.options.sortRowIndex ).find("th").each( formatCell );
        jQuery(this.element).find("tfoot tr").eq( this.options.sortRowIndex ).find("th").each( formatCell );

    },

    removeHeaderLinks : function(){
        jQuery("th").each(
            function(){
                var cell = jQuery(this);
                cell.removeData("colIndex");
                var link = cell.find("a");
                var cellHTML = link.html() || link.text();
                cell.html( cellHTML );
            }
        );
    },

    addHeaderEvents : function(){
        var that = this;
        jQuery("th a", this.element).mousedown(function(){ that._sort( jQuery(this).parent().data("colIndex") ); return false;} ).click( function(){ return false; } );
    },

    removeHeaderEvents : function(){
        jQuery("th a", this.element).die("click");
    },

    sort: function(columnNumber , direction) {

        //TODO: Validation check to make sure we are in the range of columns here

        columnNumber--;
        this._sort(columnNumber, direction);
    },

    _sort: function( elementOrIndex, direction ) {

        this.element.trigger("ncbigridshowloadingbar");

        this._killActiveSort();
        if(!this.isPageable)this.element.trigger("ncbigridresetscroll");

        var spacer = this.element.find("tbody tr.ui-ncbigrid-spacerElement");
        if(spacer.lenght===1){
            spacer.remove();
        }

        if( typeof elementOrIndex === "object" ){
            var column = elementOrIndex;
            var index = column.data("colIndex");
        }
        else{
            column = jQuery("thead th").eq(elementOrIndex);
            index = elementOrIndex;
        }

        this._setColumnDirection(index, direction)

        this._sortData();

    },

    _killActiveSort : function(){
        this._killSort = true;
        if(this._sortTimer)window.clearTimeout(this._sortTimer);
        this._sortTimer = null;
    },

    _setColumnDirection: function( index, direction ){

        var hasDirection = typeof direction !== "undefined" && direction !== null;
        direction = hasDirection ? direction : 1;

        var prevSortColumn = this.options.sortColumn;

        if(prevSortColumn===index){
            if(!hasDirection){
                direction = this.options.sortColumnDir * -1;
            }
        }
        else if(prevSortColumn!==-1){
            var prevTheadTH = jQuery(this.element).find("thead:not(.cloned):not([cloned]) tr").eq( this.options.sortRowIndex ).find("th").eq(prevSortColumn);
            var prevTfootTH = jQuery(this.element).find("tfoot tr").eq( this.options.sortRowIndex ).find("th").eq(prevSortColumn);
            prevTheadTH.add(prevTfootTH).addClass("sortNone").removeClass("sortAsc sortDsc").find("a").attr("title",this.options.titleAscending);
            jQuery(this.element).find("tbody tr:not(.ui-ncbigrid-spacerElement) td:nth-child(" + (prevSortColumn+1) + ")").removeClass("sorted");
        }

        this.options.sortColumn = index;
        this.options.sortColumnDir = direction;

        var sortClass = direction===1?"sortAsc":"sortDsc";
        var sortText = direction===1?this.options.titleDescending:this.options.titleAscending;

        var theadTH = jQuery(this.element).find("thead:not(.cloned):not([cloned]) tr").eq( this.options.sortRowIndex ).find("th").eq(index);
        var tfootTH = jQuery(this.element).find("tfoot tr").eq( this.options.sortRowIndex ).find("th").eq(index);
        theadTH.add(tfootTH).removeClass("sortAsc sortDsc sortNone").addClass( sortClass ).find("a").attr("title",sortText);

        var tbodyTds = jQuery(this.element).find("tbody tr:not(.ui-ncbigrid-spacerElement) td:nth-child(" + (index+1) + ")").addClass("sorted");

    },

    _killSort : true,

    _sortData : function(){

        this._killSort = false;

        var tbody = jQuery("tbody",this.element);

        var columnIndex = this.options.sortColumn;
        var sortColumnDir = this.options.sortColumnDir;
        var columnType = this.getColumnType(columnIndex + 1);

        var that = this;

        var clonedRows = [];
        jQuery("tr", tbody).each(
            function(){

                var row = jQuery(this);
                var cellData = that._columnTypeConversion( row.find("td").eq(columnIndex), columnType );
                clonedRows.push( { data : cellData, row: row.clone() });

            }
        );

        if (typeof columnType === "function") {
            clonedRows = clonedRows.sort(function(a, b) {
                return columnType.call(that, a, b, sortColumnDir);
            });
        }
        else {
            clonedRows = clonedRows.sort(function(a, b) {
                return that._defaultSortFnc(a, b, sortColumnDir);
            });
        }

        //var newTbody = jQuery("<tbody/>");
        var newTbody = tbody.clone().children().remove().end()//.css("display","none");

        var loopDeLoop = function(){

            var count = 0;
            while(clonedRows.length>0 && count< 10){
                if(that._killSort) break;
                var row = clonedRows.shift();
                newTbody.append(row.row);
                count++;
            }

            if(!that._killSort){
                if(clonedRows.length>0){
                    that._sortTimer = window.setTimeout( function(){loopDeLoop();}, 0);
                }
                else{
                    tbody.after(newTbody);
                    tbody.remove();
                    //newTbody.css("display","");
                    that.element.trigger("ncbigridcolumnsorted",  [that.options.sortColumn + 1, that.options.sortColumnDir]).trigger("ncbigridhideloadingbar");
                    if(!that.options.isPageable){
                        that._notifyGridUpdated();
                    }
                }
            }
            else{
                that._killSort = false;
            }

        };

        loopDeLoop();

    },

    _defaultSortFnc: function(a, b, sortColumnDir) {
        return (a.data > b.data) ? sortColumnDir : (a.data < b.data) ? -sortColumnDir : 0;
    },

    _columnTypeConversion: function(cell, columnType, isHTMLNeeded) {

        var str = "";
        if(isHTMLNeeded){
            str = cell.html();
        }
        else{
            str = cell.text();
        }

        str = jQuery.trim(str);

        switch (columnType) {
        case "int":
            aCell = parseInt(str, 10);
            break;
        case "float":
        case "num":
            aCell = parseFloat(str);
            break;
        case "date":
            aCell = new Date(str);
            break;
        case "str-sensitive":
            aCell = str;
            break;
        case "str-insensitive":
            aCell = str.toLowserCase();
            break;
        default:
            aCell = str.toLowerCase();
            break;
        }

        return aCell;

    },

    getColumnType : function( ind ){
        return this.options.columnTypes[ ind ] || "str";
    },

    sortDefaults : function(){
        if(this.options.sortColumn !== -1){
            var that = this;
            var presort = function(){
                that.sort(that.options.sortColumn, that.options.sortColumnDir);
            }
            this.element.bind("ncbigridpreready", presort);
        }
    },




    /*SCROLLING*/


    _setupScrollMarkup : function(){

        var table = jQuery(this.element);

        if( !table.parent().hasClass("jig-ncbigrid-scroll") ){
            table.wrap("<div class='ui-ncbigrid-scroll'></div>");
            table.data("addedWrapper",true);
        }
        else{
            table.parent().addClass("ui-ncbigrid-scroll");
        }

        var cellPadding = table.attr("cellpadding") || "";
        var cellSpacing = table.attr("cellspacing") || "";

        if( cellPadding.length===0 || cellSpacing.length===0 ){
            table.data("addedCellPadding", true);
            table.attr("cellpadding", "0");
        }

        if(cellSpacing.length===0){
            table.data("addedCellSpacing", true);
            table.attr("cellspacing", "0");
        }

        //if( !(this.options.usesPercentage && jQuery.browser.mozilla)) {

        if( !this.options.usesPercentage || jQuery.browser.msie ){
            //We assume ems if width is not set in the config
            var cssWidth =  this.options.width ||
                            //(this.options.usesPercentage ? table[0].style.width : 0) ||
                            (typeof table[0].currentStyle !== "undefined") ?
                                table[0].currentStyle["width"] :
                                jQuery( parseFloat( table[0].style.width || table.css("width") ,10) ).toEm();

            if(cssWidth){
                table.css("width","100%").data("ncbigrid_orgWidth", cssWidth);
                table.parent().css( "width" , cssWidth );
            }
        }

        if(this.options.scrollHeight){
            this.options.height = this.options.scrollHeight;
        }


        //REMEBER TO ADD DESTROY FOR THIS PROP WHEN CODED CORRECTLY!
        var cssHeight = this.options.height;


        if(jQuery.browser.mozilla){

            var tbody = jQuery("tbody",table);

            var capHeight = jQuery("caption",table).height() || 0 ;
            var headHeight = jQuery("thead",table).height() || 0;
            var footHeight = jQuery("tfoot",table).height() || 0;

            var pxHeight = capHeight + headHeight + footHeight;
            var desiredHeight = parseFloat(this.options.height);

            if(this.options.height && this.options.height.toString().indexOf("em")>0){
                var emHeight = parseFloat( jQuery(pxHeight).toEm() );
                var adjHeight = (desiredHeight - emHeight) + "em";
            }
            else{
                adjHeight = ( desiredHeight ) + "px";
            }

            tbody.css("max-height", adjHeight);
            tbody.css("height", adjHeight);

        }
        else{
            table.css("height","auto");
            table.parent().css( "height" , cssHeight );
        }

    },

    _setupScrollFirefox : function(){
        var table = jQuery(this.element);
        table.parent().addClass("firefox");
        jQuery("thead tr th:last-child", table).css("border-right","0px");
    },

    _setupScrollSafariChrome : function(){

        var table = jQuery(this.element);

        var wrapper = table.parent();
        wrapper.addClass("sa_ch");

        var thead = jQuery("thead",table);
        var thsHead = jQuery("th",thead);

        var caption = jQuery("caption",table);
        thead = jQuery("thead",table);
        thsHead = jQuery("th",thead);
        var tfoot = jQuery("tfoot",table);
        var thsFoot = jQuery("th",tfoot);
        var tbody = jQuery("tbody",table);
        var tds = jQuery("td",tbody);

        var newThead = thead.clone().prependTo(table).attr("cloned","true");
        var newTfoot = tfoot.clone().appendTo(table).attr("cloned","true");

        //JSL-1084 - since caption is hidden for some strange reason
        //var captionPos = caption.position();
        //var captionH = caption.height() || 0;
        //var cpt = caption.css("padding-top") || 0;
        //var cpb = caption.css("padding-bottom") || 0;
        
        //JSL-1084 - since caption is hidden for some strange reason
        var captionPos = caption.position();
        var captionH =  0;
        var cpt = 0;
        var cpb = 0;        

        var theadPos = thead.position();
        var theadH = thead.height();
        var tfootH = thead.height();
        var cols = jQuery("col",table);

        var newFootPosition = wrapper.position().top + wrapper.height() - tfootH + 1;

        var pos = thead.position();

        var theadLeft = (pos && pos.left)?pos.left : 0;

        var hasCaption = false;
        //JSL-1084 - since caption is hidden for some strange reason
        //if(caption.length>0){
        //    caption.css("position","absolute").css("top", (captionPos.top-.5) + "px").width(table.width()).height(captionH + parseInt(cpt,10) + parseInt(cpb,10));
        //    hasCaption = true;
        //}
        thead.css("position","absolute").css("top",(newThead.position().top + captionH) + "px").css("left",(theadLeft)+"px").height(captionH);
        var hasFooter = false;
        if(tfoot.length>0){
            tfoot.css("position","absolute").css("top",wrapper.height()-1 + "px").css("left",(theadLeft)+"px").height(tfootH);
            hasFooter = true;
        }

        var firstCells = jQuery("tr:eq(0) td", tbody);
        //var paddingTop = firstCells.eq(0).css("padding-top");
        //var paddingTop_adj = parseFloat(paddingTop) + parseFloat(captionH);
        //firstCells.attr("orgPaddingTop", paddingTop).css("padding-top",paddingTop_adj);

        var newTheadCells = jQuery("th",newThead);

        var numCols = newTheadCells.length;
        var ths = newThead.find("th");
        var thsPadTop = parseInt(ths.eq(0).css("padding-top"), 10);
        var lastRowColor = null;

        var resizeTable = function(){
        
        
            thsHead.each(
                function( ind ){
                    var adj = (ind+1===numCols)? 0: 1 ;
                    var width = newTheadCells.eq(ind).width() + adj;
                    jQuery(this).width( width );
                    if(hasFooter){
                        thsFoot.eq(ind).width( width );
                    }
                }
            );
        
            var diff = wrapper.width() - table.outerWidth();
            if(diff !== 16 && diff !== 15){  //16 on windows and 15 on mac!
                    wrapper.css("min-width",table.outerWidth() + 16);
                    newThead.css("min-width",table.outerWidth() + 16);
                    if(hasFooter){
                        newTfoot.css("min-width",table.outerWidth() + 16);
                    }
                    if(hasCaption){
                        caption.css("min-width",table.outerWidth() - 16);
                    }
            }

            if(hasCaption){
                caption.css("top", (wrapper.position().top+1) + "px");
                caption.width(table.width());
            }

            var wrapperPos = wrapper.position();
            var divPositionTop = wrapperPos.top;
            var theadPositionLeft = newThead.position().left;

            if(hasFooter){
                var newFootPosition2 = divPositionTop + wrapper.height() - newTheadCells.eq(0).outerHeight() + 1;
                tfoot.css("top",newFootPosition2 + "px");
            }

            thead.css("position","absolute").css("top", (divPositionTop + caption.height()) + "px").css("left", theadPositionLeft + "px");

            if(hasCaption){
                var cellH = firstCells.eq(0).height();
                var padH = thsPadTop + cellH;

                if(padH>0){
                    ths.css("padding-top", (thsPadTop + cellH ) + "px");
                }
            }

            if(lastRowColor){
                jQuery("tbody tr td.lastRowGrid", table).removeClass("lastRowGrid").css("border-bottom-color", lastRowColor);
            }

            var lastRow = jQuery("tbody tr:has(td:visible):last-child td", table);
            lastRowColor = lastRow.css("border-bottom-color");
            lastRow.css("border-bottom-color", lastRow.css("background-color")).addClass("lastRowGrid");
            
            

        }

        this._resizeListener = function(){ resizeTable(); };
        this.forceRedraw = this._resizeListener;
        this.renderingWatcher();

        jQuery(window).resize( this._resizeListener );
        jQuery(this.element).bind("ncbigridupdated", this._resizeListener);

        //counter is for Safari on mac since it sometimes shows up as 0 with rendering.
        var testCount = 0;
        function testPosition(){ //when loading, dynamic content above can screw up poisiotning, this fixes that bug
            var ths = jQuery(table).find("thead");

            if(ths.length == 2 && ths.eq(0).position().top !== ths.eq(1).position().top){
                resizeTable();
            }
            else if(ths.eq(0).position().top==0 && testCount<10){
                window.setTimeout( testPosition, 10);
                testCount++;
            }

        }
        jQuery(window).load( testPosition );

        function checkRows(){
            if(table.find("tbody td").length === 0){
                window.setTimeout( checkRows, 5);
            }
            else{
                resizeTable();
            }
        }
        checkRows();

    },

    forceRedraw : function(){
        //if a browser implementation needs this, they will assign it
    },

    //Some browsers have timing issues with drawing dynamic content after grid has adjusted once, this helps resolve this problems.
    renderingWatcher : function(){

        var timer = false;
        var that = this;
        var isHidden = false;

        var isChrome = jQuery.browser.safari;

        var _lastTop = null;

        var testPage = function(){

            if(timer){
                window.clearTimeout(timer);
            }

            var body = jQuery(document.body);
            var startHeight = body.height();

            var cnt = 0;
            function checkPageHeight(){
                if( startHeight < body.height() ){
                    startHeight = body.height();
                    that.forceRedraw();
                }

                if(cnt<100){
                    timer = window.setTimeout( checkPageHeight, 100 );
                }

                if(isChrome){
                    if(cnt===1){
                        var thead = that.element.find("thead");
                        if(thead.length==2){
                            if(thead.eq(0).find("tr").width() !== thead.eq(1).find("tr").width() ){
                                thead.hide();
                                isHidden = true;
                            }
                        }
                    }
                    else if(cnt===2 && isHidden){
                        that.element.find("thead").show();
                        that.forceRedraw();
                        isHidden = false;
                    }
                }
                else if(cnt===13){
                    that.element.hide();
                    window.setTimeout( function(){ that.element.show(); }, 2 );
                    cnt=100;
                }

                cnt++;

            }
            checkPageHeight();
        }

        jQuery(window).resize(testPage);
        testPage();


    },


    _setupScrollIE8 : function(){

        var that = this;

        function ie8Scrollable(isResize, isPartialReset) {


            var table = jQuery(that.element);
            var caption = jQuery("caption:visible", table).not(newCaption);

            var wrapper = table.parent();

            var thead = jQuery("thead.orginal", table);             
            thead.css("display","none");

            if(thead.length===0) return;
            var tfoot = jQuery("tfoot", table);
            var tbody = jQuery("tbody", table);

            var firstRowTds = tbody.find("tr:not(.ui-ncbigrid-rowHidden):has(td:visible):eq(0) td");
            var lastRowTds = tbody.find("tr:not(.ui-ncbigrid-rowHidden):has(td:visible):last td");

            var headThs = jQuery("th", thead);
            var footThs = jQuery("th", tfoot);

            var headWidths = [];
            var footWidths = [];

            that.ie8Reset = function( isPartialReset ){

                var tb = jQuery(this.element).find("tbody");
                var tdsT = tb.find("td[addedTopPadding=true]").attr("addedTopPadding","false");
                var orgTP = tdsT.eq(0).attr("orgPaddingTop");

                var tdsB = tb.find("td[addedBotPadding=true]").attr("addedBotPadding","false");
                var orgBP = tdsB.eq(0).attr("orgPaddingBot");
                tdsB.css("padding-bottom",orgBP);

                if(isPartialReset){
                    return;
                }

            };

            if(isResize){
                that.ie8Reset();
            }

            var adjHeight = thead.outerHeight();

            headThs.eq(0).attr("orgHeight",headThs.eq(0).height());
            footThs.eq(0).attr("orgHeight",footThs.eq(0).height());

            var clonedThead = that.element.find("thead.clone");
            var clonedThs=  clonedThead.find("th");
            var pos = clonedThead.position();

            var posBody = that.element.find("tbody").position();
            var posTable = that.element.position();

            var leftPos = (pos && pos.left) ? pos.left : 0;
            var topPos = (pos && pos.top) ? pos.top : 0;

            //Following code is for the genome bug where bar was placed wrong with dynamic content
            if(pos.top > posBody.top){
                leftPos = posBody.left;
                topPos = posBody.top - headThs.eq(0).height() - 1;
            }
            if(pos.top === 0 ){//&& !that.genFix){
                thead.css("display","none");
                function checkPos(){
                    if(that.element.position().top===0){
                        that.genFix = window.setTimeout( checkPos, 1 );
                    }
                    else{
                        ie8Scrollable();
                    }
                }
                checkPos();

            }
            //end genome

            var adjScrY = 0;
            if(topPos>0){
                adjScrY = that.options.isSortable?0:1;
                thead.css("position", "absolute").css("left", (leftPos) + "px").css("top", (topPos - adjScrY) + "px");
            }

            thead.find("th").each( function( ind ){

                var adj = 0;
                if(ind === 0){
                    if(that.element.width % 2 === 0){
                        adj = 1;//ind>0?1:0;
                    }
                    else{
                        adj = .5//ind%2==0?1:0;
                    }
                }
                

                var clonedTH = clonedThs.eq(ind);
                jQuery(this).width( clonedTH.outerWidth() -.6 + adj - parseFloat(clonedTH.css("padding-left")) ).height( clonedTH.height() + 4);

            });

            var pagebarAdj = (that.options.isPageable)?2:3;

            var tfootHeight = tfoot.outerHeight();
            tfoot.css("position", "absolute")
                .css("left", (leftPos - 1) + "px")
                .css("top", topPos + wrapper.outerHeight() - thead.outerHeight() - tfoot.outerHeight() + pagebarAdj );

            var adj = function(){ return 0; };

            var adj = function( obj, ind, where){
                var cell = firstRowTds.eq(ind);
                var x = cell.outerWidth() - 1;
                return x
            };

            //headThs.each( function(ind){ jQuery(this).width( adj(this, ind ) ).height( adjHeight ); } );
            footThs.each( function(ind){ jQuery(this).width( adj(this, ind ) ).height( adjHeight ); } );

            var tdPaddingBot = lastRowTds.eq(0).css("padding-bottom");
            tdPaddingBot = tdPaddingBot || 0;
            lastRowTds.eq(0).attr("orgPaddingBot",tdPaddingBot);
            tdPaddingBot = parseInt(tdPaddingBot) - 1;

            if( tfoot.length>0 && wrapper.height() < tbody.height() + tfoot.height() ){
                var paddingBotHeight = (tfoot.height() + parseInt(tdPaddingBot, 10)) + "px";
                lastRowTds.css("padding-bottom", paddingBotHeight).attr("addedBotPadding","true");
            }

            //TODO: Need to make the -2 work with the border widths
            //newCaption.css("top",wrapper.position().top + "px").css("left",wrapper.position().left + "px").width(caption.outerWidth()-2).attr("cloned","true");
            if(newCaption.length>0){
                newCaption.css("top",wrapper.position().top + "px").css("left",wrapper.position().left + "px").attr("cloned","true");
                var captWidth = caption.width();
                if(captWidth){
                    newCaption.width( captWidth );
                }
            }

            thead.css("display","block");            
            table.trigger("ncbigridieresize");

        }

        var newCaption = jQuery("caption:visible",this.element).clone().prependTo(this.element).css("position","absolute");
        jQuery(this.element).parent().addClass("ie8");
        ie8Scrollable();

        var thead = this.element.find("thead");
        var newThead = thead.clone().addClass("clone").appendTo(this.element);
        thead.addClass("orginal").hide();

        this.resetScroll = ie8Scrollable;
        this._resizeListener = function(){ ie8Scrollable(true); }
        jQuery(window).resize(
            function(){
                that._resizeListener();
                window.setTimeout( that._resizeListener, 20);
            }
        );
        jQuery(this.element).bind("ncbigridcolumnsorted", this._resizeListener ).bind("ncbigridupdated", this._resizeListener).bind("ncbigridpreready", this._resizeListener);
        this.forceRedraw = this._resizeListener;
        this.renderingWatcher();

    },

    _setupScrollIE6_7 :  function(){


        var table = jQuery(this.element);
        var wrapper = table.parent();
        wrapper.addClass("ie6_7");

        if(parseFloat(jQuery.browser.version)<7){
            var firstTag = document.all[0].text;
            if(firstTag.indexOf("<?xml") !== -1){
                wrapper.addClass("ie6_docIgnored");
            }
        }

        var caption = jQuery("caption",table);
        var x = caption.html();
        caption.css( "height", caption.outerHeight() );
        caption.html("<div class='ui-ncbigrid-scroll-caption'>" + x + "</div>");

        var thead = table.find("thead");

        var clonedThead = thead.clone(true).addClass("orginal").appendTo(table);
        thead.addClass("cloned");

        var th1 = thead.find("th");
        clonedThead.find("th").each(
            function(ind){
                var colIndex = th1.eq(ind).data("colIndex");
                if(colIndex!==null){
                    jQuery(this).data("colIndex",colIndex);
                }
            }
        );

        if(jQuery.browser.version >= 7){
            table.width(table.width() - 14);
            var lastTH = jQuery("thead.orginal tr th:last-child, tfoot tr th:last-child");
            lastTH.css("border-right-color",lastTH.css("background-color"));
            if(caption.length===0){
                thead.children().height( lastTH.height() + 1).css("top","-1px");
            }
        }
        else{
            var firstTH = jQuery("thead.orginal tr th:first-child",table);
            firstTH.css("padding-left", ( (parseInt( firstTH.css("padding-left") , 10 ) || 0 ) + 1 ) + "px" );
        }

        //Weird issue where footer shifts a pixel when filter is applied. Shifts back when removed.
        var that = this;
        var redraw = function(amt){ window["_scrollredrawIssueHeight"] = amt; that.element.hide();that.element.show(); };
        this.element.bind('ncbigridfilterapplied', function(){ redraw(-1); }).bind('ncbigridfilterremoved', function(){ redraw(0); });

        this.forceRedraw = function(){ redraw(0); setTimeout( redrawHeader, 100); }

        function resize(){

            table[0].style.width = "100%";
            table.width(table.outerWidth() - 6);

            var ths1 = clonedThead.find("th");
            var ths2 = thead.find("th");

            ths2.each( function( ind ){
                jQuery(this).css("width","auto");
            });

            ths2.each( function( ind ){
                jQuery(this).width( ths1.eq(ind).width() );
            });

            that.element.trigger("ncbigridieresize");

        }
        if(this.options.usesPercentage){
            jQuery(window).resize( resize );
            this.renderingWatcher();
        }

        function redrawHeader( isForced ){
            if(wrapper[0].scrollTop>0){
                that.element.find("thead").hide();
                window.setTimeout( function(){ that.element.find("thead").show(); }, 2 );
                if(isForced){
                    window.setTimeout( redrawHeader, 130 );
                }
            }
        }

        that.element.bind("ncbigridcolumnsorted",redrawHeader);

        that.renderingWatcher();
        if(parseFloat(jQuery.browser.version)==7){
            resize();
        }

    },

    _destroyScrollMarkup : function(){

        var table = jQuery(this.element)

        if( table.data("addedWrapper") ){
            var width = table.data("ncbigrid_orgWidth");
            if(width){
                table.css("width",width);
            }
            table.unwrap();
        }

        if(table.data("addedCellPadding")){
            table.removeAttr("cellpadding");
        }

        if(table.data("addedCellSpacing")){
            table.removeAttr("cellspacing");
        }

        this.element.removeData("addedWrapper")
                    .removeData("ncbigrid_orgWidth")
                    .removeData("addedCellPadding")
                    .removeData("addedCellSpacing");

    },

    _destroyScrollFirefox : function(){
        var table = jQuery(this.element);
        table.parent().addClass("firefox");

        var lastCell = jQuery("thead tr th:last-child", table);
        lastCell.css("border-right","1px");
    },

    _destroyScrollSafariChrome : function(){

        var table = jQuery(this.element);
        table.parent().addClass("sa_ch");
        jQuery("caption",table).css("position","").css("top","").width("").height("");
        jQuery("thead",table).not("[cloned=true]").remove().end().removeAttr("cloned");
        jQuery("tfoot",table).not("[cloned=true]").remove().end().removeAttr("cloned");

        //TODO MOVE TO COMMON
        if(this._resizeListener){
            jQuery(window).unbind("resize", this._resizeListener );
            this._resizeListener = null;
        }

    },

    _destroyScrollIE8 : function(){

        var table = jQuery(this.element);
        table.parent().addClass("ie8");
        jQuery("caption[cloned=true]").remove();

        //TODO MOVE TO COMMON
         if(this._resizeListener){
            jQuery(window).unbind("resize", this._resizeListener );
            this._resizeListener = null;
        }

        this.ie8Reset();


    },

    _destroyScrollIE6_7 : function(){
        jQuery(this.element).parent().removeClass("ie6_7");
    },

    _addResetScrollListener : function(){
        var that = this;
        this.element.bind("ncbigridresetscroll", function(){ that.resetScrollPosition(); });
    },
    _removeResetScrollListener : function(){
        this.element.unbind("ncbigridresetscroll");
    },

    resetScrollPosition : function(){

        if(jQuery.browser.mozilla){
            var scrollElem = jQuery(this.element).find("tbody")[0];
        }
        else{
            scrollElem = jQuery(this.element).parent()[0];
        }

        scrollElem.scrollTop = 0;

    },


    /* Filter Control */

    _filterControlMarkUp : '<div class="filter"><label for="filter-list">Filter:</label><input id="filter-list" type="text" /><button>Search</button></div>',
    
    _addFilterControl : function(){
        var wrapper = this.element.parent();
        var placementElement = this.element;
        if( wrapper.length>0 && (wrapper.hasClass("ui-ncbigrid-scroll") || wrapper.hasClass("ui-ncbigrid") ) ){
            placementElement = wrapper;
        }
        
        this._filterControl = jQuery(this._filterControlMarkUp).insertBefore(placementElement);
        
        var that = this;

        this._filterControl.find("button").click( function(){ that._runFilter(); } );
        if(this.options.filterAsUserTypes){
            this._filterControl.find("input").keyup( function(e){ that._runFilter(); } );
        }
        else{
            this._filterControl.find("input").keyup( function(e){ if(e.keyCode===13)that._runFilter(); } );
        }
        
    },

    _destroyFilterControl : function(){
        if(this._filterControl){
            this._filterControl.find("button").unbind("click").end().find("input").unbind("keyup").end().remove();
        }
    },
    
    _runFilter : function(){
        var txt = this._filterControl.find("input").val();
        var isCaseInsensitive = this.options.filterIsCaseInsensitive || null;
        var col = this.options.filterColumnIndex || null;
        var isInverse = this.options.filterIsInverse || null;
        this.removeFilterRows();
        if(txt.length>0){
            this.filterRows(txt, isCaseInsensitive, col, isInverse);
        }
    },




    /* Common Private Methods */

    _addCaptionCurves : function(){
        jQuery(this.element).find("caption").addClass("ui-corner-top");
        if(jQuery.browser.safari){
            this.element.find("caption").css("margin-right", "-1px");
        }        
    },

    _makeZebraStripped : function( ){

        if(this.options.isZebra){
            var visibleRows = jQuery(this.element).find("tbody tr:has(td:visible):not(.ui-ncbigrid-spacerElement)");
            visibleRows.filter(":even").addClass("ui-ncbigrid-row-even").removeClass("ui-ncbigrid-row-odd");
            visibleRows.filter(":odd").addClass("ui-ncbigrid-row-odd").removeClass("ui-ncbigrid-row-even");
        }

    },

    _addZebraWatcher : function (){
        var that = this;
        this.element.bind( "ncbigridupdated", function(){ that._makeZebraStripped() });
    },

    _removeZebraWatcher : function (){  //TODO Code this  //Add to destroy

    },

    _addRowClickWatcher : function (){
        var that = this;
        jQuery(this.element).find("tbody").click(
            function(obj){
                var cell = jQuery(obj.originalTarget).closest("td");
                var row = cell.closest("tr");
                that.element.trigger('ncbigridrowclick', [{row : row, cell : cell}]);
            }
        );
    },

    _removeRowClickWatcher : function (){  //TODO Code this  //Add to destroy

    },

    _notifyGridUpdated : function(){
        this.element.trigger( "ncbigridupdated" );
    },

    _deleteRowHelper: function(selector) {

        selector = selector || "";
        jQuery("tbody tr" + selector, this.element).remove();

    },

    _getCellDataHelper: function(row, col) {

        if(row <= 0 || row > this.getRowCount() || col < 0 || col > this.getColumnCount() )return null;

        var cell = jQuery("tbody tr:nth-child(" + row + ") td:nth-child(" + col + ")", this.element);
        if (!cell) {
            return;
        }
        else {
            var colType = this._getColumnType(col - 1);
            if(colType==="str" || !colType)colType = "str-sensitive";
            return this._csColumnTypeConversion(cell[0], colType, true);
        }

    },

    _getColumnType: function(col) {

        var columnTypes = this.options.columnTypes;
        var cl = columnTypes.length;
        if(col >= cl && cl.length !== 0){
            return undefined;
        }
        else if(cl === 0){
            return "str";
        }
        else {
           var cType = columnTypes[col];
           if(typeof cType === "string" && cType.indexOf("fnc:")===0){

               var parts = cType.substr(4,cType.length).split(".");
               cType = window[parts[0]];
               for(var i=1;i<parts.length;i++){
                   cType = cType[parts[i]];
               }

           }
           return cType;
        }

    },

    //Grab the cell content and return format that user specified
    _csColumnTypeConversion: function(cell, columnType, isHTMLNeeded) {

        var str = "";
        if(isHTMLNeeded){
            str = cell.innerHTML;
        }
        else{
            str = jQuery(cell).text();
        }

        str = jQuery.trim(str);


        switch (columnType) {
        case "int":
            aCell = parseInt(str, 10);
            break;
        case "float":
        case "num":
            aCell = parseFloat(str);
            break;
        case "date":
            aCell = new Date(str);
            break;
        case "str-sensitive":
            aCell = str;
            break;
        case "str-insensitive":
            aCell = str.toLowserCase();
            break;
        default:
            aCell = str.toLowerCase();
            break;
        }

        return aCell;

    },

    _getColumnDataHelper: function(col) {

        var rows = [];
        var ref = this;

        var colType = this._getColumnType(col - 1);
        if(colType==="str" || !colType)colType = "str-sensitive";


        jQuery("tbody tr td:nth-child(" + col + ")", this.element).each(function(ind, obj) {
            rows.push(ref._csColumnTypeConversion(obj, colType, true));
        });

        if (rows.length === 0) {
            return;
        }
        else {
            return rows;
        }

    },

    _getRowDataHelper: function(row) {

        var cols = [];
        var ref = this;

        jQuery("tbody tr:nth-child(" + row + ") td", this.element).each(function(ind, obj) {
            var colType = ref._getColumnType(ind);
            if(colType==="str" || !colType)colType = "str-sensitive";
            cols.push(ref._csColumnTypeConversion(obj, colType, true));
        });

        if (cols.length === 0) {
            return;
        }
        else {
            return cols;
        }

    },




    /* COMMON Public METHODS */

    addHTMLRowData : function( htmlStr, indx, loc ){

        var rowCount = this.getRowCount();    
        
        
        var selector = "";
        if (typeof indx == "undefined") {
            selector = ":last-child";
        }
        else {
            if(indx>rowCount){
                indx=rowCount;
            }
            else if(indx<0){
                indx = 1;
                loc = "before";
            }
            selector = ":nth-child(" + indx + ")";
        }

        if(rowCount===0){
            jQuery("tbody" + selector, this.element).append(htmlStr);
        }
        if (loc && loc.toLowerCase() === "before") {
            jQuery("tbody tr" + selector, this.element).before(htmlStr);
        }
        else {
            jQuery("tbody tr" + selector, this.element).after(htmlStr);
        }

        if (this.options.isPageable) {
            this._gotoPage(this.options.currentPage);
        }
        else{
            this._notifyGridUpdated();
        }



    },

    deleteAllRows : function() {

        this._deleteRowHelper();

        if (this.options.isPageable ) {
            this._gotoPage(0);
        }

    },

    deleteRow : function(ind) {

        if (typeof ind == "undefined") {
            return;
        }
        else if (!jQuery.isArray(ind)) {
            ind = [ind];
        }
        else { //need to make sure we delete indexes in order
            ind = ind.sort();
        }

        for (var i = ind.length - 1; i >= 0; i--) {
            this._deleteRowHelper(":nth-child(" + ind[i] + ")");
        }

        if (this.options.isPageable) {
            this._gotoPage(this.options.currentPage);
        }

    },

    _custFilterCnt:0,
    filterRows : function(txt, isCaseInsensitive, col, isInverse) {
    
        if (typeof txt == "undefined" || txt.length === 0) {
            return;
        }

        this.element.trigger("ncbigridshowloadingbar");

        if( !jQuery.isArray(txt) ){
            orgTxt = txt;
            txt = [ [txt,isCaseInsensitive,col, isInverse] ];
        }

        var rows = remainingRows = jQuery("tbody tr:not(.ncbigrid-row-filtered)", this.element);        
        var filteredRows, remainingRows;

        for( var j = 0; j < txt.length; j++){

            var currFilter = txt[j];
            var currTxt = currFilter[0];
            var _isCaseInsensitive = currFilter[1];
            var _col = currFilter[2];
            var _isInverse = currFilter[3];

            var colMatch = "";
            if (typeof _col != "undefined" && _col!==null){
                colMatch = ":nth-child(" + _col + ")";
            }

            var oppFilter = (_isInverse)?"not-":"";

            var sen = "";
            var filterFnc = null;
            if(typeof currTxt === "function"){
                sen = "-fnctionTest";
                var cnt = this._custFilterCnt;
                this._custFilterCnt = cnt + 1;
                filterFnc = "filter_temp_" + cnt;
                jQuery.ui.jig[filterFnc] = currTxt;
                currTxt = "jQuery.ui.jig." + filterFnc;
            }
            else if(currTxt.constructor && currTxt.constructor.toString().indexOf("RegExp")>0){
                sen = "-regexp";
            }
            else if(isCaseInsensitive){
                sen = "-insensitive";
            }

            if(_col!==null){
                remainingRows = remainingRows.find("td" + colMatch + ":" + oppFilter + "contains" + sen + "(" + currTxt + ")").parent();
            }
            else{
                remainingRows = remainingRows.filter(":" + oppFilter + "contains" + sen + "(" + currTxt + ")");
            }

            if(filterFnc){
                jQuery.ui.jig[filterFnc] = null;
            }

        }

        if(rows){
            filteredRows = rows.not(remainingRows).each(function(ind, obj) {
                jQuery(obj).addClass("ncbigrid-row-filtered").attr("isfiltered", "true");
            });
        }

        if (this.options.isPageable) {
            this._gotoPage(0);
        }

        //var remainingRows = jQuery("tbody tr", this.element).not(filteredRows);
        this.element.trigger('ncbigridfilterapplied', [txt, isCaseInsensitive, col, filteredRows, remainingRows]).trigger("ncbigridhideloadingbar");

        //TODO make this common code fnc, pasted too many times
        if (this.options.isPageable) {
            this._gotoPage(this.options.currentPage);
        }
        else{
            this._notifyGridUpdated();
        }

        return {
            "filteredRows": filteredRows,
            "remainingRows": remainingRows
        };

    },

    getCellData : function(row, col) {

        if (typeof row == "undefined" || typeof col == "undefined") {
            return;
        }
        else {
            return this._getCellDataHelper(row, col);
        }

    },

    getColumnCount : function( returnAll ){
        var visibleState = (returnAll===false)?":visible":"";
        var cnt = jQuery(this.element).find("tbody tr:not(.ui-ncbigrid-rowHidden):eq(0) td" + visibleState).length;
        if(cnt===0){
            cnt = jQuery(this.element).find("thead tr:last th").length;
        }
        return cnt;
    },

    getColumnData: function(col) {

        if (typeof col === "undefined") {
            return null;
        }
        else if (jQuery.isArray(col)) {
            var colData = [];
            for (var i = 0; i < col.length; i++) {
                if(col[i] <= 0 || col[i] > this.getColumnCount()) colData.push(null);
                colData.push(this._getColumnDataHelper(col[i]));
            }
            return colData;
        }
        else {
            if(col <= 0 || col > this.getColumnCount()) return null;
            return this._getColumnDataHelper(col);
        }

    },

    getColumnType: function(col) {

        return this._getColumnType(col-1);

    },

    getRowData: function(row) {

        if (typeof row === "undefined") {
            return;
        }
        else if (jQuery.isArray(row)) {
            var rowData = [];
            for (var i = 0; i < row.length; i++) {
                rowData.push(this._getRowDataHelper(row[i]));
            }
            return rowData;
        }
        else {
            return this._getRowDataHelper(row);
        }

    },

    highlightRows: function(txt, isCaseInsensitive, col, isInverse) {

        var isIE6 = jQuery.browser.msie && parseFloat(jQuery.browser.version)<7;

        if (typeof txt == "undefined" || txt.length === 0) {
            return;
        }

        this.element.trigger("ncbigridshowloadingbar");

        var highlightedRows = [];
        var sen = (isCaseInsensitive) ? "-insensitive": "";
        var oppFilter = (isInverse)?"not-":"";

        if(txt.constructor && txt.constructor.toString().indexOf("RegExp")>0){
            sen = "-regexp";
            oppFilter="";
        }

        if (typeof col != "undefined") {

            var colMatch = ":nth-child(" + col + ")";
            jQuery("tbody tr td" + colMatch + ":" + oppFilter + "contains" + sen + "(" + txt + ")", this.element).each(function(ind, obj) {
                var tr = jQuery(obj).parent();
                var classToAdd = "ui-ncbigrid-high";
                if(isIE6){
                    classToAdd = tr.hasClass("ui-ncbigrid-row-even")?"ui-ncbigrid-row-even-high-IE6":"ui-ncbigrid-row-odd-high-IE6";
                }
                tr.addClass(classToAdd);
                if(jQuery.inArray(tr,highlightedRows)===-1){
                    highlightedRows.push(tr);
                }
            });

        }
        else{
            jQuery("tbody tr:" + oppFilter + "contains" + sen + "(" + txt + ")", this.element).each(function(ind, obj) {

                var tr = jQuery(obj);
                var classToAdd = "ui-ncbigrid-high";
                if(isIE6){
                    classToAdd = tr.hasClass("ui-ncbigrid-row-even")?"ui-ncbigrid-row-even-high-IE6":"ui-ncbigrid-row-odd-high-IE6";
                }
                tr.addClass(classToAdd);
                highlightedRows.push(tr);

            });
        }

        this.element.trigger('ncbigridhighlightapplied', [txt, isCaseInsensitive, col, highlightedRows]).trigger("ncbigridhideloadingbar");

        return {
            "highlightedRows": highlightedRows
        };

    },

    removeFilterRows : function(txt, isCaseInsensitive, col, isUndo, isInverse) {

        if (typeof txt != "undefined" && txt.length > 0) {

            if(typeof isUndo === "undefined"){
                isUndo = true;
            }

            var filterTypeStart = "";
            var filterTypeEnd = "";
            if(isUndo || isInverse){
                filterTypeStart = ":not(";
                filterTypeEnd = ")";
            }

            var sen = "";
            var orgTxt = "";
            var filterFnc = null;
            if(typeof txt === "function"){
                sen = "-fnctionTest";
                var cnt = this._custFilterCnt;
                this._custFilterCnt = cnt + 1;
                filterFnc = "filter_temp_" + cnt;
                jQuery.ui.jig[filterFnc] = txt;
                orgTxt = txt;
                txt = "jQuery.ui.jig." + filterFnc;
            }
            else if(txt.constructor && txt.constructor.toString().indexOf("RegExp")>0){
                sen = "-regexp";
            }
            else if(isCaseInsensitive){
                sen = "-insensitive";
            }

            var oppFilter = (isInverse)?"not-":"";

            //refactor this out to a fnc call
            //var colMatch = "";
            if (typeof col != "undefined" && col!==null) {
                var selText = "tbody tr[isfiltered='true'] td:nth-child(" + col + ")" + filterTypeStart + ":" + oppFilter + "contains" + sen + "(" + txt + ")" + filterTypeEnd
                jQuery(selText, this.element).parents().each(function(ind, obj) {
                    jQuery(obj).removeClass("ncbigrid-row-filtered").attr("isfiltered", "false");
                });
            }
            else {
            
                var selText = "tbody tr[isfiltered='true']" + filterTypeStart + ":contains" + sen + "(" + txt + ")"+ filterTypeEnd;
                jQuery(selText, this.element).each(function(ind, obj) {
                    jQuery(obj).removeClass("ncbigrid-row-filtered").attr("isfiltered", "false");
                });
            }

        }
        else {
            jQuery("tr.ncbigrid-row-filtered", this.element).removeClass("ncbigrid-row-filtered").attr("isfiltered", "false");
        }

        if(filterFnc){
            jQuery.ui.jig[filterFnc] = null;
            txt = orgTxt;
        }

        if (this.options.isPageable) {
            this._gotoPage(0);
        }

        this.element.trigger('ncbigridfilterremoved', [txt, isCaseInsensitive, col]);

        //TODO make this common code fnc, pasted too many times
        if (this.options.isPageable) {
            this._gotoPage(this.options.currentPage);
        }
        else{
            this._notifyGridUpdated();
        }

    },

    removeHighlightRows: function(txt, isCaseInsensitive, col, isInverse) {

        var isIE6 = jQuery.browser.msie && parseFloat(jQuery.browser.version)<7;

        if (typeof txt != "undefined" && txt.length > 0) {

            var highlightedRows = [];
            var sen = (isCaseInsensitive) ? "-insensitive": "";
            var oppFilter = (isInverse)?"not-":"";

            if(txt.constructor && txt.constructor.toString().indexOf("RegExp")>0){
                sen = "-regexp";
                oppFilter = "";
            }

            if (typeof col != "undefined") {
                var colMatch = ":nth-child(" + col + ")";

                jQuery("tbody tr td" + colMatch + ":" + oppFilter + "contains" + sen + "(" + txt + ")", this.element).each(function(ind, obj) {
                    var tr = jQuery(obj).parent().removeClass("ui-ncbigrid-high");
                    if(isIE6){
                        tr.removeClass("ui-ncbigrid-row-odd-high-IE6").removeClass("ui-ncbigrid-row-even-high-IE6");
                    }

                    if(jQuery.inArray(tr,highlightedRows)===-1){
                        highlightedRows.push(tr);
                    }
                });
            }
            else{
                jQuery("tbody tr:" + oppFilter + "contains" + sen + "(" + txt + ")", this.element).each(function(ind, obj) {

                    var tr = jQuery(obj).removeClass("ui-ncbigrid-high")
                    if(isIE6){
                        tr.removeClass("ui-ncbigrid-row-odd-high-IE6").removeClass("ui-ncbigrid-row-even-high-IE6");
                    }
                    highlightedRows.push(tr);

                });
            }

        }
        else {
            jQuery(".ui-ncbigrid-high", this.element).removeClass("ui-ncbigrid-high");
            if(isIE6){
                jQuery(".ui-ncbigrid-row-odd-high-IE6, .ui-ncbigrid-row-even-high-IE6", this.element).removeClass("ui-ncbigrid-row-odd-high-IE6").removeClass("ui-ncbigrid-row-even-high-IE6");
            }
        }

        this.element.trigger('ncbigridhighlightremoved', [txt, isCaseInsensitive, col]);

    },

    setCellData: function(row, col, text) {

        jQuery("tbody tr:nth-child(" + row + ") td:nth-child(" + col + ")", this.element).text(text);
        this.element.trigger('ncbigridcellchange', [row, col, this.getCellData(row, col)]);

    },

    setRowData: function(row, data) {

        jQuery("tbody tr:nth-child(" + row + ") td", this.element).each(function(ind, obj) {
            jQuery(obj).text(data[ind]);

        });

        this.element.trigger('ncbigridrowchange', [row, this.getRowData(row)]);
    },



    /*
        show/hide columns
        //not fully tested

        -Internet Explorer 8
            fails big time - grids need to be redrawn

        -Firefox
            fails with scrollable where columns have fixed widths - get a gap still.


    */
    showColumn: function( col ){
        jQuery("tr *:nth-child(" + col +")", this.element).removeClass("ui-ncbigrid-column-hidden");

    },
    hideColumn: function( col ){
        jQuery("tr *:nth-child(" + col +")", this.element).addClass("ui-ncbigrid-column-hidden");
    },


    _loadingBar : null,
    _loadingElem : null,
	_addLoadingBar : function(){

        if(!this.options.isLoadingMessageShown) return;
		this._loadingBar = jQuery("<div class='ui-ncbigrid-loadingbar ncbipopper-wrapper ui-corner-all'>" + this.options.loadingText + "</div>").appendTo(document.body).css("display","none");

        this._loadingElem = (!jQuery.browser.mozilla && this.options.isScrollable) ? this.element.parent() : this.element;

        jQuery( this._loadingElem ).ncbipopper(
            {
                targetPosition : 'middle center',
                sourcePosition : 'middle center',
                sourceSelector : this._loadingBar,
                hasArrow : false,
                openMethod : 'custom',
                closeMethod : 'custom',
                isSourceElementCloseClick: false,
                isTriggerElementCloseClick: false,
                isDocumentCloseClick: false,
                addCloseButton : false,
                openAnimation: 'show',
                closeAnimation: 'hide'
            }
        );

        var that = this;
        this.element.bind("ncbigridshowloadingbar", function(){that._showLoadingBar(); }).bind("ncbigridhideloadingbar", function(){that._hideLoadingBar();});

	},
	_showLoadingBar : function(){
		if(this.options.isLoadingMessageShown){
            if(!this._loadingBar){
                this._addLoadingBar();
            }
            if(this.loadingCloseTimer){
                window.clearTimeout(this.loadingCloseTimer);
            }
		    this._loadingElem.ncbipopper("open");
		}
	},
	_hideLoadingBar : function(){
		if(this.options.isLoadingMessageShown){
            var that = this;
            this.loadingCloseTimer = window.setTimeout(
                function(){
                    that._loadingElem.ncbipopper("close");
                }
            , 200);

		}
	}

});





jQuery.fn.toEm = function(settings){
	settings = jQuery.extend({
		scope: 'body'
	}, settings);
	var that = parseInt(this[0],10);
	var scopeTest = jQuery('<div style="display: none; font-size: 1em; margin: 0; padding:0; height: auto; line-height: 1; border:0;">&nbsp;</div>').appendTo(settings.scope);
	var scopeVal = scopeTest.height();
	scopeTest.remove();
	return (that / scopeVal).toFixed(8) + 'em';
};
jQuery.fn.toPx = function(settings){
	settings = jQuery.extend({
		scope: 'body'
	}, settings);
	var that = parseFloat(this[0]);
	var scopeTest = jQuery('<div style="display: none; font-size: 1em; margin: 0; padding:0; height: auto; line-height: 1; border:0;">&nbsp;</div>').appendTo(settings.scope);
	var scopeVal = scopeTest.height();
	scopeTest.remove();
	return Math.round(that * scopeVal) + 'px';
};

//Extend the selector to have case insensitve searching for contains.
jQuery.extend(jQuery.expr[":"], {
    "contains-insensitive": function(elem, i, match, array) {
        return (elem.textContent || elem.innerText || "").toLowerCase().indexOf((match[3] || "").toLowerCase()) >= 0;
    }
});


jQuery.extend(jQuery.expr[":"], {
    "contains-regexp": function(elem, i, match, array) {
        var txt = jQuery.trim(elem.textContent || elem.innerText || "");

        var testGlobalCaseRE = /\/([gi]+)$/;
        var repGlobalCaseRE = /\/[gi]?$/;

        var regStr = match[3];

        var matchGlobal = regStr.match(testGlobalCaseRE);
        var gExp = (matchGlobal!==null) ? matchGlobal[1] : "";
        var rExp = regStr.replace(repGlobalCaseRE,"").replace(/^\//,"");

        var re = new RegExp( rExp, gExp );
        return txt.match( re ) !== null;

    }
});

jQuery.extend(jQuery.expr[":"], {
    "contains-fnctionTest": function(elem, i, match, array) {
        var txt = (elem.textContent || elem.innerText || "");
        var fnc = jQuery.ui.jig._getFncFromStr(match[3]);
        return fnc(txt);
    }
});

jQuery.extend(jQuery.expr[":"], {
    "not-contains": function(elem, i, match, array) {
        return (elem.textContent || elem.innerText || "").indexOf((match[3] || "")) === -1;
    }
});

jQuery.extend(jQuery.expr[":"], {
    "not-contains-insensitive": function(elem, i, match, array) {
        return (elem.textContent || elem.innerText || "").toLowerCase().indexOf((match[3] || "").toLowerCase()) === -1;
    }
});

(function(){

    var gridOpts = jQuery.ui.ncbigrid.prototype.options;

    gridOpts.webservice = null;
    gridOpts.webserviceParams = null;
    gridOpts.isServersideLoad = false;
    gridOpts.isServersidePaging = false;
    gridOpts.isServersidePagingCached = false;
    gridOpts.isServersideSort = false;
    gridOpts.maxRowCount = null;
    gridOpts.columnNames = null;

    var orgCreate = jQuery.ui.ncbigrid.prototype._create;
    var orgSortDefaults = jQuery.ui.ncbigrid.prototype.sortDefaults;
    var org_killActiveSort = jQuery.ui.ncbigrid.prototype._killActiveSort;
    var org_sortData = jQuery.ui.ncbigrid.prototype._sortData;

    var orgUpdateRows = jQuery.ui.ncbigrid.prototype.updateRows;
    var orgGetRowCount = jQuery.ui.ncbigrid.prototype.getRowCount;
    var orgGetMaxPage = jQuery.ui.ncbigrid.prototype.getMaxPage;

    jQuery.widget("ui.ncbiservergrid", jQuery.ui.ncbigrid, {

        _loadCreateCallback : false,
        _create : function(){

            if(this.options.isServersidePaging){
                this.options.isPageable = true;
            }

            if(this.options.isServersideSort){
                this.options.isSortable = true;
            }
            
            if(this.options.isServersideLoad && !this.options.isServersidePaging){
                this._loadCreateCallback = true;
                this._fetchData();
            }
            else{
                orgCreate.apply( this, arguments);
            }
            
        },

        getMaxPage : function(){
            if(this.options.isServersidePaging){
                return Math.ceil( this.getRowCount() / this.options.pageSize );
            }
            else{
                return orgGetMaxPage.apply(this,arguments);
            }
        },

        getRowCount : function(){
            if(this.options.isServersidePaging){
                return this.options.maxRowCount || 0;
            }
            else{
                return orgGetRowCount.apply(this,arguments);
            }
        },

        updateRows : function(){
            if(this.options.isServersidePaging || this.options.isServersideSort){
                this.updateRowsServer();
            }
            else{
                return orgUpdateRows.apply(this,arguments);
            }
        },

        updateRowsServer : function(){

            var rows = null;
            if(this.options.isServersidePagingCached){
                rows = this.isInCache();
            }

            if(rows){
                this._displayTheRows( rows );
            }
            else if(this._isInitialFetch && !this.options.isServersideLoad){
                this.addToCache( this.element.find("tbody").html() );
                this._isInitialFetch = false;
            }
            else{
                this._fetchData();
            }

        },

        _isInitialFetch : true,
        
        _fetchData : function(){

            var that = this;
            
            this.element.trigger("ncbigridshowloadingbar");

            if(this._activeRequest && this._activeRequest.readyState <4){
                this._activeRequest.abort();
            }

            var wsParameters = this._handlewebserviceParams();
            
            if(this.options.isServersidePaging){
                wsParameters.parameters.page = this.getCurrentPage();
                wsParameters.parameters.pageSize = this.options.pageSize;
            }

            if(this.options.isServersideSort && this.options.sortColumn!==-1){
         
                var colNames = this.options.columnNames;
                if(colNames){
                    wsParameters.parameters.sortCol = colNames[ this.options.sortColumn ];
                }
                else{
                    wsParameters.parameters.sortCol = this.options.sortColumn + 1;
                }
                wsParameters.parameters.sortDir = this.options.sortColumnDir;
            }
            
            this._activeRequest = jQuery.ajax({
              url: wsParameters.url,
              data: wsParameters.parameters,
              success: function(data, status, obj){
                            that._processData(data, status, obj);
                        },
              error: function(XMLHttpRequest, textStatus, errorThrown){
                        that._processError(XMLHttpRequest, textStatus, errorThrown);
                     }
            });

        },

        _handlewebserviceParams : function(){
            
            var url = this.options.webservice;
            var optParams = this.options.webserviceParams || this.options.webServiceParams;

            var parameters = {};

            if( optParams ){
                if(typeof optParams === "string"){
                    var questionmark = (optParams.charAt(0)!=="?" || url.indexof("?") === -1)?"?":"";
                    url = url + questionmark + optParams;
                }
                else{
                    for(var x in optParams){
                        parameters[x] = optParams[x];
                    }
                }
            }

            return { "url" : url, "parameters" : parameters }

            },

        _activeRequest : null,

        _processData : function( data, status, obj ){

            if(status==="success"){
                this._displayTheRows( data );
                this.addToCache( data );
            }
            
            if(this._loadCreateCallback){
                orgCreate.apply( this );
            }

        },
        
        _processError : function( XMLHttpRequest, textStatus, errorThrown ){
            if(typeof console !== "undefined" && console.error){
                if(console.group)console.group("Failed Request");
                console.error("The request failed to retrieve the rows");
                if(console.info){
                    console.info(XMLHttpRequest);
                    console.info(textStatus);
                    console.info(errorThrown);
                }
                if(console.group)console.groupEnd("Failed Request");
            }        
        },

        _displayTheRows : function( rows ){
            this.element.trigger("ncbigridresetscroll");
            this.element.trigger("ncbigridhideloadingbar");
            this.element.find("tbody").html( rows );
            this._notifyGridUpdated();
        },

        _cachedRows : {},

        getCacheKey : function(){

            var currentPage = this.getCurrentPage().toString();
            var pageSize = this.options.pageSize.toString();
            var sortColumn = this.options.sortColumn.toString();
            var sortColumnDir = this.options.sortColumnDir.toString();

            return currentPage + "_" + pageSize + "_" + sortColumn + "_" + sortColumnDir;

        },

        addToCache : function( rows ){
            if(this.options.isServersidePagingCached){
                this._cachedRows[ this.getCacheKey() ] = rows;
            }
        },

        isInCache : function(){
            return this._cachedRows[ this.getCacheKey() ] || null;
        },

        
        
        
        sortDefaults : function(){  
            if(!this.options.isServersideSort){
                orgSortDefaults.apply(this,arguments);
            }
        },

        _killActiveSort : function(){  
            if(!this.options.isServersideSort){
                org_killActiveSort.apply(this,arguments);
            }    
        },

        _sortData : function(){
            
            if(this.options.isServersideSort){
                this.updateRows();
            }
            else{
                org_sortData.apply(this,arguments);
            }    

        }

    });

})();/* 
ui.widgetName: ncbielastictextarea

shamelesly based on the Elastic code writen by:
@author         Jan Jarfalk
@author-website http://www.unwrongest.com
@license        MIT License - http://www.opensource.org/licenses/mit-license.php

TODO:
  write tests
*/

(function($) {
  $.widget("ui.ncbielastictextarea", { 
    // global dict containing the styles that get copied from the textarea to
    // the hidden div
    styles: {
      copy: [ 'paddingTop', 'paddingRight', 'paddingBottom', 'paddingLeft',
				        'fontSize', 'lineHeight', 'fontFamily', 'width','fontWeight']
    },

    _create: function() {
      var textarea = this.element;
      if ( textarea.attr('type') != 'textarea' ) {
        return false;
      }

      // generate a uid so that we can remove events and textareas without
      // affetcing similar widgets on the same page
      this.elastic_uid = $.ui.jig._generateId("ncbielastictextarea");

      //set word wrapping on the visible text area
      textarea.css('word-wrap','break-word');

      var twin        = $('<div />').css({
                                          'position':'absolute',
                                          'display':'none',
                                          'word-wrap':'break-word'
                                          }).attr('id',this.elastic_uid);
      var lineHeight  = parseInt(textarea.css('line-height'),10) || parseInt(textarea.css('font-size'),'10');
      var minHeight   = parseInt(textarea.css('height'),10) || (lineHeight * 3);
      var maxHeight	  = parseInt(textarea.css('max-height'),10) || Number.MAX_VALUE;
      var goalHeight  =	0;

      // Opera returns max-height of -1 if not set
      if (maxHeight < 0) { maxHeight = Number.MAX_VALUE; }

      // Append the twin to the DOM
      // We are going to meassure the height of this, not the textarea.
      twin.appendTo(textarea.parent());

      // copy the css styles from the text area to the div
      $(this.styles.copy).each( function() {
        twin.css(this.toString(),textarea.css(this.toString()));  
      });



      function setHeightAndOverflow(height, overflow){
        curratedHeight = Math.floor(parseInt(height,10));
        if(textarea.height() != curratedHeight){
          textarea.css({'height': curratedHeight + 'px','overflow':overflow});
        }
      }
      
      function _update(force) {
        // Get curated content from the textarea.
        var textAreaContent = textarea.val().replace(/&/g,'&amp;')
                                    .replace(/  /g, '&nbsp;')
                                    .replace(/<|>/g, '&gt;')
                                    .replace(/\n/g, '<br />');

        var twinContent = twin.html();
					
        if(textAreaContent+'&nbsp;' != twinContent || force == 'force'){
          // Add an extra white space so new rows are added when you are at the 
          // end of a row.
          twin.html(textAreaContent+'&nbsp;');
          // Change textarea height if twin plus the height of one line differs 
          // more than 3 pixel from textarea height
          if(Math.abs(twin.height()+lineHeight - textarea.height()) > 3){
            var goalHeight = twin.height()+lineHeight;
            if(goalHeight >= maxHeight) {
              setHeightAndOverflow(maxHeight,'auto');
            } 
            else if(goalHeight <= minHeight) {
              setHeightAndOverflow(minHeight,'hidden');
            } else {
              setHeightAndOverflow(goalHeight,'hidden');
            } 
          }
        }
      }

      // since the div is converted to a fixed width if specifed as a %,
      // have to resize when the window is resized.
      $(window).bind('resize.' + this.elastic_uid, function(){
        twin.css('width',textarea.width() + 'px');
        _update('force');
      });


      // Hide scrollbars
      textarea.css({'overflow':'hidden'});

      // set the hidden div width to match the visible one explicitely as this
      // fixes the problems when the text area doesn't have a fixed width
      twin.css('width',textarea.width() + 'px');
				
      // Update textarea size on keyup
      textarea.bind('keyup.' + this.elastic_uid, function(){ _update(); });

      // And this line is to catch the browser paste event
      textarea.bind('input.' + this.elastic_uid + ' ' + 'paste.' + this.elastic_uid ,
                    function(e){ setTimeout( _update, 250); });
      
      // fire the update, just incase the text is already longer than the text area
      _update();

    },

    destroy: function() {
      // remove events bound to the textarea
      this.element.unbind('.'+this.elastic_uid);

      // need to remove inserted div
       $('#'+ this.elastic_uid).remove();

      // and reset the textarea styles
      this.element.css({'word-wrap':'',
                         'overflow':'',
                         'height':''});
      // remove the resize event
      $(window).unbind('resize.' + this.elastic_uid);

      // then call default destroy
      $.Widget.prototype.destroy.apply(this, arguments);
    }

  }); // end widget def
    
  $.ui.ncbielastictextarea.prototype.options = {};

  $.ui.ncbielastictextarea.prototype.version = '1.2';
})(jQuery);


/**
* @file jquery.ui.ncbilinkedselects.js
* A jQuery UI widget to allow a chain of dependent select elements.
*
* @author cohenaa
*/
(function($) {

    $.widget("ui.ncbilinkedselects", {
        
        _selectEls: [], // stores collection of all selects, including this.element
        
        _paramObj: {}, // stores built up query string to be sent via AJAX request
        _localMap: null,

        options: {
            firstPopulatedFromDs: true,
            localData: null,
            localDataCallback: null,
            selects: null,
            webservice: null
        },
        
        _create: function() { 


            // store evaluated local data source for use later
            if( this.options.localData ) {
                this._localMap = eval( this.options.localData );
            }

            if( this.options.firstPopulatedFromDs ) {
                this._populateFirstSelect();
            }

            // collect the chain of select elements and save as a "static" variable
            // sets static property
            this._selectEls = this.getSelects();
            this._setEvents();

        },

        _populateFirstSelect: function() {
            if( this.options.localData ) {
                var strOrObjects = this._getFirstOptionsFromLocalDs();

                // values could be an array of strings or of name/value pairs
                // Check by checking the first index
                var isString = this._isArrayOfStrings( strOrObjects );                     
                for( var i =0; i < strOrObjects.length; i++ ) {
                    var strOrObj = strOrObjects[i];
                    if( isString ) {
                        
                        this.element.append( '<option value="' + strOrObj + '">' + strOrObj + '</option>' );
                    } else {
                        for( var string in strOrObj ) {
                            this.element.append( '<option value="' + strOrObj[string] + '">' + string + '</option>' );
                        }
                    }
                }
            } else { // populate from CGI, with empty parameters
                this._getAndAppendFromWebService( this.element, {} );                
            }   
        },

        _getFirstOptionsFromLocalDs: function() {
            var res = [];
            if( ! this.options.localDataCallback ) { // using default data structure
                for( var i = 0; i < this._localMap.length; i++ ) {
                    var opVal = this._localMap[i];
                    if ( typeof opVal === 'string' ) {
                        res.push( opVal );
                    } else if( $.isPlainObject( opVal )) { // if an object, it's name/val pairs
                        res.push(opVal);
                    }
                }
            } else {
                // if using a custom data source and a callback, call the cb with only the map parameter,
                // doing that, should return an array of the first options.
                res = this.options.localDataCallback.apply( this, [ this._localMap ] );
            }
        return res;
        },

        /**
        * Gets all the selects in the chain
        *  If config is set with jQuery selector, it gets those selects,
        * otherwise, by default, it gets "sibling" selects within a form
        * or fieldset.
        *
        * @return JQuery collection of selects
        */
        getSelects: function() {
            if ( this.options.selects ) {
                var restSelects = $( this.options.selects );
            } else {
                // if there's a fieldset, get all selects within that fieldset starting with this.element
                var ct = this.element.closest('fieldset, form');
                if( ct.length === 0 ) {
                    if( $.ui.jig._isConsole('warn') ) {
                        console.warn('jig warning: ncbilinkedselects. Your select elements must be within either a fieldset or a form element');
                    }
                }

                var selectsInCt = ct.find('select');
                var thisElSeen = false;
                var selects = [];
                for( var i = 0; i < selectsInCt.length; i++ ) {
                    var selectInCt = selectsInCt[i];
                    if ( selectInCt == this.element[0] ) {
                        thisElSeen = true;
                    }

                    // only add select if it's this.element or after
                    // and doesn't have a skip class on it.
                    if ( thisElSeen && !$( selectInCt ).hasClass('ui-ncbilinkedselects-ignore') ) {
                       selects.push( selectInCt ); 
                    }

                }
                return $(selects);



            }
            return this.element.add( restSelects ); 
        },

        /**
        * Private function that sets change events to selects
        */ 
        _setEvents: function() {
            
            var self = this;
            this._selectEls.each( function() {
                var select = $(this);
                var name = this.name;

                // Only attempt to populate next select, if this select is not the last one
                if ( ! self._isLastSelect( name ) ) {     
                    select.bind( 
                        'change', 
                        function( e ) { 
                            // reset all next selects in chain
                            self._resetNextSelects( name );
                            self._populateNext( e )  }
                    ); 
                    
                
                }
            });  
        },

        /**
        * Private fn, resets all selects that follow the select with the name supplied by parameter.
        */
        _resetNextSelects: function( name ) {
            var currSelectSeenYet = false;
            for( var i = 0; i < this._selectEls.length; i++ ) {
                var sel = $( this._selectEls[ i ] );
                if (sel.attr('name') == name ) {
                    currSelectSeenYet = true;
                    continue;
                }
                if ( currSelectSeenYet ) {
                    var options = sel.children('option');
                    options.each( function() {
                        var op = $( this );
                        if( ! op.hasClass('ui-ncbilinkedselects-perm') ) {
                            op.remove();
                        }
                    });

                }
            }  
        },

        /**
        * Private fn to dynamically construct param obj for ajax request. It
        * has to remember all parameters down the chain
        */
        _getParamObj: function( param, val ) {
            // if this is the first select, reset parameters
            if ( this._isFirstSelect( param ) ) {
                this._paramObj = {};
            }
            this._paramObj[param] = val;
            return this._paramObj;
        },

        /**
        * Private function to populate next select in chain
        */
        _populateNext: function( e ) {
            var currentSelect = $( e.target );
            var optionClicked = currentSelect.children('option:selected');

            // no need to get next select values if we are clicking on a select that 
            // is "perm"
            if( ! optionClicked.hasClass('ui-ncbilinkedselects-perm' ) ) {
                var param = currentSelect.attr('name' );
                var val = currentSelect.val();
                var nextSelect = this._getNextSelect( currentSelect.attr('name') );
    
                
                // Here we need to fork based on whether we are getting remove or local data
                // @todo: we should validate whether webservice exists?
                if ( this.options.webservice ) {
                    // Dyanamically construct the param object we will pass to $.get
                    // the param object needs to pick up all previous parameters, not just the current one.
                    var paramObj = this._getParamObj(param, val );
        
                    // AJAX request for next select down the chain
                    this._getAndAppendFromWebService( nextSelect, paramObj );
    
    
                }
                else if( this.options.localData ) {
                    // @todo invalid JSON
    
                    // if a change callback option is defined, use that to parse the data
                   
                    this._populateSelectFromLocalMap( currentSelect, optionClicked, nextSelect );
                    
                }
                else {
                    // @todo: what if we have neither webservice or localData set? There should be a test case for this.
                }
            }
        },

        /**
        * Appends option markup from a get request to webservice
        * 
        * @param select: jQuery select element
        * @paramObj: Object literal representing query parameters
        */
        _getAndAppendFromWebService: function( select, paramObj ) {
            var self = this;
            $.get( this.options.webservice, paramObj, function( a, status, xmlhttpObj ) {
                response = xmlhttpObj.responseText;
                select.append( response );
            }, 'HTML' ); 
        },

        /**
        * Given a value, returns array its options
        * @param key String The key to search the localdata source for
        * @return Returns Array of options
        */
        queryLocalService: function( name ) {
            var res = [];
            // Recursive helper fn to recurse data structure
            function getIndex( arr ) {
   
                for( var  i in arr ) {
                    var value = arr[ i ];
                    /*
                    if ( isStringMap  && value == name ) {
                        
                        console.info('regular string, and matches');
                        //console.warn('got index of value we are interested in');
                        // got index of value we are interested in
                        //console.info( i );
                        // got index of item we are interested in, its index is + 1
                        var j = parseInt( i );
                        ob = arr[ j + 1 ];
        
                        for ( var k = 0; k < ob.length; k++ ) {
                            if( ! jQuery.isArray( ob[k] ) ) {
                                res.push( ob[k] );
                            }
                        }
                        return;
                    } else { // name/values
                    */
                        
                        // get the object with the String/name passed
                        for( var p in value ) {
 
                            if( value[p] === name ) {
                                // next object in array should be object we're interested in
                                var arrObjs = arr[ parseInt(i) + 1 ];
                                for( var l = 0; l < arrObjs.length; l++ ) {
                                    var arrOb = arrObjs[l];
                                    if( ! $.isArray( arrOb ) ) {
                                        res.push( arrObjs[l] );
                                    }
                                }
                                
                                
                                return;
                            }
                        }
                    //}

                    if( jQuery.isArray( value) )  {
                        getIndex( value );
                    } 
                }
       
            } // end helper function
        
            // Start of the called get function
            // Seed recursion, getting index of passed name parameter
            var isStringMap;
            if( typeof this._localMap[0] === 'string' ) {
               isStringMap = true; 
            } else {
                isStringMap = false;
            }
            getIndex( this._localMap );
            return res;
        },
        
        /**
        * Private function that parses JSON data object, and populates next select
        * @param currentSelectVal, string the value of the current select
        * @param nextSelect jQuery object, the  select to populate, the next select to the select named by name.
        * @param e: Object. The event object
        * 
        */
        _populateSelectFromLocalMap: function( currentSelect, optionClicked, nextSelect) {
            var options;
            var cb = this.options.localDataCallback;
            if ( cb ) {
                options = cb.apply( this, [ this._localMap, currentSelect, optionClicked, this._selectEls  ]);
            } else {
                options = this.queryLocalService( currentSelect.val() );
            }

            // options could be an array of strings or array of name-value pairs
            var arrayOfStrings = this._isArrayOfStrings( options );
            for( var i = 0; i < options.length; i++ ) {
                var op = options[i];
                if( arrayOfStrings ) {
                    nextSelect.append( '<option value="' + op + '">' + op + '</option>' ); 
                } else {
                    var string
                    var value;

                    for( var p in op ) {
                      string = p;
                      value = op[p];
                    }
                     nextSelect.append( '<option value="' + value + '">' + string + '</option>' );

                }
            }
        },

        _isArrayOfStrings: function( arr ) {
            return typeof arr[ 0 ]  === 'string'; 
        },
    
        /**
        *   Private function. Returns boolean. True if name passed is last select in chain.
        */
        _isLastSelect: function( name ) {
            els = this._selectEls;
            var lastSelect = els[ els.length -1 ];   
            if ( name === lastSelect.name ) {
                return true;
            } else {
                return false;
            }
        },

        _isFirstSelect: function( name ) {
            els = this._selectEls;
            var firstSelect = els[ 0 ];   
            if ( name === firstSelect.name ) {
                return true;
            } else {
                return false;
            }
        },
        
        /**
        * Private fn. Gets the next select in the chain, given a name attribute of a select
        */
        _getNextSelect: function( name ) {
            for( var i = 0; i < this._selectEls.length; i++ ) {
                var sel = this._selectEls[ i ];
                if (sel.name == name ) {
                    return $( this._selectEls[ i + 1 ] );
                }
            }  
        }

    });



})(jQuery);
(function($) {
    // detect ie6 for windowed element fix, don't browser sniff, use a dynamic conditional comment
    // we will also need to know if it is IE since we are not doing roving tab index for ie
    var ie = $.browser.msie;
    var ie6 = ie && $.browser.version=="6.0" || false;

    // detect Safari for first items that have to have explicit width and to get width onload, not domready
    var safari = $.browser.safari || false;

    // for delay
    var timer = null;

    // private function for this widget
	var arrayMax = function(array) {
        if (typeof array !== 'undefined') {
            return Math.max.apply( Math, array );
        }
    };

	$.widget("ui.ncbimenu", {

        // a style dictionary, keep the strings centralized in one place
        styles: {
            hilited: 'ui-ncbimenu-item-active',
            item: 'ui-ncbimenu-item',
            itemFirst: 'ui-ncbimenu-item-first',
            itemFirstHilited: 'ui-ncbimenu-item-first-active',
            itemHasSubmenu: 'ui-ncbimenu-item-has-submenu',
            itemLeaf: 'ui-ncbimenu-item-leaf',
            itemNoHilite: 'ui-ncbimenu-item-no-hlt',
            itemSkip: 'ui-ncbimenu-item-skip',
            randClass: $.ui.jig._generateId('ncbimenu'), // this is put on all nodes so we can identify this instance,
            submenu: 'ui-ncbimenu-submenu',
            submenuOpen: 'ui-ncbimenu-submenu-open'
        },

		_init: function() {

            // store all menu root nodes in class property so we don't have to query dom in closeAllMenus()
            $.ui.ncbimenu._rootNodes.push( this.element);



			this.options.disabled = false;

            // remember if the menu instance is "active"
            // this means tha the current menu has not been unfocused
            // we want to know this because once we click on the menu, we want open first items' submenus on mouseover, not click
            // when we click on an element that is not part of this menu, then we want to reset it so that we have to click the first item again
            // to open it's submenu. This property is set to true when we click on the instance, and set to false in closeAllSubmenus() which is called
            // when we click on an element that is not an ancestor of this.element
            this._menuIsActive = false;
            this._setAttrs();
            this._setEvents();
            this._register();

		}, // end _init function

        _areInstances: function() {
            return $.ui.ncbimenu._areInstances;
        },
        _focus: function(a) {
            // special focus function to take into account that in safari we can't focus on an a without an href and a 0 tabindex

            var a = $(a);

            // for ie, don't manage the focus, but add href if none exists
            if ( ie ) {
                if ( !a.attr( 'href' ) ) {
                    a.attr( 'href', '#' );
                }
                return;
            }
            var condition = safari && !a.attr('href') ? true : false;
            if (condition) {
                a.attr('href', '#');
            }
            a[0].focus();
            if (condition) {
                a.removeAttr('href');
            }

        },

        _register: function() {
            $.ui.ncbimenu._areInstances = true;
        },

        _setAttrs: function() {
            var styles = this.styles;
            var that = this;

            this.firstItems =  this.element.
                                   addClass('ui-widget ui-ncbimenu ' + styles.randClass).
                                   children('li');

            this.firstItems.each( function(i) {
                var it = $(this);
                var skip = that.isSkip(it);
                // if not the first item, take out of tabindex
                if ( !ie && !skip) { // don't do the tabindex stuff on a skip item or if IE
                    if (i !== 0 ) {
                        it.children('a')[0].tabIndex = -1;
                    } else {
                        it.children('a')[0].tabIndex = 0;
                        // this is the first item, for Safari we need an href on it so it can be focusable
                        // only if it has a submenu
                        if ( safari && it.children( 'ul' ).length > 0 ) {
                            it.children('a').attr('href', '#')
                        }
                    }
                }
                if ( it.children('ul').length === 1 ) {
                   it.children('a').addClass('ui-ncbimenu-first-link-has-submenu').
                   //append('<span class="ui-icon ui-icon-carat-1-s ui-ncbimenu ' + styles.randClass + '"></span>').
                   attr( {
                       'aria-haspopup': 'true'
                   }).next('ul').attr('aria-hidden', 'true');

                // otherwise its a first item leaf node
                } else {
                    it.addClass(styles.itemLeaf);

                }

                // here, add classes to all first items
                it.
                    addClass(styles.itemFirst + ' ui-helper-reset ui-ncbimenu ' + styles.randClass).
                    attr('role', 'presentation').
                        children('a').attr('role', 'menuitem').
                        addClass(styles.randClass + ' ui-ncbimenu-link-first');

                // make sure you only put this class on non skip items, otherwise they will be considered part of the menu
                if ( !skip ) {
                    it.children('a').addClass('ui-ncbimenu');
                }

                // for Safari we need an explicit width on the first lis, set each one to its own width
                // if we set an explicit width, though some versions of FF choke
                if ( $.browser.safari) {
                    $(window).load( function() {
                        it.width( it.outerWidth() );
                    });
                }
            });
            var role = this.firstItems.length > 1 ? 'menubar' : 'menu';
            this.element.attr('role', role);
        },

        _setEvents: function() {
            var that = this;
            var elDom = this.element[0];
			// Here we delegate events and while we're at it we put the role on. We put all events on parent node, and delegate from _delegate()
            this.element.
              unbind('click.NCBIMenu').
				      bind('click.NCBIMenu', function(e) {  that._delegate(e, that) }).
				mouseover( function(e) {  that._delegate(e, that) }).
                mouseout( function(e) { that._delegate(e, that) }).
				keyup( function(e) { that._delegate(e, that) });

            // for focus events, we have to call addEventListener in capturing phase
            // when item is focused upon unhilte all other siblings
            if (document.addEventListener) {
                elDom.addEventListener('focus', function(e) { that._delegate(e, that)}, true);
            } else {
                elDom.onfocusin = function() {  that._delegate(null, that); };
            }

            // Have the document listen for some click and focus events. But attach the event only once, even if we have multiple menu instance
            if ( ! this._areInstances() ) {

                // when clicking on any element that is not a descendent of this menu, close all menu instances on page
			    $(document).click( function(e) { that._closeAllMenus(e, that) });

                // global focus event on document so we can:
                // 1) Put first item of menu back in taborder when clicking on something other than the menu
                // 2) close all menus of this instance when focusing on something other than an ncbimenu

                // helper function for closing all menus when focusing on some element on the document

                var leaveMenu = function(e) {
                    // helper function which is called when focus leaves a menu instance
                    var e = e || window.event;
                    var t = e.target || e.srcElement;


                    // in the case of Ie, when focusing on an element that is not parent li, the target will be the parent of instance's ul element
                    if ( t.tagName && t !== $(elDom).parent()[0] && ! $(t).hasClass('ui-ncbimenu') ) {

                        that._closeAllMenus(e, that);
                    }

                }

                // for focus event, we need to pass true to addEventListener since we are delegating
                if (document.addEventListener ) {
                    document.addEventListener('focus', function(e) { leaveMenu(e) } , true);

                } else {
                    // for Ie, we need onfocusin event
                    document.onfocusin = leaveMenu;
                }
            }
        },

		_delegate: function(e, that) {
			// delegates events from root node, adds handlers
			// all events are attached to root node of widget to save memory
            var type;
            var t;
            if (e) {
			    type = e.type;
			    t = e.target;
            } else {
                type = window.event.type;
                t =  window.event.srcElement;
            }

            // make sure target is an a in case there is an image or something else inside the a
            while ( t.tagName && t.tagName.toLowerCase() !== 'a') {
                t = $(t).parent()[0];
            }
            tJ = $(t);
			var liParent = tJ.parent('li');

            var styles = that.styles;
            var isFirstItem = liParent.hasClass(styles.itemFirst);

			if (type === 'click'){
                that._menuIsActive= true;
				// if its the first menu item or ie, then just toggle submenu on click
				if (isFirstItem || ie ) {
                    that._activateItem(t, type);
				} 

				// if not a leaf item, it has a submenu so don't follow links
				if( that.isSkip(liParent)){
					//do nothing -- fix me Aaron!!!!!!!!
				}
				else if (!liParent.hasClass(styles.itemLeaf)) {
					e.preventDefault();
				}
			}

			// We do all our submenu showing/hiding and active items on mouseover, but only when the target is 'a' and only if
			if (type === 'mouseover' && t.tagName && t.tagName.toLowerCase() === 'a') {
                // if mousing over first item
                if (isFirstItem ) {

                    if (this._menuIsActive) {
                        // if there is a menu open in this instance then open submenu on mouseover
                        // without delay

                        this._activateItem(t, 'mouseover', false);

                    } else {
                        // otherwise only highlight item
                        this._activateItem(t);

                    }

                }
			    if (liParent.hasClass(styles.item) ) {

                    that._activateItem(t, type );
                }
			}

			// keyboard
			if (type === 'keyup' ) {
				var pressed = e.keyCode;
                var keyD = $.ui.keyCode;

                if (pressed === keyD.DOWN) {
                    that._activateItem(t, 'down');
                }

                if (pressed === keyD.UP) {
                    that._activateItem(t, 'up');
                }

                if (pressed === keyD.RIGHT) {
                    that._activateItem(t, 'right');
                }

                if (pressed === keyD.LEFT) {
                    that._activateItem(t, 'left');
                }

                if (pressed === keyD.ESCAPE) {
                    that._closeAllMenus(e, that, true); // pass true to ignore target, this is a hack and should be cleaned up
                }

            }
            if (type === 'mouseout') {

                // when mousing out of first item, and not mousing into menu, take hilite off
                if ( liParent.hasClass( styles.itemFirst) &&  ! $(e.relatedTarget).hasClass('ui-ncbimenu') ) {
                    that._unHiliteItem(t);
                }
            }

            if ( type ==='focus' || type === 'focusin' ) {
                if ( ie &&  tJ.attr('aria-haspopup', 'true' ) ) {
                    that._closeSiblingSubmenus( tJ );
                }
                // when focusing, take off hiliting of all other menus' first items
                that._unHiliteSiblings( t );

                // also if ie, and we are tabbing out of submenu we want to close the submenu
                // we are tabbing out of

            }
		},
        _getSubMenu: function(t) {
            return t.siblings('ul');
        },
        isSkip: function(li) {
            // is the node to be skipped?
            skip = $(li).hasClass( this.styles.itemSkip) ? true : false;
            return skip;
        },
		_unHiliteItem: function(a) {
            //console.log('unhilite: ', a);
            var a = $(a);
            a.removeClass(this.styles.hilited + ' ui-ncbimenu-first-link-has-submenu-active ' + this.styles.itemFirstHilited); //[0].tabIndex = -1;
        },
        _hiliteItem: function(a) { 
            // this function (name?) sucks because it does more than hilite the item, it makes it active, but we already have a function with that name
            var styles = this.styles;
            a = $(a);
            parentLi = a.parent('li');

            // only non skip items
            if ( ! this.isSkip(parentLi) ) {
                if ( !parentLi.hasClass(styles.itemNoHilite)) {
                    if ( a.parent('li').hasClass( styles.itemFirst)) {
                        a.addClass(styles.itemFirstHilited);
                        if ( a.hasClass('ui-ncbimenu-first-link-has-submenu') ) {
							a.addClass('ui-ncbimenu-first-link-has-submenu-active');
                        }
                    } else {
                    a.addClass(this.styles.hilited);
                    }
                }
                if ( !ie ) {
                    a[0].tabIndex = 0;
                }
                this._focus(a);

                if ( !ie && $.ui.ncbimenu._activeLink ) {
                    $.ui.ncbimenu._activeLink[0].tabIndex = -1;
                }
                $.ui.ncbimenu._activeLink = a;
            }

        },
        _unHiliteSiblings: function(a) {
            var that = this;
            $(a).parent('li').siblings('li').children('a').each( function() {
                that._unHiliteItem(this);
            });
        },
        _openSubmenu: function(o) {
		
            // opens an item's submenu
            // first time called on element, dynamically figures out width of submenu
            // and stores it on the submenu's root element (ul). _openSubmenu also sets appropriate classes, elements and attributes, just the first time
            // a submenu is opened.
            // if ul element already has width of menu figured out, it retrieves it from property of element. If classes
            var styles = this.styles;
            var ul = $(o.el) || null;

			this.element.addClass("ui-ncbimenu-has-active-submenu");
			
			if(!this._firstTime){
				this.element.hide().show();				
				this._firstTime = true;
			}

            //var delay = o.delay || null;
            var callback = o.callback || null;
            //var submenuFade = this._getData('submenuFade') || 0;
            var that = this;

            // don't try to open a leaf menu's submenu, cause it don't have one
            if (ul.length === 0 ) { return; }

            if (! ul){
                throw new Error('ncbigrid: _openSubmenu() method must be passed a DOM or jQuery collection representing the menu to be opened');
            }

            // the submenu <ul>, <li>s, and <a>s in the submenu
            var submenuEls = ul.children('li').andSelf().children('a').andSelf().addClass(styles.randclass); // add random class to all elements in submenu so we can always tell if we are clicking in or out of the menu without looking up

            // remember the submenus sub elements so we don't have to keep getting them
            ul.data('jig-ncbimenu-subnodes', submenuEls);

            // stores what the width of the menu should be (the width of its largest item)
            var submenuWidth = ul.data('jig-ncbimenu-width');

            // if the submenu hasn't been opened before we need to figure out the widths and set classes on submenu
            if (typeof submenuWidth === 'undefined') {

                // set expando on element to store widths of submenu's <a> elements
                ul.data('widths', []);

                // iterate thru ul its lis and as setting classes and attributes

                // for this scope store whether the submenu being opened has at least one submenu, if it does the width needs to be increased to account for the arrow icon
                var hasSubmenu = false;
                var aWidths = [];

                submenuEls.each( function() {
                    var submenuEl = $(this);
                    var tn = submenuEl[0].tagName.toLowerCase();
                    if (tn === 'li') {
                        // all items
                        submenuEl.
                            addClass(styles.item + ' ui-helper-reset').
                            attr('role', 'presentation');

                        // items with submenus
                        if (submenuEl.children('ul').length > 0 ) {
                            submenuEl.addClass(styles.itemHasSubmenu);

                            if (! hasSubmenu) {
                                hasSubmenu = true;
                            }

                        } else { // the item is a leaf
                            submenuEl.addClass(styles.itemLeaf);

                        }

                    }

                    if (tn === 'ul') {
                        submenuEl.
                            addClass(styles.submenu).
                            attr('role', 'menu').
                            css('z-index', that.options.submenuZindex );
                    }

                    if ( tn === 'a') {
                        submenuEl.attr({
                            'role': 'menuitem'
                        });

                        // set tabindex to -1 because best practices dictate
                        // that users should use cursor keys to navigate
                        // within widgets and tab to navigate between widgets
                        // if not a skip item, set to tabindex -1
                        if ( !ie && !that.isSkip(submenuEl.parent('li'))) {
                            submenuEl[0].tabIndex = -1;
                        }

                        if (submenuEl.next('ul').length > 0 ) {
                            submenuEl.
                                attr({
                                    'aria-haspopup': 'true'
                                }).
                                append('<span class="ui-ncbimenu ui-icon ui-icon-triangle-1-e"></span>');
                        }
                        // this is being done more than necessary, fix it!

                        // we need to take the ul out of the flow of the page and not show it before making it block
                        // otherwise this causes a reflow problem in FF
                        ul.css({
                            'position': 'absolute',
                            'left': '-2000px'
                        });

                        // now make the ul block so we can query the links' widths
                        ul.css('display', 'block');

                        // to get width we must switch a elements to inline
                        // then switch back again
                        submenuEl.css('display', 'inline');
                        aWidths.push(submenuEl.outerWidth());
                        submenuEl.css('display', 'block');

                        // now set the submenu back to where it should be
                        ul.css('left', 'auto');
                    }
                }).addClass(styles.randClass + ' ui-ncbimenu');
                //console.log('hasSubmenu: ', hasSubmenu);

                // once we have all the submenu's <a> element's widths, we get the max width and store it on the ul element

                // max width of all the <a> nodes in the menu
                //console.log('maxWidth of all the a nodes in this menu', Array.max( ul.data('widths')));
                var maxWidth = arrayMax( aWidths);

                // if menu has a submenu we need to account for icon
                var menuWidth = hasSubmenu ? maxWidth += 12 : maxWidth;
                ul.data('jig-ncbimenu-width', menuWidth);
                submenuWidth = ul.data('jig-ncbimenu-width');

                // if submenu is third level or more, set left margin of ul to width of previous a's ul's stored width
                if (ul.parent('li').hasClass(styles.itemHasSubmenu)){
                    ul.css('margin-left', ul.parent('li').parent('ul').data('jig-ncbimenu-width'));
                }

            }

            // we have to set the width each time, because of IE hasLayout bug, otherwise because of explicit width
            // so when opening the menu, set each menu and its children elements to the stored width
            // we would not be able to totally hide the submenu

            // Retrieve width of widest element inside this ul including padding
            var widthIncPadding = ul.data('jig-ncbimenu-width');
            submenuEls.each( function() {
                var submenuEl = $(this);
                if (submenuEl[0].tagName.toLowerCase() === 'a') {
                    // because we have padding on the the a, we need to minus the <a> element's padding left/right and the border of the ul (1px)
                    submenuEl.css('width', widthIncPadding - (parseInt(submenuEl.css('padding-right')) + parseInt(submenuEl.css('padding-left')) + 1));
                } else {
                    submenuEl.css('width', widthIncPadding);
                }
            });

            // to show a submenu, we have to change the position to abs BEFORE showing it, otherwise it will be jittery
            /*
            if (delay) {
                window.setTimeout( function() {
                    ul.css('position', 'absolute').fadeIn( submenuFadeSpeed, function() {
                        $(this).addClass(that.styles.submenuOpen);
                        if ( typeof callback === 'function') {
                            callback();
                        }
                    });
                }, delay);
            } else { // no delay
                ul.css({ 'position': 'absolute', 'display': 'block'}).addClass(this.styles.submenuOpen);
                if ( typeof callback === 'function') {
                    callback();
                }
            }
            */
            ul.css({ 'position': 'absolute', 'display': 'block' }).
                attr('aria-hidden', 'false').
                addClass(this.styles.submenuOpen);

            if ( typeof callback === 'function') {
                callback();
            }

            // if ie 6, do fix so that elements like select don't show through menu:
            // we need to associate the iframe with the menu, so we don't generate more than one
            if ( ie6) {
                if ( !ul.data('iframe') ) { // if there isnt' already one
                    var iframeId = $.ui.jig._generateId(this.widgetName, 'iframe-fix');
                    var iframe = $(ul).createIE6LayerFix(true).attr('id', iframeId);
                    ul.data('iframe', iframeId);
                } else { // if there is an iframe already, get it and show it
                    $('#' + ul.data('iframe')).show();
                }
            }

			
        },
        _closeSubmenu: function(o) {
            var ul = $(o.el) || null;
            var delay = o.delay || null;
            var callback = o.callback || null;
            var submenuFadeSpeed = o.submenuFadeSpeed || 0;
			
            if (! ul){
                throw new Error('ncbigrid: _openSubmenu() method must be passed a DOM or jQuery collection representing the menu to be closed');
            }
            // to show a submenu, we have to change the position to abs BEFORE showing it, otherwise it will be jittery
            var that = this;

            // For IE we have to go thru and take off explicit styles of all submenu sub elements, otherwise we will
            // have a ghost menu because it is display none but hasLayout
            if ( ul.data('jig-ncbimenu-subnodes') ) {
            //console.warn(ul.data('jig-ncbimenu-subnodes'));
            ul.data('jig-ncbimenu-subnodes').each( function() {
                $(this).css('width', 'auto');
            });
            }

            // unhilte all items in submenu
            ul.children('li').each( function() {
                //if ($(this).hasClass('ui-ncbimenu-item-active')) {
                    that._unHiliteItem($(this).find('a'));
                //}
            });

            if (delay) {
                window.setTimeout( function() {
                    ul.fadeOut( submenuFadeSpeed, function() {
                        $(this).css('position', 'static').removeClass(that.styles.submenuOpen);
                        if ( typeof callback === 'function') {
                            callback();
                        }
                    });
                }, delay);
            } else { // no delay
                ul.css({ 'position': 'static', 'display': 'none'}).
                    attr('aria-hidden', 'true').
                    removeClass(this.styles.submenuOpen);
                if ( typeof callback === 'function') {
                    callback();
                }
            }

            // for ie6, we have to hide the iframe we are using to cover windowed elements like select
            // we created the iframe in _openSubmen and stored it as an expando on the ul using jQuery's data()
            if ( ie6) {
                $('#' + ul.data('iframe')).hide();
            }
        },
        _closeSiblingSubmenus: function(a, effect) {
            var that = this;
			a.parent('li').siblings('li').find('ul.ui-ncbimenu-submenu-open').each( function() {
                if (effect) {
                    that._closeSubmenu({
                        el: $(this),
                        delay: that.options.submenuDelay,
                        submenuFadeSpeed: that.options.submenuFadeSpeed
                    });
                } else {
                    that._closeSubmenu({el: $(this)});
                }
            });
        },
        _closeGrandChildSubmenu: function(a, effect) {
            var el = a.parent('li').children('ul').children('li').children('ul');
            if (effect) {
                this._closeSubmenu( {
                    el: el,
                    delay: this.options.submenuDelay,
                    submenuFadeSpeed: this.options.submenuFadeSpeed
                });
            } else {
                this._closeSubmenu( {el: el } );
            }
        },
        _closeChildSubmenu: function(a, effect) {
            var that = this;
            var el = a.siblings('ul');
            if (effect) {
                this._closeSubmenu( {
                    el: el,
                    delay: this.options.submenuDelay,
                    submenuFadeSpeed: this.options.submenuFadeSpeed
                });
            } else {
                this._closeSubmenu( {el: el } );
            }

        },
		_closeAllMenus: function(e, that, ignoreTarget) {
		
			// when clicking on an element that is not a descendent of the menu unhilite and close all submenus
            // ignoreTarget is a boolean that if set true, effectively allows us to close all menus even if target is inside menu
            // this was a hack added because the API of this function isn't quite right. This should be fixed.
            var styles = this.styles;

            // don't actually close menus if clicking or focusing on a menu instance

			if ( ! $(e.target).hasClass('ui-ncbimenu') || ignoreTarget ) {

                // get all root nodes from stored array, instead of querying DOM
                // unhilite everything and close all submenus
                // also reset first menu item to taborder
                var roots = $.ui.ncbimenu._rootNodes;
                for (var i = 0; i < roots.length; i++) {
                    var root = $(roots[i]);
                    root.
                        children('li').
                            children('a').each( function(i) {
                                // only non skip nodes

                                if ( ! that.isSkip( $(this).parent('li'))) {

                                    // reset first item's tabindex, take others out of tabindex
                                    if ( !ie ) {
                                        i == 0 ? this.tabIndex = 0 : this.tabIndex = -1;
                                    }
                                    that._unHiliteItem( this);
                                }
                            } ).end().
                        find('ul').each( function() {
                            that._closeSubmenu( { el: $(this) });
                        });
                }

                this._menuIsActive = false;

			}
			
			if(i>0){
				this.element.removeClass("ui-ncbimenu-has-active-submenu");
				this._firstTime = false;
			}
			
		},

        _getNextItem: function(a) {
            // gets next sibling link, if there are no next items, item is first one

            var currentA = a;
            var nextItem = a.parent('li').next('li');

            if (nextItem.length === 0) {
                nextItem = currentA.parent('li').siblings('li:first');
                if (nextItem.length === 0) {
                    return currentA;
                }
            }

            while ( this.isSkip(nextItem) )  {

                nextItem = nextItem.next('li');
                if (nextItem.length === 0 ) {
                    nextItem = currentA.parent('li').siblings('li:first');
                    if (nextItem.length === 0) {
                        return currentA;
                    }
                }
            }
            return nextItem.children('a');

        },

        _getPreviousItem: function(a) {
            var currentA = a;
            var prevItem = a.parent('li').prev('li');

            if (prevItem.length === 0) {
                prevItem = currentA.parent('li').siblings('li:last');
                if (prevItem.length === 0) {
                    return;
                }
            }

            while ( this.isSkip(prevItem) ) {

                prevItem = prevItem.prev('li');
                if (prevItem.length === 0 ) {
                    prevItem = currentA.parent('li').siblings('li:last');
                    if (prevItem.length === 0) {
                        return;
                    }
                }
            }
            return prevItem.children('a');
        },
        _activateItem: function(a, action, boolDelay) {

            // activates an <a> element in a menu. What happens depends on the action passed in
            // this function is flexible enough to deal with mouseovers as well as keyboard actions
            // the function is called from _delegate()

            // Possible values of action are:
            // 1) 'click'. In this case, will toggle activation of item and open submenu if there is one. This is called from
            // _delegate to toggle the first menu items

            // 2) 'mouseover'. In this case, will activate item and open submenu if there is one will close siblings' submenus and             // close grandchildren submenus. This is called from _delegate and used when user mouseovers over items


            // 3) 'up'. In this case will activate item above current <a> node that is passed in and close sibling menus. It will
            // not open submenus of the new item. If current <a> node passed in is first item, it will activate last item in menu


            // 4) 'down'. In this case will activate item below current <a> node that is passed in and close sibling menus. It will            // not open submenus of the new item. If current <a> node passed in is last item, it will activuate first item in menu.

            // 5) 'left'. In this case, will close submenu of current <a> passed in and hilite parent menu's a node

            // 6) If null is passed in as action, or nothing is passed in as action, will just hilite and focus <a> node that is passed in

            // whether to delay opening submenus
            var styles = this.styles;
            var itemFirstClass = styles.itemFirst;
            var that = this;
            var a = $(a);
            var parentLi = a.parent('li');
            var submenuFadeSpeed = this.options.submenuFadeSpeed;
            var delay = this.options.submenuDelay;
            var childSubmenu = a.next('ul');
            var action = action || null;

            switch (action) {

                // true when clicking on first items in menubar or menu, when called from _delegate, toggle first items
                case 'click':

                    // if ( ! a.hasClass( styles.hilited) ) {
                        //this._hiliteItem(a);
                    // }

                    if ( !childSubmenu.hasClass(styles.submenuOpen) ) {
                        this._openSubmenu( { el: childSubmenu } );
                    } else {
                        this._closeSubmenu( { el: childSubmenu } );
                    }

                    // if one first menu is opened, all other first menu's submenus must be closed
                    /* Getting rid of this because this is not relevent after first clicking a first level item
                    this.firstItems.each( function() {

                        if ( this !== parentLi[0] ) {
                            // close all descendent menus
                            $( this).find('ul').each( function() {
                                // only call close submenu, if submenu is open
                                var sm = $(this);
                                if ( sm.hasClass(  styles.submenuOpen) ) {
                                    that._closeSubmenu( { el: sm } );
                                }
                            });
                        }
                    });
                    */
                    break;

                case 'mouseover':
                    this._closeSiblingSubmenus(a, true); // pass true for fade effect

                    this._closeGrandChildSubmenu(a, true);

                    this._unHiliteSiblings(a);

                    this._hiliteItem(a);

                    // if clicking left or right, we call activateItem passing 'mouseover' since it
                    // does the same thing as when we mouseover. However if we click left or right on first items
                    // we don't want delay
                    if ( boolDelay !== false ) {
                        if (timer) {
                            window.clearTimeout( timer);
                        }

                        timer = window.setTimeout(
                            function() { that._openSubmenu( {
                                el: a.siblings('ul'),
                                delay: delay,
                                submenuFadeSpeed: submenuFadeSpeed
                            })}, that.options.openSubmenuDelay
                        );
                    } else {
                        this._openSubmenu( {
                            el: a.siblings('ul')
                        });
                    }
                    //console.groupEnd(action);
                    break;

                case 'up':

                    this._closeChildSubmenu(a);

                    a = this._getPreviousItem(a);


                    this._unHiliteSiblings(a);

                    this._hiliteItem(a);
                    //console.groupEnd(action);
                    break;

                case 'down':
                    // if on first item, down opens submenu
                    if ( parentLi.hasClass(itemFirstClass) ) {
                        this._openSubmenu( { el: a.siblings('ul') });
                        a = a.siblings('ul').find('li:first > a');
                        if (a.length > 0 ) {
                            this._hiliteItem(a);
                        }
                    } else {
                        this._closeChildSubmenu(a);

                        a = this._getNextItem(a);

                        this._unHiliteSiblings(a);

                        this._hiliteItem(a);
                    }
                    //console.groupEnd(action);
                    break;

                case 'right':

                    // if on a container, open up its submenu and activate first <a>
                    if (parentLi.hasClass(styles.itemHasSubmenu) ) {
                        // open submenu, move into first a

                        this._openSubmenu( {
                            el: childSubmenu,
                            callback: function() { // we need a callback here, or we can't focus on the new item
                                a = childSubmenu.children('li:first').children('a');
                                that._hiliteItem(a);
                            }
                        });
                    }

                    // if on a first <a>, activate next first item <a>
                    else if ( parentLi.hasClass(itemFirstClass) ) {
                        a = this._getNextItem(a);
                        this._activateItem(a, 'mouseover', false);
                    }

                    // if on first submenu leaf item, open up next first item submenu
                    else if ( parentLi.hasClass(styles.itemLeaf) && parentLi.parent('ul').parent('li').hasClass(itemFirstClass) ) {
                        a = this._getNextItem( parentLi.parent('ul').parent('li').children('a') ) ;
                        this._activateItem(a, 'mouseover');

                    // otherwise you are in a submenu leaf, so open up first item submenu
                    } else {

                        // traverse up until you find first item, then next first item
                        a.parents().each ( function() {
                            var p = $(this);
                            if (p.hasClass(itemFirstClass) ) {
                                a = that._getNextItem( p.children('a'));
                                that._activateItem(a, 'mouseover');
                                return;
                            }
                        });
                    }

                    break;

                case 'left':
                    // only on non first items
                    if (!parentLi.hasClass(itemFirstClass)) {
                        this._closeSubmenu( {el: a.parent('li').parent('ul') });
                        a = a.parent('li').parent('ul').prev('a');
                        this._hiliteItem(a);
                        //console.groupEnd(action);

                    }
                    // parentli's parent ul parent li is first then open prev first menu item
                    if ( parentLi.hasClass(itemFirstClass) ||
                    parentLi.parent('ul').parent('li').hasClass(itemFirstClass)) {
                        this._unHiliteItem(a);
                        a =  this._getPreviousItem(a);
                        this._activateItem(a, 'mouseover', false);
                    }
                    break;

                default:
                    // action is null, just hilite and focus
                    this._hiliteItem(a);
            }
        }

	}); // end widget def

	// these are default options that get extended by options that can be passed in to widget
	$.ui.ncbimenu.prototype.options = {
        openSubmenuDelay: 300,
        submenuZindex: '1002'
	};
    $.ui.ncbimenu._areInstances = null;
    $.ui.ncbimenu.getter = 'areInstances';

    // keep track of the active menu link. We use this for managing tabindex since there can only be one at a time
    $.ui.ncbimenu._activeLink = null;
    // store root nodes of menus, so we don't have to search the DOM multiple times
    $.ui.ncbimenu._rootNodes = [];




})(jQuery);
jQuery.widget("ui.ncbipopper", {

    options: {

        openMethod: "", //depreciated
        openEvent: "mouseover",

        openAnimation: "slideDown", //fadeIn, none

        closeMethod: "", //depreciated
        closeEvent: "mouseout",

        isEscapeKeyClose: true,

        isSourceElementCloseClick: false, //depreciated
        isTriggerElementCloseClick: true,

        isTargetElementCloseClick: false, //depreciated
        isDestElementCloseClick: false,

        isDocumentCloseClick: true,

        addCloseButton: false,

        closeAnimation: "slideUp", //fadeOut, none
        adjustFit: "autoAdjust",
        delayTimeout : 300,

        sourcePosition: "", //depreciated
        destPosition: "top left",

        sourceSelector: "", //depreciated
        destSelector: "",

        targetPosition: "", //depreciated
        triggerPosition: "bottom right",

        sourceText: null, //depreciated
        destText: null,

        multipleHandlesSelector: "",

        hasArrow: false,
        arrowDirection: "left",

        groupName: null,

        width : null,
        height : null,

		cssClass : null,
		excludeBasicCssStyles : false,
		ignoreSettingOutterWidth : false,
		
        loadingText : "loading...", //Placeholder text while function is running
        showLoadingMessage : true,  //This is used when there is a delay with serverside call and you do not want the popper to show up until response is returned.
        isDestTextCacheable : true //Determines if the function in destText is executed once or everytime it is called

    },

    _create: function () {

        this._addTriggerHandlers();
        this._addTriggerAria();
        this._addResizeListener();
        this._addEscapeListener();
        this._addDocumentClickListener();

    },

    destroy : function(){

    },

    isPopperOpen : false,
    isPopperDisplayed : false,

    _isIE6 : (jQuery.browser.msie && parseInt(jQuery.browser.version, 10) === 6),
    _shim : false,

    _addTriggerHandlers : function(){

        var openEvent = this.options.openMethod || this.options.openEvent;
        var closeEvent = this.options.closeMethod || this.options.closeEvent;

        var that = this;

        if(openEvent === closeEvent && closeEvent === "click"){

            var openers = jQuery(this.element);


            var additionalOpeners = this.options.multipleHandlesSelector;
            if(additionalOpeners){
                openers = jQuery( additionalOpeners ).add(openers);
            }

            openers.click(
                function(e){
                    that._toggleView();
                    e.preventDefault();
                }
            );

        }
        else{

            var config = {
                timeout: this.options.delayTimeout
            };

            if(openEvent==="mouseover"){
                config.over = function(){ that.open(); };
            }
            else{
                config.over = function(){};
            }

            if(closeEvent==="mouseout"){
                config.out = function(){ that._isOverTrigger = false; if(!that._isOverPopper) that.close(); };
                this.isChildHoverable = true;
                jQuery(this.element).mouseover( function(){that._isOverTrigger = true;} );
            }
            else{
                config.out = function(){};
                this.isChildHoverable = false;
            }

            if( openEvent==="mouseover" || closeEvent==="mouseout"){
                jQuery(this.element).hoverIntent( config );
            }

            if( openEvent!=="mouseover"){

                jQuery(this.element).bind(openEvent,
                    function(e){
                        that.open();
                        if(openEvent==="click"){
                            e.preventDefault();
                        }
                    }
                );

            }

            if(closeEvent!=="mouseout"){
                jQuery(this.element).bind(closeEvent,
                    function(e){
                        that.close();
                        if(closeEvent==="click"){
                            e.preventDefault();
                        }
                    }
                );
            }


            if( (this.options.isSourceElementCloseClick || this.options.isTriggerElementCloseClick) && ( closeEvent!=="click" && openEvent!=="click")){
                jQuery(this.element).click(
                    function(e){
                        that.close(e);
                        e.preventDefault();
                    }
                );
            }

        }
    },

    _removeHandlers : function(){
        //TODO: Remove only the methods this code adds!
        var openEvent = this.options.openMethod || this.options.openEvent;
        var closeEvent = this.options.closeMethod || this.options.closeEvent;
        jQuery(this.element).unbind(openEvent).unbind(closeEvent)
    },

    _toggleView : function(){
        if(this.isPopperOpen){
            this.close();
        }
        else{
            this.open();
        }
    },

    open : function(){

        if(this.isPopperOpen){
            return;
        }

        this.element.trigger("ncbipopperopen");
        this.isPopperOpen = true;
        this._closeActivePopperGroup();
        this._setActivePopperGroup();

        this._positionPopper();

        if( !this.hasCallback ||
            this.isFunctionResultCached ||
            (this.hasCallback && this.options.showLoadingMessage)
        ) {
            this._displayPopper();
        }

    },

    close : function(){

        if( !this.isPopperOpen ){
            return;
        }

        this.element.trigger("ncbipopperclose");
        this.isPopperOpen = false;
        this._hidePopper();
        this._setAriaState(false);

    },


    _initDestElement : function(){

        //Need to override setData and see if the source selector is changed. If it is, than we need to wipe out this.content


        if(this.content){
            if(!this.options.isDestTextCacheable){
                this._reloadContent();
            }
            return this.content;
        }

        var linkHash = null;
        var hash = this.element.attr("hash");
        if(hash && hash.length>1 && hash.charAt(0)==="#"){
            linkHash = hash;
        }

        var theSelector = this.options.destSelector || this.options.sourceSelector || linkHash; 
        var theText = this.options.destText || this.options.sourceText;

        var content = null;
        if (theSelector && theSelector.length > 0) { 
            var contentElement = jQuery( theSelector );
        }
        else if (theText) {
            contentElement = this._createTextHolder( theText );
        }

        content = this._wrapContent( contentElement );

        this._addCloseButton(content);
        this._addDestElementCloseClick(content);

        this.content = content;
        if(this._isIE6 && !this._shim){
            this._shim = this.content.createIE6LayerFix( true );
        }

        this._addDestAria();

        return content;

    },

    getDestElement: function(){
        return this.content || this._initDestElement();
    },

    _wrapContent : function( contentSrc ){

        //TODO: Destroy these classes
		
        contentSrc.addClass("ui-ncbipopper-content");

		if(!this.options.excludeBasicCssStyles){
			contentSrc.addClass("ui-ncbipopper-basic");
		}
		
		
        var contentElement = contentSrc;
        contentElement = this._wrapElement(contentSrc);

        if( this.options.hasArrow ){
            this._addArrows(contentElement);
        }

        if( this.isChildHoverable ){
            this._addPopperHover(contentElement);
        }

        return contentElement;

    },

    _wrapElement : function( contentElement ){

        var clonedWidth = null;
        var clonedHeight = null;

        if( !contentElement.parent().hasClass("ui-ncbipopper-wrapper") ){

            var wrapperClass = "ui-ncbipopper-wrapper";
			var additionalClass = this.options.cssClass;
			if(additionalClass){
				wrapperClass += " "  + additionalClass;
			}

            //If the parent element has overflow set to hidden, we need to move the element out of the parent.
            if(contentElement.parent().css("overflow") === "hidden"){

                contentElement.addClass("ui-ncbipopper-content-hook");

                //Make a copy of our element
                var cloned = contentElement.clone();
                var id = contentElement.attr("id");

                //Add the element to the end of the body
                var lastElement = jQuery(document.body).children().last();
                cloned = cloned.insertAfter( lastElement );

                //Need to get the width/height since appending it to the body can give a different value
                if( contentElement.hasClass("offscreen_noflow") ){
                    contentElement.removeClass("offscreen_noflow");
                    clonedWidth = contentElement.width();
                    //clonedHeight = contentElement.height();
                }

                clonedWidth = contentElement.width();
                if(!this.options.width){
                    this.options.width = clonedWidth;
                }
                contentElement.remove();

                //ste the contentElement to the new one we just created
                cloned.attr("id",id);
                contentElement = cloned;

                wrapperClass += " ui-ncbipopper-hook";

            }

            var wrapper = jQuery("<div class='" + wrapperClass + "'></div>");

            //TODO: Add class back on when destroyed.
            contentElement.removeClass("offscreen_noflow");

            var width = this.options.width || clonedWidth || contentElement.width();
            var height = this.options.height || clonedHeight || contentElement.height();

            //had issues with content being short by a pixel
            if(contentElement[0].style.width.length===0 || contentElement[0].style.width==="auto"){
                if(!this.options.width){
                    if( parseFloat( width ).toString() === width.toString() ){
                        width = parseFloat( width ) + 1;
                    }
                }
                contentElement.css("width", width );
            }

            if(this.options.height ){
                contentElement.css( "height", height );
            }            

            var width = contentElement.outerWidth();
            
			if(!this.options.ignoreSettingOutterWidth){
				wrapper.width( width )
			}
            wrapper.css("display","none");
            contentElement.wrap(wrapper).css("display","block");

        }
        return contentElement.parent();

    },

    _addArrows : function(contentElement){

        if( jQuery(".ui-ncbipopper-arrow-image",contentElement).length===1 ){
            return;
        }

        var arrowDirection = this.options.arrowDirection;

        var arrowClass = "ui-ncbipopper-arrow-" + arrowDirection;

        var sourcePosition = this.options.sourcePosition || this.options.destPosition;
        sourcePosition = sourcePosition.split(/\s/);

        if( arrowDirection === "left" || arrowDirection === "right"){
            arrowClass += " " + arrowClass + "-" + sourcePosition[0];

            contentElement.width( contentElement.width() );
            contentElement.width( contentElement.outerWidth() + 20 );

            this.options._adjustOffsetX = 0;
            this.options._adjustOffsetY = sourcePosition[0]==="top" ? -10 :
                                   sourcePosition[0]==="center" ? 0 :
                                   10;
        }
        else{
            arrowClass += " " + arrowClass + "-" + sourcePosition[1];
            //contentElement.height( contentElement.outerHeight() + 20 );

            this.options._adjustOffsetX = sourcePosition[1]==="left" ? -10 :
                                   sourcePosition[1]==="center" ? 0 :
                                   10;
            this.options._adjustOffsetY = 0;

        }

        var arrowDiv = jQuery("<div class='" + arrowClass + "'><div class='ui-ncbipopper-arrow-image'>&nbsp;</div></div>");

        var content = contentElement.children().eq(0).addClass("ui-ncbipopper-arrow-" + arrowDirection + "-content");
        if( arrowDirection === "left" || arrowDirection === "top"){
            content.before(arrowDiv)
        }
        else{
            content.after(arrowDiv);
        }

    },

    _addPopperHover : function( contentElement ){
        var that = this;
        var config = {
            timeout: this.options.delayTimeout,
            over : function(){},
            out : function(){ that._overPopper( false ); }
        };
        jQuery( contentElement ).hoverIntent( config );

        jQuery( contentElement )
            .mouseover( function(){ that._overPopper( true ); } )
            .mouseout( function(e){ that._outOfPopper(e, contentElement); } );


    },

    _overPopper : function( isOver ){

        if(isOver){
            this._pTimer = new Date();
        }

        this._isOverPopper = isOver;
        if(!isOver && !this._isOverTrigger){
            this.close();
        }

    },

    _outOfPopper : function(e, contentElement){

        var tar = jQuery( e.target || e.srcElement);

        var pos = contentElement.position();        
        var maxWidth = contentElement.width() + pos.left;
        var maxHeight = contentElement.height() + pos.top;

        if(e.pageX || e.pageY){
            var mouseX = e.pageX;
            var mouseY = e.pageY;        
        }
        else{
            var scrollY = window.pageYOffset || document.documentElement.scrollTop || document.body.scrollTop || 0;
            var scrollX = window.pageXOffset || document.documentElement.scrollLeft || document.body.scrollLeft || 0;
            var mouseX = e.clientX +  scrollX;
            var mouseY = e.clientY +  scrollY;        
        }

        var isInsideOfBounds = (maxWidth>mouseX) && (mouseX>pos.left) && (mouseY>pos.top) && (maxHeight>mouseY);

        var isChild = tar.parents( contentElement ).length > 0 && isInsideOfBounds;
        if( isChild ){
            return;
        }

        var tDiff = new Date() - this._pTimer;

        if( this.options.delayTimeout > tDiff ){
            this._overPopper( false );
        }

    },

    positionPopper : function(){
        this._positionPopper();
    },
    _positionPopper : function(){

        var source = this._initDestElement();

        var posDest = (this.options.sourcePosition || this.options.destPosition).split(/\s/);
        var posTrig = (this.options.targetPosition || this.options.triggerPosition).split(/\s/);

        var posDestStr = posDest[1] + " " + posDest[0];
        var posTrigStr = posTrig[1] + " " + posTrig[0];

        var collision = this.options.adjustFit;
        if(collision==="autoAdjust" || collision==="slide"){
            collision = "fit";
        }
        else{
            collision = "none";
        }

        if(source.css("display") === "none"){
            source.css("visibility","hidden").css("display","block");
        }

        //We have a problem with overflow hidden on our portlets and this is used to detect the tooltip positioning issues
        var trigger = jQuery(this.element);
        var triggerHeight = trigger.height();
        var triggerPositionTop = trigger.position().top;
        var triggerParentHeight = trigger.parent().height();

        var offsetX = (this.options._adjustOffsetX || 0 );
        var offsetY = ( this.options._adjustOffsetY || 0);

        //if parent has overflow hidden and content overhangs, we need to adjust the position
        if( trigger.parent().css("overflow") === "hidden" && triggerPositionTop + triggerHeight > triggerParentHeight){

                var visibleHeight = triggerParentHeight - triggerPositionTop - triggerHeight;

                if(posTrig[0]==="center" || posTrig[0]==="middle"){
                    offsetY += visibleHeight/2;
                }
                else if(posTrig[0]==="bottom"){
                    offsetY += visibleHeight;
                }

        }

        var offsetPosition =  offsetX + " " + offsetY;

        source.position({
            my: posDestStr,
            of: this.element,
            at: posTrigStr,
            offset: offsetPosition,
            collision: collision
        });


        if(this._shim){

        var theMeat = this.content.find(".ui-ncbipopper-content");
            var theMeatPos = theMeat.position();

            var newPosTop = parseInt(this.content.css("top"),10) + parseInt( theMeatPos.top ,10);
            var newPosLeft = parseInt(this.content.css("left"),10) + parseInt( theMeatPos.left ,10);

            this._shim.css(
                {
                    "top" :  newPosTop + "px",
                    "left" :  newPosLeft + "px",
                    "z-index" : this.content.css("z-index") - 1
                }
            ).width(this.content.width() - parseInt( theMeatPos.left ,10)).height(this.content.height() - parseInt( theMeatPos.top ,10));

        }

    },

    _displayPopper : function(){

        var that = this;
        var source = this.getDestElement();
        
        var triggerElem = source.data("trigger");
        if(triggerElem && triggerElem.get(0) != this.element.get(0) && triggerElem.ncbipopper("isOpen")){  //see if this element is shared with another trigger and is open
            source.data("doNotClose",true);
			window.setTimeout( function(){ source.data("doNotClose",false); }, this.options.delayTimeout ); //make sure this parameter does not stay set
        }
        source.data("trigger", this.element);       

        var openAnimation = this.options.openAnimation;
        if(openAnimation === "none"){
            source.css("visibility","visible");
            that.element.stop(true, true).trigger("ncbipopperopencomplete");
            that.isPopperDisplayed = true;
            if(this._shim){
                this._shim.show();
            }
        }
        else{
            source.stop(true, true);
            source.css("visibility","visible").css("display","none");
            source[openAnimation]( 290, function(){
                that.element.trigger("ncbipopperopencomplete");
                that.isPopperDisplayed = true;
                if(that._shim)that._shim.show();
            });
        }
        this._setAriaState(true);

    },

    _hidePopper : function(){

        var that = this;
        var source = this.getDestElement();

        if(source.data("doNotClose")){
            that.isPopperDisplayed = false;
            source.data("doNotClose", false);
        }
        else{
            var closeAnimation = this.options.closeAnimation;
            if(closeAnimation === "none"){
                source.css("display","none");
                that.element.stop(true, true).trigger("ncbipopperclosecomplete");
                that.isPopperDisplayed = false;
            }
            else{
                source.stop(true, true);
                source[closeAnimation]( 150, function(){
                    that.element.trigger("ncbipopperclosecomplete");
                    that.isPopperDisplayed = false;
                });
            }

            if(this._shim){
                this._shim.hide();
            }
        }


    },

    _closeActivePopperGroup : function(){
        var groupName = this.options.groupName;
        if(groupName){
            var elem = jQuery.ui.ncbipopper.openGroups[groupName];
            if(elem && elem!==this.element){
                jQuery(elem).ncbipopper("close");
                jQuery.ui.ncbipopper.openGroups[groupName] = null;
            }
        }
    },

    _setActivePopperGroup : function(){
        var groupName = this.options.groupName;
        if(groupName){
            jQuery.ui.ncbipopper.openGroups[groupName] = this.element;
        }
    },

    _addEscapeListener : function(){

        if(this.options.isEscapeKeyClose){
            this._registerGlobalEscapeListener();
            var that = this;
            //TODO: DESTROY THIS!!
            jQuery(document).bind("ncbipopperescapepressed", function(){ that.close(); } );
        }

    },

    _registerGlobalEscapeListener : function(){

        if(jQuery.ui.ncbipopper.isGlobalEscapeRegistered){
            return;
        }

        jQuery.ui.ncbipopper.isGlobalEscapeRegistered = true;

        var obj = document;
        var meth = "keypress";               
        if(!jQuery.browser.msie&&!jQuery.browser.mozilla){
            obj=window;
            meth="keydown";
        }
        
        jQuery(obj)[meth](
            function(e){
                if(e.keyCode === 27){
                    jQuery(document).trigger("ncbipopperescapepressed");
                }
            }
        )
        
    },

    _addDocumentClickListener : function(){

        if(this.options.isDocumentCloseClick){
            this._registerGlobalClickListener();
            var that = this;
            //TODO: DESTROY THIS!!
            jQuery(document).bind("ncbipopperdocumentclick", function( obj, element){ that._checkDocumentClick( obj, element ); } );
        }

    },

    _registerGlobalClickListener : function(){

        if(jQuery.ui.ncbipopper.isGlobalClickRegistered){
            return;
        }

        jQuery.ui.ncbipopper.isGlobalClickRegistered = true;

        jQuery(document).click(
            function(e){
                jQuery(document).trigger("ncbipopperdocumentclick", [(e.srcElement || e.target)] );
            }
        )

    },

    _addResizeListener : function(){
        this._registerGlobalResizeListener();
        var that = this;
        jQuery(window).bind("ncbipopperdocumentresize", function(){ that._pageResized();} );
    },

    _registerGlobalResizeListener : function(){

        if(jQuery.ui.ncbipopper.isGlobalResizeRegistered){
            return;
        }

        jQuery.ui.ncbipopper.isGlobalResizeRegistered = true;

        jQuery(window).resize(
            function(e){
                jQuery(window).trigger( "ncbipopperdocumentresize" );
            }
        )

    },

    _pageResized : function(){
        if(this.isPopperOpen){
            this._positionPopper();
        }
    },

    _checkDocumentClick : function( obj, element ){

        if(element && this.isPopperOpen){

            element = jQuery(element);

            if( this._isSameElementOrChild(jQuery(this.element), element)
                || this._isSameElementOrChild(this.getDestElement(), element) ){
                return;
            }

            var multipleHandles = this.options.multipleHandlesSelector;

            if(multipleHandles ){
                var isHandle = false;
                var that = this;
                jQuery( multipleHandles ).each(
                    function(){
                        if(that._isSameElementOrChild( jQuery(this), element)){
                            isHandle = true;
                            return;
                        }
                    }
                );
                if(isHandle){
                    return;
                }
            }
            this.close();
        }

    },

    _isSameElementOrChild : function(parent, child){
        return (parent[0] == child[0]) || (jQuery(parent).has(child).length===1);
    },

    _addCloseButton: function ( content ) {

        if(!this.options.addCloseButton){
            return;
        }

        //depreciate ncbipopper-close-button in 2.0
        var closeButton = jQuery(".ui-ncbipopper-close-button, .ncbipopper-close-button", content);

        //TODO: DESTROY THIS ELEMENT
        if(closeButton.length===0){
            closeButton = jQuery("<button class='ui-ncbipopper-close-button ui-ncbipopper-close-button-generated'>x</button>").attr("generated", "true").prependTo(content);
        }

        //TODO: DESTROY THIS EVENT
        if(closeButton){
            var that = this;
            closeButton.click(
                function(e){
                    that.close();
                    e.preventDefault();
                }
            )
        }

    },

    _addDestElementCloseClick: function(content){

        var isDestElementCloseClick = this.options.isTargetElementCloseClick || this.options.isDestElementCloseClick;
        //TODO: Destroy this event
        if(isDestElementCloseClick){
            var that = this;
            content.click(
                function(e){
                    that.close();
                    e.preventDefault();
                }
            )
        }

    },

    _createTextHolder : function( theText ){

        var elem = jQuery("<div></div>");

        var customFnc = this._getFunction( theText );
        if (typeof customFnc === "function") {
            theText = this._getText( customFnc );
        }

        elem.css("display","none").attr("generated","true").html(theText).appendTo(document.body);

        return elem;

    },

    _getFunction: function(theText){

        var fnc = null;
        if(jQuery.ui.jig && jQuery.ui.jig._getFncFromStr){
            fnc = jQuery.ui.jig._getFncFromStr(theText);
        }
        else if(typeof theText === "function"){
            fnc = theText;
        }
        return fnc;
    },

    _getText: function( customFunc ){

        if(!customFunc){
            var fncString = this.options.destText || this.options.sourceText;
            customFunc = this._getFunction( fncString );
        }

        var that = this;
        var callBackFnc = function (html) {
            that._setHTML(html);
        }

        this.hasCallback = true;
        var theText = customFunc.call(this.element[0], callBackFnc) || this.options.loadingText;

        //mark function ran one time, don't mark if function uses callback functionality
        if(theText !== this.options.loadingText){
            this.isFunctionResultCached = this.options.isDestTextCacheable;
        }

        return theText;

    },

    _reloadContent: function(){
        var theText = this._getText();
        var elm = jQuery(".ui-ncbipopper-content", this.getDestElement());
        if (elm) {
            elm.html(theText);
        }
    },

    _setHTML: function (html) {
        var elm = jQuery(".ui-ncbipopper-content", this.getDestElement());
        if (elm) {
            elm.html(html);
            this.isFunctionResultCached = this.options.isDestTextCacheable;
            if(!this.options.showLoadingMessage && !this.isPopperDisplayed){
                this._displayPopper();
            }
        }
    },

    _addTriggerAria: function() {
        //TODO: Destroy this
        this.element.attr("role","button")
                    .attr("aria-expanded",false)
	},

    _addDestAria: function() {
        //TODO Destroy this
        var popper = this.getDestElement();
        popper.attr("aria-live", "assertive")
                     .attr("aria-hidden", true)
                     .addClass("ui-helper-reset");
	},

    _setAriaState: function(isOpen){
        this.element.attr("aria-expanded", isOpen);
        var popper = this.getDestElement();
        popper.attr("aria-hidden", !isOpen);
    },
    
    isOpen: function(){
       return this.isPopperOpen;
    }

});

jQuery.ui.ncbipopper.openGroups = {};
jQuery.ui.ncbipopper.isGlobalEscapeRegistered = false;
jQuery.ui.ncbipopper.isGlobalClickRegistered = false;
jQuery.ui.ncbipopper.isGlobalResizeRegistered = false;

jQuery.ui.ncbipopper._selector = ".jig-ncbipopper";

jQuery.ui.ncbipopper.getSelector = function () {
    return this._selector;
};
/*
ui.widgetName: ncbislideshow

@crafted by     Jody Clements, clement2@ncbi.nlm.nih.gov

*/

(function($) {
  $.widget("ui.ncbislideshow", {
    options: {
      fadeSpeed: 500,
      slideInterval: 4000, //15000,
      currentSlide: 1,
      height: '117px',
      width: '410px', 
      randomStart: false
    },

    _create: function() {
      // add styles
      this.element.addClass('ui-ncbislideshow')
        .children('ul').addClass('ui-ncbislideshow-slides');
      // figure out how many slides there are
      this.options.slideCount = this.element.find('ul.ui-ncbislideshow-slides > li').length

      if( this.options.randomStart ) {
        this._setRandomStart();
      }

      // build list for slide selection
      this.element.append(this._generateSkipList());

      // set image dimensions
      this.element.find('ul.ui-ncbislideshow-slides')
        .height( this.options.height)
        .width( this.options.width);

      // start animation
      this.element.find('.ui-ncbislideshow-skip li a:eq('+ this.options.currentSlide +')').addClass('active');
      this.element.find('.ui-ncbislideshow-slides li').hide();
      this.element.find('.ui-ncbislideshow-slides li:nth-child('+ this.options.currentSlide +')').show();
      this.startAnimation();
    },

    _setRandomStart: function() {
        var randomNum = Math.floor ( Math.random ( ) * this.options.slideCount + 1);
        this.element.ncbislideshow('option', 'currentSlide', randomNum );
    },

    startAnimation: function () {
      var self = this;
      this.element.find('span.ui-icon')
        .addClass('ui-icon-pause').removeClass('ui-icon-play')
        .attr('title','click to pause');
      this.options.animTimeout = setTimeout(function(){
        self._rotateSlides();
      },this.options.slideInterval);
    },

    stopAnimation: function () {
      this.element.find('span.ui-icon')
        .addClass('ui-icon-play').removeClass('ui-icon-pause')
        .attr('title','click to play');
      clearTimeout(this.options.animTimeout);
      delete this.options.animTimeout;
    },

    _rotateSlides: function () {
      var opts = this.options;
      var el   = this.element;
      this._advanceSlide();

      this.startAnimation();
    },

    _advanceSlide: function() {
      var opts = this.options;
      var el   = this.element;
      if (opts.currentSlide >= this.options.slideCount) {
        opts.currentSlide = 1;
      }
      else {
        opts.currentSlide++;
      }
      el.find('.ui-ncbislideshow-slides li:visible').fadeOut(opts.fadeSpeed);
      el.find('.ui-ncbislideshow-slides li:nth-child('+ opts.currentSlide +')').fadeIn(opts.fadeSpeed);
      el.find('.ui-ncbislideshow-skip > li > a').removeClass('active');
      el.find('.ui-ncbislideshow-skip li a:eq('+ opts.currentSlide +')').addClass('active');
    },

    skiptoSlide: function () {
      var opts = this.options;
      var el   = this.element;
      el.find('.ui-ncbislideshow-slides li:visible').fadeOut(opts.fadeSpeed);
      el.find('.ui-ncbislideshow-slides li:nth-child('+ opts.currentSlide +')').fadeIn(opts.fadeSpeed);
      el.find('.ui-ncbislideshow-skip > li > a').removeClass('active');
      el.find('.ui-ncbislideshow-skip li a:eq('+ opts.currentSlide +')').addClass('active');
      if(this.options.animTimeout){
        this.stopAnimation();
        this.startAnimation();
      }
    },

    _generateSkipList: function() {
      var self = this;
      var opts = this.options;
      var pauseLink = $('<a>').attr('href','#').attr('tabindex','-1')
        .append($('<span>').attr('title','click to pause')
        .addClass('ui-icon ui-icon-pause'))
        .bind('click', function (e) {
          if (opts.animTimeout) {
            self.stopAnimation();
          }
          else {
            self.startAnimation();
          }
          e.preventDefault();
        });

      var list = $('<ul>').width(opts.width).addClass('ui-ncbislideshow-skip')
        .append($('<li>').addClass('ui-ncbislideshow-playPause')
          .append(pauseLink));
      // add a 'skip to' link for each slide
      this.element.find('ul.ui-ncbislideshow-slides > li').each(function(i) {
        var num = i + 1;
        var slideLink = $('<a>' + num + '</a>').attr('href','#').attr('tabindex','-1')
          .bind('click', function (e) {
            if( self.options.currentSlide === num ) {
              return;
            }
            self.options.currentSlide = num;
            self.skiptoSlide();
            e.preventDefault();
          });
        list.append($('<li>').append(slideLink));
      });

      return list
    },

    destroy: function () {
      // teardown in reverse order to the create
      this.element.find('.ui-ncbislideshow-slides li').show();
      // stop animation
      this.stopAnimation();
      // remove slide selection list
      this.element.children('.ui-ncbislideshow-skip').remove();
      // remove styles
      this.element.removeClass('ui-ncbislideshow')
        .children('ul').removeClass('ui-ncbislideshow-slides');
      $.Widget.prototype.destroy.apply(this, arguments);
    }

  }); // end widget def

  $.ui.ncbislideshow.prototype.version = '2.0';
})(jQuery);
jQuery.widget("ui.ncbitabpopper", {

    options : {
        "offsetLeft": -20,
        "offsetTop" : 0,
        "innerOffset" : 0
    },

    _create: function(){ 
        this._addhandlers();
        this._createPoppers();
    },
    
    _addhandlers: function(){
    
        var ref = this;
        
        jQuery("a[sourceContent], button[sourceContent]", this.element).click(function(event){ 
            var valFnc = this.getAttribute("tabvalidate");
            var e = event;
            var evt = ref._open(event, this, valFnc);
            
            if(evt!==false && this.onfooclick){
                this.onfooclick(e);
            }
            
        });
        
        jQuery(document).click(function(event){
            ref._globalClick(event);
        });

        jQuery(window).resize(function(event){
            ref._positionBox(event);
        });
        
    },
    
    _popBox : null,
    
    _createPoppers: function(){
        var divCover = "<div class='ui-ncbi-tabpopper-cover'></div>";        
        this._popBoxCover = jQuery(divCover).appendTo(document.body);
    },
    
    _activeMenu: null,
    
    _open: function(event, elem, valFnc){

        if(event != null){
            event.preventDefault();
            event.stopPropagation();
        }
        
        if (this._activeMenu !== null) {

            var isSameTab = (this._activeMenu[0] == elem);
            this.close();
                         
            if (isSameTab) {
                return;
            }
            
        }
        
        if(valFnc){
            var ref = this;
            if(valFnc.indexOf(".") === -1){
                var fnc = window[valFnc];                
            }
            else{
                fnc = window;
                var pathParts = valFnc.split(".");
                for(var i=0;i<pathParts.length;i++){
                    fnc = fnc[pathParts[i]];
                }
            }
            var x = fnc( function(){ ref._openPane(elem); } );        
            if(x!==true)return;            
        }

        this._openPane(elem);
        
    },
    
    _openPane : function(elem){
        this._activeMenu = jQuery(elem);
        if(this._activeMenu.length>0){
            this._popBox = jQuery("#" + this._activeMenu.attr("sourceContent"));
            this._popBox.addClass("ui-ncbi-tabpopper");
            this._positionBox();
        }    
    },
    
    open : function( elem ){
        if(typeof elem === "string"){
            elem = jQuery(elem);
        }
        var att = elem.attr("tabvalidate");
        this._open(null, elem, att);           
    },
    
    _globalClick: function(event){
    
        if (this._activeMenu !== null) {
        
            var tar = event.target;
            while (tar != null) {

                if (tar == this._popBox[0] || tar == this._popBoxCover[0]) {
                    break;
                }
                
                tar = jQuery(tar).parent();
                if (tar.length > 0) {
                    tar = tar[0];
                }
                else {
                    break;
                }
            }
            if (tar.length === 0) {
                this.close();
            }
            
        }
        
    },
    
    close: function(){
        if (this._activeMenu !== null) {

            var that = this;
            var oldActiveMenu = this._activeMenu;
            var foo = function(){
                oldActiveMenu.removeClass("active");
                that.element.trigger("tabpopperclose",[oldActiveMenu]);
            }
            this._popBox.css("z-index",1001).slideUp(170,foo);
            this._popBoxCover.css("z-index",1002).css("display","none")
            this._activeMenu = null;
        }
    },
    
    
    _positionBox: function(){
    
        var link = this._activeMenu;
        if(!link) return;
        
        var pos = link.position();
        var h = link.height();
        
        var pdB = Math.floor(parseFloat(link.css("padding-bottom"))) || 0;
        var pdL = Math.floor(parseFloat(link.css("padding-left"))) || 0;
        var pdR = Math.floor(parseFloat(link.css("padding-right"))) || 0;
        var bdL = Math.floor(parseFloat(link.css("border-left-width"))) || 0;
        var maR = Math.floor(parseFloat(link.css("margin-right"))) || 0;
        var maL = Math.floor(parseFloat(link.css("margin-left"))) || 0;
        var maB =  parseFloat(link.css("margin-bottom")) || 0;        
        if(isNaN(maB)){
            maB = 0;
        } else{
            maB = Math.floor(maB);     
        }
        
        var top = pos.top + h + pdB + this.options.offsetTop - maB;
        var left = pos.left + this.options.offsetLeft;
                
        if (left < 0) {
            left = 0;
        }
        else if (this._popBox.width() + left > jQuery(window).width()) {
                var adj = jQuery(window).width() - this._popBox.width()-12;
                left = (adj > 0) ? adj : 0;
        }
        
        var aWidth = link.width() + pdL + pdR - 2 * this.options.innerOffset;
        var aLeft = pos.left + bdL + maL + this.options.innerOffset;
        
        link.addClass("active");
        
        var that = this;
        function notifyOpen(){
            that.element.trigger("tabpopperopen",[link]);
        }
        
        this._popBox.css("top", top + "px").css("left", left + "px").css("z-index",1003).slideDown(170, notifyOpen);
        this._popBoxCover.width(aWidth).css("top", top + "px").css("left", aLeft + "px").css("z-index",1004).slideDown(100);
        
    }
    
});


/*********
ui.ncbitoggler:
UI Plug in that allows toggling display of some slave node when a master is clicked.
*/

(function($) {

    
    $.widget("ui.ncbitoggler", { // params: name of widget, prototype. Keep widget in ui namespace.
    // a style dictionary, keep the strings of class names centralized in one place
        styles: {
            ariaWrapper: 'ui-ncbitoggler-live',
            groupMasterReplaceNode: 'span.ui-ncbitoggler-group-replace-txt',
            groupMasterStateClose: 'ui-ncbitoggler-group-master-to-close',
            groupMasterStateOpen: 'ui-ncbitoggler-group-master-to-open',
            master: 'ui-ncbitoggler',
            masterOpen: 'ui-ncbitoggler-open',
            slave: 'ui-ncbitoggler-slave',
            slaveOpen: 'ui-ncbitoggler-slave-open',
            icon: "ui-icon-triangle-1-e",
            iconOpen: "ui-icon-triangle-1-s",
            iconPlusMinusBig: 'ui-icon-plus-minus-big',
            iconPlusMinusBigOpen: 'ui-icon-plus-minus-big-open'
        },

        _create: function() { // _init is used for setting up classes, binding events etc

            // in case you want to reinitialize a disabled toggler
            this._setOption('disabled', false);
            this.addedHref = null; // if href is added via script, we need to revert back with destroy method
            //console.log(this._getData('contextNode'));
            this.cookieName = null; // the key of the open or closed cookie, used for remembering state of toggler
            this.remember = null; // remember if remembering state
            this.rememberCookie = null; // the actual cookie
            this.startOpen = null; // indicates whether the toggler starts in the open state or not
            this.iconSpan = null; // icon node
            this.initSlavesClass = null; // remember the initial class of slaves, so we can revert them in destroy
            this.appendTextNode = null;
            var master = this.element;
            var masterD = master[0];
            var styles = this.styles;
            var that = this;
            var isIcon = this.options.isIcon;

            master.addClass('ui-widget ui-ncbitoggler');
            if ( !isIcon) {
                master.addClass('ui-ncbitoggler-no-icon');
            }
            // make sure markup in an <a> node and that it has an href value
            var href = master.attr('href');
            (! href) ? this.addedHref = true : this.addedHref = false;
            this._validate(href);
            
            // check to see if span around master text exists, if not wrap it
            if ( master.children('span.ui-ncbitoggler-master-text').length === 0) {
                master.contents().
                filter(function() {
                    return this.nodeType === 3;
                }).each( function() {
                    var el = $(this);
                    if ( jQuery.trim(this.nodeValue).length > 0) {
                        el.wrap('<span class="ui-ncbitoggler-master-text"></span>');
                    }
                })
            }

            
            // check to see that if user set openAppended text or closedAppended text, there is an existing span node for it
            if ( this.options.openedAppendText !== '' || this.options.closedAppendText !== '') {

                this.appendTextNode = master.children('span.ui-ncbitoggler-appended-text');
                if (this.appendTextNode.length === 0) {
                    throw new Error('ncbitoggler: when setting openAppendText or closedAppendText options, must have an existing <span class="ui-ncbitoggler-appended-text"> node as first child of master');
                }
            } 
            
            // if we are remembering state, check the id attribute and set/get the cookie
            this.remember = this.options.remember || false;
            if (this.remember) {
                var masterId = this.element.attr('id');
                if (! masterId) { 
                    if ( window.console && console.error) {
                        console.error("ncbitoggler: master node " + masterD + " must have a unique id attribute when remembering state");
                    }
                    return;
                 }
                this.cookieName = this.options.cookieNamePrefix + masterId;
                this.rememberCookie = $.cookie(this.cookieName);
            }
            
            // get master's initial state either from class name, or from cookie
            if ( ! this.rememberCookie ) {
                this.startOpen = (master.hasClass('jig-ncbitoggler-open') || master.hasClass(styles.masterOpen) || this.options.initOpen ) ? true: false;
            // there is a cookie, so we get state from the cookie
            } else { 
                this.startOpen = (this.rememberCookie === 'open')? true : false;
            }

            // add 'real' display class based on initial state, so that its added in init and can be taken off in destroy 
            if (this.startOpen) {
                master.removeClass(this.styles.master);
                master.addClass(styles.masterOpen);
            } else {
                master.removeClass(this.styles.masterOpen);
                master.addClass(styles.master);
            }

            if (isIcon) {
                // Decide whether we need to add span for icon or not, remember the icon node so we can remove it in destroy
                this._initMasterIcon();
            }
            
            this.slaves = this._getSlaves(); 

            // Now that we have we have slaves, if this widget was disabled and then initialized, enable it and reset
            this.slaves.
                addClass('ui-widget ui-ncbitoggler').
                removeClass('ui-ncbitoggler-disabled ui-state-disabled');


            // init slaves to closed, if not already and we are not initializing open
            // remember initial state of slaves, so we can revert them in destroy
            if (this.startOpen) { 
                this.slaves.removeClass(styles.slave);
                this.slaves.addClass(styles.slaveOpen);
                this.initSlavesClass = styles.slaveOpen;
            } else { 
                this.slaves.removeClass(styles.slaveOpen)
                this.slaves.addClass(styles.slave)
                this.initSlavesClass = styles.slave;

            }

            this._initAria();
            this._doGrouping();

            // in all cases, set the click event on the master
            master.click( function(e) { that.toggle(e, that) } );
           
        }, // end _init function 
        
        _getGroupMaster: function() {
            // returns open or close group link for a given toggler instance
            var styles = this.styles;
            var classNames = this.element[0].className.split(/\s+/);
            for (var i = 0; i < classNames.length; i++) {
                var className = classNames[i];
                var groupMasterClass = className.match(/ui-ncbitoggler-group-(.+)$/);
                if (groupMasterClass) {
                    var groupName = groupMasterClass[1];
                    var groupMaster = $('a.ui-ncbitoggler-group-open-' + groupName + ', a.ui-ncbitoggler-group-close-' + groupName, this.options.contextNode);
                    if (groupMaster.length === 1) {
                        if ( groupMaster.hasClass('ui-ncbitoggler-group-open-' + groupName) ) {
                            groupMaster.addClass(styles.groupMasterStateOpen);
                            
                        }
                        if ( groupMaster.hasClass('ui-ncbitoggler-group-close-' + groupName) ) {
                            groupMaster.addClass(styles.groupMasterStateClose);
                        }
                        return groupMaster;
                    }
                }
            }
        },

        _doGrouping: function() {
            // does all the grouping functionality so that we can have an <a> node control a group of toggler instances

            // for each master element, get its master group controller
            var groupMaster = this._getGroupMaster();

            if (groupMaster) {
                var that = this;
                var styles = this.styles;

                // first we have to store all togglers in group master, so we can tell if we are on the last one later. We need to know if we are on 
                // the last one so that we can change the state of the group master
                var togglers = groupMaster.data('togglers');
                var masterId = this.element.id || $.ui.jig._generateId(this.widgetName);
                if ( ! this.element[0].id) {
                    this.element[0].id = $.ui.jig._generateId( this.widgetName);
                }

                
                if ( typeof togglers === 'undefined'){
                    
                    groupMaster.data('togglers', []);
                    groupMaster.data('togglers').push(this.element);
                } else {
                    togglers.push(this.element);
                }
         
                // the node we will swap the text out and into
                var replaceTxtNode = groupMaster.find(styles.groupMasterReplaceNode);
                groupMaster.click( function(e) {
                    e.preventDefault();
                    var togglers = groupMaster.data('togglers');
                    var lastToggler = togglers[togglers.length-1][0];

                    // what state are we in? we need to know so we know whether to open or close
                    if ( groupMaster.hasClass(styles.groupMasterStateOpen) ) {
                        that.open();
                        if (replaceTxtNode.length > 0) {
                            replaceTxtNode.text( that.options.groupMasterCloseReplaceText);
                        }

                        // if this instance is the last one, then change state
                        if ( that.element[0] === lastToggler) {
                            groupMaster.
                                removeClass(styles.groupMasterStateOpen).
                                addClass(styles.groupMasterStateClose);
                        }
                    } else {
                        that.close();
                        if (replaceTxtNode.length > 0) {
                            replaceTxtNode.text( that.options.groupMasterOpenReplaceText);
                        }
                        if ( that.element[0] === lastToggler) {
                            groupMaster.
                                removeClass(styles.groupMasterStateClose).
                                addClass(styles.groupMasterStateOpen);
                        }
    
                    }
                });
            }

        },
        _initAria: function() {
            var that = this;
            var open = this.startOpen;
            this.element.
            attr({
                'role': 'button',
                'aria-expanded': open
            });

            this.slaves.attr({ 
                'aria-hidden': open? false : true
            }); 

            // wrap slaves in div for live regions
            // TODO should check if live regions is there or
            // not, if there, don't wrap.
            if ( this.options.liveRegions ) {
                this.slaves.each( function() {
                    $(this).wrap('<div class="ui-helper-reset" aria-live="assertive">');
                }); 
            }
        },
        
    

        _validate: function(href) {
            var el = this.element;
            var validTags = ['a', 'button' ];
            var masterTagName = el[0].tagName.toLowerCase();
            if ( $.inArray( masterTagName, validTags ) === -1 ) {
                throw new Error('ncbitoggler: master node must be one of the following elements: ' + validTags.toString() );
            }       
            if ( ! href) {
                el.attr('href', '#');
            }
        },
        _initMasterIcon: function() {
            // adds icon if not there, remembers icon for later reference when destroying and updating
            var el = this.element;
            var styles = this.styles;
            
            // get the type of icon, default is small arrow
            if ( this.options.indicator == 'plus-minus-big') {
                this.iconOpenClass = styles.iconPlusMinusBigOpen;    
                this.iconCloseClass = styles.iconPlusMinusBig; 
            } else {
                this.iconOpenClass = styles.iconOpen;    
                this.iconCloseClass = styles.icon; 
            }
            
            this.iconSpan = el.find('span.ui-icon');

            if ( this.iconSpan.length === 0 ) {
                // there is no icon, we will add it
                // get a reference to the newly created span
                this.iconSpan = $('<span class="ui-icon"></span>');

                // add the appropriate class to icon to indicate if its open or closed
                (this.startOpen) ? this.iconSpan.addClass(this.iconOpenClass) : this.iconSpan.addClass(this.iconCloseClass);
                el.append( this.iconSpan);
            } else { // icon is already in markup source
                if (this.startOpen) {
                    this.iconSpan.removeClass(this.iconCloseClass);
                    this.iconSpan.addClass(this.iconOpenClass);
                }
            }
        },
        getSlaves: function() {
            return this._slaves;
        },
        _getSlaves: function() {

            // this private function both returns the slaves and remembers them, so that they can be
            // accessed after render. This is necessary because the slaves are wrapped in an aria div
            // and that div could be returned instead of the actual slaves

            // If nonsibling slaves, then slaves are nodes with ids = values of slave's 'toggles' attr
            var el = this.element;
            var togglesAttr = el.attr('toggles');
            var slaves;
            if ( togglesAttr ) { 
                this.isSibling = false;
                var ids = togglesAttr.split(/ +/);
                var selector = '';
                for (var i = 0; i < ids.length; i++) {
                    if (i !== ids.length - 1) {
                        selector += '#' + ids[i] + ',';
                    } else {
                        selector += '#' + ids[i];
                    }
                }
                
                slaves = $(selector);

            // could be, master is inside a header, therefore slaves are parent's next sibling
            } else if( el.parent(':header').length > 0 ) {
                // add zoom: 1 to parent for IE7 haslayout bug
                // to do, this is a bad place to do that, kind of disorganized
                // also selector here can be more efficient, but it's fast anyway
                el.parent(':header').
                    parent().
                    css('zoom', '1' );
                this.isSibling = true;
                // put zoom 1 on heading because of haslayout bug
                slaves =  el.parent(':header').css('zoom', '1').next();

            // If none of the above, slaves are next sibling of master
            } else {
                this.isSibling = true;
                slaves =  el.next();
            } 
            // remember at this point so we can get it via public method
            this._slaves = slaves;
            return this._slaves;
        },

        _openSlaves: function() {
           // here 'this' is each slave element
            var slaves = this.slaves;
            var iconSpan = this.iconSpan;
            var styles = this.styles;

            // slave is closed, so switch classes
            slaves.
                removeClass( styles.slave).
                addClass( styles.slaveOpen).
                //css('display', '').
                attr('aria-hidden', 'false');
    
            // if append text, do it now
            var openAppend = this.options.openedAppendText;   
            if ( openAppend !== '' ) { // we got the append node in _init
                this.appendTextNode.html(openAppend);
            }
            
            // when slaves are done opening, take off style property
            // put on my animation
            slaves.css('display', '');
        },
        _closeSlaves: function() {
           // here 'this' is each slave element
            var slaves = this.slaves;
            var iconSpan = this.iconSpan;
            var styles = this.styles;

            // slave is open, so switch classes
            slaves.
                removeClass( styles.slaveOpen).
                addClass( styles.slave).
                //css('display', '').
                attr('aria-hidden', 'true');
    
            // if append text, do it now
            var closeAppend = this.options.closedAppendText;   
            if ( closeAppend !== '' ) { // we got the append node in _init
                this.appendTextNode.html(closeAppend);
            }

            // when slaves are done opening, take off style property
            // put on my animation
            slaves.css('display', '');
        },
        _fixHasLayout: function(that) {
            // if there's an element in the slave and IE7 sometimes content in slave disappears
            // when slave is opened.
            window.setTimeout( function() {
                that.slaves.addClass('foo').
                            removeClass('foo');

            }, 1);
        },
        open: function( callback ) {
                var master = this.element;
                var that = this;
                var styles = this.styles;
                if ( master.hasClass(styles.master) ) {

                    master.
                        removeClass(styles.master).
                        addClass(styles.masterOpen).
                        attr('aria-expanded', 'true');
                        
                    // update master icon
                    if (this.options.isIcon) {
                        this.iconSpan.
                            removeClass(this.iconCloseClass).
                            addClass(this.iconOpenClass);
                    }
                        
                    // if remembering state, update cookie
                    if ( this.remember) {
                        $.cookie(this.cookieName, 'open');
                    }
    
                    if (this.options.animation === 'slide') {
                        this.slaves.slideToggle(this.options.speed, function() {
                            that._openSlaves();
                            if ( $.browser.msie) {
                                that._fixHasLayout(that);
                            }
                            master.trigger('ncbitoggleropen');
                            if ( callback ) {
                                callback();
                            }
                        });
    
                    } else {

                        this._openSlaves();
                        if ( $.browser.msie) {
                            that._fixHasLayout(that);
                        }
                        master.trigger('ncbitoggleropen');
                        if ( callback ) {
                            callback();
                        }

                    }
                }

            return master;

        },
        
        close: function( callback ) {
                var master = this.element;
                var that = this;
                var styles = this.styles;
                if ( master.hasClass( styles.masterOpen)) {

                    master.
                        removeClass(styles.masterOpen).
                        addClass(styles.master).
                        attr('aria-expanded', 'false');
                        
                    // update master icon
                    if (this.options.isIcon) {
                        this.iconSpan.
                            removeClass(this.iconOpenClass).
                            addClass(this.iconCloseClass);
                    }
                        
                    // if remembering state, update cookie
                    if ( this.remember) {
                        $.cookie(this.cookieName, 'closed');
                    }
    
                    if (this.options.animation === 'slide') {
                        this.slaves.slideToggle(this.options.speed, function() {
                            that._closeSlaves();
                            master.
                                trigger('ncbitogglerclosed').
                                trigger('ncbitogglerclose');

                            if ( callback) {
                                callback();
                            }
                        });
    
                    } else {
                        this._closeSlaves();
                        master.trigger('ncbitogglerclosed');
                        if ( callback) {
                            callback();
                        }
                    }
                }

            return master;
        },

        toggle: function(e, that, callback) {
            // toggle toggles appearance of slaves, given a master element
            // the element can either be a dom node or jQuery collection
            // toggle can be called from init, or programatically
            // if called from init, it is a handler and gets the event object
            // if there are no arguments, we know, it was called programatically, sort of
            // this API can be improved because we might want to pass a speed, or other stuff
            

            // I know this part is messy, and should be cleaned up
            var argLen = arguments.length;
            if (argLen === 0 || argLen === 1) { 
                 // if called programatically there is no 'that', it's just this
                 var that = this;
                // if one arg, it's a callback
                if ( argLen === 1 ) {
                    callback = arguments[0];
                }
            } else {
                e.preventDefault();
            }
            var master = this.element;
            var styles = this.styles;
            var iconSpan = this.iconSpan;
            // toggle the master between ui-toggler and ui-toggler-open
            if ( master.hasClass(this.styles.master) ) { // started closed, now it's open
                that.open( callback );   
            } else { // started open, now its closed
                that.close( callback );
            }           
        },
        
        _removeHandlers: function() {
            // this may unbind events other than the one we attached
            this.element.unbind('click');
            this.element.click( function(e) { e.preventDefault(); });
        },

        destroy: function() {
            // inherit from UI framework's destroy method           
            $.Widget.prototype.destroy.apply(this, arguments);

            var el = this.element;
            var styles = this.styles;

            // Remove classes we put on master element, including disabled classes if present
            el.removeClass(styles.master + ' ' + styles.masterOpen + ' ' + 'ui-state-disabled ui-widget ui-ncbitoggler-disabled').
                removeAttr('role').
                removeAttr('aria-expanded');

            // Remove classes we put on slaves 
            this.slaves.
                removeClass('ui-ncbitoggler-slave ui-ncbitoggler-slave-open ui-state-disabled ui-widget ui-ncbitoggler-disabled');
                
            // I hate jQuery's remove(), remove the icon that we put in
            if (this.options.isIcon) {
                var iconD = el.find('span.ui-icon')[0];
                el[0].removeChild(iconD);
            }

            // bug here, that if we disable then destroy, the href isn't taken off if it was put on in init.
            if (this.addedHref) {
                el.removeAttr('href');
            }

            if ( this.options.liveRegions ) {
                // take out aria div, if there is one
                 
            } 
            
            // take off events
            this._removeHandlers();
            return this;
        },
        disable: function() {
            // inherit from UI framework's destroy method           
            $.Widget.prototype.disable.apply(this, arguments);
            this._removeHandlers();
            this.slaves.addClass('ui-ncbitoggler-disabled ui-state-disabled');
            return this;

        },
        enable: function() {
            $.Widget.prototype.enable.apply(this, arguments);
            this.slaves.removeClass('ui-ncbitoggler-disabled ui-state-disabled');
            var that = this;
            this.element.click( function(e) { that.toggle(e, that) } );
            return this;
        }
    }); // end widget def

    // these are default options that get extended by options that can be passed in to widget
    $.ui.ncbitoggler.prototype.options = {
        animation: 'slide',
        closedAppendText: '',
        cookieNamePrefix: 'jig-tog-rem-',
        indicator: 'small-arrow',
        initOpen: false,
        isIcon: true,
        liveRegions: true,
        openedAppendText: '',
        remember: false,
        speed: 50
    };


})(jQuery);


/*
ui.widgetName: ncbitree_base

@crafted by     Jody Clements, clement2@ncbi.nlm.nih.gov

shamelesly based on the tree code writen by:
@author         Scott Jehl, scott@filamentgroup.com
@author-website http://www.filamentgroup.com/
@license        MIT License - filamentgroup.com/examples/mit-license.txt

TODO:
  add ui-widget css
*/

(function($) {
  $.widget("ui.ncbitree_base", {
    options: {
      expanded: '',
      speed: 120,
      remember: false
    },

    _create: function() {
      this.list = this.element.eq(0);
      if ( this.list[0].nodeName == 'UL' ) {
        this._treeify();
      }
    },

    _treeify: function () {
      // set up classes and aria attributes
      var list = this.list;
      var self = this;
      var opts = this.options;
      list.attr({'role': 'tree'}).addClass('ui-widget');
      //set first node's tabindex to 0
      list.find('a:eq(0)').attr('tabindex','0');
      //set all others to -1
      list.find('a:gt(0)').attr('tabindex','-1');

      list.find('ul').attr({'role': 'group'}).addClass('tree-group-collapsed');
      list.find('li').attr({'role': 'treeitem'});

      //find tree group parents
      list.find('li:has(ul)')
          .attr('aria-expanded', 'false').find('>a')

      // wrap parent link in a span and add toggle link
      list.find('li:has(ul)').find('>a')
          .wrap('<span class="tree-parent"/>');
          //.before('<a class="tree-parent" href="#"></a>');

      //bind the custom events
      list
      //expand a tree node
      .bind('expand.treeEvents',function(event){
        self._expand(event.target);
        self._trigger('expand', event);
      })
      //collapse a tree node
      .bind('collapse.treeEvents',function(event){
        self._collapse(event.target);
        self._trigger('collapse', event);
      })
      .bind('toggle.treeEvents',function(event){
        self._toggle(event.target);
      })
      //shift focus down one item
      .bind('traverseDown.treeEvents',function(event){
        var target = list.find('span[tabindex=0]');
        if (!target.length) {
          target = list.find('a[tabindex=0]');
        }
        if (!target.length) {
          target = $(event.target);
        }
        var targetLi = target.parent();
        if(targetLi.is('[aria-expanded=true]')){
          target.next().find('a, span').eq(0).focus();
        }
        else if(targetLi.next().length) {
          targetLi.next().find('a, span').eq(0).focus();
        }
        else {
          targetLi.parents('li').next().find('a, span').eq(0).focus();
        }
      })
      //shift focus up one item
      .bind('traverseUp.treeEvents',function(event){
        var target = list.find('span[tabindex=0]');
        if (!target.length) {
          target = list.find('a[tabindex=0]');
        }
        if (!target.length) {
          target = $(event.target);
        }
        var targetLi = target.parent();
        if(targetLi.prev().length){
          if( targetLi.prev().is('[aria-expanded=true]') ){
            targetLi.prev().find('li:visible:last a, li:visible:last span').eq(0).focus();
          }
          else{
            targetLi.prev().find('a, span').eq(0).focus();
          }
        }
        else {
          targetLi.parents('li:eq(0)').find('a, span').eq(0).focus();
        }
      });

      //bind native events
      list
        .bind('focus.treeEvents', function(event){
          //deactivate previously active tree node, if one exists
          list.find('[tabindex=0]').attr('tabindex','-1').removeClass('ui-state-focus');
          //assign 0 tabindex to focused item
          $(event.target).attr('tabindex','0').addClass('ui-state-focus');
        })
        .bind('click.treeEvents', function(event){
          //save reference to event target
          var target = $(event.target);
          //check if target is a tree node
          if( target.is('span.tree-parent') ){
            target.trigger('toggle');
            target.eq(0).focus();
            //return click event false because it's a tree node (folder)
            event.preventDefault();
          }
        })
        .bind('keydown.treeEvents', function(event){
            var target = list.find('span[tabindex=0], a[tabindex=0]');
            //check for arrow keys
            if(event.keyCode == 37 || event.keyCode == 38 || event.keyCode == 39 || event.keyCode == 40){
              //if key is left arrow and list is collapsed
              if(event.keyCode == 37 && target.parent().is('[aria-expanded=true]')){
                target.trigger('collapse');
              }
              //if key is right arrow and list is collapsed
              if(event.keyCode == 39 && target.parent().is('[aria-expanded=false]')){
                target.trigger('expand');
              }
              //if key is up arrow
              if(event.keyCode == 38){
                target.trigger('traverseUp');
              }
              //if key is down arrow
              if(event.keyCode == 40){
                target.trigger('traverseDown');
              }
              //return any of these keycodes false
              return false;
            }
            //check if enter or space was pressed on a tree node
            else if((event.keyCode == 13 || event.keyCode == 32) && target.is('span.tree-parent')){
              target.trigger('toggle');
              //return click event false because it's a tree node (folder)
              return false;
            }
        });



      var id = list.attr('id');
      if (id && $.cookie('ncbitree') && $.parseJSON($.cookie('ncbitree'))[id]) {
        var state = $.parseJSON($.cookie('ncbitree'))[id];
        // load state from cookie and set tree accordingly
        self._load_state(state);
      }
      else {
        //expanded at load
        list.find(opts.expanded).find('>span').each(function() {
          self._expand(this);
        });
      }


      list.find('li[role=treeitem] > a, li[role=treeitem] > span')
        .addClass('ui-state-default')
        .bind('mouseenter.treeEvents', function() {
          $(this).addClass('ui-state-hover');
        })
        .bind('mouseleave.treeEvents', function() {
          $(this).removeClass('ui-state-hover');
        });


    },

    _expand: function (target) {
      var opts = this.options;
      var self = this;
      var list = this.list;
      var t = (target) ? $(target) : list.find('a[tabindex=0]');
      t.addClass('ui-state-active').removeClass('ui-state-default');
      t.next().hide().removeClass('tree-group-collapsed').slideDown(opts.speed, function(){
        $(this).removeAttr('style');
        t.parent().attr('aria-expanded', 'true').addClass('tree-expanded');
        if (opts.remember) {
          self._save_state();
        }
      });
    },

    _collapse: function(target) {
      var opts = this.options;
      var self = this;
      var list = this.list;
      var t = (target) ? $(target) : list.find('a[tabindex=0]');
      t.removeClass('ui-state-active').addClass('ui-state-default');
      t.next().slideUp(opts.speed, function(){
        t.parent().attr('aria-expanded', 'false').removeClass('tree-expanded');
        $(this).addClass('tree-group-collapsed').removeAttr('style');
        if (opts.remember) {
          self._save_state();
        }
      });
    },

    _toggle: function(target) {
      var list = this.list;
      var t = (target) ? $(target) : list.find('a[tabindex=0]');
      //check if target parent LI is collapsed
      if( t.parent().is('[aria-expanded=false]') ){
        //call expand function on the target
        t.trigger('expand');
      }
      //otherwise, parent must be expanded
      else{
        //collapse the target
        t.trigger('collapse');
      }
    },

    expand: function(selector, recursive) {
      var list = this.list;
      if (recursive) {
        ancestor_spans = list.find(selector).parentsUntil('[role=tree]').find('>span');
        ancestor_spans.each(function () {
          $(this).trigger('expand');
        });
      }
      list.find(selector).find('>span').trigger('expand');
      this._trigger('expanded', null);
    },

    collapse: function (selector) {
      var list = this.list;
      list.find(selector).find('>span').trigger('collapse');
    },

    destroy: function() {
      $.Widget.prototype.destroy.apply(this, arguments); //default
      var list = this.list;
      //remove attributes
      list.removeAttr('role').removeClass('ui-widget');
      list.find('a').removeAttr('tabindex')
        .removeClass('ui-state-focus ui-state-default ui-state-hover ui-state-active');
      list.find('ul').removeAttr('role').removeClass('tree-group-collapsed');
      list.find('li').removeAttr('role').removeClass('tree-expanded');
      list.find('li:has(ul)')
          .removeAttr('aria-expanded').find('>a')
          .removeClass('tree-parent');
      list.find('span[class*="tree-parent"]>a').unwrap();
      list.find(this.options.expanded).removeAttr('aria-expanded');

      //unbind events
      list.unbind('.treeEvents');
    },

    _load_state: function(state_data) {
      var self = this;
      var list = this.list;
      state = state_data.split('');
      list.find('li[aria-expanded]').each(function(i){
        if (state[i] == 1) {
          $(this).find('>span').each(function () {
            self._expand(this);
          });
        }
        else {
          $(this).find('>span').each(function() {
            self._collapse(this);
          });
        }
      });
    },

    _save_state: function() {
      var list = this.list;
      var id = list.attr('id');
      if (id) {
        state_data = {};
        if ($.cookie('ncbitree')) {
          state_data = $.parseJSON($.cookie('ncbitree'));
        }
        var state = '';
        list.find('li[aria-expanded]').each(function(){
          state += ($(this).attr('aria-expanded') == 'true') ? '1' : '0';
        });
        state_data[id] = state;
        data = JSON.stringify(state_data);
        $.cookie('ncbitree', data, {path: '/'});
      }
    }

  }); // end widget def

  $.ui.ncbitree_base.prototype.version = '0.2';

})(jQuery);
/*
ui.widgetName: ncbitree

@crafted by     Jody Clements, clement2@ncbi.nlm.nih.gov

*/

(function($) {
  $.widget("ui.ncbitree", $.ui.ncbitree_base, {
    options: {
      expanded: ''
    },

    _create: function() {
      $.ui.ncbitree_base.prototype._create.call(this);
      this.list.addClass('jig-tree');
    },

    destroy: function () {
      // teardown in reverse order to the create
      this.list.removeClass('jig-tree');
      $.ui.ncbitree_base.prototype.destroy.call(this);
    }

  }); // end widget def

  $.ui.ncbitree.prototype.version = '0.1';
})(jQuery);

//for(var i=0;i<10;i++){
//	console.warn("I am editing this so it will cause issues!");
//}
(function(){

	//Copy the ncbipopper code so we can call these after we override them with additional features
    var orgCreate = jQuery.ui.ncbipopper.prototype._create;
    var orgOpen = jQuery.ui.ncbipopper.prototype.open;
    var orgClose = jQuery.ui.ncbipopper.prototype.close;

	//We are extending ncbipopper
	jQuery.widget("ui.ncbilinksmenu", jQuery.ui.ncbipopper, 
		{
			_create : function(){
				this._checkHrefForWebService();
				this._setPopperOptions();
				orgCreate.apply( this, arguments);
				this._handleLinksMenu();				
			},
			
			//Check to see if we are using href for a webservice so it works for disabled JavaScript
			_checkHrefForWebService: function(){
				if(! this.options.webservice && !this.options.destSelector && !this.options.localJSON){
					this.options.webservice = this.element.attr("href");
				}
			},
			
			//Set up the options for the base popper
			_setPopperOptions : function(){
				var opts = this.options;
				opts.openEvent = "click";
				opts.closeEvent = "xmouseout";
				if(opts.loadingMessage){
					opts.showLoadingMessage = true;
				}
				if(!opts.width){
					opts.width = "200px";
				}
				opts.cssClass = "ui-ncbilinksmenu";
				opts.ignoreSettingOutterWidth = true;
				opts.excludeBasicCssStyles = true;
				
				//If we are using a webservice/JSON, we need to have a placeholder with the loading message in place. 
				if(this.options.webservice || this.options.localJSON){ 
					this.options.destText = "<ul class='linksmenu-ul'><li class='ui-ncbilinksmenu-loadingMessage'><span>" + this.options.loadingText + "</span></li></ul>";
				}
			},
			
			_handleLinksMenu : function(){
	
				var popper = this.getDestElement();
				popper.find("li").attr("role","presentation");
	
				var links = this.getDestElement().find("a");

				var that = this;
				links.attr( { "role":"menuitem", "tabindex" : "-1" } ).click(
					function( evt ){
						that._pingDetails( evt, this, "linksmenuclick" );
						that.close();
					}
				);
				
			},
			
			_setTriggerId : function(){
				var id = this.element.attr("id");
				if( id ){
					this.getDestElement().data("triggerId", "#" + id).attr("data-jigtriggerid", "#" + id);
				}
			},
			
			_pingDetails : function( evt, elem, action ){
				//log data to the server
				//TODO: See if we can get more data into the logs at some point.
				if( typeof ncbi !== "undefined" && ncbi.sg && ncbi.sg.ping){		
					var pingData = {
					};
					ncbi.sg.ping( elem, evt.originalEvent, action, pingData );
				}	
			},
			
			close : function(){
				//Remove listeners for keyboard navigation
				this._destroyKeyListeners();				
				orgClose.apply( this, arguments);
			},
			
			open : function(){ 
				
				//Add listeners for keyboard navigation
				this._setUpKeyListeners();
				
				//Set up an id that links to what opened up menu [used mainly for testing framework]
				this._setTriggerId();
				
				//Call the orginal popup open method so the popper shows up
				orgOpen.apply( this, arguments);
				
				//See if we need to call a webservice and make sure we have not already done it.
				if(!this._contentFetched || !this.options.isDestTextCacheable){	
					var webservice = this.options.webservice;
					var localJSON = this.options.localJSON;
					
					if(webservice){
						//See if the item is in the global cache, if it is use it!
						var cachedItem = this._getCachedItem();
						if(	cachedItem ){ 
							this._handleResponse(cachedItem.data, cachedItem.contentType, true);
						}
						else{
							//Make the call to the webservice.
							var that = this;						
							if(this.options.isJSONP){ //some reason the normal ajax() call is not setting callback for jsonp so had to do it this way.
								var ampOrQm = webservice.indexOf("?")>0?"&":"?";
								webservice+= ampOrQm + "callback=?"
								jQuery.getJSON( webservice, {}, function(x){ that._loadContentJSON(x); });
							}
							else{ 
								jQuery.ajax({
									  url : webservice,
									  success : function(data,status,xhr){ that._handleResponse(data, xhr.getResponseHeader( "content-type" )); },
									  error : function(e){ 
										var err = "<ul class='linksmenu-ul'><li class='ui-ncbilinksmenu-errorMessage'><span>Error Loading Data</span></li></ul>";
										that.getDestElement().find("ul.linksmenu-ul").replaceWith( err );
										window.setTimeout( function(){ that.close(); }, 4000); 
									}
								});
							}
						}
					}
					else if(localJSON){
						this._loadContentJSON( localJSON );
					}
					
				}
				
			},
			
			_handleResponse : function(data, contentType, wasCached){
				
				//Store the data to a global cache [all links menus on the page share their info so we do not have to call the server more than once for each page load
				if(!wasCached){
					this._cachedCalls[this.options.webservice] = { "data" : data, "contentType" : contentType };
				}
				
				//Determine what the content type is so we can load the data correctly into the menu
				if(contentType=="application/json"){
					this._loadContentJSON( data );
				}
				else{
					this._loadContent( data );
				}
			},
			
			_cachedResponses : {},
			
			_loadContentJSON : function( data ){

				var that = this;
				var menuUl = jQuery("<ul></ul>").addClass("linksmenu-ul").attr("role","menu");
				
				//Creates the links based on the data in the JSON object.
				//Note that click event stores a string for the script so we need to eval it :( - Thanks JSON for not supporting funcitons. 
				function buildLink( menuItem, attachTo){
						var text = menuItem.text;
						var href = menuItem.href || "#";
						var target = menuItem.target || "";
						var click = menuItem.click || "";
						var rel = menuItem.rel || "";
						if(target){
							target = ' target="' + target + '"';
						}					
						if(rel){
							rel = ' rel="' + rel + '"';
						}					
						var li = jQuery("<li></li>");
						var link = jQuery("<a href='" + href + "'" + target + rel +">" + text + "</a>");
						if(click){
							try{
								var clickFnc = new Function(" return " + click + "()");
								link.click( function(e){ clickFnc.call(this,e,that); } );
							}
							catch(e){
								if(typeof console!=="undefined" && console.error){
									console.error( e );
								}
							}
						}
						li.append(link);
						attachTo.append( li ); 								
				}
				
				//Loop through the link list in a recursive manner.
				function buildMenu( linksList, parentNode){
					for(var i=0;i<linksList.length;i++){					
						var currentNode = linksList[i];
						
						//Check if we have a heading or a link
						//If a heading we creatiung the heading and loop through its links in a recursive manner. 
						var heading = currentNode.heading;
						if(heading){
							var li = jQuery("<li></li>").attr("role","heading");
							var span = jQuery("<span>" + heading + "</span>");
							li.append(span);
							var newLinks = currentNode.links;
							if(newLinks){
								var newUL = jQuery("<ul></ul>");							
								buildMenu(newLinks,newUL);
								li.append(newUL);
							}
							parentNode.append(li);
						}
						else{
							buildLink(currentNode,parentNode);
						}					
					}
				}
				
				
				var menuLinks = data.links;
				if(menuLinks){
					buildMenu(menuLinks,menuUl);
				}
				
				this._contentFetched = true;

				//Replace the menuy
				var popper = this.getDestElement();
				popper.find("ul.linksmenu-ul").replaceWith(menuUl);				
				
				//Add event handlers to the links
				this._handleLinksMenu();
				
				this.element.trigger("ncbilinksmenujsonmenuloaded");
				
			},
			
			_loadContent : function( data ){
			
				var newCode = data;

				//Mark's request of being able to return a valid html page so if js diabled the menu would still work
				//That is if the menu items do not require JS in the first place. 
				if( data.match(/(<html)/i) ){ 
					var newHTML = jQuery("<div></div>").html(data);
					var newMenu = newHTML.find(".linksmenu-ul");
					if(newMenu.length>1){
						newMenu = newMenu.filter(this.options.webserviceMenuSelector );
					}
					newCode = newMenu.clone();
					if(!newCode){
						console.error("no element found")
						newCode = "<ul class='linksmenu-ul'><li class='ui-ncbilinksmenu-errorMessage'><span>Error Loading Data</span></li></ul>";
					}
				}
			
				this._contentFetched = true;
				this.getDestElement().find("ul.linksmenu-ul").replaceWith( newCode ).addClass("linksmenu-ul");
				this._handleLinksMenu();
			},
			
			//Keyboard accessibility for menu options. Use arrow key to move through menu and enter to select it.
			_setUpKeyListeners : function(){
				if(this._popupkeylistener) return;

				var that = this;
				this.selectedOption = -1;
				this._popupkeylistener = function(e){
					var isCancel = that._moveMenuOption( e.keyCode );
					if(isCancel){
						e.preventDefault();
					}
				}
				jQuery(window).keypress(this._popupkeylistener);
				
			},
			
			_destroyKeyListeners : function(){
				if(this._popupkeylistener){
					jQuery(window).unbind("keypress", this._popupkeylistener);
					this.selectedOption=null;
				}
			},
			
			_keys : { up : 38, down : 40, enter : 13 },
			_moveMenuOption : function( keyCode ){
				
				var k = this._keys;
				switch(keyCode){
					case k.up:
						this._moveSelection(-1);
						cancelAction = true;			
						break;
					case k.down:
						this._moveSelection(1);
						cancelAction = true;
						break;										
					case k.enter:
						cancelAction = false;
						break;
					default:
						cancelAction = false;					
				}
				
				return cancelAction;
			
			},
			
			//Call by keypress - loops through elements setting focus to them. 
			_moveSelection : function( direction ){
				if(this.selectedOption===undefined || this.selectedOption===null){
					this.selectedOption = -1;
				}
				
				this.selectedOption += direction;
				
				var lis = this.getDestElement().find("a");
				var cnt = lis.length;
				if(this.selectedOption<0){
					this.selectedOption = cnt-1;
				}
				else if(this.selectedOption >= cnt){
					this.selectedOption = 0;
				}
				
				lis.eq( this.selectedOption ).focus();
			
			},
			
			_cachedCalls : {},			
			_getCachedItem : function( ){
				//Make sure we are allowed to fetch from cache for this widget
				if(!this.options.isDestTextCacheable){
					return null;
				}
				var ws = this.options.webservice;
				return this._cachedCalls[ ws ];
			}
			
		}
	);
})();
/*
 * jQuery UI Datepicker 1.8
 *
 * Copyright (c) 2010 AUTHORS.txt (http://jqueryui.com/about)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * http://docs.jquery.com/UI/Datepicker
 *
 * Depends:
 *	jquery.ui.core.js
 */

(function($) { // hide the namespace

$.extend($.ui, { datepicker: { version: "1.8" } });

var PROP_NAME = 'datepicker';
var dpuuid = new Date().getTime();

/* Date picker manager.
   Use the singleton instance of this class, $.datepicker, to interact with the date picker.
   Settings for (groups of) date pickers are maintained in an instance object,
   allowing multiple different settings on the same page. */

function Datepicker() {
	this.debug = false; // Change this to true to start debugging
	this._curInst = null; // The current instance in use
	this._keyEvent = false; // If the last event was a key event
	this._disabledInputs = []; // List of date picker inputs that have been disabled
	this._datepickerShowing = false; // True if the popup picker is showing , false if not
	this._inDialog = false; // True if showing within a "dialog", false if not
	this._mainDivId = 'ui-datepicker-div'; // The ID of the main datepicker division
	this._inlineClass = 'ui-datepicker-inline'; // The name of the inline marker class
	this._appendClass = 'ui-datepicker-append'; // The name of the append marker class
	this._triggerClass = 'ui-datepicker-trigger'; // The name of the trigger marker class
	this._dialogClass = 'ui-datepicker-dialog'; // The name of the dialog marker class
	this._disableClass = 'ui-datepicker-disabled'; // The name of the disabled covering marker class
	this._unselectableClass = 'ui-datepicker-unselectable'; // The name of the unselectable cell marker class
	this._currentClass = 'ui-datepicker-current-day'; // The name of the current day marker class
	this._dayOverClass = 'ui-datepicker-days-cell-over'; // The name of the day hover marker class
	this.regional = []; // Available regional settings, indexed by language code
	this.regional[''] = { // Default regional settings
		closeText: 'Done', // Display text for close link
		prevText: 'Prev', // Display text for previous month link
		nextText: 'Next', // Display text for next month link
		currentText: 'Today', // Display text for current month link
		monthNames: ['January','February','March','April','May','June',
			'July','August','September','October','November','December'], // Names of months for drop-down and formatting
		monthNamesShort: ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'], // For formatting
		dayNames: ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'], // For formatting
		dayNamesShort: ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'], // For formatting
		dayNamesMin: ['Su','Mo','Tu','We','Th','Fr','Sa'], // Column headings for days starting at Sunday
		weekHeader: 'Wk', // Column header for week of the year
		dateFormat: 'mm/dd/yy', // See format options on parseDate
		firstDay: 0, // The first day of the week, Sun = 0, Mon = 1, ...
		isRTL: false, // True if right-to-left language, false if left-to-right
		showMonthAfterYear: false, // True if the year select precedes month, false for month then year
		yearSuffix: '' // Additional text to append to the year in the month headers
	};
	this._defaults = { // Global defaults for all the date picker instances
		showOn: 'focus', // 'focus' for popup on focus,
			// 'button' for trigger button, or 'both' for either
		showAnim: 'show', // Name of jQuery animation for popup
		showOptions: {}, // Options for enhanced animations
		defaultDate: null, // Used when field is blank: actual date,
			// +/-number for offset from today, null for today
		appendText: '', // Display text following the input box, e.g. showing the format
		buttonText: '...', // Text for trigger button
		buttonImage: '', // URL for trigger button image
		buttonImageOnly: false, // True if the image appears alone, false if it appears on a button
		hideIfNoPrevNext: false, // True to hide next/previous month links
			// if not applicable, false to just disable them
		navigationAsDateFormat: false, // True if date formatting applied to prev/today/next links
		gotoCurrent: false, // True if today link goes back to current selection instead
		changeMonth: false, // True if month can be selected directly, false if only prev/next
		changeYear: false, // True if year can be selected directly, false if only prev/next
		yearRange: 'c-10:c+10', // Range of years to display in drop-down,
			// either relative to today's year (-nn:+nn), relative to currently displayed year
			// (c-nn:c+nn), absolute (nnnn:nnnn), or a combination of the above (nnnn:-n)
		showOtherMonths: false, // True to show dates in other months, false to leave blank
		selectOtherMonths: false, // True to allow selection of dates in other months, false for unselectable
		showWeek: false, // True to show week of the year, false to not show it
		calculateWeek: this.iso8601Week, // How to calculate the week of the year,
			// takes a Date and returns the number of the week for it
		shortYearCutoff: '+10', // Short year values < this are in the current century,
			// > this are in the previous century,
			// string value starting with '+' for current year + value
		minDate: null, // The earliest selectable date, or null for no limit
		maxDate: null, // The latest selectable date, or null for no limit
		duration: '_default', // Duration of display/closure
		beforeShowDay: null, // Function that takes a date and returns an array with
			// [0] = true if selectable, false if not, [1] = custom CSS class name(s) or '',
			// [2] = cell title (optional), e.g. $.datepicker.noWeekends
		beforeShow: null, // Function that takes an input field and
			// returns a set of custom settings for the date picker
		onSelect: null, // Define a callback function when a date is selected
		onChangeMonthYear: null, // Define a callback function when the month or year is changed
		onClose: null, // Define a callback function when the datepicker is closed
		numberOfMonths: 1, // Number of months to show at a time
		showCurrentAtPos: 0, // The position in multipe months at which to show the current month (starting at 0)
		stepMonths: 1, // Number of months to step back/forward
		stepBigMonths: 12, // Number of months to step back/forward for the big links
		altField: '', // Selector for an alternate field to store selected dates into
		altFormat: '', // The date format to use for the alternate field
		constrainInput: true, // The input is constrained by the current date format
		showButtonPanel: false, // True to show button panel, false to not show it
		autoSize: false // True to size the input for the date format, false to leave as is
	};
	$.extend(this._defaults, this.regional['']);
	this.dpDiv = $('<div id="' + this._mainDivId + '" class="ui-datepicker ui-widget ui-widget-content ui-helper-clearfix ui-corner-all ui-helper-hidden-accessible"></div>');
}

$.extend(Datepicker.prototype, {
	/* Class name added to elements to indicate already configured with a date picker. */
	markerClassName: 'hasDatepicker',

	/* Debug logging (if enabled). */
	log: function () {
		if (this.debug)
			console.log.apply('', arguments);
	},
	
	// TODO rename to "widget" when switching to widget factory
	_widgetDatepicker: function() {
		return this.dpDiv;
	},

	/* Override the default settings for all instances of the date picker.
	   @param  settings  object - the new settings to use as defaults (anonymous object)
	   @return the manager object */
	setDefaults: function(settings) {
		extendRemove(this._defaults, settings || {});
		return this;
	},

	/* Attach the date picker to a jQuery selection.
	   @param  target    element - the target input field or division or span
	   @param  settings  object - the new settings to use for this date picker instance (anonymous) */
	_attachDatepicker: function(target, settings) {
		// check for settings on the control itself - in namespace 'date:'
		var inlineSettings = null;
		for (var attrName in this._defaults) {
			var attrValue = target.getAttribute('date:' + attrName);
			if (attrValue) {
				inlineSettings = inlineSettings || {};
				try {
					inlineSettings[attrName] = eval(attrValue);
				} catch (err) {
					inlineSettings[attrName] = attrValue;
				}
			}
		}
		var nodeName = target.nodeName.toLowerCase();
		var inline = (nodeName == 'div' || nodeName == 'span');
		if (!target.id)
			target.id = 'dp' + (++this.uuid);
		var inst = this._newInst($(target), inline);
		inst.settings = $.extend({}, settings || {}, inlineSettings || {});
		if (nodeName == 'input') {
			this._connectDatepicker(target, inst);
		} else if (inline) {
			this._inlineDatepicker(target, inst);
		}
	},

	/* Create a new instance object. */
	_newInst: function(target, inline) {
		var id = target[0].id.replace(/([^A-Za-z0-9_])/g, '\\\\$1'); // escape jQuery meta chars
		return {id: id, input: target, // associated target
			selectedDay: 0, selectedMonth: 0, selectedYear: 0, // current selection
			drawMonth: 0, drawYear: 0, // month being drawn
			inline: inline, // is datepicker inline or not
			dpDiv: (!inline ? this.dpDiv : // presentation div
			$('<div class="' + this._inlineClass + ' ui-datepicker ui-widget ui-widget-content ui-helper-clearfix ui-corner-all"></div>'))};
	},

	/* Attach the date picker to an input field. */
	_connectDatepicker: function(target, inst) {
		var input = $(target);
		inst.append = $([]);
		inst.trigger = $([]);
		if (input.hasClass(this.markerClassName))
			return;
		this._attachments(input, inst);
		input.addClass(this.markerClassName).keydown(this._doKeyDown).
			keypress(this._doKeyPress).keyup(this._doKeyUp).
			bind("setData.datepicker", function(event, key, value) {
				inst.settings[key] = value;
			}).bind("getData.datepicker", function(event, key) {
				return this._get(inst, key);
			});
		this._autoSize(inst);
		$.data(target, PROP_NAME, inst);
	},

	/* Make attachments based on settings. */
	_attachments: function(input, inst) {
		var appendText = this._get(inst, 'appendText');
		var isRTL = this._get(inst, 'isRTL');
		if (inst.append)
			inst.append.remove();
		if (appendText) {
			inst.append = $('<span class="' + this._appendClass + '">' + appendText + '</span>');
			input[isRTL ? 'before' : 'after'](inst.append);
		}
		input.unbind('focus', this._showDatepicker);
		if (inst.trigger)
			inst.trigger.remove();
		var showOn = this._get(inst, 'showOn');
		if (showOn == 'focus' || showOn == 'both') // pop-up date picker when in the marked field
			input.focus(this._showDatepicker);
		if (showOn == 'button' || showOn == 'both') { // pop-up date picker when button clicked
			var buttonText = this._get(inst, 'buttonText');
			var buttonImage = this._get(inst, 'buttonImage');
			inst.trigger = $(this._get(inst, 'buttonImageOnly') ?
				$('<img/>').addClass(this._triggerClass).
					attr({ src: buttonImage, alt: buttonText, title: buttonText }) :
				$('<button type="button"></button>').addClass(this._triggerClass).
					html(buttonImage == '' ? buttonText : $('<img/>').attr(
					{ src:buttonImage, alt:buttonText, title:buttonText })));
			input[isRTL ? 'before' : 'after'](inst.trigger);
			inst.trigger.click(function() {
				if ($.datepicker._datepickerShowing && $.datepicker._lastInput == input[0])
					$.datepicker._hideDatepicker();
				else
					$.datepicker._showDatepicker(input[0]);
				return false;
			});
		}
	},

	/* Apply the maximum length for the date format. */
	_autoSize: function(inst) {
		if (this._get(inst, 'autoSize') && !inst.inline) {
			var date = new Date(2009, 12 - 1, 20); // Ensure double digits
			var dateFormat = this._get(inst, 'dateFormat');
			if (dateFormat.match(/[DM]/)) {
				var findMax = function(names) {
					var max = 0;
					var maxI = 0;
					for (var i = 0; i < names.length; i++) {
						if (names[i].length > max) {
							max = names[i].length;
							maxI = i;
						}
					}
					return maxI;
				};
				date.setMonth(findMax(this._get(inst, (dateFormat.match(/MM/) ?
					'monthNames' : 'monthNamesShort'))));
				date.setDate(findMax(this._get(inst, (dateFormat.match(/DD/) ?
					'dayNames' : 'dayNamesShort'))) + 20 - date.getDay());
			}
			inst.input.attr('size', this._formatDate(inst, date).length);
		}
	},

	/* Attach an inline date picker to a div. */
	_inlineDatepicker: function(target, inst) {
		var divSpan = $(target);
		if (divSpan.hasClass(this.markerClassName))
			return;
		divSpan.addClass(this.markerClassName).append(inst.dpDiv).
			bind("setData.datepicker", function(event, key, value){
				inst.settings[key] = value;
			}).bind("getData.datepicker", function(event, key){
				return this._get(inst, key);
			});
		$.data(target, PROP_NAME, inst);
		this._setDate(inst, this._getDefaultDate(inst), true);
		this._updateDatepicker(inst);
		this._updateAlternate(inst);
	},

	/* Pop-up the date picker in a "dialog" box.
	   @param  input     element - ignored
	   @param  date      string or Date - the initial date to display
	   @param  onSelect  function - the function to call when a date is selected
	   @param  settings  object - update the dialog date picker instance's settings (anonymous object)
	   @param  pos       int[2] - coordinates for the dialog's position within the screen or
	                     event - with x/y coordinates or
	                     leave empty for default (screen centre)
	   @return the manager object */
	_dialogDatepicker: function(input, date, onSelect, settings, pos) {
		var inst = this._dialogInst; // internal instance
		if (!inst) {
			var id = 'dp' + (++this.uuid);
			this._dialogInput = $('<input type="text" id="' + id +
				'" style="position: absolute; top: -100px; width: 0px; z-index: -10;"/>');
			this._dialogInput.keydown(this._doKeyDown);
			$('body').append(this._dialogInput);
			inst = this._dialogInst = this._newInst(this._dialogInput, false);
			inst.settings = {};
			$.data(this._dialogInput[0], PROP_NAME, inst);
		}
		extendRemove(inst.settings, settings || {});
		date = (date && date.constructor == Date ? this._formatDate(inst, date) : date);
		this._dialogInput.val(date);

		this._pos = (pos ? (pos.length ? pos : [pos.pageX, pos.pageY]) : null);
		if (!this._pos) {
			var browserWidth = document.documentElement.clientWidth;
			var browserHeight = document.documentElement.clientHeight;
			var scrollX = document.documentElement.scrollLeft || document.body.scrollLeft;
			var scrollY = document.documentElement.scrollTop || document.body.scrollTop;
			this._pos = // should use actual width/height below
				[(browserWidth / 2) - 100 + scrollX, (browserHeight / 2) - 150 + scrollY];
		}

		// move input on screen for focus, but hidden behind dialog
		this._dialogInput.css('left', (this._pos[0] + 20) + 'px').css('top', this._pos[1] + 'px');
		inst.settings.onSelect = onSelect;
		this._inDialog = true;
		this.dpDiv.addClass(this._dialogClass);
		this._showDatepicker(this._dialogInput[0]);
		if ($.blockUI)
			$.blockUI(this.dpDiv);
		$.data(this._dialogInput[0], PROP_NAME, inst);
		return this;
	},

	/* Detach a datepicker from its control.
	   @param  target    element - the target input field or division or span */
	_destroyDatepicker: function(target) {
		var $target = $(target);
		var inst = $.data(target, PROP_NAME);
		if (!$target.hasClass(this.markerClassName)) {
			return;
		}
		var nodeName = target.nodeName.toLowerCase();
		$.removeData(target, PROP_NAME);
		if (nodeName == 'input') {
			inst.append.remove();
			inst.trigger.remove();
			$target.removeClass(this.markerClassName).
				unbind('focus', this._showDatepicker).
				unbind('keydown', this._doKeyDown).
				unbind('keypress', this._doKeyPress).
				unbind('keyup', this._doKeyUp);
		} else if (nodeName == 'div' || nodeName == 'span')
			$target.removeClass(this.markerClassName).empty();
	},

	/* Enable the date picker to a jQuery selection.
	   @param  target    element - the target input field or division or span */
	_enableDatepicker: function(target) {
		var $target = $(target);
		var inst = $.data(target, PROP_NAME);
		if (!$target.hasClass(this.markerClassName)) {
			return;
		}
		var nodeName = target.nodeName.toLowerCase();
		if (nodeName == 'input') {
			target.disabled = false;
			inst.trigger.filter('button').
				each(function() { this.disabled = false; }).end().
				filter('img').css({opacity: '1.0', cursor: ''});
		}
		else if (nodeName == 'div' || nodeName == 'span') {
			var inline = $target.children('.' + this._inlineClass);
			inline.children().removeClass('ui-state-disabled');
		}
		this._disabledInputs = $.map(this._disabledInputs,
			function(value) { return (value == target ? null : value); }); // delete entry
	},

	/* Disable the date picker to a jQuery selection.
	   @param  target    element - the target input field or division or span */
	_disableDatepicker: function(target) {
		var $target = $(target);
		var inst = $.data(target, PROP_NAME);
		if (!$target.hasClass(this.markerClassName)) {
			return;
		}
		var nodeName = target.nodeName.toLowerCase();
		if (nodeName == 'input') {
			target.disabled = true;
			inst.trigger.filter('button').
				each(function() { this.disabled = true; }).end().
				filter('img').css({opacity: '0.5', cursor: 'default'});
		}
		else if (nodeName == 'div' || nodeName == 'span') {
			var inline = $target.children('.' + this._inlineClass);
			inline.children().addClass('ui-state-disabled');
		}
		this._disabledInputs = $.map(this._disabledInputs,
			function(value) { return (value == target ? null : value); }); // delete entry
		this._disabledInputs[this._disabledInputs.length] = target;
	},

	/* Is the first field in a jQuery collection disabled as a datepicker?
	   @param  target    element - the target input field or division or span
	   @return boolean - true if disabled, false if enabled */
	_isDisabledDatepicker: function(target) {
		if (!target) {
			return false;
		}
		for (var i = 0; i < this._disabledInputs.length; i++) {
			if (this._disabledInputs[i] == target)
				return true;
		}
		return false;
	},

	/* Retrieve the instance data for the target control.
	   @param  target  element - the target input field or division or span
	   @return  object - the associated instance data
	   @throws  error if a jQuery problem getting data */
	_getInst: function(target) {
		try {
			return $.data(target, PROP_NAME);
		}
		catch (err) {
			throw 'Missing instance data for this datepicker';
		}
	},

	/* Update or retrieve the settings for a date picker attached to an input field or division.
	   @param  target  element - the target input field or division or span
	   @param  name    object - the new settings to update or
	                   string - the name of the setting to change or retrieve,
	                   when retrieving also 'all' for all instance settings or
	                   'defaults' for all global defaults
	   @param  value   any - the new value for the setting
	                   (omit if above is an object or to retrieve a value) */
	_optionDatepicker: function(target, name, value) {
		var inst = this._getInst(target);
		if (arguments.length == 2 && typeof name == 'string') {
			return (name == 'defaults' ? $.extend({}, $.datepicker._defaults) :
				(inst ? (name == 'all' ? $.extend({}, inst.settings) :
				this._get(inst, name)) : null));
		}
		var settings = name || {};
		if (typeof name == 'string') {
			settings = {};
			settings[name] = value;
		}
		if (inst) {
			if (this._curInst == inst) {
				this._hideDatepicker();
			}
			var date = this._getDateDatepicker(target, true);
			extendRemove(inst.settings, settings);
			this._attachments($(target), inst);
			this._autoSize(inst);
			this._setDateDatepicker(target, date);
			this._updateDatepicker(inst);
		}
	},

	// change method deprecated
	_changeDatepicker: function(target, name, value) {
		this._optionDatepicker(target, name, value);
	},

	/* Redraw the date picker attached to an input field or division.
	   @param  target  element - the target input field or division or span */
	_refreshDatepicker: function(target) {
		var inst = this._getInst(target);
		if (inst) {
			this._updateDatepicker(inst);
		}
	},

	/* Set the dates for a jQuery selection.
	   @param  target   element - the target input field or division or span
	   @param  date     Date - the new date */
	_setDateDatepicker: function(target, date) {
		var inst = this._getInst(target);
		if (inst) {
			this._setDate(inst, date);
			this._updateDatepicker(inst);
			this._updateAlternate(inst);
		}
	},

	/* Get the date(s) for the first entry in a jQuery selection.
	   @param  target     element - the target input field or division or span
	   @param  noDefault  boolean - true if no default date is to be used
	   @return Date - the current date */
	_getDateDatepicker: function(target, noDefault) {
		var inst = this._getInst(target);
		if (inst && !inst.inline)
			this._setDateFromField(inst, noDefault);
		return (inst ? this._getDate(inst) : null);
	},

	/* Handle keystrokes. */
	_doKeyDown: function(event) {
		var inst = $.datepicker._getInst(event.target);
		var handled = true;
		var isRTL = inst.dpDiv.is('.ui-datepicker-rtl');
		inst._keyEvent = true;
		if ($.datepicker._datepickerShowing)
			switch (event.keyCode) {
				case 9: $.datepicker._hideDatepicker();
						handled = false;
						break; // hide on tab out
				case 13: var sel = $('td.' + $.datepicker._dayOverClass, inst.dpDiv).
							add($('td.' + $.datepicker._currentClass, inst.dpDiv));
						if (sel[0])
							$.datepicker._selectDay(event.target, inst.selectedMonth, inst.selectedYear, sel[0]);
						else
							$.datepicker._hideDatepicker();
						return false; // don't submit the form
						break; // select the value on enter
				case 27: $.datepicker._hideDatepicker();
						break; // hide on escape
				case 33: $.datepicker._adjustDate(event.target, (event.ctrlKey ?
							-$.datepicker._get(inst, 'stepBigMonths') :
							-$.datepicker._get(inst, 'stepMonths')), 'M');
						break; // previous month/year on page up/+ ctrl
				case 34: $.datepicker._adjustDate(event.target, (event.ctrlKey ?
							+$.datepicker._get(inst, 'stepBigMonths') :
							+$.datepicker._get(inst, 'stepMonths')), 'M');
						break; // next month/year on page down/+ ctrl
				case 35: if (event.ctrlKey || event.metaKey) $.datepicker._clearDate(event.target);
						handled = event.ctrlKey || event.metaKey;
						break; // clear on ctrl or command +end
				case 36: if (event.ctrlKey || event.metaKey) $.datepicker._gotoToday(event.target);
						handled = event.ctrlKey || event.metaKey;
						break; // current on ctrl or command +home
				case 37: if (event.ctrlKey || event.metaKey) $.datepicker._adjustDate(event.target, (isRTL ? +1 : -1), 'D');
						handled = event.ctrlKey || event.metaKey;
						// -1 day on ctrl or command +left
						if (event.originalEvent.altKey) $.datepicker._adjustDate(event.target, (event.ctrlKey ?
									-$.datepicker._get(inst, 'stepBigMonths') :
									-$.datepicker._get(inst, 'stepMonths')), 'M');
						// next month/year on alt +left on Mac
						break;
				case 38: if (event.ctrlKey || event.metaKey) $.datepicker._adjustDate(event.target, -7, 'D');
						handled = event.ctrlKey || event.metaKey;
						break; // -1 week on ctrl or command +up
				case 39: if (event.ctrlKey || event.metaKey) $.datepicker._adjustDate(event.target, (isRTL ? -1 : +1), 'D');
						handled = event.ctrlKey || event.metaKey;
						// +1 day on ctrl or command +right
						if (event.originalEvent.altKey) $.datepicker._adjustDate(event.target, (event.ctrlKey ?
									+$.datepicker._get(inst, 'stepBigMonths') :
									+$.datepicker._get(inst, 'stepMonths')), 'M');
						// next month/year on alt +right
						break;
				case 40: if (event.ctrlKey || event.metaKey) $.datepicker._adjustDate(event.target, +7, 'D');
						handled = event.ctrlKey || event.metaKey;
						break; // +1 week on ctrl or command +down
				default: handled = false;
			}
		else if (event.keyCode == 36 && event.ctrlKey) // display the date picker on ctrl+home
			$.datepicker._showDatepicker(this);
		else {
			handled = false;
		}
		if (handled) {
			event.preventDefault();
			event.stopPropagation();
		}
	},

	/* Filter entered characters - based on date format. */
	_doKeyPress: function(event) {
		var inst = $.datepicker._getInst(event.target);
		if ($.datepicker._get(inst, 'constrainInput')) {
			var chars = $.datepicker._possibleChars($.datepicker._get(inst, 'dateFormat'));
			var chr = String.fromCharCode(event.charCode == undefined ? event.keyCode : event.charCode);
			return event.ctrlKey || (chr < ' ' || !chars || chars.indexOf(chr) > -1);
		}
	},

	/* Synchronise manual entry and field/alternate field. */
	_doKeyUp: function(event) {
		var inst = $.datepicker._getInst(event.target);
		if (inst.input.val() != inst.lastVal) {
			try {
				var date = $.datepicker.parseDate($.datepicker._get(inst, 'dateFormat'),
					(inst.input ? inst.input.val() : null),
					$.datepicker._getFormatConfig(inst));
				if (date) { // only if valid
					$.datepicker._setDateFromField(inst);
					$.datepicker._updateAlternate(inst);
					$.datepicker._updateDatepicker(inst);
				}
			}
			catch (event) {
				$.datepicker.log(event);
			}
		}
		return true;
	},

	/* Pop-up the date picker for a given input field.
	   @param  input  element - the input field attached to the date picker or
	                  event - if triggered by focus */
	_showDatepicker: function(input) {
		input = input.target || input;
		if (input.nodeName.toLowerCase() != 'input') // find from button/image trigger
			input = $('input', input.parentNode)[0];
		if ($.datepicker._isDisabledDatepicker(input) || $.datepicker._lastInput == input) // already here
			return;
		var inst = $.datepicker._getInst(input);
		if ($.datepicker._curInst && $.datepicker._curInst != inst) {
			$.datepicker._curInst.dpDiv.stop(true, true);
		}
		var beforeShow = $.datepicker._get(inst, 'beforeShow');
		extendRemove(inst.settings, (beforeShow ? beforeShow.apply(input, [input, inst]) : {}));
		inst.lastVal = null;
		$.datepicker._lastInput = input;
		$.datepicker._setDateFromField(inst);
		if ($.datepicker._inDialog) // hide cursor
			input.value = '';
		if (!$.datepicker._pos) { // position below input
			$.datepicker._pos = $.datepicker._findPos(input);
			$.datepicker._pos[1] += input.offsetHeight; // add the height
		}
		var isFixed = false;
		$(input).parents().each(function() {
			isFixed |= $(this).css('position') == 'fixed';
			return !isFixed;
		});
		if (isFixed && $.browser.opera) { // correction for Opera when fixed and scrolled
			$.datepicker._pos[0] -= document.documentElement.scrollLeft;
			$.datepicker._pos[1] -= document.documentElement.scrollTop;
		}
		var offset = {left: $.datepicker._pos[0], top: $.datepicker._pos[1]};
		$.datepicker._pos = null;
		// determine sizing offscreen
		inst.dpDiv.css({position: 'absolute', display: 'block', top: '-1000px'});
		$.datepicker._updateDatepicker(inst);
		// fix width for dynamic number of date pickers
		// and adjust position before showing
		offset = $.datepicker._checkOffset(inst, offset, isFixed);
		inst.dpDiv.css({position: ($.datepicker._inDialog && $.blockUI ?
			'static' : (isFixed ? 'fixed' : 'absolute')), display: 'none',
			left: offset.left + 'px', top: offset.top + 'px'});
		if (!inst.inline) {
			var showAnim = $.datepicker._get(inst, 'showAnim');
			var duration = $.datepicker._get(inst, 'duration');
			var postProcess = function() {
				$.datepicker._datepickerShowing = true;
				var borders = $.datepicker._getBorders(inst.dpDiv);
				inst.dpDiv.find('iframe.ui-datepicker-cover'). // IE6- only
					css({left: -borders[0], top: -borders[1],
						width: inst.dpDiv.outerWidth(), height: inst.dpDiv.outerHeight()});
			};
			inst.dpDiv.zIndex($(input).zIndex()+1);
			if ($.effects && $.effects[showAnim])
				inst.dpDiv.show(showAnim, $.datepicker._get(inst, 'showOptions'), duration, postProcess);
			else
				inst.dpDiv[showAnim || 'show']((showAnim ? duration : null), postProcess);
			if (!showAnim || !duration)
				postProcess();
			if (inst.input.is(':visible') && !inst.input.is(':disabled'))
				inst.input.focus();
			$.datepicker._curInst = inst;
		}
	},

	/* Generate the date picker content. */
	_updateDatepicker: function(inst) {
		var self = this;
		var borders = $.datepicker._getBorders(inst.dpDiv);
		inst.dpDiv.empty().append(this._generateHTML(inst))
			.find('iframe.ui-datepicker-cover') // IE6- only
				.css({left: -borders[0], top: -borders[1],
					width: inst.dpDiv.outerWidth(), height: inst.dpDiv.outerHeight()})
			.end()
			.find('button, .ui-datepicker-prev, .ui-datepicker-next, .ui-datepicker-calendar td a')
				.bind('mouseout', function(){
					$(this).removeClass('ui-state-hover');
					if(this.className.indexOf('ui-datepicker-prev') != -1) $(this).removeClass('ui-datepicker-prev-hover');
					if(this.className.indexOf('ui-datepicker-next') != -1) $(this).removeClass('ui-datepicker-next-hover');
				})
				.bind('mouseover', function(){
					if (!self._isDisabledDatepicker( inst.inline ? inst.dpDiv.parent()[0] : inst.input[0])) {
						$(this).parents('.ui-datepicker-calendar').find('a').removeClass('ui-state-hover');
						$(this).addClass('ui-state-hover');
						if(this.className.indexOf('ui-datepicker-prev') != -1) $(this).addClass('ui-datepicker-prev-hover');
						if(this.className.indexOf('ui-datepicker-next') != -1) $(this).addClass('ui-datepicker-next-hover');
					}
				})
			.end()
			.find('.' + this._dayOverClass + ' a')
				.trigger('mouseover')
			.end();
		var numMonths = this._getNumberOfMonths(inst);
		var cols = numMonths[1];
		var width = 17;
		if (cols > 1)
			inst.dpDiv.addClass('ui-datepicker-multi-' + cols).css('width', (width * cols) + 'em');
		else
			inst.dpDiv.removeClass('ui-datepicker-multi-2 ui-datepicker-multi-3 ui-datepicker-multi-4').width('');
		inst.dpDiv[(numMonths[0] != 1 || numMonths[1] != 1 ? 'add' : 'remove') +
			'Class']('ui-datepicker-multi');
		inst.dpDiv[(this._get(inst, 'isRTL') ? 'add' : 'remove') +
			'Class']('ui-datepicker-rtl');
		if (inst == $.datepicker._curInst && $.datepicker._datepickerShowing && inst.input &&
				inst.input.is(':visible') && !inst.input.is(':disabled'))
			inst.input.focus();
	},

	/* Retrieve the size of left and top borders for an element.
	   @param  elem  (jQuery object) the element of interest
	   @return  (number[2]) the left and top borders */
	_getBorders: function(elem) {
		var convert = function(value) {
			return {thin: 1, medium: 2, thick: 3}[value] || value;
		};
		return [parseFloat(convert(elem.css('border-left-width'))),
			parseFloat(convert(elem.css('border-top-width')))];
	},

	/* Check positioning to remain on screen. */
	_checkOffset: function(inst, offset, isFixed) {
		var dpWidth = inst.dpDiv.outerWidth();
		var dpHeight = inst.dpDiv.outerHeight();
		var inputWidth = inst.input ? inst.input.outerWidth() : 0;
		var inputHeight = inst.input ? inst.input.outerHeight() : 0;
		var viewWidth = document.documentElement.clientWidth + $(document).scrollLeft();
		var viewHeight = document.documentElement.clientHeight + $(document).scrollTop();

		offset.left -= (this._get(inst, 'isRTL') ? (dpWidth - inputWidth) : 0);
		offset.left -= (isFixed && offset.left == inst.input.offset().left) ? $(document).scrollLeft() : 0;
		offset.top -= (isFixed && offset.top == (inst.input.offset().top + inputHeight)) ? $(document).scrollTop() : 0;

		// now check if datepicker is showing outside window viewport - move to a better place if so.
		offset.left -= Math.min(offset.left, (offset.left + dpWidth > viewWidth && viewWidth > dpWidth) ?
			Math.abs(offset.left + dpWidth - viewWidth) : 0);
		offset.top -= Math.min(offset.top, (offset.top + dpHeight > viewHeight && viewHeight > dpHeight) ?
			Math.abs(dpHeight + inputHeight) : 0);

		return offset;
	},

	/* Find an object's position on the screen. */
	_findPos: function(obj) {
		var inst = this._getInst(obj);
		var isRTL = this._get(inst, 'isRTL');
        while (obj && (obj.type == 'hidden' || obj.nodeType != 1)) {
            obj = obj[isRTL ? 'previousSibling' : 'nextSibling'];
        }
        var position = $(obj).offset();
	    return [position.left, position.top];
	},

	/* Hide the date picker from view.
	   @param  input  element - the input field attached to the date picker */
	_hideDatepicker: function(input) {
		var inst = this._curInst;
		if (!inst || (input && inst != $.data(input, PROP_NAME)))
			return;
		if (this._datepickerShowing) {
			var showAnim = this._get(inst, 'showAnim');
			var duration = this._get(inst, 'duration');
			var postProcess = function() {
				$.datepicker._tidyDialog(inst);
				this._curInst = null;
			};
			if ($.effects && $.effects[showAnim])
				inst.dpDiv.hide(showAnim, $.datepicker._get(inst, 'showOptions'), duration, postProcess);
			else
				inst.dpDiv[(showAnim == 'slideDown' ? 'slideUp' :
					(showAnim == 'fadeIn' ? 'fadeOut' : 'hide'))]((showAnim ? duration : null), postProcess);
			if (!showAnim)
				postProcess();
			var onClose = this._get(inst, 'onClose');
			if (onClose)
				onClose.apply((inst.input ? inst.input[0] : null),
					[(inst.input ? inst.input.val() : ''), inst]);  // trigger custom callback
			this._datepickerShowing = false;
			this._lastInput = null;
			if (this._inDialog) {
				this._dialogInput.css({ position: 'absolute', left: '0', top: '-100px' });
				if ($.blockUI) {
					$.unblockUI();
					$('body').append(this.dpDiv);
				}
			}
			this._inDialog = false;
		}
	},

	/* Tidy up after a dialog display. */
	_tidyDialog: function(inst) {
		inst.dpDiv.removeClass(this._dialogClass).unbind('.ui-datepicker-calendar');
	},

	/* Close date picker if clicked elsewhere. */
	_checkExternalClick: function(event) {
		if (!$.datepicker._curInst)
			return;
		var $target = $(event.target);
		if ($target[0].id != $.datepicker._mainDivId &&
				$target.parents('#' + $.datepicker._mainDivId).length == 0 &&
				!$target.hasClass($.datepicker.markerClassName) &&
				!$target.hasClass($.datepicker._triggerClass) &&
				$.datepicker._datepickerShowing && !($.datepicker._inDialog && $.blockUI))
			$.datepicker._hideDatepicker();
	},

	/* Adjust one of the date sub-fields. */
	_adjustDate: function(id, offset, period) {
		var target = $(id);
		var inst = this._getInst(target[0]);
		if (this._isDisabledDatepicker(target[0])) {
			return;
		}
		this._adjustInstDate(inst, offset +
			(period == 'M' ? this._get(inst, 'showCurrentAtPos') : 0), // undo positioning
			period);
		this._updateDatepicker(inst);
	},

	/* Action for current link. */
	_gotoToday: function(id) {
		var target = $(id);
		var inst = this._getInst(target[0]);
		if (this._get(inst, 'gotoCurrent') && inst.currentDay) {
			inst.selectedDay = inst.currentDay;
			inst.drawMonth = inst.selectedMonth = inst.currentMonth;
			inst.drawYear = inst.selectedYear = inst.currentYear;
		}
		else {
			var date = new Date();
			inst.selectedDay = date.getDate();
			inst.drawMonth = inst.selectedMonth = date.getMonth();
			inst.drawYear = inst.selectedYear = date.getFullYear();
		}
		this._notifyChange(inst);
		this._adjustDate(target);
	},

	/* Action for selecting a new month/year. */
	_selectMonthYear: function(id, select, period) {
		var target = $(id);
		var inst = this._getInst(target[0]);
		inst._selectingMonthYear = false;
		inst['selected' + (period == 'M' ? 'Month' : 'Year')] =
		inst['draw' + (period == 'M' ? 'Month' : 'Year')] =
			parseInt(select.options[select.selectedIndex].value,10);
		this._notifyChange(inst);
		this._adjustDate(target);
	},

	/* Restore input focus after not changing month/year. */
	_clickMonthYear: function(id) {
		var target = $(id);
		var inst = this._getInst(target[0]);
		if (inst.input && inst._selectingMonthYear && !$.browser.msie)
			inst.input.focus();
		inst._selectingMonthYear = !inst._selectingMonthYear;
	},

	/* Action for selecting a day. */
	_selectDay: function(id, month, year, td) {
		var target = $(id);
		if ($(td).hasClass(this._unselectableClass) || this._isDisabledDatepicker(target[0])) {
			return;
		}
		var inst = this._getInst(target[0]);
		inst.selectedDay = inst.currentDay = $('a', td).html();
		inst.selectedMonth = inst.currentMonth = month;
		inst.selectedYear = inst.currentYear = year;
		this._selectDate(id, this._formatDate(inst,
			inst.currentDay, inst.currentMonth, inst.currentYear));
	},

	/* Erase the input field and hide the date picker. */
	_clearDate: function(id) {
		var target = $(id);
		var inst = this._getInst(target[0]);
		this._selectDate(target, '');
	},

	/* Update the input field with the selected date. */
	_selectDate: function(id, dateStr) {
		var target = $(id);
		var inst = this._getInst(target[0]);
		dateStr = (dateStr != null ? dateStr : this._formatDate(inst));
		if (inst.input)
			inst.input.val(dateStr);
		this._updateAlternate(inst);
		var onSelect = this._get(inst, 'onSelect');
		if (onSelect)
			onSelect.apply((inst.input ? inst.input[0] : null), [dateStr, inst]);  // trigger custom callback
		else if (inst.input)
			inst.input.trigger('change'); // fire the change event
		if (inst.inline)
			this._updateDatepicker(inst);
		else {
			this._hideDatepicker();
			this._lastInput = inst.input[0];
			if (typeof(inst.input[0]) != 'object')
				inst.input.focus(); // restore focus
			this._lastInput = null;
		}
	},

	/* Update any alternate field to synchronise with the main field. */
	_updateAlternate: function(inst) {
		var altField = this._get(inst, 'altField');
		if (altField) { // update alternate field too
			var altFormat = this._get(inst, 'altFormat') || this._get(inst, 'dateFormat');
			var date = this._getDate(inst);
			var dateStr = this.formatDate(altFormat, date, this._getFormatConfig(inst));
			$(altField).each(function() { $(this).val(dateStr); });
		}
	},

	/* Set as beforeShowDay function to prevent selection of weekends.
	   @param  date  Date - the date to customise
	   @return [boolean, string] - is this date selectable?, what is its CSS class? */
	noWeekends: function(date) {
		var day = date.getDay();
		return [(day > 0 && day < 6), ''];
	},

	/* Set as calculateWeek to determine the week of the year based on the ISO 8601 definition.
	   @param  date  Date - the date to get the week for
	   @return  number - the number of the week within the year that contains this date */
	iso8601Week: function(date) {
		var checkDate = new Date(date.getTime());
		// Find Thursday of this week starting on Monday
		checkDate.setDate(checkDate.getDate() + 4 - (checkDate.getDay() || 7));
		var time = checkDate.getTime();
		checkDate.setMonth(0); // Compare with Jan 1
		checkDate.setDate(1);
		return Math.floor(Math.round((time - checkDate) / 86400000) / 7) + 1;
	},

	/* Parse a string value into a date object.
	   See formatDate below for the possible formats.

	   @param  format    string - the expected format of the date
	   @param  value     string - the date in the above format
	   @param  settings  Object - attributes include:
	                     shortYearCutoff  number - the cutoff year for determining the century (optional)
	                     dayNamesShort    string[7] - abbreviated names of the days from Sunday (optional)
	                     dayNames         string[7] - names of the days from Sunday (optional)
	                     monthNamesShort  string[12] - abbreviated names of the months (optional)
	                     monthNames       string[12] - names of the months (optional)
	   @return  Date - the extracted date value or null if value is blank */
	parseDate: function (format, value, settings) {
		if (format == null || value == null)
			throw 'Invalid arguments';
		value = (typeof value == 'object' ? value.toString() : value + '');
		if (value == '')
			return null;
		var shortYearCutoff = (settings ? settings.shortYearCutoff : null) || this._defaults.shortYearCutoff;
		var dayNamesShort = (settings ? settings.dayNamesShort : null) || this._defaults.dayNamesShort;
		var dayNames = (settings ? settings.dayNames : null) || this._defaults.dayNames;
		var monthNamesShort = (settings ? settings.monthNamesShort : null) || this._defaults.monthNamesShort;
		var monthNames = (settings ? settings.monthNames : null) || this._defaults.monthNames;
		var year = -1;
		var month = -1;
		var day = -1;
		var doy = -1;
		var literal = false;
		// Check whether a format character is doubled
		var lookAhead = function(match) {
			var matches = (iFormat + 1 < format.length && format.charAt(iFormat + 1) == match);
			if (matches)
				iFormat++;
			return matches;
		};
		// Extract a number from the string value
		var getNumber = function(match) {
			lookAhead(match);
			var size = (match == '@' ? 14 : (match == '!' ? 20 :
				(match == 'y' ? 4 : (match == 'o' ? 3 : 2))));
			var digits = new RegExp('^\\d{1,' + size + '}');
			var num = value.substring(iValue).match(digits);
			if (!num)
				throw 'Missing number at position ' + iValue;
			iValue += num[0].length;
			return parseInt(num[0], 10);
		};
		// Extract a name from the string value and convert to an index
		var getName = function(match, shortNames, longNames) {
			var names = (lookAhead(match) ? longNames : shortNames);
			for (var i = 0; i < names.length; i++) {
				if (value.substr(iValue, names[i].length) == names[i]) {
					iValue += names[i].length;
					return i + 1;
				}
			}
			throw 'Unknown name at position ' + iValue;
		};
		// Confirm that a literal character matches the string value
		var checkLiteral = function() {
			if (value.charAt(iValue) != format.charAt(iFormat))
				throw 'Unexpected literal at position ' + iValue;
			iValue++;
		};
		var iValue = 0;
		for (var iFormat = 0; iFormat < format.length; iFormat++) {
			if (literal)
				if (format.charAt(iFormat) == "'" && !lookAhead("'"))
					literal = false;
				else
					checkLiteral();
			else
				switch (format.charAt(iFormat)) {
					case 'd':
						day = getNumber('d');
						break;
					case 'D':
						getName('D', dayNamesShort, dayNames);
						break;
					case 'o':
						doy = getNumber('o');
						break;
					case 'm':
						month = getNumber('m');
						break;
					case 'M':
						month = getName('M', monthNamesShort, monthNames);
						break;
					case 'y':
						year = getNumber('y');
						break;
					case '@':
						var date = new Date(getNumber('@'));
						year = date.getFullYear();
						month = date.getMonth() + 1;
						day = date.getDate();
						break;
					case '!':
						var date = new Date((getNumber('!') - this._ticksTo1970) / 10000);
						year = date.getFullYear();
						month = date.getMonth() + 1;
						day = date.getDate();
						break;
					case "'":
						if (lookAhead("'"))
							checkLiteral();
						else
							literal = true;
						break;
					default:
						checkLiteral();
				}
		}
		if (year == -1)
			year = new Date().getFullYear();
		else if (year < 100)
			year += new Date().getFullYear() - new Date().getFullYear() % 100 +
				(year <= shortYearCutoff ? 0 : -100);
		if (doy > -1) {
			month = 1;
			day = doy;
			do {
				var dim = this._getDaysInMonth(year, month - 1);
				if (day <= dim)
					break;
				month++;
				day -= dim;
			} while (true);
		}
		var date = this._daylightSavingAdjust(new Date(year, month - 1, day));
		if (date.getFullYear() != year || date.getMonth() + 1 != month || date.getDate() != day)
			throw 'Invalid date'; // E.g. 31/02/*
		return date;
	},

	/* Standard date formats. */
	ATOM: 'yy-mm-dd', // RFC 3339 (ISO 8601)
	COOKIE: 'D, dd M yy',
	ISO_8601: 'yy-mm-dd',
	RFC_822: 'D, d M y',
	RFC_850: 'DD, dd-M-y',
	RFC_1036: 'D, d M y',
	RFC_1123: 'D, d M yy',
	RFC_2822: 'D, d M yy',
	RSS: 'D, d M y', // RFC 822
	TICKS: '!',
	TIMESTAMP: '@',
	W3C: 'yy-mm-dd', // ISO 8601

	_ticksTo1970: (((1970 - 1) * 365 + Math.floor(1970 / 4) - Math.floor(1970 / 100) +
		Math.floor(1970 / 400)) * 24 * 60 * 60 * 10000000),

	/* Format a date object into a string value.
	   The format can be combinations of the following:
	   d  - day of month (no leading zero)
	   dd - day of month (two digit)
	   o  - day of year (no leading zeros)
	   oo - day of year (three digit)
	   D  - day name short
	   DD - day name long
	   m  - month of year (no leading zero)
	   mm - month of year (two digit)
	   M  - month name short
	   MM - month name long
	   y  - year (two digit)
	   yy - year (four digit)
	   @ - Unix timestamp (ms since 01/01/1970)
	   ! - Windows ticks (100ns since 01/01/0001)
	   '...' - literal text
	   '' - single quote

	   @param  format    string - the desired format of the date
	   @param  date      Date - the date value to format
	   @param  settings  Object - attributes include:
	                     dayNamesShort    string[7] - abbreviated names of the days from Sunday (optional)
	                     dayNames         string[7] - names of the days from Sunday (optional)
	                     monthNamesShort  string[12] - abbreviated names of the months (optional)
	                     monthNames       string[12] - names of the months (optional)
	   @return  string - the date in the above format */
	formatDate: function (format, date, settings) {
		if (!date)
			return '';
		var dayNamesShort = (settings ? settings.dayNamesShort : null) || this._defaults.dayNamesShort;
		var dayNames = (settings ? settings.dayNames : null) || this._defaults.dayNames;
		var monthNamesShort = (settings ? settings.monthNamesShort : null) || this._defaults.monthNamesShort;
		var monthNames = (settings ? settings.monthNames : null) || this._defaults.monthNames;
		// Check whether a format character is doubled
		var lookAhead = function(match) {
			var matches = (iFormat + 1 < format.length && format.charAt(iFormat + 1) == match);
			if (matches)
				iFormat++;
			return matches;
		};
		// Format a number, with leading zero if necessary
		var formatNumber = function(match, value, len) {
			var num = '' + value;
			if (lookAhead(match))
				while (num.length < len)
					num = '0' + num;
			return num;
		};
		// Format a name, short or long as requested
		var formatName = function(match, value, shortNames, longNames) {
			return (lookAhead(match) ? longNames[value] : shortNames[value]);
		};
		var output = '';
		var literal = false;
		if (date)
			for (var iFormat = 0; iFormat < format.length; iFormat++) {
				if (literal)
					if (format.charAt(iFormat) == "'" && !lookAhead("'"))
						literal = false;
					else
						output += format.charAt(iFormat);
				else
					switch (format.charAt(iFormat)) {
						case 'd':
							output += formatNumber('d', date.getDate(), 2);
							break;
						case 'D':
							output += formatName('D', date.getDay(), dayNamesShort, dayNames);
							break;
						case 'o':
							output += formatNumber('o',
								(date.getTime() - new Date(date.getFullYear(), 0, 0).getTime()) / 86400000, 3);
							break;
						case 'm':
							output += formatNumber('m', date.getMonth() + 1, 2);
							break;
						case 'M':
							output += formatName('M', date.getMonth(), monthNamesShort, monthNames);
							break;
						case 'y':
							output += (lookAhead('y') ? date.getFullYear() :
								(date.getYear() % 100 < 10 ? '0' : '') + date.getYear() % 100);
							break;
						case '@':
							output += date.getTime();
							break;
						case '!':
							output += date.getTime() * 10000 + this._ticksTo1970;
							break;
						case "'":
							if (lookAhead("'"))
								output += "'";
							else
								literal = true;
							break;
						default:
							output += format.charAt(iFormat);
					}
			}
		return output;
	},

	/* Extract all possible characters from the date format. */
	_possibleChars: function (format) {
		var chars = '';
		var literal = false;
		// Check whether a format character is doubled
		var lookAhead = function(match) {
			var matches = (iFormat + 1 < format.length && format.charAt(iFormat + 1) == match);
			if (matches)
				iFormat++;
			return matches;
		};
		for (var iFormat = 0; iFormat < format.length; iFormat++)
			if (literal)
				if (format.charAt(iFormat) == "'" && !lookAhead("'"))
					literal = false;
				else
					chars += format.charAt(iFormat);
			else
				switch (format.charAt(iFormat)) {
					case 'd': case 'm': case 'y': case '@':
						chars += '0123456789';
						break;
					case 'D': case 'M':
						return null; // Accept anything
					case "'":
						if (lookAhead("'"))
							chars += "'";
						else
							literal = true;
						break;
					default:
						chars += format.charAt(iFormat);
				}
		return chars;
	},

	/* Get a setting value, defaulting if necessary. */
	_get: function(inst, name) {
		return inst.settings[name] !== undefined ?
			inst.settings[name] : this._defaults[name];
	},

	/* Parse existing date and initialise date picker. */
	_setDateFromField: function(inst, noDefault) {
		if (inst.input.val() == inst.lastVal) {
			return;
		}
		var dateFormat = this._get(inst, 'dateFormat');
		var dates = inst.lastVal = inst.input ? inst.input.val() : null;
		var date, defaultDate;
		date = defaultDate = this._getDefaultDate(inst);
		var settings = this._getFormatConfig(inst);
		try {
			date = this.parseDate(dateFormat, dates, settings) || defaultDate;
		} catch (event) {
			this.log(event);
			dates = (noDefault ? '' : dates);
		}
		inst.selectedDay = date.getDate();
		inst.drawMonth = inst.selectedMonth = date.getMonth();
		inst.drawYear = inst.selectedYear = date.getFullYear();
		inst.currentDay = (dates ? date.getDate() : 0);
		inst.currentMonth = (dates ? date.getMonth() : 0);
		inst.currentYear = (dates ? date.getFullYear() : 0);
		this._adjustInstDate(inst);
	},

	/* Retrieve the default date shown on opening. */
	_getDefaultDate: function(inst) {
		return this._restrictMinMax(inst,
			this._determineDate(inst, this._get(inst, 'defaultDate'), new Date()));
	},

	/* A date may be specified as an exact value or a relative one. */
	_determineDate: function(inst, date, defaultDate) {
		var offsetNumeric = function(offset) {
			var date = new Date();
			date.setDate(date.getDate() + offset);
			return date;
		};
		var offsetString = function(offset) {
			try {
				return $.datepicker.parseDate($.datepicker._get(inst, 'dateFormat'),
					offset, $.datepicker._getFormatConfig(inst));
			}
			catch (e) {
				// Ignore
			}
			var date = (offset.toLowerCase().match(/^c/) ?
				$.datepicker._getDate(inst) : null) || new Date();
			var year = date.getFullYear();
			var month = date.getMonth();
			var day = date.getDate();
			var pattern = /([+-]?[0-9]+)\s*(d|D|w|W|m|M|y|Y)?/g;
			var matches = pattern.exec(offset);
			while (matches) {
				switch (matches[2] || 'd') {
					case 'd' : case 'D' :
						day += parseInt(matches[1],10); break;
					case 'w' : case 'W' :
						day += parseInt(matches[1],10) * 7; break;
					case 'm' : case 'M' :
						month += parseInt(matches[1],10);
						day = Math.min(day, $.datepicker._getDaysInMonth(year, month));
						break;
					case 'y': case 'Y' :
						year += parseInt(matches[1],10);
						day = Math.min(day, $.datepicker._getDaysInMonth(year, month));
						break;
				}
				matches = pattern.exec(offset);
			}
			return new Date(year, month, day);
		};
		date = (date == null ? defaultDate : (typeof date == 'string' ? offsetString(date) :
			(typeof date == 'number' ? (isNaN(date) ? defaultDate : offsetNumeric(date)) : date)));
		date = (date && date.toString() == 'Invalid Date' ? defaultDate : date);
		if (date) {
			date.setHours(0);
			date.setMinutes(0);
			date.setSeconds(0);
			date.setMilliseconds(0);
		}
		return this._daylightSavingAdjust(date);
	},

	/* Handle switch to/from daylight saving.
	   Hours may be non-zero on daylight saving cut-over:
	   > 12 when midnight changeover, but then cannot generate
	   midnight datetime, so jump to 1AM, otherwise reset.
	   @param  date  (Date) the date to check
	   @return  (Date) the corrected date */
	_daylightSavingAdjust: function(date) {
		if (!date) return null;
		date.setHours(date.getHours() > 12 ? date.getHours() + 2 : 0);
		return date;
	},

	/* Set the date(s) directly. */
	_setDate: function(inst, date, noChange) {
		var clear = !(date);
		var origMonth = inst.selectedMonth;
		var origYear = inst.selectedYear;
		date = this._restrictMinMax(inst, this._determineDate(inst, date, new Date()));
		inst.selectedDay = inst.currentDay = date.getDate();
		inst.drawMonth = inst.selectedMonth = inst.currentMonth = date.getMonth();
		inst.drawYear = inst.selectedYear = inst.currentYear = date.getFullYear();
		if ((origMonth != inst.selectedMonth || origYear != inst.selectedYear) && !noChange)
			this._notifyChange(inst);
		this._adjustInstDate(inst);
		if (inst.input) {
			inst.input.val(clear ? '' : this._formatDate(inst));
		}
	},

	/* Retrieve the date(s) directly. */
	_getDate: function(inst) {
		var startDate = (!inst.currentYear || (inst.input && inst.input.val() == '') ? null :
			this._daylightSavingAdjust(new Date(
			inst.currentYear, inst.currentMonth, inst.currentDay)));
			return startDate;
	},

	/* Generate the HTML for the current state of the date picker. */
	_generateHTML: function(inst) {
		var today = new Date();
		today = this._daylightSavingAdjust(
			new Date(today.getFullYear(), today.getMonth(), today.getDate())); // clear time
		var isRTL = this._get(inst, 'isRTL');
		var showButtonPanel = this._get(inst, 'showButtonPanel');
		var hideIfNoPrevNext = this._get(inst, 'hideIfNoPrevNext');
		var navigationAsDateFormat = this._get(inst, 'navigationAsDateFormat');
		var numMonths = this._getNumberOfMonths(inst);
		var showCurrentAtPos = this._get(inst, 'showCurrentAtPos');
		var stepMonths = this._get(inst, 'stepMonths');
		var isMultiMonth = (numMonths[0] != 1 || numMonths[1] != 1);
		var currentDate = this._daylightSavingAdjust((!inst.currentDay ? new Date(9999, 9, 9) :
			new Date(inst.currentYear, inst.currentMonth, inst.currentDay)));
		var minDate = this._getMinMaxDate(inst, 'min');
		var maxDate = this._getMinMaxDate(inst, 'max');
		var drawMonth = inst.drawMonth - showCurrentAtPos;
		var drawYear = inst.drawYear;
		if (drawMonth < 0) {
			drawMonth += 12;
			drawYear--;
		}
		if (maxDate) {
			var maxDraw = this._daylightSavingAdjust(new Date(maxDate.getFullYear(),
				maxDate.getMonth() - (numMonths[0] * numMonths[1]) + 1, maxDate.getDate()));
			maxDraw = (minDate && maxDraw < minDate ? minDate : maxDraw);
			while (this._daylightSavingAdjust(new Date(drawYear, drawMonth, 1)) > maxDraw) {
				drawMonth--;
				if (drawMonth < 0) {
					drawMonth = 11;
					drawYear--;
				}
			}
		}
		inst.drawMonth = drawMonth;
		inst.drawYear = drawYear;
		var prevText = this._get(inst, 'prevText');
		prevText = (!navigationAsDateFormat ? prevText : this.formatDate(prevText,
			this._daylightSavingAdjust(new Date(drawYear, drawMonth - stepMonths, 1)),
			this._getFormatConfig(inst)));
		var prev = (this._canAdjustMonth(inst, -1, drawYear, drawMonth) ?
			'<a class="ui-datepicker-prev ui-corner-all" onclick="DP_jQuery_' + dpuuid +
			'.datepicker._adjustDate(\'#' + inst.id + '\', -' + stepMonths + ', \'M\');"' +
			' title="' + prevText + '"><span class="ui-icon ui-icon-circle-triangle-' + ( isRTL ? 'e' : 'w') + '">' + prevText + '</span></a>' :
			(hideIfNoPrevNext ? '' : '<a class="ui-datepicker-prev ui-corner-all ui-state-disabled" title="'+ prevText +'"><span class="ui-icon ui-icon-circle-triangle-' + ( isRTL ? 'e' : 'w') + '">' + prevText + '</span></a>'));
		var nextText = this._get(inst, 'nextText');
		nextText = (!navigationAsDateFormat ? nextText : this.formatDate(nextText,
			this._daylightSavingAdjust(new Date(drawYear, drawMonth + stepMonths, 1)),
			this._getFormatConfig(inst)));
		var next = (this._canAdjustMonth(inst, +1, drawYear, drawMonth) ?
			'<a class="ui-datepicker-next ui-corner-all" onclick="DP_jQuery_' + dpuuid +
			'.datepicker._adjustDate(\'#' + inst.id + '\', +' + stepMonths + ', \'M\');"' +
			' title="' + nextText + '"><span class="ui-icon ui-icon-circle-triangle-' + ( isRTL ? 'w' : 'e') + '">' + nextText + '</span></a>' :
			(hideIfNoPrevNext ? '' : '<a class="ui-datepicker-next ui-corner-all ui-state-disabled" title="'+ nextText + '"><span class="ui-icon ui-icon-circle-triangle-' + ( isRTL ? 'w' : 'e') + '">' + nextText + '</span></a>'));
		var currentText = this._get(inst, 'currentText');
		var gotoDate = (this._get(inst, 'gotoCurrent') && inst.currentDay ? currentDate : today);
		currentText = (!navigationAsDateFormat ? currentText :
			this.formatDate(currentText, gotoDate, this._getFormatConfig(inst)));
		var controls = (!inst.inline ? '<button type="button" class="ui-datepicker-close ui-state-default ui-priority-primary ui-corner-all" onclick="DP_jQuery_' + dpuuid +
			'.datepicker._hideDatepicker();">' + this._get(inst, 'closeText') + '</button>' : '');
		var buttonPanel = (showButtonPanel) ? '<div class="ui-datepicker-buttonpane ui-widget-content">' + (isRTL ? controls : '') +
			(this._isInRange(inst, gotoDate) ? '<button type="button" class="ui-datepicker-current ui-state-default ui-priority-secondary ui-corner-all" onclick="DP_jQuery_' + dpuuid +
			'.datepicker._gotoToday(\'#' + inst.id + '\');"' +
			'>' + currentText + '</button>' : '') + (isRTL ? '' : controls) + '</div>' : '';
		var firstDay = parseInt(this._get(inst, 'firstDay'),10);
		firstDay = (isNaN(firstDay) ? 0 : firstDay);
		var showWeek = this._get(inst, 'showWeek');
		var dayNames = this._get(inst, 'dayNames');
		var dayNamesShort = this._get(inst, 'dayNamesShort');
		var dayNamesMin = this._get(inst, 'dayNamesMin');
		var monthNames = this._get(inst, 'monthNames');
		var monthNamesShort = this._get(inst, 'monthNamesShort');
		var beforeShowDay = this._get(inst, 'beforeShowDay');
		var showOtherMonths = this._get(inst, 'showOtherMonths');
		var selectOtherMonths = this._get(inst, 'selectOtherMonths');
		var calculateWeek = this._get(inst, 'calculateWeek') || this.iso8601Week;
		var defaultDate = this._getDefaultDate(inst);
		var html = '';
		for (var row = 0; row < numMonths[0]; row++) {
			var group = '';
			for (var col = 0; col < numMonths[1]; col++) {
				var selectedDate = this._daylightSavingAdjust(new Date(drawYear, drawMonth, inst.selectedDay));
				var cornerClass = ' ui-corner-all';
				var calender = '';
				if (isMultiMonth) {
					calender += '<div class="ui-datepicker-group';
					if (numMonths[1] > 1)
						switch (col) {
							case 0: calender += ' ui-datepicker-group-first';
								cornerClass = ' ui-corner-' + (isRTL ? 'right' : 'left'); break;
							case numMonths[1]-1: calender += ' ui-datepicker-group-last';
								cornerClass = ' ui-corner-' + (isRTL ? 'left' : 'right'); break;
							default: calender += ' ui-datepicker-group-middle'; cornerClass = ''; break;
						}
					calender += '">';
				}
				calender += '<div class="ui-datepicker-header ui-widget-header ui-helper-clearfix' + cornerClass + '">' +
					(/all|left/.test(cornerClass) && row == 0 ? (isRTL ? next : prev) : '') +
					(/all|right/.test(cornerClass) && row == 0 ? (isRTL ? prev : next) : '') +
					this._generateMonthYearHeader(inst, drawMonth, drawYear, minDate, maxDate,
					row > 0 || col > 0, monthNames, monthNamesShort) + // draw month headers
					'</div><table class="ui-datepicker-calendar"><thead>' +
					'<tr>';
				var thead = (showWeek ? '<th class="ui-datepicker-week-col">' + this._get(inst, 'weekHeader') + '</th>' : '');
				for (var dow = 0; dow < 7; dow++) { // days of the week
					var day = (dow + firstDay) % 7;
					thead += '<th' + ((dow + firstDay + 6) % 7 >= 5 ? ' class="ui-datepicker-week-end"' : '') + '>' +
						'<span title="' + dayNames[day] + '">' + dayNamesMin[day] + '</span></th>';
				}
				calender += thead + '</tr></thead><tbody>';
				var daysInMonth = this._getDaysInMonth(drawYear, drawMonth);
				if (drawYear == inst.selectedYear && drawMonth == inst.selectedMonth)
					inst.selectedDay = Math.min(inst.selectedDay, daysInMonth);
				var leadDays = (this._getFirstDayOfMonth(drawYear, drawMonth) - firstDay + 7) % 7;
				var numRows = (isMultiMonth ? 6 : Math.ceil((leadDays + daysInMonth) / 7)); // calculate the number of rows to generate
				var printDate = this._daylightSavingAdjust(new Date(drawYear, drawMonth, 1 - leadDays));
				for (var dRow = 0; dRow < numRows; dRow++) { // create date picker rows
					calender += '<tr>';
					var tbody = (!showWeek ? '' : '<td class="ui-datepicker-week-col">' +
						this._get(inst, 'calculateWeek')(printDate) + '</td>');
					for (var dow = 0; dow < 7; dow++) { // create date picker days
						var daySettings = (beforeShowDay ?
							beforeShowDay.apply((inst.input ? inst.input[0] : null), [printDate]) : [true, '']);
						var otherMonth = (printDate.getMonth() != drawMonth);
						var unselectable = (otherMonth && !selectOtherMonths) || !daySettings[0] ||
							(minDate && printDate < minDate) || (maxDate && printDate > maxDate);
						tbody += '<td class="' +
							((dow + firstDay + 6) % 7 >= 5 ? ' ui-datepicker-week-end' : '') + // highlight weekends
							(otherMonth ? ' ui-datepicker-other-month' : '') + // highlight days from other months
							((printDate.getTime() == selectedDate.getTime() && drawMonth == inst.selectedMonth && inst._keyEvent) || // user pressed key
							(defaultDate.getTime() == printDate.getTime() && defaultDate.getTime() == selectedDate.getTime()) ?
							// or defaultDate is current printedDate and defaultDate is selectedDate
							' ' + this._dayOverClass : '') + // highlight selected day
							(unselectable ? ' ' + this._unselectableClass + ' ui-state-disabled': '') +  // highlight unselectable days
							(otherMonth && !showOtherMonths ? '' : ' ' + daySettings[1] + // highlight custom dates
							(printDate.getTime() == currentDate.getTime() ? ' ' + this._currentClass : '') + // highlight selected day
							(printDate.getTime() == today.getTime() ? ' ui-datepicker-today' : '')) + '"' + // highlight today (if different)
							((!otherMonth || showOtherMonths) && daySettings[2] ? ' title="' + daySettings[2] + '"' : '') + // cell title
							(unselectable ? '' : ' onclick="DP_jQuery_' + dpuuid + '.datepicker._selectDay(\'#' +
							inst.id + '\',' + printDate.getMonth() + ',' + printDate.getFullYear() + ', this);return false;"') + '>' + // actions
							(otherMonth && !showOtherMonths ? '&#xa0;' : // display for other months
							(unselectable ? '<span class="ui-state-default">' + printDate.getDate() + '</span>' : '<a class="ui-state-default' +
							(printDate.getTime() == today.getTime() ? ' ui-state-highlight' : '') +
							(printDate.getTime() == currentDate.getTime() ? ' ui-state-active' : '') + // highlight selected day
							(otherMonth ? ' ui-priority-secondary' : '') + // distinguish dates from other months
							'" href="#">' + printDate.getDate() + '</a>')) + '</td>'; // display selectable date
						printDate.setDate(printDate.getDate() + 1);
						printDate = this._daylightSavingAdjust(printDate);
					}
					calender += tbody + '</tr>';
				}
				drawMonth++;
				if (drawMonth > 11) {
					drawMonth = 0;
					drawYear++;
				}
				calender += '</tbody></table>' + (isMultiMonth ? '</div>' + 
							((numMonths[0] > 0 && col == numMonths[1]-1) ? '<div class="ui-datepicker-row-break"></div>' : '') : '');
				group += calender;
			}
			html += group;
		}
		html += buttonPanel + ($.browser.msie && parseInt($.browser.version,10) < 7 && !inst.inline ?
			'<iframe src="javascript:false;" class="ui-datepicker-cover" frameborder="0"></iframe>' : '');
		inst._keyEvent = false;
		return html;
	},

	/* Generate the month and year header. */
	_generateMonthYearHeader: function(inst, drawMonth, drawYear, minDate, maxDate,
			secondary, monthNames, monthNamesShort) {
		var changeMonth = this._get(inst, 'changeMonth');
		var changeYear = this._get(inst, 'changeYear');
		var showMonthAfterYear = this._get(inst, 'showMonthAfterYear');
		var html = '<div class="ui-datepicker-title">';
		var monthHtml = '';
		// month selection
		if (secondary || !changeMonth)
			monthHtml += '<span class="ui-datepicker-month">' + monthNames[drawMonth] + '</span>';
		else {
			var inMinYear = (minDate && minDate.getFullYear() == drawYear);
			var inMaxYear = (maxDate && maxDate.getFullYear() == drawYear);
			monthHtml += '<select class="ui-datepicker-month" ' +
				'onchange="DP_jQuery_' + dpuuid + '.datepicker._selectMonthYear(\'#' + inst.id + '\', this, \'M\');" ' +
				'onclick="DP_jQuery_' + dpuuid + '.datepicker._clickMonthYear(\'#' + inst.id + '\');"' +
			 	'>';
			for (var month = 0; month < 12; month++) {
				if ((!inMinYear || month >= minDate.getMonth()) &&
						(!inMaxYear || month <= maxDate.getMonth()))
					monthHtml += '<option value="' + month + '"' +
						(month == drawMonth ? ' selected="selected"' : '') +
						'>' + monthNamesShort[month] + '</option>';
			}
			monthHtml += '</select>';
		}
		if (!showMonthAfterYear)
			html += monthHtml + (secondary || !(changeMonth && changeYear) ? '&#xa0;' : '');
		// year selection
		if (secondary || !changeYear)
			html += '<span class="ui-datepicker-year">' + drawYear + '</span>';
		else {
			// determine range of years to display
			var years = this._get(inst, 'yearRange').split(':');
			var thisYear = new Date().getFullYear();
			var determineYear = function(value) {
				var year = (value.match(/c[+-].*/) ? drawYear + parseInt(value.substring(1), 10) :
					(value.match(/[+-].*/) ? thisYear + parseInt(value, 10) :
					parseInt(value, 10)));
				return (isNaN(year) ? thisYear : year);
			};
			var year = determineYear(years[0]);
			var endYear = Math.max(year, determineYear(years[1] || ''));
			year = (minDate ? Math.max(year, minDate.getFullYear()) : year);
			endYear = (maxDate ? Math.min(endYear, maxDate.getFullYear()) : endYear);
			html += '<select class="ui-datepicker-year" ' +
				'onchange="DP_jQuery_' + dpuuid + '.datepicker._selectMonthYear(\'#' + inst.id + '\', this, \'Y\');" ' +
				'onclick="DP_jQuery_' + dpuuid + '.datepicker._clickMonthYear(\'#' + inst.id + '\');"' +
				'>';
			for (; year <= endYear; year++) {
				html += '<option value="' + year + '"' +
					(year == drawYear ? ' selected="selected"' : '') +
					'>' + year + '</option>';
			}
			html += '</select>';
		}
		html += this._get(inst, 'yearSuffix');
		if (showMonthAfterYear)
			html += (secondary || !(changeMonth && changeYear) ? '&#xa0;' : '') + monthHtml;
		html += '</div>'; // Close datepicker_header
		return html;
	},

	/* Adjust one of the date sub-fields. */
	_adjustInstDate: function(inst, offset, period) {
		var year = inst.drawYear + (period == 'Y' ? offset : 0);
		var month = inst.drawMonth + (period == 'M' ? offset : 0);
		var day = Math.min(inst.selectedDay, this._getDaysInMonth(year, month)) +
			(period == 'D' ? offset : 0);
		var date = this._restrictMinMax(inst,
			this._daylightSavingAdjust(new Date(year, month, day)));
		inst.selectedDay = date.getDate();
		inst.drawMonth = inst.selectedMonth = date.getMonth();
		inst.drawYear = inst.selectedYear = date.getFullYear();
		if (period == 'M' || period == 'Y')
			this._notifyChange(inst);
	},

	/* Ensure a date is within any min/max bounds. */
	_restrictMinMax: function(inst, date) {
		var minDate = this._getMinMaxDate(inst, 'min');
		var maxDate = this._getMinMaxDate(inst, 'max');
		date = (minDate && date < minDate ? minDate : date);
		date = (maxDate && date > maxDate ? maxDate : date);
		return date;
	},

	/* Notify change of month/year. */
	_notifyChange: function(inst) {
		var onChange = this._get(inst, 'onChangeMonthYear');
		if (onChange)
			onChange.apply((inst.input ? inst.input[0] : null),
				[inst.selectedYear, inst.selectedMonth + 1, inst]);
	},

	/* Determine the number of months to show. */
	_getNumberOfMonths: function(inst) {
		var numMonths = this._get(inst, 'numberOfMonths');
		return (numMonths == null ? [1, 1] : (typeof numMonths == 'number' ? [1, numMonths] : numMonths));
	},

	/* Determine the current maximum date - ensure no time components are set. */
	_getMinMaxDate: function(inst, minMax) {
		return this._determineDate(inst, this._get(inst, minMax + 'Date'), null);
	},

	/* Find the number of days in a given month. */
	_getDaysInMonth: function(year, month) {
		return 32 - new Date(year, month, 32).getDate();
	},

	/* Find the day of the week of the first of a month. */
	_getFirstDayOfMonth: function(year, month) {
		return new Date(year, month, 1).getDay();
	},

	/* Determines if we should allow a "next/prev" month display change. */
	_canAdjustMonth: function(inst, offset, curYear, curMonth) {
		var numMonths = this._getNumberOfMonths(inst);
		var date = this._daylightSavingAdjust(new Date(curYear,
			curMonth + (offset < 0 ? offset : numMonths[0] * numMonths[1]), 1));
		if (offset < 0)
			date.setDate(this._getDaysInMonth(date.getFullYear(), date.getMonth()));
		return this._isInRange(inst, date);
	},

	/* Is the given date in the accepted range? */
	_isInRange: function(inst, date) {
		var minDate = this._getMinMaxDate(inst, 'min');
		var maxDate = this._getMinMaxDate(inst, 'max');
		return ((!minDate || date.getTime() >= minDate.getTime()) &&
			(!maxDate || date.getTime() <= maxDate.getTime()));
	},

	/* Provide the configuration settings for formatting/parsing. */
	_getFormatConfig: function(inst) {
		var shortYearCutoff = this._get(inst, 'shortYearCutoff');
		shortYearCutoff = (typeof shortYearCutoff != 'string' ? shortYearCutoff :
			new Date().getFullYear() % 100 + parseInt(shortYearCutoff, 10));
		return {shortYearCutoff: shortYearCutoff,
			dayNamesShort: this._get(inst, 'dayNamesShort'), dayNames: this._get(inst, 'dayNames'),
			monthNamesShort: this._get(inst, 'monthNamesShort'), monthNames: this._get(inst, 'monthNames')};
	},

	/* Format the given date for display. */
	_formatDate: function(inst, day, month, year) {
		if (!day) {
			inst.currentDay = inst.selectedDay;
			inst.currentMonth = inst.selectedMonth;
			inst.currentYear = inst.selectedYear;
		}
		var date = (day ? (typeof day == 'object' ? day :
			this._daylightSavingAdjust(new Date(year, month, day))) :
			this._daylightSavingAdjust(new Date(inst.currentYear, inst.currentMonth, inst.currentDay)));
		return this.formatDate(this._get(inst, 'dateFormat'), date, this._getFormatConfig(inst));
	}
});

/* jQuery extend now ignores nulls! */
function extendRemove(target, props) {
	$.extend(target, props);
	for (var name in props)
		if (props[name] == null || props[name] == undefined)
			target[name] = props[name];
	return target;
};

/* Determine whether an object is an array. */
function isArray(a) {
	return (a && (($.browser.safari && typeof a == 'object' && a.length) ||
		(a.constructor && a.constructor.toString().match(/\Array\(\)/))));
};

/* Invoke the datepicker functionality.
   @param  options  string - a command, optionally followed by additional parameters or
                    Object - settings for attaching new datepicker functionality
   @return  jQuery object */
$.fn.datepicker = function(options){

	/* Initialise the date picker. */
	if (!$.datepicker.initialized) {
		$(document).mousedown($.datepicker._checkExternalClick).
			find('body').append($.datepicker.dpDiv);
		$.datepicker.initialized = true;
	}

	var otherArgs = Array.prototype.slice.call(arguments, 1);
	if (typeof options == 'string' && (options == 'isDisabled' || options == 'getDate' || options == 'widget'))
		return $.datepicker['_' + options + 'Datepicker'].
			apply($.datepicker, [this[0]].concat(otherArgs));
	if (options == 'option' && arguments.length == 2 && typeof arguments[1] == 'string')
		return $.datepicker['_' + options + 'Datepicker'].
			apply($.datepicker, [this[0]].concat(otherArgs));
	return this.each(function() {
		typeof options == 'string' ?
			$.datepicker['_' + options + 'Datepicker'].
				apply($.datepicker, [this].concat(otherArgs)) :
			$.datepicker._attachDatepicker(this, options);
	});
};

$.datepicker = new Datepicker(); // singleton instance
$.datepicker.initialized = false;
$.datepicker.uuid = new Date().getTime();
$.datepicker.version = "1.8";

// Workaround for #4055
// Add another global to avoid noConflict issues with inline event handlers
window['DP_jQuery_' + dpuuid] = $;

})(jQuery);

/* 
This is not meant to stand-alone, it is a wrapper to ui.datepicker and makes ui.datepicker seem like a ui widget.
We can't extend this like other widgets, since ui.datepicker is not a factory widget like the others.

*/

somens = {};

somens.foo = function( dateText, inst ) {
    alert('The day you picked is: ' + inst.selectedDay  );
};


(function($) {
    
    $.widget("ui.ncbidatepicker", { // params: name of widget, prototype. Keep widget in ui namespace.
        _create: function() { // _init is used for setting up classes, binding events etc
 
            // datepicker API documentation is weird. Events are really options with callbacks. No actual events are triggered.
            // so, loop thru options, if they are callbacks, get fn object from config
            var callbackOptionNames = [ 'beforeShow', 'beforeShowDay', 'onChangeMonthYear', 'onClose', 'onSelect' ]
            var options = this.options;
            for ( var optionName in options ) {
                if ( $.inArray( optionName, callbackOptionNames ) !== -1 ) {
                    this.options[ optionName ] = $.ui.jig._getFncFromStr( this.options[ optionName ], this  );
                } 
            }
            this.element.datepicker( this.options );
            /*
            var that = this;
            this.element.datepicker({
                onClose: function(dateText, inst) { that._onClose(dateText, inst);  },
                onSelect: function(dateText, inst) { that._onSelect(dateText, inst);  },
            });
            */

        },
        show: function() {
            var args = [];
            for ( var i = 0; i < arguments.length; i++ ) {
                args.push( arguments[ i ] );
            }
            var params =  ['show'].concat( args );
            return this.element.datepicker.apply(this.element, params);
        },
        hide: function() {
            var args = [];
            for ( var i = 0; i < arguments.length; i++ ) {
                args.push( arguments[ i ] );
            }
            var params =  ['hide'].concat( args );
            return this.element.datepicker.apply(this.element, params);
        },
        destroy: function() {
            this.element.datepicker('destroy');
        },
        disable: function() {
            this.element.datepicker('disable');
        },
        enable: function() {
            this.element.datepicker('enable');
        },
        option: function() {
            var args = [];
            for ( var i = 0; i < arguments.length; i++ ) {
                args.push( arguments[ i ] );
            }
            var params =  ['option'].concat( args );
            return this.element.datepicker.apply(this.element, params);
        },
        dialog: function() {
            var args = [];
            for ( var i = 0; i < arguments.length; i++ ) {
                args.push( arguments[ i ] );
            }
            var params =  ['dialog'].concat( args );
            return this.element.datepicker.apply(this.element, params);
        },
        isDisabled: function() {
            return this.element.datepicker('isDisabled');
        },
        getDate: function() {
            return this.element.datepicker('getDate');
        },
        setDate: function() {
            var args = [];
            for ( var i = 0; i < arguments.length; i++ ) {
                args.push( arguments[ i ] );
            }
            var params =  ['setDate'].concat( args );
            return this.element.datepicker.apply(this.element, params);
        },

        ncbionclose : function( fnc ){
            this.options.ncbionclose = fnc;
        },

        _onClose : function(dateText, inst){ 
            var fncOnClose = this._getOption("ncbionclose"); 
            if( fncOnClose ){
                fncOnClose(dateText, inst);
            }
            this.element.trigger("ncbionclose", { dateText : dateText, inst : inst } );
        },


        ncbionselect : function( fnc ){
            this.options.ncbionselect = fnc;
        },

        _onSelect : function(dateText, inst){ 
            var fncOnClose = this._getData("ncbionselect"); 
            if( fncOnClose ){
                fncOnClose(dateText, inst);
            }
            this.element.trigger("ncbionselect", { dateText : dateText, inst : inst } );
        }

    }); // end widget def

})(jQuery);
/*
 * jQuery UI Dialog 1.8
 *
 * Copyright (c) 2010 AUTHORS.txt (http://jqueryui.com/about)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * http://docs.jquery.com/UI/Dialog
 *
 * Depends:
 *	jquery.ui.core.js
 *	jquery.ui.widget.js
 *  jquery.ui.button.js
 *	jquery.ui.draggable.js
 *	jquery.ui.mouse.js
 *	jquery.ui.position.js
 *	jquery.ui.resizable.js
 */
(function($) {

var uiDialogClasses =
	'ui-dialog ' +
	'ui-widget ' +
	'ui-widget-content ' +
	'ui-corner-all ';

$.widget("ui.dialog", {
	options: {
		autoOpen: true,
		buttons: {},
		closeOnEscape: true,
		closeText: 'close',
		dialogClass: '',
		draggable: true,
		hide: null,
		height: 'auto',
		maxHeight: false,
		maxWidth: false,
		minHeight: 150,
		minWidth: 150,
		modal: false,
		position: 'center',
		resizable: true,
		show: null,
		stack: true,
		title: '',
		width: 300,
		zIndex: 1000
	},
	_create: function() {
		this.originalTitle = this.element.attr('title');

		var self = this,
			options = self.options,

			title = options.title || self.originalTitle || '&#160;',
			titleId = $.ui.dialog.getTitleId(self.element),

			uiDialog = (self.uiDialog = $('<div></div>'))
				.appendTo(document.body)
				.hide()
				.addClass(uiDialogClasses + options.dialogClass)
				.css({
					zIndex: options.zIndex
				})
				// setting tabIndex makes the div focusable
				// setting outline to 0 prevents a border on focus in Mozilla
				.attr('tabIndex', -1).css('outline', 0).keydown(function(event) {
					if (options.closeOnEscape && event.keyCode &&
						event.keyCode === $.ui.keyCode.ESCAPE) {
						
						self.close(event);
						event.preventDefault();
					}
				})
				.attr({
					role: 'dialog',
					'aria-labelledby': titleId
				})
				.mousedown(function(event) {
					self.moveToTop(false, event);
				}),

			uiDialogContent = self.element
				.show()
				.removeAttr('title')
				.addClass(
					'ui-dialog-content ' +
					'ui-widget-content')
				.appendTo(uiDialog),

			uiDialogTitlebar = (self.uiDialogTitlebar = $('<div></div>'))
				.addClass(
					'ui-dialog-titlebar ' +
					'ui-widget-header ' +
					'ui-corner-all ' +
					'ui-helper-clearfix'
				)
				.prependTo(uiDialog),

			uiDialogTitlebarClose = $('<a href="#"></a>')
				.addClass(
					'ui-dialog-titlebar-close ' +
					'ui-corner-all'
				)
				.attr('role', 'button')
				.hover(
					function() {
						uiDialogTitlebarClose.addClass('ui-state-hover');
					},
					function() {
						uiDialogTitlebarClose.removeClass('ui-state-hover');
					}
				)
				.focus(function() {
					uiDialogTitlebarClose.addClass('ui-state-focus');
				})
				.blur(function() {
					uiDialogTitlebarClose.removeClass('ui-state-focus');
				})
				.click(function(event) {
					self.close(event);
					return false;
				})
				.appendTo(uiDialogTitlebar),

			uiDialogTitlebarCloseText = (self.uiDialogTitlebarCloseText = $('<span></span>'))
				.addClass(
					'ui-icon ' +
					'ui-icon-closethick'
				)
				.text(options.closeText)
				.appendTo(uiDialogTitlebarClose),

			uiDialogTitle = $('<span></span>')
				.addClass('ui-dialog-title')
				.attr('id', titleId)
				.html(title)
				.prependTo(uiDialogTitlebar);

		//handling of deprecated beforeclose (vs beforeClose) option
		//Ticket #4669 http://dev.jqueryui.com/ticket/4669
		//TODO: remove in 1.9pre
		if ($.isFunction(options.beforeclose) && !$.isFunction(options.beforeClose)) {
			options.beforeClose = options.beforeclose;
		}

		uiDialogTitlebar.find("*").add(uiDialogTitlebar).disableSelection();

		if (options.draggable && $.fn.draggable) {
			self._makeDraggable();
		}
		if (options.resizable && $.fn.resizable) {
			self._makeResizable();
		}

		self._createButtons(options.buttons);
		self._isOpen = false;

		if ($.fn.bgiframe) {
			uiDialog.bgiframe();
		}
	},
	_init: function() {
		if ( this.options.autoOpen ) {
			this.open();
		}
	},

	destroy: function() {
		var self = this;
		
		if (self.overlay) {
			self.overlay.destroy();
		}
		self.uiDialog.hide();
		self.element
			.unbind('.dialog')
			.removeData('dialog')
			.removeClass('ui-dialog-content ui-widget-content')
			.hide().appendTo('body');
		self.uiDialog.remove();

		if (self.originalTitle) {
			self.element.attr('title', self.originalTitle);
		}

		return self;
	},
	
	widget: function() {
		return this.uiDialog;
	},

	close: function(event) {
		var self = this,
			maxZ;
		
		if (false === self._trigger('beforeClose', event)) {
			return;
		}

		if (self.overlay) {
			self.overlay.destroy();
		}
		self.uiDialog.unbind('keypress.ui-dialog');

		self._isOpen = false;

		if (self.options.hide) {
			self.uiDialog.hide(self.options.hide, function() {
				self._trigger('close', event);
			});
		} else {
			self.uiDialog.hide();
			self._trigger('close', event);
		}

		$.ui.dialog.overlay.resize();

		// adjust the maxZ to allow other modal dialogs to continue to work (see #4309)
		if (self.options.modal) {
			maxZ = 0;
			$('.ui-dialog').each(function() {
				if (this !== self.uiDialog[0]) {
					maxZ = Math.max(maxZ, $(this).css('z-index'));
				}
			});
			$.ui.dialog.maxZ = maxZ;
		}

		return self;
	},

	isOpen: function() {
		return this._isOpen;
	},

	// the force parameter allows us to move modal dialogs to their correct
	// position on open
	moveToTop: function(force, event) {
		var self = this,
			options = self.options,
			saveScroll;
		
		if ((options.modal && !force) ||
			(!options.stack && !options.modal)) {
			return self._trigger('focus', event);
		}
		
		if (options.zIndex > $.ui.dialog.maxZ) {
			$.ui.dialog.maxZ = options.zIndex;
		}
		if (self.overlay) {
			$.ui.dialog.maxZ += 1;
			self.overlay.$el.css('z-index', $.ui.dialog.overlay.maxZ = $.ui.dialog.maxZ);
		}

		//Save and then restore scroll since Opera 9.5+ resets when parent z-Index is changed.
		//  http://ui.jquery.com/bugs/ticket/3193
		saveScroll = { scrollTop: self.element.attr('scrollTop'), scrollLeft: self.element.attr('scrollLeft') };
		$.ui.dialog.maxZ += 1;
		self.uiDialog.css('z-index', $.ui.dialog.maxZ);
		self.element.attr(saveScroll);
		self._trigger('focus', event);

		return self;
	},

	open: function() {
		if (this._isOpen) { return; }

		var self = this,
			options = self.options,
			uiDialog = self.uiDialog;

		self.overlay = options.modal ? new $.ui.dialog.overlay(self) : null;
		if (uiDialog.next().length) {
			uiDialog.appendTo('body');
		}
		self._size();
		self._position(options.position);
		uiDialog.show(options.show);
		self.moveToTop(true);

		// prevent tabbing out of modal dialogs
		if (options.modal) {
			uiDialog.bind('keypress.ui-dialog', function(event) {
				if (event.keyCode !== $.ui.keyCode.TAB) {
					return;
				}
	
				var tabbables = $(':tabbable', this),
					first = tabbables.filter(':first'),
					last  = tabbables.filter(':last');
	
				if (event.target === last[0] && !event.shiftKey) {
					first.focus(1);
					return false;
				} else if (event.target === first[0] && event.shiftKey) {
					last.focus(1);
					return false;
				}
			});
		}

		// set focus to the first tabbable element in the content area or the first button
		// if there are no tabbable elements, set focus on the dialog itself
		$([])
			.add(uiDialog.find('.ui-dialog-content :tabbable:first'))
			.add(uiDialog.find('.ui-dialog-buttonpane :tabbable:first'))
			.add(uiDialog)
			.filter(':first')
			.focus();

		self._trigger('open');
		self._isOpen = true;

		return self;
	},

	_createButtons: function(buttons) {
		var self = this,
			hasButtons = false,
			uiDialogButtonPane = $('<div></div>')
				.addClass(
					'ui-dialog-buttonpane ' +
					'ui-widget-content ' +
					'ui-helper-clearfix'
				);

		// if we already have a button pane, remove it
		self.uiDialog.find('.ui-dialog-buttonpane').remove();

		if (typeof buttons === 'object' && buttons !== null) {
			$.each(buttons, function() {
				return !(hasButtons = true);
			});
		}
		if (hasButtons) {
			$.each(buttons, function(name, fn) {
				var button = $('<button type="button"></button>')
					.text(name)
					.click(function() { fn.apply(self.element[0], arguments); })
					.appendTo(uiDialogButtonPane);
				if ($.fn.button) {
					button.button();
				}
			});
			uiDialogButtonPane.appendTo(self.uiDialog);
		}
	},

	_makeDraggable: function() {
		var self = this,
			options = self.options,
			doc = $(document),
			heightBeforeDrag;

		function filteredUi(ui) {
			return {
				position: ui.position,
				offset: ui.offset
			};
		}

		self.uiDialog.draggable({
			cancel: '.ui-dialog-content, .ui-dialog-titlebar-close',
			handle: '.ui-dialog-titlebar',
			containment: 'document',
			start: function(event, ui) {
				heightBeforeDrag = options.height === "auto" ? "auto" : $(this).height();
				$(this).height($(this).height()).addClass("ui-dialog-dragging");
				self._trigger('dragStart', event, filteredUi(ui));
			},
			drag: function(event, ui) {
				self._trigger('drag', event, filteredUi(ui));
			},
			stop: function(event, ui) {
				options.position = [ui.position.left - doc.scrollLeft(),
					ui.position.top - doc.scrollTop()];
				$(this).removeClass("ui-dialog-dragging").height(heightBeforeDrag);
				self._trigger('dragStop', event, filteredUi(ui));
				$.ui.dialog.overlay.resize();
			}
		});
	},

	_makeResizable: function(handles) {
		handles = (handles === undefined ? this.options.resizable : handles);
		var self = this,
			options = self.options,
			// .ui-resizable has position: relative defined in the stylesheet
			// but dialogs have to use absolute or fixed positioning
			position = self.uiDialog.css('position'),
			resizeHandles = (typeof handles === 'string' ?
				handles	:
				'n,e,s,w,se,sw,ne,nw'
			);

		function filteredUi(ui) {
			return {
				originalPosition: ui.originalPosition,
				originalSize: ui.originalSize,
				position: ui.position,
				size: ui.size
			};
		}

		self.uiDialog.resizable({
			cancel: '.ui-dialog-content',
			containment: 'document',
			alsoResize: self.element,
			maxWidth: options.maxWidth,
			maxHeight: options.maxHeight,
			minWidth: options.minWidth,
			minHeight: self._minHeight(),
			handles: resizeHandles,
			start: function(event, ui) {
				$(this).addClass("ui-dialog-resizing");
				self._trigger('resizeStart', event, filteredUi(ui));
			},
			resize: function(event, ui) {
				self._trigger('resize', event, filteredUi(ui));
			},
			stop: function(event, ui) {
				$(this).removeClass("ui-dialog-resizing");
				options.height = $(this).height();
				options.width = $(this).width();
				self._trigger('resizeStop', event, filteredUi(ui));
				$.ui.dialog.overlay.resize();
			}
		})
		.css('position', position)
		.find('.ui-resizable-se').addClass('ui-icon ui-icon-grip-diagonal-se');
	},

	_minHeight: function() {
		var options = this.options;

		if (options.height === 'auto') {
			return options.minHeight;
		} else {
			return Math.min(options.minHeight, options.height);
		}
	},

	_position: function(position) {
		var myAt = [],
			offset = [0, 0],
			isVisible;

		position = position || $.ui.dialog.prototype.options.position;

		// deep extending converts arrays to objects in jQuery <= 1.3.2 :-(
//		if (typeof position == 'string' || $.isArray(position)) {
//			myAt = $.isArray(position) ? position : position.split(' ');

		if (typeof position === 'string' || (typeof position === 'object' && '0' in position)) {
			myAt = position.split ? position.split(' ') : [position[0], position[1]];
			if (myAt.length === 1) {
				myAt[1] = myAt[0];
			}

			$.each(['left', 'top'], function(i, offsetPosition) {
				if (+myAt[i] === myAt[i]) {
					offset[i] = myAt[i];
					myAt[i] = offsetPosition;
				}
			});
		} else if (typeof position === 'object') {
			if ('left' in position) {
				myAt[0] = 'left';
				offset[0] = position.left;
			} else if ('right' in position) {
				myAt[0] = 'right';
				offset[0] = -position.right;
			}

			if ('top' in position) {
				myAt[1] = 'top';
				offset[1] = position.top;
			} else if ('bottom' in position) {
				myAt[1] = 'bottom';
				offset[1] = -position.bottom;
			}
		}

		// need to show the dialog to get the actual offset in the position plugin
		isVisible = this.uiDialog.is(':visible');
		if (!isVisible) {
			this.uiDialog.show();
		}
		this.uiDialog
			// workaround for jQuery bug #5781 http://dev.jquery.com/ticket/5781
			.css({ top: 0, left: 0 })
			.position({
				my: myAt.join(' '),
				at: myAt.join(' '),
				offset: offset.join(' '),
				of: window,
				collision: 'fit',
				// ensure that the titlebar is never outside the document
				using: function(pos) {
					var topOffset = $(this).css(pos).offset().top;
					if (topOffset < 0) {
						$(this).css('top', pos.top - topOffset);
					}
				}
			});
		if (!isVisible) {
			this.uiDialog.hide();
		}
	},

	_setOption: function(key, value){
		var self = this,
			uiDialog = self.uiDialog,
			isResizable = uiDialog.is(':data(resizable)'),
			resize = false;
		
		switch (key) {
			//handling of deprecated beforeclose (vs beforeClose) option
			//Ticket #4669 http://dev.jqueryui.com/ticket/4669
			//TODO: remove in 1.9pre
			case "beforeclose":
				key = "beforeClose";
				break;
			case "buttons":
				self._createButtons(value);
				break;
			case "closeText":
				// convert whatever was passed in to a string, for text() to not throw up
				self.uiDialogTitlebarCloseText.text("" + value);
				break;
			case "dialogClass":
				uiDialog
					.removeClass(self.options.dialogClass)
					.addClass(uiDialogClasses + value);
				break;
			case "disabled":
				if (value) {
					uiDialog.addClass('ui-dialog-disabled');
				} else {
					uiDialog.removeClass('ui-dialog-disabled');
				}
				break;
			case "draggable":
				if (value) {
					self._makeDraggable();
				} else {
					uiDialog.draggable('destroy');
				}
				break;
			case "height":
				resize = true;
				break;
			case "maxHeight":
				if (isResizable) {
					uiDialog.resizable('option', 'maxHeight', value);
				}
				resize = true;
				break;
			case "maxWidth":
				if (isResizable) {
					uiDialog.resizable('option', 'maxWidth', value);
				}
				resize = true;
				break;
			case "minHeight":
				if (isResizable) {
					uiDialog.resizable('option', 'minHeight', value);
				}
				resize = true;
				break;
			case "minWidth":
				if (isResizable) {
					uiDialog.resizable('option', 'minWidth', value);
				}
				resize = true;
				break;
			case "position":
				self._position(value);
				break;
			case "resizable":
				// currently resizable, becoming non-resizable
				if (isResizable && !value) {
					uiDialog.resizable('destroy');
				}

				// currently resizable, changing handles
				if (isResizable && typeof value === 'string') {
					uiDialog.resizable('option', 'handles', value);
				}

				// currently non-resizable, becoming resizable
				if (!isResizable && value !== false) {
					self._makeResizable(value);
				}
				break;
			case "title":
				// convert whatever was passed in o a string, for html() to not throw up
				$(".ui-dialog-title", self.uiDialogTitlebar).html("" + (value || '&#160;'));
				break;
			case "width":
				resize = true;
				break;
		}

		$.Widget.prototype._setOption.apply(self, arguments);
		if (resize) {
			self._size();
		}
	},

	_size: function() {
		/* If the user has resized the dialog, the .ui-dialog and .ui-dialog-content
		 * divs will both have width and height set, so we need to reset them
		 */
		var options = this.options,
			nonContentHeight;

		// reset content sizing
		// hide for non content measurement because height: 0 doesn't work in IE quirks mode (see #4350)
		this.element.css('width', 'auto')
			.hide();

		// reset wrapper sizing
		// determine the height of all the non-content elements
		nonContentHeight = this.uiDialog.css({
				height: 'auto',
				width: options.width
			})
			.height();

		this.element
			.css(options.height === 'auto' ? {
					minHeight: Math.max(options.minHeight - nonContentHeight, 0),
					height: 'auto'
				} : {
					minHeight: 0,
					height: Math.max(options.height - nonContentHeight, 0)				
			})
			.show();

		if (this.uiDialog.is(':data(resizable)')) {
			this.uiDialog.resizable('option', 'minHeight', this._minHeight());
		}
	}
});

$.extend($.ui.dialog, {
	version: "1.8",

	uuid: 0,
	maxZ: 0,

	getTitleId: function($el) {
		var id = $el.attr('id');
		if (!id) {
			this.uuid += 1;
			id = this.uuid;
		}
		return 'ui-dialog-title-' + id;
	},

	overlay: function(dialog) {
		this.$el = $.ui.dialog.overlay.create(dialog);
	}
});

$.extend($.ui.dialog.overlay, {
	instances: [],
	// reuse old instances due to IE memory leak with alpha transparency (see #5185)
	oldInstances: [],
	maxZ: 0,
	events: $.map('focus,mousedown,mouseup,keydown,keypress,click'.split(','),
		function(event) { return event + '.dialog-overlay'; }).join(' '),
	create: function(dialog) {
		if (this.instances.length === 0) {
			// prevent use of anchors and inputs
			// we use a setTimeout in case the overlay is created from an
			// event that we're going to be cancelling (see #2804)
			setTimeout(function() {
				// handle $(el).dialog().dialog('close') (see #4065)
				if ($.ui.dialog.overlay.instances.length) {
					$(document).bind($.ui.dialog.overlay.events, function(event) {
						// stop events if the z-index of the target is < the z-index of the overlay
						return ($(event.target).zIndex() >= $.ui.dialog.overlay.maxZ);
					});
				}
			}, 1);

			// allow closing by pressing the escape key
			$(document).bind('keydown.dialog-overlay', function(event) {
				if (dialog.options.closeOnEscape && event.keyCode &&
					event.keyCode === $.ui.keyCode.ESCAPE) {
					
					dialog.close(event);
					event.preventDefault();
				}
			});

			// handle window resize
			$(window).bind('resize.dialog-overlay', $.ui.dialog.overlay.resize);
		}

		var $el = (this.oldInstances.pop() || $('<div></div>').addClass('ui-widget-overlay'))
			.appendTo(document.body)
			.css({
				width: this.width(),
				height: this.height()
			});

		if ($.fn.bgiframe) {
			$el.bgiframe();
		}

		this.instances.push($el);
		return $el;
	},

	destroy: function($el) {
		this.oldInstances.push(this.instances.splice($.inArray($el, this.instances), 1)[0]);

		if (this.instances.length === 0) {
			$([document, window]).unbind('.dialog-overlay');
		}

		$el.remove();
		
		// adjust the maxZ to allow other modal dialogs to continue to work (see #4309)
		var maxZ = 0;
		$.each(this.instances, function() {
			maxZ = Math.max(maxZ, this.css('z-index'));
		});
		this.maxZ = maxZ;
	},

	height: function() {
		var scrollHeight,
			offsetHeight;
		// handle IE 6
		if ($.browser.msie && $.browser.version < 7) {
			scrollHeight = Math.max(
				document.documentElement.scrollHeight,
				document.body.scrollHeight
			);
			offsetHeight = Math.max(
				document.documentElement.offsetHeight,
				document.body.offsetHeight
			);

			if (scrollHeight < offsetHeight) {
				return $(window).height() + 'px';
			} else {
				return scrollHeight + 'px';
			}
		// handle "good" browsers
		} else {
			return $(document).height() + 'px';
		}
	},

	width: function() {
		var scrollWidth,
			offsetWidth;
		// handle IE 6
		if ($.browser.msie && $.browser.version < 7) {
			scrollWidth = Math.max(
				document.documentElement.scrollWidth,
				document.body.scrollWidth
			);
			offsetWidth = Math.max(
				document.documentElement.offsetWidth,
				document.body.offsetWidth
			);

			if (scrollWidth < offsetWidth) {
				return $(window).width() + 'px';
			} else {
				return scrollWidth + 'px';
			}
		// handle "good" browsers
		} else {
			return $(document).width() + 'px';
		}
	},

	resize: function() {
		/* If the dialog is draggable and the user drags it past the
		 * right edge of the window, the document becomes wider so we
		 * need to stretch the overlay. If the user then drags the
		 * dialog back to the left, the document will become narrower,
		 * so we need to shrink the overlay to the appropriate size.
		 * This is handled by shrinking the overlay before setting it
		 * to the full document size.
		 */
		var $overlays = $([]);
		$.each($.ui.dialog.overlay.instances, function() {
			$overlays = $overlays.add(this);
		});

		$overlays.css({
			width: 0,
			height: 0
		}).css({
			width: $.ui.dialog.overlay.width(),
			height: $.ui.dialog.overlay.height()
		});
	}
});

$.extend($.ui.dialog.overlay.prototype, {
	destroy: function() {
		$.ui.dialog.overlay.destroy(this.$el);
	}
});

}(jQuery));

/* 
This is not meant to stand-alone, it is a wrapper to ui.dialog and makes ui.dialog respond to events from triggers
*/


(function($) {


    $.widget("ui.ncbidialog", $.ui.dialog, {
        // copy all functions from dialog to here
        options: {
            autoOpen: false,
            draggable: false,
            isTitleVisible: true,
            open: null,
            openEvent: 'click',
            resizable: false,
            destSelector: null
        },
        
        _create: function() { 
            // get associated dialogNode(s) from trigger node, then make dialog node "this.element"
            this._triggerNode = this.element;

            // remember if dialog node's content should be gotten from AJAX 
            // this must be set before this.getDialogNode is called
            this._ajaxable = this._ajaxable( this._triggerNode );

            var dialogNode = this.getDialogNode();
            this.element = dialogNode;
            
            // if no dialog console error if console exists but keep going...
            if ( dialogNode.length === 0 ) {
 
                if ( $.ui.jig._isConsole('error') ) {
                    console.error('ncbidialog: No matching dialog node found with selector "' + destSelector + '".');
                }
                return;
            }
            
            // TO DO: This should be handled in framework code in extend function
            this._parseCallbacks();
            this._overrideEvents();

            dialogNode.dialog( this.options );

            if ( !this.options.isTitleVisible ) {
                dialogNode.
                    prev( '.ui-dialog-titlebar' ).
                    addClass( 'ui-helper-hidden-accessible' ).
                    css('position', 'absolute' ); // override framework class
            }

            this._renderButtonMarkup();

            // attach events on trigger to open dialog
            var that = this;

            this._triggerNode[ this.options.openEvent ]( function( e ) {
            //this._triggerNode[ this._getData('openEvent') ]( function( e ) {
                e.preventDefault();
                var trigger = $( e.target );
                if ( that._ajaxable ) {
                    that._getContent();
                } 
                that.element.dialog('open'); 
            });

        },
        _overrideEvents: function() {
            /*** 
            Listen for dialog's events, then fire ncbidialog's events
            TO DO: This fnc is temporary. This functionality should happen from extending the widget
            ***/
            var that = this;
            var trigNode = this._triggerNode;
            var dialogEventOptions = [ 'dialogopen', 'dialogbeforeclose', 'dialogfocus', 'dialogdragstart', 'dialogdrag', 'dialogdragstop', 'dialogresizestart', 'dialogresize', 'dialogresizestop', 'dialogclose' ]
            
            for ( var i = 0; i < dialogEventOptions.length; i++ ) {
                var eventOption = dialogEventOptions[ i ];
                (function( eventOption ) {
                    that.element.bind( eventOption, function() {
                        trigNode.trigger( 'ncbi' + eventOption );
                    });
                })( eventOption );
            }
            /*
            dialogNode.bind('dialogopen', function() {
                dialogNode.trigger('ncbidialogopen');
            });
            */

        },
        _parseCallbacks: function() {
            // TO DO: this should be done by framework and should be handled by extend function
            var callbackOpts = [ 'open', 'beforeClose', 'focus', 'dragStart', 'drag', 'dragStop', 'resizeStart', 'resize', 'resizeStop', 'close' ];
            var getFncFromStr = $.ui.jig._getFncFromStr;

            for ( var i = 0; i < callbackOpts.length; i++ ) {
                var callbackOpt = callbackOpts[ i ];
                var suppliedOpt = this.options[ callbackOpt ];
                if ( suppliedOpt ) {
                    var fn = getFncFromStr( suppliedOpt );
                    this.options[ callbackOpt ] = fn;
                } 
            }

        },
        getDialogNode: function() {
            // return the dialog node given the trigger, if it already has been returned at some point, retrieve from cache
            /*  return the dialog node given the trigger, if it already has been returned at some point since page load, get it from cache
                we try to get the dialog node from the href of the trigger. User may not 
                always want to use the href to point to destination node, esp. if there are > 1 dest nodes/per trigger
                but in most cases there are not
            */
            if ( typeof this._dialogNode === 'undefined' ) {
                var href = this._triggerNode.attr( 'href' );
                if ( href && href !== '' && ! this._ajaxable ) { // try to get dest from href
                    this._dialogNode = $( $.trim( href ) );
                } else { // otherwise get it from config
                    this._dialogNode = $( this.options.destSelector ); 
                }
            }
            
            return this._dialogNode;
        },
        _renderButtonMarkup: function() {
            // default ui.dialog forces user to define buttons as an option. This is messy
            // especially when defining handlers in markup using jig
            // this function finds buttons that user marks up and makes them look like dialog buttons
            this._jigButtons = this.element.
                find('button,input[type=submit]').
                addClass('ui-state-default ui-corner-all');

            if ( this._jigButtons.length === 0 ) { return; }

            this._jigButtons.wrapAll('<div class="ui-dialog-buttonpane ui-widget-content ui-helper-clearfix"></div>')
            this._doButtonHover();

        },
        _doButtonHover: function() {
            // only called if there are user-defined buttons in markup
            this._jigButtons.hover( function() {
                var b = $( this );
                if ( b.hasClass('ui-state-hover' ) ) {
                    b.removeClass('ui-state-hover');
                } else {
                    b.addClass('ui-state-hover');
                }
            });
            
        },
        _ajaxable: function( trigger ) {
  
            this._href = $.trim( trigger.attr('href') );
            if ( this._href.charAt( 0 ) !== '#' && this._href !== '' ) {
                return true;
            } else {
                return false;
            }     
            
        },
        close: function() {
            return this.element.dialog.apply( this.element, ['close'] );

        },
        open: function() {
            return this.element.dialog.apply( this.element, ['open'] );

        },
        isOpen: function() {
            return this.element.dialog.apply( this.element, ['isOpen'] );
        },
        widget: function() {
            return this.element.dialog.apply( this.element, ['widget'] );

        },
        moveToTop: function() {
            var d = this.getDialogNode();
            d.dialog('moveToTop');
        },
        option: function() {
            var params = ['option'];
            for ( var i = 0; i < arguments.length; i++ ) {
                params.push( arguments[ i ] );
            }            
      
            return this.element.dialog.apply( this.element, params );

        },
        enable: function() {
            return this.element.dialog.apply( this.element, ['enable'] );
        },
        disable: function() {
            return this.element.dialog.apply( this.element, ['disable'] );

        },
        destroy: function() {
            return this.element.dialog.apply( this.element, ['destroy'] );

        },

        _getContent: function( ) {
            // gets content of dialog via ajax call
            this.element.load( this._href );
            
        }

    });

  


})(jQuery);

