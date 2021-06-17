/*
ui.widgetName: ncbislideshow

@crafted by     Jody Clements, clement2@ncbi.nlm.nih.gov

*/

(function($) {
  $.widget("ui.ncbislideshow", {
    options: {
      fadeSpeed: 500,
      slideInterval: 4000, //15000,
      currentSlide: 1,
      height: '117px',
      width: '410px', 
      randomStart: false
    },

    _create: function() {
      // add styles
      this.element.addClass('ui-ncbislideshow')
        .children('ul').addClass('ui-ncbislideshow-slides');
      // figure out how many slides there are
      this.options.slideCount = this.element.find('ul.ui-ncbislideshow-slides > li').length

      if( this.options.randomStart ) {
        this._setRandomStart();
      }

      // build list for slide selection
      this.element.append(this._generateSkipList());

      // set image dimensions
      this.element.find('ul.ui-ncbislideshow-slides')
        .height( this.options.height)
        .width( this.options.width);

      // start animation
      this.element.find('.ui-ncbislideshow-skip li a:eq('+ this.options.currentSlide +')').addClass('active');
      this.element.find('.ui-ncbislideshow-slides li').hide();
      this.element.find('.ui-ncbislideshow-slides li:nth-child('+ this.options.currentSlide +')').show();
      this.startAnimation();
    },

    _setRandomStart: function() {
        var randomNum = Math.floor ( Math.random ( ) * this.options.slideCount + 1);
        this.element.ncbislideshow('option', 'currentSlide', randomNum );
    },

    startAnimation: function () {
      var self = this;
      this.element.find('span.ui-icon')
        .addClass('ui-icon-pause').removeClass('ui-icon-play')
        .attr('title','click to pause');
      this.options.animTimeout = setTimeout(function(){
        self._rotateSlides();
      },this.options.slideInterval);
    },

    stopAnimation: function () {
      this.element.find('span.ui-icon')
        .addClass('ui-icon-play').removeClass('ui-icon-pause')
        .attr('title','click to play');
      clearTimeout(this.options.animTimeout);
      delete this.options.animTimeout;
    },

    _rotateSlides: function () {
      var opts = this.options;
      var el   = this.element;
      this._advanceSlide();

      this.startAnimation();
    },

    _advanceSlide: function() {
      var opts = this.options;
      var el   = this.element;
      if (opts.currentSlide >= this.options.slideCount) {
        opts.currentSlide = 1;
      }
      else {
        opts.currentSlide++;
      }
      el.find('.ui-ncbislideshow-slides li:visible').fadeOut(opts.fadeSpeed);
      el.find('.ui-ncbislideshow-slides li:nth-child('+ opts.currentSlide +')').fadeIn(opts.fadeSpeed);
      el.find('.ui-ncbislideshow-skip > li > a').removeClass('active');
      el.find('.ui-ncbislideshow-skip li a:eq('+ opts.currentSlide +')').addClass('active');
    },

    skiptoSlide: function () {
      var opts = this.options;
      var el   = this.element;
      el.find('.ui-ncbislideshow-slides li:visible').fadeOut(opts.fadeSpeed);
      el.find('.ui-ncbislideshow-slides li:nth-child('+ opts.currentSlide +')').fadeIn(opts.fadeSpeed);
      el.find('.ui-ncbislideshow-skip > li > a').removeClass('active');
      el.find('.ui-ncbislideshow-skip li a:eq('+ opts.currentSlide +')').addClass('active');
      if(this.options.animTimeout){
        this.stopAnimation();
        this.startAnimation();
      }
    },

    _generateSkipList: function() {
      var self = this;
      var opts = this.options;
      var pauseLink = $('<a>').attr('href','#').attr('tabindex','-1')
        .append($('<span>').attr('title','click to pause')
        .addClass('ui-icon ui-icon-pause'))
        .bind('click', function (e) {
          if (opts.animTimeout) {
            self.stopAnimation();
          }
          else {
            self.startAnimation();
          }
          e.preventDefault();
        });

      var list = $('<ul>').width(opts.width).addClass('ui-ncbislideshow-skip')
        .append($('<li>').addClass('ui-ncbislideshow-playPause')
          .append(pauseLink));
      // add a 'skip to' link for each slide
      this.element.find('ul.ui-ncbislideshow-slides > li').each(function(i) {
        var num = i + 1;
        var slideLink = $('<a>' + num + '</a>').attr('href','#').attr('tabindex','-1')
          .bind('click', function (e) {
            if( self.options.currentSlide === num ) {
              return;
            }
            self.options.currentSlide = num;
            self.skiptoSlide();
            e.preventDefault();
          });
        list.append($('<li>').append(slideLink));
      });

      return list
    },

    destroy: function () {
      // teardown in reverse order to the create
      this.element.find('.ui-ncbislideshow-slides li').show();
      // stop animation
      this.stopAnimation();
      // remove slide selection list
      this.element.children('.ui-ncbislideshow-skip').remove();
      // remove styles
      this.element.removeClass('ui-ncbislideshow')
        .children('ul').removeClass('ui-ncbislideshow-slides');
      $.Widget.prototype.destroy.apply(this, arguments);
    }

  }); // end widget def

  $.ui.ncbislideshow.prototype.version = '2.0';
})(jQuery);
