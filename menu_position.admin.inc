<?php
/**
 * @file
 * Provides infrequently used functions and hooks for menu_position.
 */

/**
 * Routes menu_position links to homepage; normally overridden.
 */
function menu_position_router() {
  drupal_goto('<front>');
}

/**
 * Form definition: global settings for Menu position rules.
 */
function menu_position_settings_form($form, &$form_state) {
  $form = array();
  $form['menu_position_active_link_display'] = array(
    '#type' => 'radios',
    '#title' => t('When a menu position rule matches:'),
    '#options' => array(
      'child' => t("Insert the current page's title into the menu tree."),
      'parent' => t('Mark the rule\'s parent menu item as being "active".'),
      'none' => t('Don\'t mark any menu as being "active".'),
    ),
    '#default_value' => variable_get('menu_position_active_link_display', 'child'),
    '#description' => t("By default, a matching menu position rule will insert the current page's title into the menu tree just below the rule's parent menu item."),
  );
  return system_settings_form($form);
}

/**
 * Implements hook_form_FORM_ID_alter().
 */
function _menu_position_form_menu_overview_form_alter(&$form, &$form_state) {
  // Retrieve all of the rules' mlids.
  $rules = db_query('SELECT rid, mlid FROM {menu_position_rules} WHERE enabled = :enabled ORDER BY weight, rid', array(':enabled' => 1));
  foreach ($rules as $rule) {
    $mlid = $rule->mlid;
    if (!empty($form['mlid:' . $mlid]['#item']['mlid']) && $mlid == $form['mlid:' . $mlid]['#item']['mlid']) {
      // Remove link and "disabled" text from the menu item's title.
      $form['mlid:' . $mlid]['title']['#markup'] = strip_tags(str_replace(' (' . t('disabled') . ')', '', $form['mlid:' . $mlid]['title']['#markup']));
      // Ensure that the menu item cannot be enabled.
      $form['mlid:' . $mlid]['hidden']['#default_value'] = TRUE;
      $form['mlid:' . $mlid]['hidden']['#disabled'] = TRUE;
      // Alter the edit link for this menu item.
      $form['mlid:' . $mlid]['operations']['edit']['#href'] = 'admin/structure/menu-position/' . $rule->rid . '/edit';
      $form['mlid:' . $mlid]['operations']['edit']['#options'] = array('query' => array('destination' => $_GET['q']));
    }
  }
}

/**
 * Implements hook_form_menu_edit_item_alter().
 *
 * This handles the edge case of another module accidentally exposing (or of a
 * user hacking the URL to) the standard "menu link edit" form for a menu
 * position rule's hidden menu link. We alter the form so that it is not posible
 * for the link to be edited.
 */
function _menu_position_form_menu_edit_item_alter(&$form, &$form_state) {
  // Retrieve all of the rules' mlids.
  $mlids = db_query('SELECT mlid FROM {menu_position_rules} WHERE mlid = :mlid ORDER BY weight, rid', array(':mlid' => (int) $form['mlid']['#value']))->fetchAll();
  if (!empty($mlids)) {
    // If the form hasn't been submitted, display a warning.
    if (empty($form_state['input'])) {
      drupal_set_message(t('This menu item cannot be edited.'), 'warning');
    }
    // Disable all the normal form elements.
    foreach (array('link_title', 'description', 'enabled', 'expanded', 'parent', 'weight') as $key) {
      $form[$key]['#disabled'] = TRUE;
    }
    // Remove the validator.
    $key = array_search('menu_edit_item_validate', $form['#validate']);
    if ($key !== FALSE) {
      unset($form['#validate'][$key]);
    }
    // Replace the standard submit handler with our own.
    $key = array_search('menu_edit_item_submit', $form['#submit']);
    if ($key !== FALSE) {
      $form['#submit'][$key] = 'menu_position_edit_item_submit';
    }
    // Replace the Save button with a Cancel button.
    unset($form['actions']['submit']);
    $form['actions']['cancel'] = array(
      '#type' => 'submit',
      '#value' => t('Cancel'),
    );
  }
}

/**
 * Implements hook_menu_link_update().
 */
function _menu_position_menu_link_update($link) {
  $rules = db_query('SELECT rid, plid FROM {menu_position_rules} WHERE mlid = :mlid ORDER BY weight, rid', array(':mlid' => $link['mlid']));
  foreach ($rules as $rule) {
    // Check if the user has altered the parent menu item.
    if ($link['plid'] != $rule->plid) {
      // Update the rule with the new parent.
      db_update('menu_position_rules')
        ->fields(array(
          'menu_name' => $link['menu_name'],
          'plid' => $link['plid'],
          ))
        ->condition('rid', $rule->rid)
        ->execute();
    }
  }
}

/**
 * Process menu and menu item add/edit form submissions for menu_position links.
 */
function menu_position_edit_item_submit($form, &$form_state) {
  // Redirect to the menu edit form and display a message.
  list($menu_name, ) = explode(':', $form['parent']['#default_value']);
  $form_state['redirect'] = 'admin/structure/menu/manage/' . $menu_name;
  drupal_set_message(t('Your configuration was not saved.'), 'error');
}

/**
 * Fix rules after module has been re-enabled.
 *
 * During menu_position_enable(), existing rules are flagged with a zero-value
 * mlid. We fix that here.
 */
function menu_position_enable_helper() {
  // Find rules with zero-value menu links.
  $rules = db_query('SELECT rid, plid, admin_title FROM {menu_position_rules} WHERE enabled = :enabled AND mlid = :mlid', array(':enabled' => 1, ':mlid' => 0))->fetchAll();
  if (!empty($rules)) {
    drupal_set_message(t('Existing menu position rules were discovered and have now been re-configured so they will continue to work.'));
  }
  foreach ($rules as $rule) {
    $mlid = menu_position_add_menu_link($rule->rid, $rule->plid, $rule->admin_title);
    db_update('menu_position_rules')
      ->fields(array('mlid' => $mlid))
      ->condition('rid', $rule->rid)
      ->execute();
  }
}

/**
 * Menu callback: orders rules.
 */
function menu_position_rules_form_callback() {
  // This is a total hack. @see menu_position_enable(). You shouldn't be doing
  // non-Form API stuff in a form definition. So we've created this wrapper
  // callback to run the hack and then return the form definition.
  menu_position_enable_helper();

  return drupal_get_form('menu_position_rules_form');
}

/**
 * Form definition: orders rules.
 */
function menu_position_rules_form($form, &$form_state) {
  // We're re-using classes from the menu module.
  $form['#attached']['css'] = array(drupal_get_path('module', 'menu') . '/menu.css');

  $rules = db_query('SELECT rid, admin_title, plid, menu_name, enabled, weight FROM {menu_position_rules} ORDER BY weight, rid')->fetchAll();
  $delta = count($rules);

  $menus = menu_get_menus();

  // Default message if no rules.
  if ($delta == 0) {
    $form['rules'] = array(
      '#markup' => '<p>' . t('No rules have been created yet.') . '</p>',
    );
  }
  else {
    $form['rules'] = array(
      '#tree' => TRUE,
      '#theme' => 'menu_position_rules_order',
    );
    foreach ($rules as $rule) {
      $menu_link = menu_link_load($rule->plid);
      $form['rules'][$rule->rid] = array(
        'title' => array(
          '#markup' => '<strong>' . check_plain($rule->admin_title) . '</strong> (' . t('Positioned under: %title', array('%title' => check_plain($menu_link['title']))) . ')',
        ),
        'menu_name' => array(
          '#markup' => check_plain($menus[$rule->menu_name]),
        ),
        'enabled' => array(
          '#type' => 'checkbox',
          '#default_value' => $rule->enabled,
        ),
        'weight' => array(
          '#type' => 'weight',
          '#default_value' => $rule->weight,
          '#delta' => max($delta, 5),
          '#id' => 'edit-rule-' . $rule->rid,
        ),
        'operations' => array(
          'edit-link' => array(
            '#type' => 'link',
            '#title' => t('edit'),
            '#href' => 'admin/structure/menu-position/' . $rule->rid . '/edit',
          ),
          'delete-link' => array(
            '#type' => 'link',
            '#title' => t('delete'),
            '#href' => 'admin/structure/menu-position/' . $rule->rid . '/delete',
          ),
        ),
      );
    }
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save'),
    );
  }

  return $form;
}

/**
 * Handles form submission for menu_position_rules_form().
 */
function menu_position_rules_form_submit($form, &$form_state) {
  foreach ($form_state['values']['rules'] as $rid => $values) {
    $rule = menu_position_rule_load($rid);
    // Only save the menu position rule if a value was changed.
    if ($rule->enabled != $values['enabled'] || $rule->weight != $values['weight']) {
      $rule->enabled = $values['enabled'];
      $rule->weight = $values['weight'];
      menu_position_rule_save($rule);
    }
  }
  drupal_set_message(t('The new rules ordering has been applied.'));
}

/**
 * Returns HTML for the menu position rules form.
 */
function theme_menu_position_rules_order($variables) {
  $element = $variables['element'];
  drupal_add_tabledrag('menu-position-rules', 'order', 'sibling', 'rule-weight');

  $variables = array(
    'header' => array(
      t('Rule'),
      t('Affected menu'),
      array(
        'data' => t('Enabled'),
        'class' => array('checkbox'),
      ),
      t('Weight'),
      array(
        'data' => t('Operations'),
        'colspan' => '2',
      ),
    ),
    'rows' => array(),
    'attributes' => array('id' => 'menu-position-rules'),
  );

  // Generate table of draggable menu names.
  foreach (element_children($element) as $rule) {
    // Add special classes to be used for tabledrag.js.
    $element[$rule]['weight']['#attributes']['class'] = array('rule-weight');
    // Render the title, enabled, and weight columns.
    $data = array(
      drupal_render($element[$rule]['title']),
      drupal_render($element[$rule]['menu_name']),
      array(
        'data' => drupal_render($element[$rule]['enabled']),
        'class' => array('checkbox', 'menu-enabled'),
      ),
      drupal_render($element[$rule]['weight']),
    );
    // Render the operations links.
    foreach (element_children($element[$rule]['operations']) as $op) {
      $data[] = array(
        'data' => drupal_render($element[$rule]['operations'][$op]),
        'class' => array('menu-operations'),
      );
    }
    $variables['rows'][] = array(
      'data' => $data,
      'class' => array('draggable'),
    );
  }

  return theme('table', $variables);
}

/**
 * Returns form to add or edit a menu position rule.
 */
function menu_position_rule_edit_form($form, &$form_state, $rule = NULL) {
dsm($rule);
  if (!isset($rule)) {
    
  }
  else {
    // Grab the rule from the database.
    $rule;
    $form_state['#menu-position-rule'] = menu_position_read_rule($rid);
  }
  // Set the default values.
  if (!isset($rule)) {
    $rule = new stdClass();
    $rule->rid = NULL;
    $rule->admin_title = '';
    $rule->menu_name = '';
    $rule->plid = NULL;
    $rule->mlid = NULL;
    $rule->enabled = 1;
    $rule->weight = 0;
  }

  foreach (array('rid', 'enabled', 'mlid', 'weight') as $field) {
    $form[$field] = array(
      '#type' => 'value',
      '#value' => $rule->{$field},
    );
  }

  $form['admin_title'] = array(
    '#type' => 'textfield',
    '#default_value' => $rule->admin_title,
    '#title' => t('Administrative title'),
    '#description' => t('This title will be used administratively to identify this rule.'),
    '#required' => TRUE,
  );
  // Parent menu item.
  if (!empty($rule->mlid)) {
    $options = menu_parent_options(menu_get_menus(), menu_link_load($rule->mlid));
    $default = $rule->menu_name . ':' . $rule->plid;
  }
  else {
    $options = menu_parent_options(menu_get_menus(), array('mlid' => 0));
    $default = 'main-menu:0';
  }
  $form['plid'] = array(
    '#type' => 'select',
    '#title' => t('Parent menu item'),
    '#required' => TRUE,
    '#options' => $options,
    '#default_value' => $default,
    '#description' => t('Select the place in the menu where the rule should position its menu links.'),
  );

  // Place holder for all condition plug-ins.
  // Visibility settings.
  $form['conditions_title'] = array(
    '#type' => 'item',
    '#title' => t('Conditions'),
    '#description' => t('All the conditions must be met before a rule is applied.'),
  );
  $form['conditions'] = array(
    '#type' => 'vertical_tabs',
    '#tree' => TRUE,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );
  if ($rule->rid) {
    $form['actions']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete'),
    );
  }
  $form['actions']['cancel'] = array(
    '#type' => 'link',
    '#title' => t('Cancel'),
    '#href' => isset($_GET['destination']) ? $_GET['destination'] : 'admin/structure/menu-position',
  );

  // Add conditions.
  foreach (menu_position_get_plugins() as $plugin) {
    // Load the required include file.
    if (!empty($plugin['file'])) {
      $file = pathinfo($plugin['file']);
      // Allow plugins to be in a sub-directory.
      if ($file['dirname']) {
        $file['filename'] = $file['dirname'] . '/' . $file['filename'];
      }
      module_load_include($file['extension'], $plugin['module'], $file['filename']);
    }
    // Call form callback to add additional form elements.
    $function = $plugin['form_callback'];
    if (function_exists($function)) {
      $function($form, $form_state, $rule);
    }
  }

  return $form;
}

/**
 * Validates the form for menu_position_rule_edit_form().
 */
function menu_position_rule_edit_form_validate($form, &$form_state) {
  // Check if the user deleted the rule.
  if (!empty($form['delete']) && $form_state['values']['op'] == $form['delete']['#value']) {
    drupal_goto('admin/structure/menu-position/' . $form_state['values']['rid'] . '/delete');
    return;
  }
  // Don't allow the user to select a menu name instead of a menu item.
  list($menu_name, $plid) = explode(':', $form_state['values']['plid']);
  if ($plid == 0) {
    form_set_error('plid', t('Please select a menu item. You have selected the name of a menu.'));
  }
}

/**
 * Handles form submission for menu_position_rule_edit_form().
 */
function menu_position_rule_edit_form_submit($form, &$form_state) {
  form_state_values_clean($form_state);

  // Clean out the extra parameter added by vertical tabs elements.
  unset($form_state['values']['conditions']['conditions__active_tab']);

  $rule = (object) $form_state['values'];
  list($rule->menu_name, $rule->plid) = explode(':', $rule->plid);
  menu_position_rule_save($rule);

  drupal_set_message(t('The %title rule has been saved.', array('%title' => $rule->admin_title)));
  $form_state['redirect'] = 'admin/structure/menu-position';
}

/**
 * Menu callback: confirms deletion of rule.
 */
function menu_position_rule_delete_form($form, &$form_state, $rule) {
  $form['#rule'] = $rule;
  $form['rid'] = array('#type' => 'value', '#value' => $rule->rid);

  return confirm_form(
    $form,
    t('Are you sure you want to delete the %title rule?', array('%title' => $rule->admin_title)),
    'admin/structure/menu-position',
    NULL,
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Handles form submission for menu_position_delete_rule_form().
 */
function menu_position_rule_delete_form_submit($form, &$form_state) {
  $rule = menu_position_rule_load($form_state['values']['rid']);
  menu_position_rule_delete($form_state['values']['rid']);
  drupal_set_message(t('The %title rule has been deleted.', array('%title' => $title)));
  $form_state['redirect'] = 'admin/structure/menu-position';
}
