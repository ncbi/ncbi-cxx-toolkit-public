    // jig-util.js
    // provides utility methods for all ui widgets intended for jig framework
    
    if ( typeof jQuery.ui.jig === 'undefined') {
        jQuery.ui.jig = {};
    } 
    
    jQuery.ui.jig._decodeJSON = function(contentAttr, nameAttr) {
    // decodes a string to a JSON object
        try {
            return eval('({' + contentAttr + '})');
        } catch(e) {
            if ( jQuery.ui.jig._isConsole('info')  ) {
                console.error("Error parsing parameter metatag: '" + nameAttr + "'. "  + e.message);
            }
        }
    };
    
    jQuery.ui.jig._setLocalConfig = function(element, options) {
        // gets config string from a $ element, decodes to JSON, extends options, returns options
        var configAttr = element.attr('config');
        if (configAttr) {
            var localConfig = jQuery.ui.jig._decodeJSON(configAttr);
            jQuery.extend(options, localConfig);
        }
        return options;
    };
    
    jQuery.ui.jig._generateId = function(strWidgetName, strModifier) {
        // generates a "unique id" for a jig widget. Can be used to generated class names too of course.
        // returns string
        // strWidgetName is the name of the widget, for example "ncbimenu".
        // for example,
        // $.ui.jig._generateId("ncbimenu"); // returns 'ui-ncbimenu-1'
        // use optional strModifier to further refine id. For example,
        // $.ui.jig._generateId("ncbimenu", "foo"); // returns 'ui-ncbimenu-foo-1'
        var strModifier = (typeof strModifier !== 'undefined') ? '-' + strModifier : '';
        arguments.callee.i++;
        var strGeneratedId = 'ui-' + strWidgetName + strModifier + '-' + arguments.callee.i;
    
        // check to make sure this id isn't already on page, if it is up the index #
        while (document.getElementById(strGeneratedId)){
            arguments.callee.i++;
            strGeneratedId = 'ui-' + strWidgetName + strModifier + arguments.callee.i;
        }
        return strGeneratedId;
        
    }
    jQuery.ui.jig._generateId.i = 0;

    /**
    * Returns whether a console object and one of its methods
    * is defined.
    *
    * @param method String. The method to check for.
    *
    * @return boolean Whether console or a method is defined.
    */
    jQuery.ui.jig._isConsole = function(method) {
        var consoleDefined = typeof window.console !== 'undefined' || false;
        if ( consoleDefined ) {
            var methodDefined = typeof window.console[ method ] !== 'undefined' || false;
        }
        if ( consoleDefined && methodDefined ) {
            return true;
        } else {
            return false;
        } 
    };
	
    jQuery.ui.jig._getFncFromStr = function( fnc ){	
        var newFunc = null;
        var typeOfFnc = typeof fnc;
        if(typeOfFnc === "function"){
            newFunc = fnc;
        }
        else if(typeOfFnc === "string" /*&& fnc.toLowerCase().indexOf("fnc:")===0*/ ){	
            var parts = fnc.replace(/^[^:]*:\s*/, "").replace(/\s*$/,"").split(".");            
            newFunc = window[ parts[0] ];
            if(newFunc){
                for(var i=1;i<parts.length;i++){               
                    newFunc = newFunc[ parts[i] ];
                    if(!newFunc){
                        newFunc = null;
                        break;
                    }
                }
            }
            else{
                newFunc = null;
            }
        }
        return newFunc;		
    };

    jQuery.ui.jig.registerPageHeightWatcher = function(){
        var body = jQuery(document.body);
        if( body.data("hasHeightWatcher") ){
            return;
        }
        body.data("_lastHeight", null);
        body.data("hasHeightWatcher", true);     
        body.data("heightWatcher", window.setInterval( function(){ jQuery.ui.jig.watchPageHeight(); }, 250) );
    };

    jQuery.ui.jig.watchPageHeight = function(){
        var body = jQuery(document.body);
        var currentHeight = body.height();
        var lastHeight = body.data("_lastHeight");
        if(lastHeight!==currentHeight){
            if(lastHeight){
                body.trigger("ncbijigpageheightchanged");
            }
            body.data("_lastHeight", currentHeight);
        }
    };
