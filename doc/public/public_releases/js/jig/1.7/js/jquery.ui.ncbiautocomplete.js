
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

