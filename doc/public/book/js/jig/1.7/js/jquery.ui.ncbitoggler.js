
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


