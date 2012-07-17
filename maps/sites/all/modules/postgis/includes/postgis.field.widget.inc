<?php

/**
 * @file
 * Field for geospatial data using the Field API. Implements field widget.
 */

/**
 * Implements hook_field_widget_info().
 */
function postgis_field_widget_info() {

  $widgets = array(
    'wkt' => array(
      'label' => t('Textfield'),
      'field types' => array('postgis'),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_CUSTOM,
        'default value'   => FIELD_BEHAVIOR_NONE,
      ),
    ),
  );

  if (module_exists('openlayers')) {
    $widgets['openlayers'] = array(
      'label' => t('OpenLayers Map'),
      'field types' => array('postgis'),
      'settings' => array(
        'map'      => 'postgis_widget_map',
        'behavior' => 'openlayers_behavior_drawfeatures',
      ),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_CUSTOM,
        'default value'   => FIELD_BEHAVIOR_NONE,
      ),
    );
  }

  return $widgets;
}

/**
 * Implements hook_field_widget_settings_form().
 */
function postgis_field_widget_settings_form($field, $instance) {

  $widget = $instance['widget'];
  $settings = $widget['settings'];
  $form = array();

  if ($widget['type'] == 'openlayers') {

    $maps = openlayers_map_options();
    $map = isset($settings['map']) ? $settings['map'] : 'postgis_widget_map';

    $form['map'] = array(
      '#type' => 'select',
      '#title' => t('OpenLayers Map'),
      '#default_value' => $map,
      '#options' => $maps,
      '#description' => t('Select which OpenLayers map you would like to use.'),
    );

    $behaviors = openlayers_behaviors();
    foreach ($behaviors as $index => $behavior) {
      $behaviors[$index] = $behavior['title'];
    }
    $behavior = isset($settings['behavior']) ? $settings['behavior'] : 'openlayers_behavior_drawfeatures';

    $form['behavior'] = array(
      '#type' => 'select',
      '#title' => t('OpenLayers Behavior'),
      '#default_value' => $behavior,
      '#options' => $behaviors,
      '#description' => t('Select an OpenLayers behavior which handles drawing of features.'),
    );

  }

  return $form;
}

/**
 * Renders map or text field for WKT in content editing screen.
 * 
 * Implements hook_field_widget_form().
 */
function postgis_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {
  $widget = $instance['widget'];
  $settings = $widget['settings'];

  $set = new PostgisGeometrySet($field['settings']['type'], $field['settings']['srid']);
  $set->fromGeometry($items);
  $value = $set->getText();

  $element[$delta] = array(
    '#delta' => $delta,
  );

  if (module_exists('openlayers') && $instance['widget']['type'] == 'openlayers') {

    switch ($field['settings']['type']) {
      case 'point':
      case 'multipoint':
        $types = array('point');
        break;
      case 'linestring':
      case 'multilinestring':
        $types = array('path');
        break;
      case 'polygon':
      case 'multipolygon':
        $types = array('polygon');
        break;
      case 'geometrycollection':
        $types = array('point', 'path', 'polygon');
        break;
      default:
        $types = array();
    }

    $map = openlayers_map_load($settings['map']);
    $behavior = isset($map->data['behaviors'][$settings['behavior']]) ?
        $map->data['behaviors'][$settings['behavior']] : array();

    $textarea_id = $field['field_name'] . '_wkt' . $delta;
    $map->data['behaviors'][$settings['behavior']] = array_merge($behavior, array(
      'element_id'    => $textarea_id,
      'srid'          => $field['settings']['srid'],
      'feature_types' => $types,
      // Drupal's unlimited value of -1 is not known to OpenLayers and thus needs to be converted to NULL
      'feature_limit' => ($field['cardinality'] > 0) ? $field['cardinality'] : NULL,
    ));

    $element[$delta]['map'] = array(
      '#title' => $instance['label'],
      '#type' => 'item',
      '#markup' => '<div class="form-item" style="display:block">' . openlayers_render_map($map->data) . '</div>',
      '#description' => $element['#description'],
    );

    $element[$delta]['wkt'] = array(
      '#type' => 'textarea',
      '#attributes' => array('id' => $textarea_id, 'style' => 'display: none;'),
      // Using a format that OpenLayers can handle
      '#default_value' => ($value === 'GEOMETRYCOLLECTION(EMPTY)') ? '' : $value,
      '#required' => $element['#required'],
      '#resizable' => FALSE,
    );

    // Remove empty GEOMETRYCOLLECTION coming from OpenLayers.
    $element[$delta]['script'] = array(
      '#markup' => '<script type="text/javascript">(function($){var field = $("#'.$textarea_id.'"); if (field.val() == "GEOMETRYCOLLECTION()") {field.html("")}})(jQuery);</script>',
    );
  }
  else {
    $element[$delta]['wkt'] = array(
      '#title' => t('Well Known Text'),
      '#type' => 'textfield',
      '#default_value' => $value,
      '#required' => $element['#required'],
    );
  }
  return $element;
}

