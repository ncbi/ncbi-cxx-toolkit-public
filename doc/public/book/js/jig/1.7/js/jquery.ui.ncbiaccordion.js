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
