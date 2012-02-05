<?php
/**
 * @file
 * Provides the user_roles rule plugin for the Menu Position module.
 * Author - Konstantin Ordoulis - Konordo - www.konordo.com
 */

/**
 * Checks if the currently logged in user has role.
 *
 * This is the condition callback that will be used when a rule is evaluated. If
 * the condition was not added to the rule, this callback will not be called.
 *
 * @param $variables
 *   An array containing each of the variables saved in the database necessary
 *   to evaluate this condition of the rule.
 * @return
 *   TRUE if condition applies successfully. Otherwise FALSE.
 */
function menu_position_menu_position_condition_user_role($variables) {
  $flag = FALSE;
  
  //Load the currently logged in user
  global $user;

  // Grab the variables stored statically in the rule.
  $user_role = $variables['user_role'];
    
  foreach($user->roles as $role_id => $role_name) {
    if (array_key_exists($role_id, $user_role)) {
      $flag = TRUE;
    }
  }
  return $flag;
}

/**
 * Adds form elements for the user_role plugin to the rule configuration form.
 *
 * If this condition was active in the current rule, the plug-in variables will
 * be available in $form_state['#menu-position-rule']['conditions']['user_role'].
 *
 * It is the resposibility of this hook to add any necessary form validation and
 * submission handlers.
 *
 * @param $form
 *   A reference to the "add/edit rule" form array. New form elements should be
 *   added directly to this array.
 * @param $form_state
 *   A reference to the current form state.
 */
function menu_position_menu_position_rule_user_role_form(&$form, &$form_state) {
  $roles = user_roles(TRUE);
  // If this is an existing rule, load the variables stored in the rule for this plugin.
  $variables = !empty($form_state['#menu-position-rule']['conditions']['user_role']) ? $form_state['#menu-position-rule']['conditions']['user_role'] : array();

  // To ensure that the plugin's form elements are placed inside vertical tabs,
  // all elements should be placed inside a collapsed fielset inside the
  // $form['conditions'] array.
  $form['conditions']['user_role'] = array(
    '#type' => 'fieldset',
    '#title' => t('User Role'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#attached' => array(
      // Ensures a proper summary is added to its vertical tab.
      'js' => array(drupal_get_path('module', 'menu_position') . '/plugins/menu_position.user_role.js'),
    ),
  );
  $form['conditions']['user_role']['user_role'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Roles'),
    '#default_value' => !empty($variables['user_role']) ? $variables['user_role'] : array(),
    '#options' => $roles, 
    '#description' => t('Apply rule only to users with this role(s).'),
    '#weight' => -20,
  );

  // If we have a validation handler, we can add it this way. Or we could add
  // a per-element validation handler with '#element_validate' above.
  $form['#validate'][] = 'menu_position_menu_position_rule_user_role_form_validate';

  // Add a submit handler.
  $form['#submit'][] = 'menu_position_menu_position_rule_user_role_form_submit';
}

/**
 * Prepares the plugin's variables to be stored in the rule.
 *
 * If the plugin's form elements indicate that the condition needs to be
 * included with the rule, the submit handler must, at the very least, set:
 * $form_state['conditions']['user_role'] = array(). Optionally, the plugin can add
 * to this array any static variables to be stored in the database with the rule
 * configuration.
 *
 * If, after this submit handler is run, the $form_state['conditions']['user_role']
 * variables array is not set, this plugin will not be added as a condition for
 * this rule.
 *
 * @param $form
 *   A reference to the "add/edit rule" form array.
 * @param $form_state
 *   A reference to the current form state, including submitted values.
 */
function menu_position_menu_position_rule_user_role_form_submit(&$form, &$form_state) {
  // The user has added our plugin's form elements as a condition for the rule.
if (!empty($form_state['values']['user_role'])) {
    // Remove any 0 valued options.
    foreach ($form_state['values']['user_role'] as $key => $value) {
      if ($value === 0) {
        unset($form_state['values']['user_role'][$key]);
      }
    }
    // Determine if any checkboxes were on.
    if (!empty($form_state['values']['user_role'])) {
      // Add this plugin's variables to the rule.
      $variables = array(
        'user_role' => $form_state['values']['user_role'],
      );
      $form_state['values']['conditions']['user_role'] = $variables;
    }
  } 
}