<?php

/**
 * @file
 * Shared functions for the Scanner module.
 */

/**
 * Attempt to stretch the amount of time available for processing.
 *
 * This way timeouts won't interrupt search and replace actions. This only works
 * in hosting environments where changing PHP and Apache settings on the fly is
 * allowed.
 *
 * @param string $setting
 *   The name of the PHP setting to change.
 * @param string $value
 *   The new value to assign.
 * @param bool $verbose
 *   If set to TRUE, an extra message will be displayed indicating the status of
 *   the execution.
 *
 * @return bool
 *   Indicates whether the setting is changed.
 */
function _scanner_change_env($setting, $value, $verbose = FALSE) {
  $old_value = ini_get($setting);

  if ($old_value != $value && $old_value != 0) {
    if (ini_set($setting, $value)) {
      if ($verbose) {
        backdrop_set_message(t('%setting changed from %old_value to %value.', array(
          '%setting' => $setting,
          '%old_value' => $old_value,
          '%value' => $value,
        )));
      }
      return TRUE;
    }
    else {
      if ($verbose) {
        backdrop_set_message(t('%setting could not be changed from %old_value to %value.', array(
          '%setting' => $setting,
          '%old_value' => $old_value,
          '%value' => $value,
        )), 'error');
      }
      return FALSE;
    }
  }
  return FALSE;
}

/**
 * Comparison function for sorting fields by table/field label.
 *
 * @param array $left
 *   One field.
 * @param array $right
 *   The other field.
 *
 * @return int
 *   Comparison value determining which order these two fields should be sorted
 *   in relation to each other based on field label.
 */
function _scanner_compare_fields_by_label(array $left, array $right) {
  $cmp = strcmp($left['type'], $right['type']);
  if ($cmp != 0) {
    return $cmp;
  }

  return strcmp($left['field_label'], $right['field_label']);
}

/**
 * Comparison function for sorting fields by table/field name.
 *
 * @param array $left
 *   One field.
 * @param array $right
 *   The other field.
 *
 * @return int
 *   Comparison value determining which order these two fields should be sorted
 *   in relation to each other based on field name.
 */
function _scanner_compare_fields_by_name(array $left, array $right) {
  $cmp = strcmp($left['type'], $right['type']);
  if ($cmp != 0) {
    return $cmp;
  }

  return strcmp($left['field'], $right['field']);
}

/**
 * Handles the actual search and replace.
 *
 * @param string $searchtype
 *   The type of search to perform, defaults to 'search'.
 *
 * @return string
 *   The themed results.
 */
function _scanner_execute($searchtype = 'search') {
  global $user;

  // Variables to monitor possible timeout.
  $max_execution_time = ini_get('max_execution_time');
  $start_time = REQUEST_TIME;
  $expanded = FALSE;

  // Get process and undo data if saved from timeout.
  $config = config('scanner.settings');
  $processed = $_SESSION['scanner_partially_processed'] ?? [];
  $undo_data = $_SESSION['scanner_partial_undo'] ?? [];

  // Get the field collection field to use when joining revisions.
  $fc_revision_field = db_table_exists('field_collection_item_revision') ? 'revision_id' : 'value';

  unset($_SESSION['scanner_status']);
  $search = $_SESSION['scanner_search'];
  $replace = $_SESSION['scanner_replace'];
  $preceded = $_SESSION['scanner_preceded'];
  $followed = $_SESSION['scanner_followed'];
  $mode = $_SESSION['scanner_mode'];
  // Case sensitivity flag for use in php preg_search and preg_replace.
  $flag = $mode ? NULL : 'i';
  $wholeword = $_SESSION['scanner_wholeword'];
  $regex = $_SESSION['scanner_regex'];
  $published = $_SESSION['scanner_published'];
  $pathauto = $_SESSION['scanner_pathauto'];
  $terms = $_SESSION['scanner_terms'] ?? NULL;
  $results = NULL;

  dpm($_SESSION, 'Session Data');
  dpm($flag);


  if ($searchtype == 'search') {
    backdrop_set_message(t('Searching for: [%search] ...', ['%search' => $search]));
  }
  // Searchtype == 'replace'.
  else {
    backdrop_set_message(t('Replacing [%search] with [%replace] ...', [
      '%search' => $search,
      '%replace' => $replace,
    ]));
  }

  $preceded_php = '';
  if (!empty($preceded)) {
    if (!$regex) {
      $preceded = addcslashes($preceded, SCANNER_REGEX_CHARS);
    }
    $preceded_php = '(?<=' . $preceded . ')';
  }
  $followed_php = '';
  if (!empty($followed)) {
    if (!$regex) {
      $followed = addcslashes($followed, SCANNER_REGEX_CHARS);
    }
    $followed_php = '(?=' . $followed . ')';
  }

  // Case 1.
  if ($wholeword && $regex) {
    $where = "[[:<:]]" . $preceded . $search . $followed . "[[:>:]]";
    $search_php = '\b' . $preceded_php . $search . $followed_php . '\b';
  }
  // Case 2.
  elseif ($wholeword && !$regex) {
    $where = "[[:<:]]" . $preceded . addcslashes($search, SCANNER_REGEX_CHARS) . $followed . "[[:>:]]";
    $search_php = '\b' . $preceded_php . addcslashes($search, SCANNER_REGEX_CHARS) . $followed_php . '\b';
  }
  // Case 3.
  elseif (!$wholeword && $regex) {
    $where = $preceded . $search . $followed;
    $search_php = $preceded_php . $search . $followed_php;
  }
  // Case 4.
  // !wholeword and !regex:
  else {
    $where = $preceded . addcslashes($search, SCANNER_REGEX_CHARS) . $followed;
    $search_php = $preceded_php . addcslashes($search, SCANNER_REGEX_CHARS) . $followed_php;
  }

  // If terms selected, then put together extra join and where clause.
  $join = '';
  if (is_array($terms) && count($terms)) {
    $terms_where = [];
    $terms_params = [];
    foreach ($terms as $term) {
      $terms_where[] = 'tn.tid = %d';
      $terms_params[] = $term;
    }
    $join = 'INNER JOIN {taxonomy_term_node} tn ON t.nid = tn.nid';
    $where .= ' AND (' . implode(' OR ', $terms_where) . ')';
  }

  $table_map = _scanner_get_selected_tables_map();
  //dpm($table_map);
  usort($table_map, '_scanner_compare_fields_by_name');

  // Examine each field instance as chosen in settings.
  foreach ($table_map as $map) {
    $table = $map['table'];
    $field = $map['field'];
    $field_label = $map['field_label'];
    $type = $map['type'];
    $module = $map['module'] ?? NULL;
    $field_collection_parents = $map['field_collection_parents'] ?? NULL;

    // Allow the table suffix to be altered.
    $suffix = 'value';
    // Trigger hook_scanner_field_suffix_alter().
    backdrop_alter('scanner_field_suffix', $suffix, $map);

    $query = db_select($table, 't');
    if ($table == 'node_revision') {
      $vid = 'vid';
    }
    else {
      $field = $field . '_' . $suffix;
      $field_label = $field_label . '_' . $suffix;
      $vid = 'revision_id';
    }
    if (!empty($field_collection_parents)) {
      $cnt_fc = count($field_collection_parents);

      // Loop through the parents backwards, so that the joins can all be
      // created.
      for ($i = $cnt_fc; $i > 0; $i--) {
        $fc_this = $field_collection_parents[$i - 1];
        $fc_alias = 'fc' . ($i - 1);
        $fc_table = 'field_revision_' . $fc_this;
        $prev_alias = ($i == $cnt_fc) ? 't' : ('fc' . $i);
        $query->join($fc_table, $fc_alias,
          format_string('!PREV_ALIAS.entity_id = !FC_ALIAS.!FC_THIS_value AND !PREV_ALIAS.revision_id = !FC_ALIAS.!FC_THIS_!FC_REV',
            [
              '!FC_ALIAS' => $fc_alias,
              '!FC_THIS' => $fc_this,
              '!PREV_ALIAS' => $prev_alias,
              '!FC_REV' => $fc_revision_field,
            ]));
        if ($i == 1) {
          $query->join('node', 'n',
            format_string('!FC_ALIAS.entity_id = n.nid AND !FC_ALIAS.revision_id = n.vid', ['!FC_ALIAS' => $fc_alias]));
        }
      }
    }
    else {
      // Only add the node join if the field is not paragraph-specific.
      if (strpos($type, 'paragraph:') === FALSE) {
        $query->join('node', 'n', 't.' . $vid . ' = n.vid');
      }
    }
    if (is_array($terms) && !empty($terms)) {
      $terms_or = db_or();
      $query->join('taxonomy_index', 'tx', 'n.nid = tx.nid');
      foreach ($terms as $term) {
        $terms_or->condition('tx.tid', $term);
      }
      $query->condition($terms_or);
    }
    // Add correct suffix for paragraph fields.
    if (strpos($map['type'], 'paragraph:') !== FALSE) {
      $query->distinct();

      // Ensure '_value' is only added if not already present.
      if (!str_ends_with($field, '_value')) {
        $field .= '_value';
      }

      // Add fields to SELECT for paragraphs.
      $query->addField('t', $field, 'content');
      $query->addField('t', 'bundle', 'bundle');
      $query->fields('t', ['delta']);
      $query->addField('pi', 'item_id', 'entity_id'); // Add entity_id for paragraph replacement.

      // Join with paragraphs_item table.
      $query->join('paragraphs_item', 'pi', 'pi.item_id = t.entity_id AND pi.revision_id = t.revision_id');

      // Dynamically determine the field_name for the paragraphs reference.
      $field_name = db_query("
        SELECT DISTINCT field_name
        FROM {paragraphs_item}
        WHERE bundle = :bundle
        LIMIT 1
    ", [':bundle' => str_replace('paragraph:', '', $map['type'])])->fetchField();

      if (!$field_name) {
        throw new Exception('Could not determine the field name for paragraphs.');
      }

      // Construct the paragraphs field table dynamically.
      $paragraphs_field_table = 'field_data_' . $field_name;

      // Join with the dynamically determined paragraphs field table.
      $query->join($paragraphs_field_table, 'fp', "fp.{$field_name}_value = pi.item_id");

      // Join with the node table to get node information.
      $query->join('node', 'n', 'n.nid = fp.entity_id');

      // Add node-specific fields.
      $query->addField('n', 'nid', 'nid');
      $query->addField('n', 'title', 'title');

      // Filter by paragraph bundle.
      $query->condition('t.bundle', str_replace('paragraph:', '', $type), '=');
    } else {
      // Regular node-based fields.
      $query->addField('t', $field, 'content');

      if ($table != 'node_revision') {
        $query->fields('t', ['delta']);
      }

      // Add node-specific fields.
      $query->fields('n', ['nid', 'title']);
      $query->condition('n.type', $type, '=');

      // Filter by node type.
      $query->condition('n.type', $type, '=');

      // Retrieve the search term from the session.
      $search_term = $_SESSION['scanner_search'] ?? '';
      if (empty($search_term)) {
        throw new Exception('Search term is required but not provided.');
      }
      //dpm($search_term, 'Search Term');

      // Properly construct the CONVERT statement with enforced spacing.
      $charset_conversion = "CONVERT(t.$field USING utf8mb4)";
      //dpm($charset_conversion, 'Charset Conversion');

      // Build the base where condition.
      $query->where("(CONVERT(t.$field USING utf8mb4) REGEXP :search_term)", [':search_term' => $search_term]);

      // Check if a summary field exists and add its condition directly.
      if (!empty($summary_field)) {
        $query->where("(CONVERT(t.$summary_field USING utf8mb4) REGEXP :search_term)", [':search_term' => $search_term]);
      }

      // Add debugging to ensure conditions are constructed correctly.
      //dpm((string) $query, 'Query After Adding Conditions');


      if ($published) {
        $query->condition('n.status', '1', '=');
      }
    }


    // Triggers hook_scanner_query_alter().
    // Allow the query to be modified.
    backdrop_alter('scanner_query', $query, $map, $table, $field);
    // Debug the constructed SQL query.
    //dpm($field_name, 'Dynamic Paragraph Field Name');

    //dpm((string) $query, 'Constructed SQL Query');
    $result = $query->execute();
    $shutting_down = FALSE;

    // Perform the search or replace on each hit for the current field instance.
    foreach ($result as $row) {
      //dpm($row);
      // Results of an entity property, e.g. the node title, won't have a
      // 'delta' attribute, so make sure there is one.
      if (!isset($row->delta)) {
        $row->delta = 0;
      }

      $content = $row->content;
      $summary = $row->summary ?? '';
      $matches = [];
      $text = '';

      // If the max_execution_time setting has been set then check for possible
      // timeout. If within 5 seconds of timeout, attempt to expand environment.
      if ($max_execution_time > 0 && REQUEST_TIME >= ($start_time + $max_execution_time - 5)) {
        if (!$expanded) {
          if ($user->uid > 0) {
            $verbose = TRUE;
          }
          else {
            $verbose = FALSE;
          }
          if (_scanner_change_env('max_execution_time', '600', $verbose)) {
            backdrop_set_message(t('Default max_execution_time too small and changed to 10 minutes.'), 'error');
            $max_execution_time = 600;
          }
          $expanded = TRUE;
        }
        // If expanded environment still running out of time, shutdown process.
        else {
          $shutting_down = TRUE;
          $config->set('scanner_partially_processed_' . $user->uid, $processed);
          $config->set('scanner_partial_undo_' . $user->uid, $undo_data);
          $config->save();
          if ($searchtype == 'search') {
            backdrop_set_message(t('Did not have enough time to complete search.'), 'error');
          }
          else {
            backdrop_set_message(t('Did not have enough time to complete. Please re-submit replace'), 'error');
          }
          break 2;
        }
      }

      $node = node_load($row->nid);

      //dpm($content, 'Search Target');
      //dpm($flag, 'Regex Flag');

      // Build the regular expression used later.
      $regexstr = "/$search_php/$flag";
      //dpm($regexstr);

      // Search.
      if ($searchtype == 'search') {
        $matches = ['0' => []];
        $hits = 0;

        // Assign matches in the base text field to $matches[0].
        // Trigger hook_scanner_preg_match_all().
        foreach (module_implements('scanner_preg_match_all') as $module_name) {
          $function = $module_name . '_scanner_preg_match_all';
          $new_matches = [];
          $hits += $function($new_matches, $regexstr, $row);
          $matches = array_merge($matches, $new_matches);
        }

        if ($hits > 0) {
          $context_length = 70;
          $text .= '<ul>';
          foreach ($matches as $key => $item) {
            $string = $key == 0 ? $content : $summary;
            foreach ($item as $match) {
              //dpm($match, "Individual Match");
              $text .= '<li>';
              // If ($key == 1) {
              // $text .= '<i>Summary:</i> ';
              // }.
              // Don't want substr to wrap.
              $start = $match[1] - $context_length > 0 ? $match[1]
                - $context_length : 0;
              // If the match is close to the beginning of the string, need
              // less context.
              $length = $match[1] >= $context_length ? $context_length
                : $match[1];

              if ($prepend = substr($string, $start, $length)) {
                if ($length == $context_length) {
                  $text .= '...';
                }
                $text .= htmlentities($prepend, ENT_COMPAT, 'UTF-8');
              }

              $text .= '<strong>' . htmlentities($match[0], ENT_COMPAT, 'UTF-8')
                . '</strong>';

              if ($append = substr(
                $string,
                $match[1] + strlen($match[0]),
                $context_length
              )
              ) {
                $text .= htmlentities($append, ENT_COMPAT, 'UTF-8');
                if (strlen($string) - ($match[1] + strlen($match[0]))
                  > $context_length
                ) {
                  $text .= '...';
                }
              }
              $text .= '</li>';
            }
          }
          $text .= '</ul>';
        }
        else {
          $text
            = '<div class="messages warning"><h2 class="element-invisible">Warning message</h2>'
            . t(
              "Can't display search result due to conflict between search term and internal preg_match_all function."
            )
            . '</div>';
        }

        if ($hits > 0) {
          $results[] = [
            'title' => $row->title,
            'type' => $type,
            'count' => $hits,
            'field' => $field,
            'field_label' => $field_label,
            'nid' => $row->nid,
            'text' => $text,
          ];
          //dpm($results, 'Results Updated');
        }
        else {
          //dpm('No matches for this field, skipping.', 'Skip Adding to
          // Results');
        }
      }


        // Replace (and check to see if already processed).
      elseif (!isset($processed[$field][$row->nid][$row->delta])) {
        // Check first if pathauto_persist, a newer version of pathauto, or some
        // other module has already set $node->path['pathauto']. If not, set it
        // to false (to prevent pathauto from touching the node during
        // node_save()) if a custom alias exists that doesn't follow pathauto
        // rules.
        if (!isset($node->path['pathauto']) && module_exists('pathauto') && $pathauto) {
          [$id, , $bundle] = entity_extract_ids('node', $node);
          if (!empty($id)) {
            module_load_include('inc', 'pathauto');
            $uri = entity_uri('node', $node);
            $path = backdrop_get_path_alias($uri['path']);
            $pathauto_alias = pathauto_create_alias('node', 'return', $uri['path'], ['node' => $node], $bundle);
            $node->path['pathauto'] = ($path != $uri['path'] && $path == $pathauto_alias);
          }
        }

        $hits = 0;
        preg_match('/(.+)_' . $suffix . '$/', $field, $matches);

        // Paragraphs.
        if (strpos($type, 'paragraph:') !== FALSE) {
          // Load the paragraph entity.
          $paragraph = paragraphs_item_load($row->entity_id);
          if (!$paragraph) {
            backdrop_set_message(t('Failed to load paragraph with ID %id', ['%id' => $row->entity_id]), 'error');
            continue;
          }

          // Extract the base field name from the full field name (e.g., "field_text_in_the_paragraph").
          preg_match('/(.+)_value$/', $field, $matches);
          $base_field_name = $matches[1] ?? $field;

          // Validate the existence of the base field in the paragraph entity.
          if (!isset($paragraph->{$base_field_name})) {
            backdrop_set_message(t('Field %field does not exist on paragraph %id', [
              '%field' => $base_field_name,
              '%id' => $row->entity_id,
            ]), 'warning');
            continue;
          }

          // Prepare replacement.
          $hits = 0;
          $field_data = &$paragraph->{$base_field_name};

          foreach ($field_data as $langcode => &$values) {
            foreach ($values as &$value) {
              if (isset($value['value'])) {
                $value['value'] = preg_replace($regexstr, $replace, $value['value'], -1, $hit_count);
                $hits += $hit_count;
              }

              if (isset($value['summary'])) {
                $value['summary'] = preg_replace($regexstr, $replace, $value['summary'], -1, $hit_count);
                $hits += $hit_count;
              }
            }
          }

          // Update the paragraph's host entity dynamically and save the paragraph.
          $host_node = $paragraph->hostEntity();
          if ($host_node) {
            $paragraph->setHostEntity('node', $host_node);
          }

          $paragraph->save();

          // Add results for logging.
          $results[] = [
            'title' => $host_node->title ?? 'Unknown Host',
            'nid' => $host_node->nid ?? 'N/A',
            'type' => $type,
            'count' => $hits,
            'field' => $field,
            'field_label' => $field_label,
          ];

          continue;
        }

        // Field collections.
        if (!empty($field_collection_parents)) {
          foreach ($node->{$field_collection_parents[0]} as $fc_lang => $fc_data) {
            foreach ($fc_data as $key => $fc_item) {
              $fc = field_collection_item_load($fc_item['value']);
              $fc_changed = FALSE;
              foreach ($fc->{$matches[1]}[LANGUAGE_NONE] as $fc_key => $fc_val) {
                $fc_hits = 0;
                $fc_content = preg_replace($regexstr, $replace, $fc_val[$suffix], -1, $fc_hits);
                if ($fc_content != $fc_val['value']) {
                  $fc_changed = TRUE;
                  $fc->{$matches[1]}[LANGUAGE_NONE][$fc_key][$suffix] = $fc_content;
                }

                // Also need to handle the summary part of text+summary fields.
                if (isset($fc_val['summary'])) {
                  $summary_hits = 0;
                  $fc_summary = preg_replace($regexstr, $replace, $fc_val['summary'], -1, $summary_hits);
                  if ($fc_summary != $fc_val['summary']) {
                    $fc_hits += $summary_hits;
                    $fc_changed = TRUE;
                    $fc->{$matches[1]}[LANGUAGE_NONE][$fc_key]['summary'] = $fc_summary;
                  }
                }

                if ($fc_hits > 0) {
                  $results[] = [
                    'title' => $node->title,
                    'type' => $node->type,
                    'count' => $fc_hits,
                    'field' => $field,
                    'field_label' => $field_label,
                    'nid' => $node->nid,
                  ];
                }
              }

              // If field collection revision handling is enabled, update the
              // revision ID on the field.
              // @todo Handle scenarios were the same FC is updated multiple
              // times on the same request.
              if ($fc_revision_field == 'revision_id') {
                $fc->revision = 1;
              }

              // Update the field collection.
              $fc->save(TRUE);

              // If field collection revision handling is enabled, update the
              // revision ID on the field; the entity's revision_id is updated
              // during the save() method, so this is safe to do.
              if ($fc_revision_field == 'revision_id') {
                $node->{$field_collection_parents[0]}[$fc_lang][$key]['revision_id'] = $fc->revision_id;
              }
            }
          }
        }

        // Normal node fields.
        else {
          // Skip processing if this is a paragraph field.
          if (strpos($type, 'paragraph:') !== FALSE) {
            continue;
          }

          // Normal node fields.
          $hits = 0;

          // Trigger hook_scanner_preg_replace().
          foreach (module_implements('scanner_preg_replace') as $module_name) {
            $function = $module_name . '_scanner_preg_replace';
            $hits += $function($node, $field, $matches, $row, $regexstr, $replace);
          }

          // Update the counter.
          $results[] = [
            'title' => $node->title,
            'nid' => $node->nid,
            'type' => $node->type,
            'count' => $hits,
            'field' => $field,
            'field_label' => $field_label,
          ];
        }

        // A revision only created for the first change of the node. Subsequent
        // changes of the same node do not generate additional revisions.
        // @todo Need a better way of handling this.
        if (!isset($undo_data[$node->nid]['new_vid'])) {
          $node->revision = TRUE;
          $node->log = t('@name replaced %search with %replace via Scanner Search and Replace module.', [
            '@name' => $user->name,
            '%search' => $search,
            '%replace' => $replace,
          ]);
          $undo_data[$node->nid]['old_vid'] = $node->vid;
        }
        node_save($node);

        // Array to log completed fields in case of shutdown.
        $processed[$field][$row->nid][$row->delta] = TRUE;

        // Undo data construction.
        // Now set to updated vid after node_save().
        $undo_data[$node->nid]['new_vid'] = $node->vid;
      }
    }
  }

  // If completed.
  if (isset($shutting_down) && !$shutting_down) {
    $config->clear('scanner_partially_processed_' . $user->uid);
    $config->clear('scanner_partial_undo_' . $user->uid);
    $config->save();
  }

  if ($searchtype == 'search') {
    return theme('scanner_results', ['results' => $results]);
  }
  // Searchtype == 'replace'.
  // Searchtype == 'replace'.
  else {
    // Ensure $undo_data is initialized as an array if it is null or undefined.
    $undo_data = $undo_data ?? [];

    if (count($undo_data) && !$shutting_down) {
      db_insert('scanner')
        ->fields([
          'undo_data' => serialize($undo_data),
          'undone' => 0,
          'searched' => $search,
          'replaced' => $replace,
          'count' => count($undo_data),
          'time' => REQUEST_TIME,
        ])
        ->execute();
    }

    return theme('scanner_replace_results', ['results' => $results]);
  }

}

/**
 * Get all text fields.
 *
 * @return array
 *   List of all fields, each of which is an array containing relevant data
 *   used for diplaying/querying.
 */
/**
 * Get all text fields.
 *
 * @return array
 *   List of all fields, each of which is an array containing relevant data
 *   used for displaying/querying.
 */

/**
 * Get all text fields.
 *
 * @return array
 *   List of all fields, each of which is an array containing relevant data
 *   used for displaying/querying.
 */
function _scanner_get_all_tables_map() {
  $tables_map = [];

  // Add node fields, including the title.
  foreach (node_type_get_types() as $node_type) {
    // Add the title field explicitly.
    $tables_map[] = [
      'type' => $node_type->type,
      'field' => 'title',
      'field_label' => t('Title'),
      'table' => 'node_revision',
    ];

    $field_instances = field_info_instances('node', $node_type->type);
    foreach ($field_instances as $field_name => $field_instance) {
      $field_info = field_info_field($field_name);
      if (in_array($field_info['type'], ['text', 'text_long', 'text_with_summary'])) {
        $tables_map[] = [
          'type' => $node_type->type,
          'field' => $field_name,
          'field_label' => $field_instance['label'],
          'table' => 'field_revision_' . $field_name,
        ];
      }
    }
  }

  // Add paragraph fields.
  $paragraph_bundles = field_info_bundles('paragraphs_item');
  foreach ($paragraph_bundles as $bundle_name => $bundle_info) {
    $field_instances = field_info_instances('paragraphs_item', $bundle_name);
    foreach ($field_instances as $field_name => $instance) {
      $field_info = field_info_field($field_name);
      if (in_array($field_info['type'], ['text', 'text_long', 'text_with_summary'])) {
        $tables_map[] = [
          'type' => 'paragraph:' . $bundle_name,
          'field' => $field_name,
          'field_label' => $instance['label'],
          'table' => 'field_revision_' . $field_name,
        ];
      }
    }
  }

  // Trigger hook_scanner_fields_alter().
  backdrop_alter('scanner_fields', $tables_map);

  //dpm($tables_map);
  return $tables_map;
}

/**
 * Get the fields that have been selected for scanning.
 *
 * @return array
 *   Map of selected fields and tables.
 */
/**
 * Get the fields that have been selected for scanning.
 *
 * @return array
 *   Map of selected fields and tables.
 */
function _scanner_get_selected_tables_map() {
  $tables_map = _scanner_get_all_tables_map();
  //dpm($tables_map);
  $config = config('scanner.settings');

  /*foreach ($tables_map as $i => $item) {
    // Handle node fields.
    if (strpos($item['type'], 'paragraph:') === 0) {
      // Handle paragraph fields separately.
      $bundle_name = str_replace('paragraph:', '', $item['type']);
      $key = 'scanner_paragraphs_' . $item['field'] . '_' . $item['table'] . '_' . $bundle_name;
    } else {
      // Handle node fields.
      $key = 'scanner_nodes_' . $item['field'] . '_' . $item['table'] . '_' . $item['type'];
    }

    // Check if the field is enabled in the configuration.
    if (!$config->get($key, TRUE)) {
      unset($tables_map[$i]);
    }
  }*/

  foreach ($tables_map as $i => $item) {
    $key = 'scanner_' . $item['field'] . '_' . $item['table'] . '_' . $item['type'];
    if (!$config->get($key, TRUE)) {
      unset($tables_map[$i]);
    }
  }

  //dpm($tables_map);
  // Reindex array to maintain order and avoid gaps.
  return array_values($tables_map);
}
