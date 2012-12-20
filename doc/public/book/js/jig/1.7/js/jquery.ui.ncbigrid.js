
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
