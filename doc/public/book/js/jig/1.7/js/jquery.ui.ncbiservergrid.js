
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

})();