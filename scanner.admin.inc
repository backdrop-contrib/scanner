<?php

/**
 * @file
 * Administrative / settings functionality.
 */

// The shared functions were moved into a separate file.
include_once 'scanner.inc';

/**
 * The main settings form.
 */
function scanner_admin_form($form, &$form_state) {
  $config = config('scanner.settings');

  $form['settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('Default options'),
  );
  $form['settings']['scanner_mode'] = array(
    '#type' => 'checkbox',
    '#title' => t('Default: Case Sensitive Search Mode'),
    '#default_value' => $config->get('scanner_mode'),
  );
  $form['settings']['scanner_wholeword'] = array(
    '#type' => 'checkbox',
    '#title' => t('Default: Match Whole Word'),
    '#default_value' => $config->get('scanner_wholeword'),
  );
  $form['settings']['scanner_regex'] = array(
    '#type' => 'checkbox',
    '#title' => t('Default: Regular Expression Search'),
    '#default_value' => $config->get('scanner_regex'),
  );
  $form['settings']['scanner_published'] = array(
    '#type' => 'checkbox',
    '#title' => t('Default: Search Published Nodes Only'),
    '#default_value' => $config->get('scanner_published'),
  );
  $form['settings']['scanner_pathauto'] = array(
    '#type' => 'checkbox',
    '#title' => t('Default: Maintain Custom Aliases'),
    '#default_value' => $config->get('scanner_pathauto'),
  );

  if (module_exists('taxonomy')) {
    $vocabularies = taxonomy_get_vocabularies();

    if (count($vocabularies)) {
      $options = array();
      foreach ($vocabularies as $vocabulary) {
        $options[$vocabulary->machine_name] = $vocabulary->name;
      }

      $form['taxonomy'] = array(
        '#type' => 'fieldset',
        '#title' => t('Allow restrictions by terms in a vocabulary'),
        '#description' => t('Adds search option to only match nodes tagged with specified terms from these vocabularies.'),
      );
      $form['taxonomy']['scanner_vocabulary'] = array(
        '#type' => 'checkboxes',
        '#options' => $options,
        '#default_value' => $config->get('scanner_vocabulary'),
      );
    }
  }

  $form['fields'] = array(
    '#type' => 'fieldset',
    '#title' => t('Fields that will be searched'),
    '#description' => t('Fields are listed in [nodetype: fieldname] order:'),
  );

  $field_configs = _scanner_get_all_tables_map();
  usort($field_configs, '_scanner_compare_fields_by_name');
  foreach ($field_configs as $item) {
    $key = 'scanner_' . $item['field'] . '_' . $item['table'] . '_' . $item['type'];
    $form['fields'][$key] = array(
      '#type' => 'checkbox',
      '#title' => filter_xss('<strong>' . $item['type'] . ':</strong> ' . $item['field'], array('strong')),
      '#default_value' => $config->get($key, TRUE),
    );
  }

  $form['#submit'][] = 'scanner_admin_form_submit';

  return system_settings_form($form);
}

/**
 * Submit handler for scanner_admin_form.
 */
function scanner_admin_form_submit($form, &$form_state) {
  $config = config('scanner.settings');
  $config->set('scanner_mode', $form_state['values']['scanner_mode']);
  $config->set('scanner_wholeword', $form_state['values']['scanner_wholeword']);
  $config->set('scanner_regex', $form_state['values']['scanner_regex']);
  $config->set('scanner_published', $form_state['values']['scanner_published']);
  $config->set('scanner_pathauto', $form_state['values']['scanner_pathauto']);

  if (isset($form_state['values']['scanner_vocabulary'])) {
    $config->set('scanner_vocabulary', array_filter($form_state['values']['scanner_vocabulary']));
  }

  $field_configs = _scanner_get_all_tables_map();
  usort($field_configs, '_scanner_compare_fields_by_name');
  foreach ($field_configs as $item) {
    $key = 'scanner_' . $item['field'] . '_' . $item['table'] . '_' . $item['type'];
    $config->set($key, $form_state['values'][$key]);
  }

  $config->save();
}



