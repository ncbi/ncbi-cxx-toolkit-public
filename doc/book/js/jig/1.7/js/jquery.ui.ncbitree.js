/*
ui.widgetName: ncbitree

@crafted by     Jody Clements, clement2@ncbi.nlm.nih.gov

*/

(function($) {
  $.widget("ui.ncbitree", $.ui.ncbitree_base, {
    options: {
      expanded: ''
    },

    _create: function() {
      $.ui.ncbitree_base.prototype._create.call(this);
      this.list.addClass('jig-tree');
    },

    destroy: function () {
      // teardown in reverse order to the create
      this.list.removeClass('jig-tree');
      $.ui.ncbitree_base.prototype.destroy.call(this);
    }

  }); // end widget def

  $.ui.ncbitree.prototype.version = '0.1';
})(jQuery);

