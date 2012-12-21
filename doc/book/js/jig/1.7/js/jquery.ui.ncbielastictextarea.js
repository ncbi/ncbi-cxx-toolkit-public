/* 
ui.widgetName: ncbielastictextarea

shamelesly based on the Elastic code writen by:
@author         Jan Jarfalk
@author-website http://www.unwrongest.com
@license        MIT License - http://www.opensource.org/licenses/mit-license.php

TODO:
  write tests
*/

(function($) {
  $.widget("ui.ncbielastictextarea", { 
    // global dict containing the styles that get copied from the textarea to
    // the hidden div
    styles: {
      copy: [ 'paddingTop', 'paddingRight', 'paddingBottom', 'paddingLeft',
				        'fontSize', 'lineHeight', 'fontFamily', 'width','fontWeight']
    },

    _create: function() {
      var textarea = this.element;
      if ( textarea.attr('type') != 'textarea' ) {
        return false;
      }

      // generate a uid so that we can remove events and textareas without
      // affetcing similar widgets on the same page
      this.elastic_uid = $.ui.jig._generateId("ncbielastictextarea");

      //set word wrapping on the visible text area
      textarea.css('word-wrap','break-word');

      var twin        = $('<div />').css({
                                          'position':'absolute',
                                          'display':'none',
                                          'word-wrap':'break-word'
                                          }).attr('id',this.elastic_uid);
      var lineHeight  = parseInt(textarea.css('line-height'),10) || parseInt(textarea.css('font-size'),'10');
      var minHeight   = parseInt(textarea.css('height'),10) || (lineHeight * 3);
      var maxHeight	  = parseInt(textarea.css('max-height'),10) || Number.MAX_VALUE;
      var goalHeight  =	0;

      // Opera returns max-height of -1 if not set
      if (maxHeight < 0) { maxHeight = Number.MAX_VALUE; }

      // Append the twin to the DOM
      // We are going to meassure the height of this, not the textarea.
      twin.appendTo(textarea.parent());

      // copy the css styles from the text area to the div
      $(this.styles.copy).each( function() {
        twin.css(this.toString(),textarea.css(this.toString()));  
      });



      function setHeightAndOverflow(height, overflow){
        curratedHeight = Math.floor(parseInt(height,10));
        if(textarea.height() != curratedHeight){
          textarea.css({'height': curratedHeight + 'px','overflow':overflow});
        }
      }
      
      function _update(force) {
        // Get curated content from the textarea.
        var textAreaContent = textarea.val().replace(/&/g,'&amp;')
                                    .replace(/  /g, '&nbsp;')
                                    .replace(/<|>/g, '&gt;')
                                    .replace(/\n/g, '<br />');

        var twinContent = twin.html();
					
        if(textAreaContent+'&nbsp;' != twinContent || force == 'force'){
          // Add an extra white space so new rows are added when you are at the 
          // end of a row.
          twin.html(textAreaContent+'&nbsp;');
          // Change textarea height if twin plus the height of one line differs 
          // more than 3 pixel from textarea height
          if(Math.abs(twin.height()+lineHeight - textarea.height()) > 3){
            var goalHeight = twin.height()+lineHeight;
            if(goalHeight >= maxHeight) {
              setHeightAndOverflow(maxHeight,'auto');
            } 
            else if(goalHeight <= minHeight) {
              setHeightAndOverflow(minHeight,'hidden');
            } else {
              setHeightAndOverflow(goalHeight,'hidden');
            } 
          }
        }
      }

      // since the div is converted to a fixed width if specifed as a %,
      // have to resize when the window is resized.
      $(window).bind('resize.' + this.elastic_uid, function(){
        twin.css('width',textarea.width() + 'px');
        _update('force');
      });


      // Hide scrollbars
      textarea.css({'overflow':'hidden'});

      // set the hidden div width to match the visible one explicitely as this
      // fixes the problems when the text area doesn't have a fixed width
      twin.css('width',textarea.width() + 'px');
				
      // Update textarea size on keyup
      textarea.bind('keyup.' + this.elastic_uid, function(){ _update(); });

      // And this line is to catch the browser paste event
      textarea.bind('input.' + this.elastic_uid + ' ' + 'paste.' + this.elastic_uid ,
                    function(e){ setTimeout( _update, 250); });
      
      // fire the update, just incase the text is already longer than the text area
      _update();

    },

    destroy: function() {
      // remove events bound to the textarea
      this.element.unbind('.'+this.elastic_uid);

      // need to remove inserted div
       $('#'+ this.elastic_uid).remove();

      // and reset the textarea styles
      this.element.css({'word-wrap':'',
                         'overflow':'',
                         'height':''});
      // remove the resize event
      $(window).unbind('resize.' + this.elastic_uid);

      // then call default destroy
      $.Widget.prototype.destroy.apply(this, arguments);
    }

  }); // end widget def
    
  $.ui.ncbielastictextarea.prototype.options = {};

  $.ui.ncbielastictextarea.prototype.version = '1.2';
})(jQuery);

