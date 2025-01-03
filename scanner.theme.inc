<?php

/**
 * @file
 * Theme callbacks for the scanner module.
 */

/**
 * Theme the search results.
 */
function theme_scanner_results($variables) {
  $results = $variables['results'];
  $output = NULL;
  if (is_array($results)) {
    $total = count($results);

    backdrop_set_message(filter_xss('Found matches in ' . $total . ' fields. <a href="#results">See below</a> for details.', ['a']));
    $output = '<p>Found matches in ' . $total . ' fields:</p>';
    $output .= '<ol class="scanner-results scanner-search-results">';
    foreach ($results as $item) {
      $output .= theme('scanner_item', ['item' => $item]);
    }
    $output .= '</ol>';
    // @todo use pager to split up results.
  }
  else {
    backdrop_set_message(t('No matches found. Check the !url for fields that can be searched.', ['!url' => l(t('settings'), 'admin/config/content/scanner')]), 'warning');
  }

  return $output;
}

/**
 * Theme each search result hit.
 */
function theme_scanner_item($variables) {
  $output = '';
  $item = $variables['item'];
  $item['count'] = $item['count']; // No need to handle 0 cases anymore.


  $output .= '<li class="scanner-result">';

  // Check if this is a paragraph result.
  if (strpos($item['type'], 'paragraph:') === 0) {
    // Paragraph-specific result.
    $bundle = str_replace('paragraph:', '', $item['type']);
    $output .= '<strong>Paragraph - ' . str_replace('_', ' ', $bundle) . '</strong><br />';
    $output .= '<strong>Field:</strong> ' . $item['field_label'] . '<br />';
    // Use `nid` for view and edit links.
    $output .= check_plain($item['title']) . ' | ' . l(t('view'), 'node/' . $item['nid']) . ' | ' . l(t('edit'), 'node/' . $item['nid'] . '/edit') . '<br />';
    $output .= '[1 match in the field]<br />';
    $output .= $item['text'];
  } else {
    // Node-specific result.
    $output .= '<strong>Node - ' . check_plain($item['type']) . '</strong><br />';
    $output .= '<strong>Field:</strong> ' . $item['field_label'] . '<br />';
    // Use `nid` for view and edit links.
    $output .= check_plain($item['title']) . ' | ' . l(t('view'), 'node/' . $item['nid']) . ' | ' . l(t('edit'), 'node/' . $item['nid'] . '/edit') . '<br />';
    $output .= '[1 match in the field]<br />';
    $output .= $item['text'];
  }

  $output .= '</li>';

  return $output;
}

/**
 * Theme the replace results.
 */
function theme_scanner_replace_results($variables) {
  $results = $variables['results'];
  $output = '';
  if (is_array($results)) {
    backdrop_set_message(filter_xss('Replaced items in ' . count($results) . ' fields. <a href="#results">See below</a> for details.', ['a']));
    $output = '<p>Replaced items in ' . count($results) . ' fields:</p>';
    $output .= '<ol class="scanner-results scanner-replace-results">';
    foreach ($results as $item) {
      $output .= theme('scanner_replace_item', ['item' => $item]);
    }
    $output .= '</ol>';
    // @todo use pager to split up results.
  }
  else {
    backdrop_set_message(t('No matches found. Check the !url for fields that can be searched.', ['!url' => l(t('settings'), 'admin/config/content/scanner')]), 'warning');
  }

  return $output;
}

/**
 * Theme each replace result hit.
 */
function theme_scanner_replace_item($variables) {
  $output = '';
  $item = $variables['item'];
  $item['count'] = $item['count'] > 0 ? $item['count'] : 'One or more';
  $output .= '<li class="scanner-result">';
  $output .= '<span class="scanner-title">' . l($item['title'], 'node/' . $item['nid']) . '</span><br />';
  $output .= '<span class="scanner-info">[' . $item['count'] . ' replacements in ' . $item['type'] . ' ' . $item['field_label'] . ' field]</span>';
  $output .= '</li>';

  return $output;
}
