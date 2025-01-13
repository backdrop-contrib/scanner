<?php

// The shared functions were moved into a separate file.
include_once 'scanner.inc';

/**
 * Form constructor for the confirmation form.
 */
function scanner_confirm_form($form, &$form_state)
{
  $form = array();

  $form['#attached']['js'][] = backdrop_get_path('module', 'scanner') . '/scanner.js';
  $form['#attached']['css'][] = backdrop_get_path('module', 'scanner') . '/scanner.css';

  $search    = $_SESSION['scanner_search'];
  $replace   = $_SESSION['scanner_replace'];
  $preceded  = $_SESSION['scanner_preceded'];
  $followed  = $_SESSION['scanner_followed'];
  $wholeword = $_SESSION['scanner_wholeword'];
  $regex     = $_SESSION['scanner_regex'];
  $mode      = $_SESSION['scanner_mode'];

  $modetxt = ($mode) ? t('Case sensitive') : t('Not case sensitive: will replace any matches regardless of capitalization.');

  $msg = '<p>' . t('Are you sure you want to make the following replacement?') . '</p>';
  $msg .= '<div class="scanner-confirm">';
  $msg .= '<label>' . t('Search for:') . '</label>';
  $msg .= ' [' . check_plain($search) . ']';
  $msg .= '</div>';
  if ($preceded) {
    $msg .= '<div class="scanner-confirm">';
    $msg .= '<label>' . t('Preceded by:') . '</label>';
    $msg .= ' [' . check_plain($preceded) . ']';
    $msg .= '</div>';
  }
  if ($followed) {
    $msg .= '<div class="scanner-confirm">';
    $msg .= '<label>' . t('Followed by:') . '</label>';
    $msg .= ' [' . check_plain($followed) . ']';
    $msg .= '</div>';
  }
  $msg .= '<div class="scanner-confirm">';
  $msg .= '<label>' . t('Replace with:') . '</label>';
  $msg .= ' [' . check_plain($replace) . ']';
  $msg .= '</div>';
  if ($replace === '') {
    $msg .= ' <span class="warning">' . t('This will delete any occurrences of the search terms!') . '</span>';
  }
  $msg .= '</div>';
  $msg .= '<div class="scanner-confirm">';
  $msg .= '<label>' . t('Mode:') . '</label>';
  $msg .= $modetxt . '</div>';
  if ($wholeword) {
    $msg .= '<div class="scanner-confirm">';
    $msg .= '<label>' . t('Match whole word:') . '</label>';
    $msg .= ' ' . t('Yes');
    $msg .= '</div>';
  }
  if ($regex) {
    $msg .= '<div class="scanner-confirm">';
    $msg .= '<label>' . t('Use regular expressions:') . '</label>';
    $msg .= ' ' . t('Yes');
    $msg .= '</div>';
  }

  $form['warning'] = array(
    '#type' => 'item',
    '#markup' => $msg,
  );
  $form['confirm'] = array(
    '#type' => 'submit',
    '#value' => t('Yes, Continue'),
    '#submit' => array('scanner_confirm_form_submit'),
  );

  $form['cancel'] = array(
    '#type' => 'submit',
    '#value' => t('No, Cancel'),
    '#submit' => array('scanner_confirm_form_cancel_submit'),
  );

  return $form;
}

/**
 * Form submission handler for scanner_confirm_form().
 */
function scanner_confirm_form_submit($form, &$form_state)
{
  if ($form_state['values']['op'] == t('Yes, Continue')) {
    $_SESSION['scanner_status'] = SCANNER_STATUS_GO_REPLACE;
  } else {
    unset($_SESSION['scanner_status']);
  }

  $form_state['redirect'] = 'admin/content/scanner';
}

/**
 * Form submission handler for the cancel button in scanner_confirm_form().
 */
function scanner_confirm_form_cancel_submit($form, &$form_state)
{
  unset($_SESSION['scanner_status']);
  $form_state['redirect'] = 'admin/content/scanner';
}

/**
 * Form constructor for the search and replace form.
 */
function scanner_form($form, &$form_state)
{
  $form = array();

  if (isset($_SESSION['scanner_search'])) {
    $search = $_SESSION['scanner_search'];
  } else {
    $search = NULL;
  }

  if (isset($_SESSION['scanner_replace'])) {
    $replace = $_SESSION['scanner_replace'];
  } else {
    $replace = NULL;
  }
  if (isset($_SESSION['scanner_preceded'])) {
    $preceded = $_SESSION['scanner_preceded'];
  } else {
    $preceded = NULL;
  }

  if (isset($_SESSION['scanner_followed'])) {
    $followed = $_SESSION['scanner_followed'];
  } else {
    $followed = NULL;
  }

  $mode = $_SESSION['scanner_mode'] ?? config_get('scanner.settings', 'scanner_mode', 0);
  $wholeword = $_SESSION['scanner_wholeword'] ?? config_get('scanner.settings', 'scanner_wholeword', 0);
  $regex = $_SESSION['scanner_regex'] ?? config_get('scanner.settings', 'scanner_regex', 0);
  $published = $_SESSION['scanner_published'] ?? config_get('scanner.settings', 'scanner_published', 1);
  $pathauto = $_SESSION['scanner_pathauto'] ?? config_get('scanner.settings', 'scanner_pathauto', 1);

  if (isset($_SESSION['scanner_terms'])) {
    $terms = $_SESSION['scanner_terms'];
  } else {
    $terms = NULL;
  }

  $form['settings_link'] = array(
    '#markup' => t('The list of fields which will be searched and other options can be controlled from the !link.', array(
      '!link' => l(t('settings page'), 'admin/config/content/scanner'),
    )),
    '#prefix' => '<p>',
    '#suffix' => '</p>',
    '#access' => user_access('administer scanner settings'),
  );

  $form['search'] = array(
    '#type' => 'textfield',
    '#default_value' => $search,
    '#title' => t('Step 1: Search for'),
    '#maxlength' => 256,
  );
  $form['submit_search'] = array(
    '#type' => 'submit',
    '#value' => t('Search'),
  );

  $form['replace'] = array(
    '#type' => 'textfield',
    '#default_value' => $replace,
    '#title' => t('Step 2: Replace with'),
    '#maxlength' => 256,
    '#access' => user_access('perform search and replace') ? TRUE : FALSE,
  );
  $form['submit_replace'] = array(
    '#type' => 'submit',
    '#value' => t('Replace'),
    '#access' => user_access('perform search and replace') ? TRUE : FALSE,
  );

  $form['options'] = array(
    '#type' => 'fieldset',
    '#title' => t('Search Options'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  $form['options']['surrounding'] = array(
    '#type' => 'fieldset',
    '#title' => t('Surrounding Text'),
    '#collapsible' => FALSE,
    '#description' => t('You can limit matches by providing the text that should appear immediately before or after the search text. Remember to account for spaces. Note: Case sensitivity and regular expression options will all apply here, too. Whole word is not recommended.'),
  );
  $form['options']['surrounding']['preceded'] = array(
    '#type' => 'textfield',
    '#title' => t('Preceded by'),
    '#default_value' => $preceded,
    '#maxlength' => 256,
  );
  $form['options']['surrounding']['followed'] = array(
    '#type' => 'textfield',
    '#title' => t('Followed by'),
    '#default_value' => $followed,
    '#maxlength' => 256,
  );

  $form['options']['mode'] = array(
    '#type' => 'checkbox',
    '#title' => t('Case sensitive search'),
    '#default_value' => $mode,
    '#description' => t("Check this if the search should only return results that exactly match the capitalization of your search terms."),
  );
  $form['options']['wholeword'] = array(
    '#type' => 'checkbox',
    '#title' => t('Match whole word'),
    '#default_value' => $wholeword,
    '#description' => t("Check this if you don't want the search to match any partial words. For instance, if you search for 'run', a whole word search will <em>not</em> match 'running'."),
  );
  $form['options']['regex'] = array(
    '#type' => 'checkbox',
    '#title' => t('Use regular expressions in search'),
    '#default_value' => $regex,
    '#description' => t('Check this if you want to use regular expressions in your search terms.'),
  );
  $form['options']['published'] = array(
    '#type' => 'checkbox',
    '#title' => t('Published nodes only'),
    '#default_value' => $published,
    '#description' => t('Check this if you only want your search and replace to affect fields in nodes that are published.'),
  );
  $form['options']['pathauto'] = array(
    '#type' => 'checkbox',
    '#title' => t('Maintain custom aliases'),
    '#default_value' => $pathauto,
    '#description' => t("Prevent custom URL aliases from being overwritten with ones generated from Path Auto's URL alias patterns."),
  );

  $scanner_vocabularies = config_get('scanner.settings', 'scanner_vocabulary', array());
  $scanner_vocabularies = is_array($scanner_vocabularies) ? array_filter($scanner_vocabularies) : array();

  if (count($scanner_vocabularies)) {
    $vocabularies = taxonomy_get_vocabularies();
    $options = array();

    foreach ($vocabularies as $vocabulary) {
      if (in_array($vocabulary->machine_name, $scanner_vocabularies)) {
        // Ensure the machine name is correctly passed to the function
        $tree = taxonomy_get_tree($vocabulary->machine_name);

        if ($tree && count($tree) > 0) {
          $options[$vocabulary->name] = array();
          foreach ($tree as $term) {
            $options[$vocabulary->name][$term->tid] = str_repeat('-', $term->depth) . $term->name;
          }
        } else {
          $options[$vocabulary->name] = array(
            '#markup' => t('No terms available for vocabulary: @vocabulary', ['@vocabulary' => $vocabulary->name])
          );
        }
      } else {
        backdrop_set_message(t('Vocabulary machine name not found in scanner vocabularies: @machine_name', ['@machine_name' => $vocabulary->machine_name]), 'warning');
      }
    }

    $form['options']['terms'] = array(
      '#type' => 'select',
      '#title' => t('Only match nodes with these terms'),
      '#options' => $options,
      '#default_value' => $terms,
      '#multiple' => TRUE,
    );
  } else {
    $form['options']['terms'] = array(
      '#markup' => t('No vocabularies selected for scanning. Please update your settings.')
    );
  }

  return $form;
}

/**
 * Form validation handler for scanner_form().
 */
function scanner_form_validate($form, &$form_state)
{
  $search = trim($form_state['values']['search']);
  if ($search == '') {
    form_set_error('search', t('Please enter some keywords.'));
  }
}

/**
 * Form submission handler for scanner_form().
 */
function scanner_form_submit($form, &$form_state)
{
  // Save form input into session.
  $_SESSION['scanner_search']   = $form_state['values']['search'];
  $_SESSION['scanner_preceded'] = $form_state['values']['preceded'];
  $_SESSION['scanner_followed'] = $form_state['values']['followed'];
  $_SESSION['scanner_mode']      = $form_state['values']['mode'];
  $_SESSION['scanner_wholeword'] = $form_state['values']['wholeword'];
  $_SESSION['scanner_regex']     = $form_state['values']['regex'];
  $_SESSION['scanner_published'] = $form_state['values']['published'];
  $_SESSION['scanner_pathauto']  = $form_state['values']['pathauto'];
  if (isset($form_state['values']['terms'])) {
    $_SESSION['scanner_terms'] = $form_state['values']['terms'];
  }
  $_SESSION['scanner_replace'] = $form_state['values']['replace'];

  if ($form_state['values']['op'] == t('Replace')) {
    $_SESSION['scanner_status'] = SCANNER_STATUS_GO_CONFIRM;
  } else {
    $_SESSION['scanner_status'] = SCANNER_STATUS_GO_SEARCH;
  }
  $form_state['redirect'] = 'admin/content/scanner';
}

/**
 * Display table of executed replace actions with undo/redo operation.
 */
function scanner_undo_page()
{
  $header = array(
    t('Date'),
    t('Searched'),
    t('Replaced'),
    t('Count'),
    t('Operation'),
  );

  $undoQuery = db_select('scanner', 's');
  $undoQuery->fields('s', array(
    'undo_id',
    'time',
    'searched',
    'replaced',
    'count',
    'undone',
  ))
    ->orderBy('undo_id', 'DESC');

  $sandrs = $undoQuery->execute();

  $rows = array();

  foreach ($sandrs as $sandr) {
    if ($sandr->undone) {
      $operation = l(t('Redo'), 'admin/content/scanner/undo/confirm', array(
        'query' => array('undo_id' => $sandr->undo_id),
      ));
    } else {
      $operation = l(t('Undo'), 'admin/content/scanner/undo/confirm', array(
        'query' => array('undo_id' => $sandr->undo_id),
      ));
    }

    $rows[] = array(
      format_date($sandr->time),
      check_plain($sandr->searched),
      check_plain($sandr->replaced),
      $sandr->count,
      $operation,
    );
  }

  return theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'attributes' => NULL,
    'caption' => 'Prior Search and Replace Events',
  ));
}

/**
 * Form constructor for the undo confirmation form.
 */
function scanner_undo_confirm_form($form, &$form_state)
{
  $undo_id = $_GET['undo_id'];

  if ($undo_id > 0) {
    $query = db_select('scanner', 's');
    $query->fields('s', array('undo_id', 'searched', 'replaced'))
      ->condition('undo_id', $undo_id, '=');

    $result = $query->execute();

    foreach ($result as $undo) {
      $undo = $undo;
    }
  }

  if (isset($undo) && $undo->undo_id > 0) {
    $form['info'] = array(
      '#markup' => '<h2>' . t('Do you want to undo:') . '</h2>' . '<h3>' . t('Searched for:') . '</h3>' . '<p>[<em>' . check_plain($undo->searched) . '</em>]</p>' . '<h3>' . t('Replaced with:') . '</h3>' . '<p>[<em>' . check_plain($undo->replaced) . '</em>]</p>',
    );

    $form['undo_id'] = array(
      '#type' => 'hidden',
      '#value' => $undo->undo_id,
    );

    $form['confirm'] = array(
      '#type' => 'submit',
      '#value' => t('Yes, Continue'),
      '#submit' => array('scanner_undo_confirm_form_submit'),
    );

    $form['cancel'] = array(
      '#type' => 'submit',
      '#value' => t('No, Cancel'),
      '#submit' => array('scanner_undo_confirm_form_cancel_submit'),
    );
  } else {
    $form['info'] = array(
      '#markup' => '<h2>' . t('No undo event was found') . '</h2>',
    );
  }

  return $form;
}

/**
 * Form submission handler for the confirm button.
 */
function scanner_undo_confirm_form_submit($form, &$form_state)
{
  if ($form_state['values']['op'] == t('Yes, Continue')) {
    // Fetch the undo data.
    $undo = db_select('scanner', 's')
      ->fields('s', ['undo_data', 'undone'])
      ->condition('undo_id', $form_state['values']['undo_id'], '=')
      ->execute()
      ->fetchObject();

    if (!$undo) {
      backdrop_set_message(t('No undo data found for the specified ID.'), 'error');
      return;
    }

    $undos = unserialize($undo->undo_data);
    $count = 0;
    $new_undone_value = ($undo->undone == 0) ? 1 : 0;

    foreach ($undos as $nid => $sandr_event) {
      dpm($sandr_event);

      // Revert the node to the specified revision.
      $vid = ($undo->undone == 0) ? $sandr_event['old_vid'] : $sandr_event['new_vid'];
      $node = node_load($nid, $vid);

      if ($node) {
        $node->is_active_revision = 1;
        $revision_timestamp = !empty($node->revision_timestamp) && is_numeric($node->revision_timestamp) && $node->revision_timestamp > 0
          ? $node->revision_timestamp
          : REQUEST_TIME;

        $node->log = t('Copy of the revision from %date via Search and Replace Undo', [
          '%date' => format_date($revision_timestamp),
        ]);

        node_save($node);
        $count++;
        // Handle paragraphs within the node.
        if (!empty($sandr_event['paragraphs'])) {
          foreach ($sandr_event['paragraphs'] as $paragraph_id => $paragraph_event) {
            $paragraph_old_vid = ($undo->undone == 0) ? $paragraph_event['old_vid'] : $paragraph_event['new_vid'];

            // Fetch the bundle for the paragraph dynamically.
            $paragraph_bundle = db_query("SELECT bundle FROM {paragraphs_item} WHERE item_id = :item_id", [
              ':item_id' => $paragraph_id,
            ])->fetchField();

            if (!$paragraph_bundle) {
              backdrop_set_message(t('Failed to retrieve the bundle for paragraph ID @id.', ['@id' => $paragraph_id]), 'error');
              continue;
            }

            // Step 1: Update the paragraph revision ID in the paragraphs_item table.
            db_update('paragraphs_item')
              ->fields(['revision_id' => $paragraph_old_vid])
              ->condition('item_id', $paragraph_id)
              ->execute();

            // Step 2: Update the field_data_* table for the paragraph reference field dynamically.
            $field_instances = field_info_instances('node', $node->type);

            foreach ($field_instances as $field_name => $instance) {
              $field_info = field_info_field($field_name);

              // Check if this field is a paragraphs reference field.
              if ($field_info['type'] === 'paragraphs') {
                // Find and update the field_data_* table for this field.
                db_update("field_data_$field_name")
                  ->fields(['field_paragraphs_revision_id' => $paragraph_old_vid])
                  ->condition('field_paragraphs_value', $paragraph_id)
                  ->execute();
              }
            }

            // Step 3: Sync the field_data_* table with the field_revision_* table for the paragraph fields.
            $fields = field_info_instances('paragraphs_item', $paragraph_bundle);
            foreach ($fields as $field_name => $field_instance) {
              if (strpos($field_name, 'field_') === 0) {
                // Fetch the field data from field_revision_* for the old revision ID.
                $field_data = db_query("SELECT * FROM {field_revision_$field_name} WHERE entity_id = :entity_id AND revision_id = :revision_id", [
                  ':entity_id' => $paragraph_id,
                  ':revision_id' => $paragraph_old_vid,
                ])->fetchAllAssoc('delta');

                // Update the field_data_* table with the values from field_revision_*.
                foreach ($field_data as $delta => $data) {
                  $update_fields = [];
                  foreach ($data as $column => $value) {
                    if (!in_array($column, ['revision_id', 'entity_id', 'delta'])) {
                      $update_fields[$column] = $value;
                    }
                  }

                  // Perform the update on the field_data_* table.
                  db_update("field_data_$field_name")
                    ->fields($update_fields)
                    ->condition('entity_id', $paragraph_id)
                    ->condition('delta', $delta)
                    ->execute();
                }
              }
            }

            // Step 4: Validate the paragraph's revision update.
            $current_revision_id = db_query("SELECT revision_id FROM {paragraphs_item} WHERE item_id = :item_id", [
              ':item_id' => $paragraph_id,
            ])->fetchField();

            if ($current_revision_id != $paragraph_old_vid) {
              backdrop_set_message(t('Paragraph ID @id has a mismatched revision ID: expected @expected, found @found.', [
                '@id' => $paragraph_id,
                '@expected' => $paragraph_old_vid,
                '@found' => $current_revision_id,
              ]), 'warning');
            }
          }
        }
      }
    }

    // Update the `undone` status in the database.
    db_update('scanner')
      ->fields(['undone' => $new_undone_value])
      ->condition('undo_id', $form_state['values']['undo_id'])
      ->execute();

    backdrop_set_message(t('@count nodes reverted.', ['@count' => $count]));
  } else {
    backdrop_set_message(t('Undo / Redo canceled'));
  }

  // Redirect back to the undo administration page.
  $form_state['redirect'] = 'admin/content/scanner/undo';
}

/**
 * Form submission handler for the cancel button.
 */
function scanner_undo_confirm_form_cancel_submit($form, &$form_state)
{
  backdrop_set_message(t('Undo / Redo canceled'));
  $form_state['redirect'] = 'admin/content/scanner/undo';
}

/**
 * Menu callback; presents the scan form and results.
 */
function scanner_view()
{
  $output = '';

  backdrop_add_css('
    #scanner-form .form-submit { margin-top:0; }
    #scanner-form .form-item { margin-bottom:0; }
  ', array('type' => 'inline'));

  backdrop_add_js("
    jQuery(document).ready(function() {
      var searchfield = jQuery('#edit-search');
      jQuery('input[type=submit][value=" . str_replace('\'', '\\\'', t('Search')) . "]').click(function() {
        var chars = searchfield.val().length;
        if (chars == 0) {
          alert('" . str_replace('\'', '\\\'', t('Please provide some search text and try again.')) . "');
          searchfield.addClass('error');
          searchfield[0].focus();
          return false;
        } else if (chars < 3) {
          return confirm('" . str_replace('\'', '\\\'', t('Searching for a keyword that has fewer than three characters could take a long time. Are you sure you want to continue?')) . "');
        }
        return true;
      });
      searchfield.keyup(function() {
        searchfield.removeClass('error');
      });
    });
  ", array('type' => 'inline', 'group' => JS_DEFAULT));

  if (isset($_SESSION['scanner_search'])) {
    $search = $_SESSION['scanner_search'];
  } else {
    $search = NULL;
  }

  if (isset($_SESSION['scanner_status'])) {
    $status = $_SESSION['scanner_status'];
  } else {
    $status = NULL;
  }

  if (!is_null($search) && $status >= SCANNER_STATUS_GO_SEARCH) {
    if ($status == SCANNER_STATUS_GO_CONFIRM) {
      backdrop_goto('admin/content/scanner/scan/confirm');
    } elseif ($status == SCANNER_STATUS_GO_REPLACE) {
      $resulttxt = '<a name="results"></a>' . t('Replacement Results');
      $results = _scanner_execute('replace');
    } else {
      $resulttxt = t('Search Results');
      $results = _scanner_execute('search');
    }

    if ($results) {
      $results = '<a name="results"></a><div><h2>' . $resulttxt . '</h2>' . $results;
    } else {
      $results = t('Your search yielded no results.');
    }

    $scanner_form = backdrop_get_form('scanner_form');
    $output = backdrop_render($scanner_form);
    $output .= $results;

    unset($_SESSION['scanner_search']);
    unset($_SESSION['scanner_replace']);
    unset($_SESSION['scanner_preceded']);
    unset($_SESSION['scanner_followed']);
    unset($_SESSION['scanner_mode']);
    unset($_SESSION['scanner_wholeword']);
    unset($_SESSION['scanner_published']);
    unset($_SESSION['scanner_pathauto']);
    unset($_SESSION['scanner_regex']);
    unset($_SESSION['scanner_terms']);
    unset($_SESSION['scanner_status']);

    return $output;
  }

  $scanner_form = backdrop_get_form('scanner_form');
  $output = backdrop_render($scanner_form);

  return $output;
}
