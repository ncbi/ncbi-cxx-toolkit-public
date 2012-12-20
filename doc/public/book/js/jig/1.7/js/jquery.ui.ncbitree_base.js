/*
ui.widgetName: ncbitree_base

@crafted by     Jody Clements, clement2@ncbi.nlm.nih.gov

shamelesly based on the tree code writen by:
@author         Scott Jehl, scott@filamentgroup.com
@author-website http://www.filamentgroup.com/
@license        MIT License - filamentgroup.com/examples/mit-license.txt

TODO:
  add ui-widget css
*/

(function($) {
  $.widget("ui.ncbitree_base", {
    options: {
      expanded: '',
      speed: 120,
      remember: false
    },

    _create: function() {
      this.list = this.element.eq(0);
      if ( this.list[0].nodeName == 'UL' ) {
        this._treeify();
      }
    },

    _treeify: function () {
      // set up classes and aria attributes
      var list = this.list;
      var self = this;
      var opts = this.options;
      list.attr({'role': 'tree'}).addClass('ui-widget');
      //set first node's tabindex to 0
      list.find('a:eq(0)').attr('tabindex','0');
      //set all others to -1
      list.find('a:gt(0)').attr('tabindex','-1');

      list.find('ul').attr({'role': 'group'}).addClass('tree-group-collapsed');
      list.find('li').attr({'role': 'treeitem'});

      //find tree group parents
      list.find('li:has(ul)')
          .attr('aria-expanded', 'false').find('>a')

      // wrap parent link in a span and add toggle link
      list.find('li:has(ul)').find('>a')
          .wrap('<span class="tree-parent"/>');
          //.before('<a class="tree-parent" href="#"></a>');

      //bind the custom events
      list
      //expand a tree node
      .bind('expand.treeEvents',function(event){
        self._expand(event.target);
        self._trigger('expand', event);
      })
      //collapse a tree node
      .bind('collapse.treeEvents',function(event){
        self._collapse(event.target);
        self._trigger('collapse', event);
      })
      .bind('toggle.treeEvents',function(event){
        self._toggle(event.target);
      })
      //shift focus down one item
      .bind('traverseDown.treeEvents',function(event){
        var target = list.find('span[tabindex=0]');
        if (!target.length) {
          target = list.find('a[tabindex=0]');
        }
        if (!target.length) {
          target = $(event.target);
        }
        var targetLi = target.parent();
        if(targetLi.is('[aria-expanded=true]')){
          target.next().find('a, span').eq(0).focus();
        }
        else if(targetLi.next().length) {
          targetLi.next().find('a, span').eq(0).focus();
        }
        else {
          targetLi.parents('li').next().find('a, span').eq(0).focus();
        }
      })
      //shift focus up one item
      .bind('traverseUp.treeEvents',function(event){
        var target = list.find('span[tabindex=0]');
        if (!target.length) {
          target = list.find('a[tabindex=0]');
        }
        if (!target.length) {
          target = $(event.target);
        }
        var targetLi = target.parent();
        if(targetLi.prev().length){
          if( targetLi.prev().is('[aria-expanded=true]') ){
            targetLi.prev().find('li:visible:last a, li:visible:last span').eq(0).focus();
          }
          else{
            targetLi.prev().find('a, span').eq(0).focus();
          }
        }
        else {
          targetLi.parents('li:eq(0)').find('a, span').eq(0).focus();
        }
      });

      //bind native events
      list
        .bind('focus.treeEvents', function(event){
          //deactivate previously active tree node, if one exists
          list.find('[tabindex=0]').attr('tabindex','-1').removeClass('ui-state-focus');
          //assign 0 tabindex to focused item
          $(event.target).attr('tabindex','0').addClass('ui-state-focus');
        })
        .bind('click.treeEvents', function(event){
          //save reference to event target
          var target = $(event.target);
          //check if target is a tree node
          if( target.is('span.tree-parent') ){
            target.trigger('toggle');
            target.eq(0).focus();
            //return click event false because it's a tree node (folder)
            event.preventDefault();
          }
        })
        .bind('keydown.treeEvents', function(event){
            var target = list.find('span[tabindex=0], a[tabindex=0]');
            //check for arrow keys
            if(event.keyCode == 37 || event.keyCode == 38 || event.keyCode == 39 || event.keyCode == 40){
              //if key is left arrow and list is collapsed
              if(event.keyCode == 37 && target.parent().is('[aria-expanded=true]')){
                target.trigger('collapse');
              }
              //if key is right arrow and list is collapsed
              if(event.keyCode == 39 && target.parent().is('[aria-expanded=false]')){
                target.trigger('expand');
              }
              //if key is up arrow
              if(event.keyCode == 38){
                target.trigger('traverseUp');
              }
              //if key is down arrow
              if(event.keyCode == 40){
                target.trigger('traverseDown');
              }
              //return any of these keycodes false
              return false;
            }
            //check if enter or space was pressed on a tree node
            else if((event.keyCode == 13 || event.keyCode == 32) && target.is('span.tree-parent')){
              target.trigger('toggle');
              //return click event false because it's a tree node (folder)
              return false;
            }
        });



      var id = list.attr('id');
      if (id && $.cookie('ncbitree') && $.parseJSON($.cookie('ncbitree'))[id]) {
        var state = $.parseJSON($.cookie('ncbitree'))[id];
        // load state from cookie and set tree accordingly
        self._load_state(state);
      }
      else {
        //expanded at load
        list.find(opts.expanded).find('>span').each(function() {
          self._expand(this);
        });
      }


      list.find('li[role=treeitem] > a, li[role=treeitem] > span')
        .addClass('ui-state-default')
        .bind('mouseenter.treeEvents', function() {
          $(this).addClass('ui-state-hover');
        })
        .bind('mouseleave.treeEvents', function() {
          $(this).removeClass('ui-state-hover');
        });


    },

    _expand: function (target) {
      var opts = this.options;
      var self = this;
      var list = this.list;
      var t = (target) ? $(target) : list.find('a[tabindex=0]');
      t.addClass('ui-state-active').removeClass('ui-state-default');
      t.next().hide().removeClass('tree-group-collapsed').slideDown(opts.speed, function(){
        $(this).removeAttr('style');
        t.parent().attr('aria-expanded', 'true').addClass('tree-expanded');
        if (opts.remember) {
          self._save_state();
        }
      });
    },

    _collapse: function(target) {
      var opts = this.options;
      var self = this;
      var list = this.list;
      var t = (target) ? $(target) : list.find('a[tabindex=0]');
      t.removeClass('ui-state-active').addClass('ui-state-default');
      t.next().slideUp(opts.speed, function(){
        t.parent().attr('aria-expanded', 'false').removeClass('tree-expanded');
        $(this).addClass('tree-group-collapsed').removeAttr('style');
        if (opts.remember) {
          self._save_state();
        }
      });
    },

    _toggle: function(target) {
      var list = this.list;
      var t = (target) ? $(target) : list.find('a[tabindex=0]');
      //check if target parent LI is collapsed
      if( t.parent().is('[aria-expanded=false]') ){
        //call expand function on the target
        t.trigger('expand');
      }
      //otherwise, parent must be expanded
      else{
        //collapse the target
        t.trigger('collapse');
      }
    },

    expand: function(selector, recursive) {
      var list = this.list;
      if (recursive) {
        ancestor_spans = list.find(selector).parentsUntil('[role=tree]').find('>span');
        ancestor_spans.each(function () {
          $(this).trigger('expand');
        });
      }
      list.find(selector).find('>span').trigger('expand');
      this._trigger('expanded', null);
    },

    collapse: function (selector) {
      var list = this.list;
      list.find(selector).find('>span').trigger('collapse');
    },

    destroy: function() {
      $.Widget.prototype.destroy.apply(this, arguments); //default
      var list = this.list;
      //remove attributes
      list.removeAttr('role').removeClass('ui-widget');
      list.find('a').removeAttr('tabindex')
        .removeClass('ui-state-focus ui-state-default ui-state-hover ui-state-active');
      list.find('ul').removeAttr('role').removeClass('tree-group-collapsed');
      list.find('li').removeAttr('role').removeClass('tree-expanded');
      list.find('li:has(ul)')
          .removeAttr('aria-expanded').find('>a')
          .removeClass('tree-parent');
      list.find('span[class*="tree-parent"]>a').unwrap();
      list.find(this.options.expanded).removeAttr('aria-expanded');

      //unbind events
      list.unbind('.treeEvents');
    },

    _load_state: function(state_data) {
      var self = this;
      var list = this.list;
      state = state_data.split('');
      list.find('li[aria-expanded]').each(function(i){
        if (state[i] == 1) {
          $(this).find('>span').each(function () {
            self._expand(this);
          });
        }
        else {
          $(this).find('>span').each(function() {
            self._collapse(this);
          });
        }
      });
    },

    _save_state: function() {
      var list = this.list;
      var id = list.attr('id');
      if (id) {
        state_data = {};
        if ($.cookie('ncbitree')) {
          state_data = $.parseJSON($.cookie('ncbitree'));
        }
        var state = '';
        list.find('li[aria-expanded]').each(function(){
          state += ($(this).attr('aria-expanded') == 'true') ? '1' : '0';
        });
        state_data[id] = state;
        data = JSON.stringify(state_data);
        $.cookie('ncbitree', data, {path: '/'});
      }
    }

  }); // end widget def

  $.ui.ncbitree_base.prototype.version = '0.2';

})(jQuery);
