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

