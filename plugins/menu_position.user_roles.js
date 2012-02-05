(function ($) {

/**
 * Provide the summary information for the user_role plugin's vertical tab.
 */
Drupal.behaviors.menuPositionUserRoleSettingsSummary = {
  attach: function (context) {
    $('fieldset#menu-position-rule', context).drupalSetSummary(function (context) {
      var vals = [];
      $('input[type="checkbox"]:checked', context).each(function () {
        vals.push($.trim($(this).next('label').text()));
      });
      if (!vals.length) {
        vals.push(Drupal.t('Any Role'));
      }
      return vals.join(', ');
    });
  }
};

})(jQuery);