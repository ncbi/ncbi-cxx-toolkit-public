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

