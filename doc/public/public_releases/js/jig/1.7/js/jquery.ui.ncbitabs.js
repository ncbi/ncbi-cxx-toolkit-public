jQuery.widget("ui.ncbitabs", {
    _create: function() {
        // Delegate to the native tabs widget. 
        // Extend the native widget's options hash.
        
        if(this.element.hasClass("jig-tabs")){
            if(typeof console !== "undefined" && console.warn){
                console.warn("The classname widget identifier jig-tabs has been depreciated, please change it to jig-ncbitabs");
            }
        }
        
        var newObj = {};
        jQuery.extend(newObj, this.options, jQuery.ui.tabs.prototype.options );
        this.element.tabs( newObj );
    }
});
