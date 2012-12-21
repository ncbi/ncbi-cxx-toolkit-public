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
