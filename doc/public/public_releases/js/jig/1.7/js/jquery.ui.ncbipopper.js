
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
