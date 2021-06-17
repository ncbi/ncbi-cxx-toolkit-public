
/**
* @file jquery.ui.ncbilinkedselects.js
* A jQuery UI widget to allow a chain of dependent select elements.
*
* @author cohenaa
*/
(function($) {

    $.widget("ui.ncbilinkedselects", {
        
        _selectEls: [], // stores collection of all selects, including this.element
        
        _paramObj: {}, // stores built up query string to be sent via AJAX request
        _localMap: null,

        options: {
            firstPopulatedFromDs: true,
            localData: null,
            localDataCallback: null,
            selects: null,
            webservice: null
        },
        
        _create: function() { 


            // store evaluated local data source for use later
            if( this.options.localData ) {
                this._localMap = eval( this.options.localData );
            }

            if( this.options.firstPopulatedFromDs ) {
                this._populateFirstSelect();
            }

            // collect the chain of select elements and save as a "static" variable
            // sets static property
            this._selectEls = this.getSelects();
            this._setEvents();

        },

        _populateFirstSelect: function() {
            if( this.options.localData ) {
                var strOrObjects = this._getFirstOptionsFromLocalDs();

                // values could be an array of strings or of name/value pairs
                // Check by checking the first index
                var isString = this._isArrayOfStrings( strOrObjects );                     
                for( var i =0; i < strOrObjects.length; i++ ) {
                    var strOrObj = strOrObjects[i];
                    if( isString ) {
                        
                        this.element.append( '<option value="' + strOrObj + '">' + strOrObj + '</option>' );
                    } else {
                        for( var string in strOrObj ) {
                            this.element.append( '<option value="' + strOrObj[string] + '">' + string + '</option>' );
                        }
                    }
                }
            } else { // populate from CGI, with empty parameters
                this._getAndAppendFromWebService( this.element, {} );                
            }   
        },

        _getFirstOptionsFromLocalDs: function() {
            var res = [];
            if( ! this.options.localDataCallback ) { // using default data structure
                for( var i = 0; i < this._localMap.length; i++ ) {
                    var opVal = this._localMap[i];
                    if ( typeof opVal === 'string' ) {
                        res.push( opVal );
                    } else if( $.isPlainObject( opVal )) { // if an object, it's name/val pairs
                        res.push(opVal);
                    }
                }
            } else {
                // if using a custom data source and a callback, call the cb with only the map parameter,
                // doing that, should return an array of the first options.
                res = this.options.localDataCallback.apply( this, [ this._localMap ] );
            }
        return res;
        },

        /**
        * Gets all the selects in the chain
        *  If config is set with jQuery selector, it gets those selects,
        * otherwise, by default, it gets "sibling" selects within a form
        * or fieldset.
        *
        * @return JQuery collection of selects
        */
        getSelects: function() {
            if ( this.options.selects ) {
                var restSelects = $( this.options.selects );
            } else {
                // if there's a fieldset, get all selects within that fieldset starting with this.element
                var ct = this.element.closest('fieldset, form');
                if( ct.length === 0 ) {
                    if( $.ui.jig._isConsole('warn') ) {
                        console.warn('jig warning: ncbilinkedselects. Your select elements must be within either a fieldset or a form element');
                    }
                }

                var selectsInCt = ct.find('select');
                var thisElSeen = false;
                var selects = [];
                for( var i = 0; i < selectsInCt.length; i++ ) {
                    var selectInCt = selectsInCt[i];
                    if ( selectInCt == this.element[0] ) {
                        thisElSeen = true;
                    }

                    // only add select if it's this.element or after
                    // and doesn't have a skip class on it.
                    if ( thisElSeen && !$( selectInCt ).hasClass('ui-ncbilinkedselects-ignore') ) {
                       selects.push( selectInCt ); 
                    }

                }
                return $(selects);



            }
            return this.element.add( restSelects ); 
        },

        /**
        * Private function that sets change events to selects
        */ 
        _setEvents: function() {
            
            var self = this;
            this._selectEls.each( function() {
                var select = $(this);
                var name = this.name;

                // Only attempt to populate next select, if this select is not the last one
                if ( ! self._isLastSelect( name ) ) {     
                    select.bind( 
                        'change', 
                        function( e ) { 
                            // reset all next selects in chain
                            self._resetNextSelects( name );
                            self._populateNext( e )  }
                    ); 
                    
                
                }
            });  
        },

        /**
        * Private fn, resets all selects that follow the select with the name supplied by parameter.
        */
        _resetNextSelects: function( name ) {
            var currSelectSeenYet = false;
            for( var i = 0; i < this._selectEls.length; i++ ) {
                var sel = $( this._selectEls[ i ] );
                if (sel.attr('name') == name ) {
                    currSelectSeenYet = true;
                    continue;
                }
                if ( currSelectSeenYet ) {
                    var options = sel.children('option');
                    options.each( function() {
                        var op = $( this );
                        if( ! op.hasClass('ui-ncbilinkedselects-perm') ) {
                            op.remove();
                        }
                    });

                }
            }  
        },

        /**
        * Private fn to dynamically construct param obj for ajax request. It
        * has to remember all parameters down the chain
        */
        _getParamObj: function( param, val ) {
            // if this is the first select, reset parameters
            if ( this._isFirstSelect( param ) ) {
                this._paramObj = {};
            }
            this._paramObj[param] = val;
            return this._paramObj;
        },

        /**
        * Private function to populate next select in chain
        */
        _populateNext: function( e ) {
            var currentSelect = $( e.target );
            var optionClicked = currentSelect.children('option:selected');

            // no need to get next select values if we are clicking on a select that 
            // is "perm"
            if( ! optionClicked.hasClass('ui-ncbilinkedselects-perm' ) ) {
                var param = currentSelect.attr('name' );
                var val = currentSelect.val();
                var nextSelect = this._getNextSelect( currentSelect.attr('name') );
    
                
                // Here we need to fork based on whether we are getting remove or local data
                // @todo: we should validate whether webservice exists?
                if ( this.options.webservice ) {
                    // Dyanamically construct the param object we will pass to $.get
                    // the param object needs to pick up all previous parameters, not just the current one.
                    var paramObj = this._getParamObj(param, val );
        
                    // AJAX request for next select down the chain
                    this._getAndAppendFromWebService( nextSelect, paramObj );
    
    
                }
                else if( this.options.localData ) {
                    // @todo invalid JSON
    
                    // if a change callback option is defined, use that to parse the data
                   
                    this._populateSelectFromLocalMap( currentSelect, optionClicked, nextSelect );
                    
                }
                else {
                    // @todo: what if we have neither webservice or localData set? There should be a test case for this.
                }
            }
        },

        /**
        * Appends option markup from a get request to webservice
        * 
        * @param select: jQuery select element
        * @paramObj: Object literal representing query parameters
        */
        _getAndAppendFromWebService: function( select, paramObj ) {
            var self = this;
            $.get( this.options.webservice, paramObj, function( a, status, xmlhttpObj ) {
                response = xmlhttpObj.responseText;
                select.append( response );
            }, 'HTML' ); 
        },

        /**
        * Given a value, returns array its options
        * @param key String The key to search the localdata source for
        * @return Returns Array of options
        */
        queryLocalService: function( name ) {
            var res = [];
            // Recursive helper fn to recurse data structure
            function getIndex( arr ) {
   
                for( var  i in arr ) {
                    var value = arr[ i ];
                    /*
                    if ( isStringMap  && value == name ) {
                        
                        console.info('regular string, and matches');
                        //console.warn('got index of value we are interested in');
                        // got index of value we are interested in
                        //console.info( i );
                        // got index of item we are interested in, its index is + 1
                        var j = parseInt( i );
                        ob = arr[ j + 1 ];
        
                        for ( var k = 0; k < ob.length; k++ ) {
                            if( ! jQuery.isArray( ob[k] ) ) {
                                res.push( ob[k] );
                            }
                        }
                        return;
                    } else { // name/values
                    */
                        
                        // get the object with the String/name passed
                        for( var p in value ) {
 
                            if( value[p] === name ) {
                                // next object in array should be object we're interested in
                                var arrObjs = arr[ parseInt(i) + 1 ];
                                for( var l = 0; l < arrObjs.length; l++ ) {
                                    var arrOb = arrObjs[l];
                                    if( ! $.isArray( arrOb ) ) {
                                        res.push( arrObjs[l] );
                                    }
                                }
                                
                                
                                return;
                            }
                        }
                    //}

                    if( jQuery.isArray( value) )  {
                        getIndex( value );
                    } 
                }
       
            } // end helper function
        
            // Start of the called get function
            // Seed recursion, getting index of passed name parameter
            var isStringMap;
            if( typeof this._localMap[0] === 'string' ) {
               isStringMap = true; 
            } else {
                isStringMap = false;
            }
            getIndex( this._localMap );
            return res;
        },
        
        /**
        * Private function that parses JSON data object, and populates next select
        * @param currentSelectVal, string the value of the current select
        * @param nextSelect jQuery object, the  select to populate, the next select to the select named by name.
        * @param e: Object. The event object
        * 
        */
        _populateSelectFromLocalMap: function( currentSelect, optionClicked, nextSelect) {
            var options;
            var cb = this.options.localDataCallback;
            if ( cb ) {
                options = cb.apply( this, [ this._localMap, currentSelect, optionClicked, this._selectEls  ]);
            } else {
                options = this.queryLocalService( currentSelect.val() );
            }

            // options could be an array of strings or array of name-value pairs
            var arrayOfStrings = this._isArrayOfStrings( options );
            for( var i = 0; i < options.length; i++ ) {
                var op = options[i];
                if( arrayOfStrings ) {
                    nextSelect.append( '<option value="' + op + '">' + op + '</option>' ); 
                } else {
                    var string
                    var value;

                    for( var p in op ) {
                      string = p;
                      value = op[p];
                    }
                     nextSelect.append( '<option value="' + value + '">' + string + '</option>' );

                }
            }
        },

        _isArrayOfStrings: function( arr ) {
            return typeof arr[ 0 ]  === 'string'; 
        },
    
        /**
        *   Private function. Returns boolean. True if name passed is last select in chain.
        */
        _isLastSelect: function( name ) {
            els = this._selectEls;
            var lastSelect = els[ els.length -1 ];   
            if ( name === lastSelect.name ) {
                return true;
            } else {
                return false;
            }
        },

        _isFirstSelect: function( name ) {
            els = this._selectEls;
            var firstSelect = els[ 0 ];   
            if ( name === firstSelect.name ) {
                return true;
            } else {
                return false;
            }
        },
        
        /**
        * Private fn. Gets the next select in the chain, given a name attribute of a select
        */
        _getNextSelect: function( name ) {
            for( var i = 0; i < this._selectEls.length; i++ ) {
                var sel = this._selectEls[ i ];
                if (sel.name == name ) {
                    return $( this._selectEls[ i + 1 ] );
                }
            }  
        }

    });



})(jQuery);
