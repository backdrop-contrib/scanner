/**
 * @file
 * Custom JS for the Search and Replace Scanner module.
 */

(function ($) {

  /**
   * Prevents submit button double-click on replace confirmation form.
   */
  Backdrop.behaviors.hideSubmitButton = {
    attach: function (context) {
      $('input#edit-confirm', context).click(function () {
        $('.scanner-buttons', context).css('position', 'relative')
          .append('<div class="scanner-button-msg"><p>Replacing items... please wait...</p></div>');
        $('.scanner-button-msg', context).click(function () {
          return false;
        });
        return true;
      });
    }
  };

})(jQuery);
