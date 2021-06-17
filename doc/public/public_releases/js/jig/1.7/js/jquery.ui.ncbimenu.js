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