<?php

/**
 * @file
 * Field for geospatial data using the Field API.
 */

/**
 * Implements hook_field_info().
 */
function postgis_field_info() {
  return array(
    'postgis' => array(
      'label' => t('Geospatial data'),
      'description' => t('This field stores geospatial data in the database.'),
      'settings' => array(
        'type' => 'point',
        'srid' => POSTGIS_DEFAULT_SRID,
      ),
      'instance_settings' => array(),
      'default_widget' => 'wkt',
      'default_formatter' => 'wkt',
    ),
  );
}

/**
 * Form that is shown when a postgis field is added to a content type.
 * 
 * Implements hook_field_settings_form().
 */
function postgis_field_settings_form($field, $instance, $has_data) {

  $types = array(
    'point' => t('Point'),
    'linestring' => t('Linestring'),
    'polygon' => t('Polygon'),
    'multipoint' => t('Multi Point'),
    'multilinestring' => t('Multi Linestring'),
    'multipolygon'  => t('Multi Polygon'),
    'geometrycollection' => t('Geometry Collection'),
  );

  $form['type'] = array(
    '#type' => 'select',
    '#title' => t('Geometry type'),
    '#options' => $types,
    '#default_value' => $field['settings']['type'],
    '#description' => t('Choose the geometry that best describes the information you are storing. "Point" is suitable for addresses and other fixed points, but you may also choose "Linestring" for lines or tracks, and "Polygon" for shapes and areas. If you have different types of geometry choose "Geometry Collection". To store multiple geometries of the same type in one table row the "Multi" geometry types and "Geometry Collection" are available.'),
    '#disabled' => $has_data,
  );

  $form['srid'] = array(
    '#type' => 'textfield',
    '#title' => t('SRID'),
    '#default_value' => $field['settings']['srid'],
    '#description' => t('Select the map projection that shall be used to map the globe to a plain surface. In general 4326 fits the whole globe pretty well but local projections might give more precise results for specific areas (at the cost of more distortions in other areas).'),
    '#disabled' => $has_data,
  );

  return $form;
}

/**
 * Implements hook_field_validate().
 */
function postgis_field_validate($entity_type, $entity, $field, $instance, $langcode, $items, &$errors) {
  foreach ($items as $delta => $item) {
    // Catch empty input.
    $wkt = isset($item['wkt']) ? $item['wkt'] : '';

    $set = new PostgisGeometrySet($field['settings']['type'], $field['settings']['srid'], $field['cardinality']);
    $set->fromText($wkt);

    $validationError = $set->validate($field['field_name']);
    // Add validation error if validation returned an array with error description
    if (is_array($validationError)) {
      $errors[$field['field_name']][$langcode][$delta][] = $validationError;
    }
  }
}

/**
 * Implements hook_field_is_empty().
 */
function postgis_field_is_empty($item, $field) {

  $set = new PostgisGeometrySet($field['settings']['type'], $field['settings']['srid']);
  $set->fromText($item['wkt']);

  return $set->isEmpty();
}


/**
 * Combines entries from multiple items in a single widgets into a single collection.
 * 
 * Implements hook_field_presave().
 */
function postgis_field_presave($entity_type, $entity, $field, $instance, $langcode, &$items) {
  $set = new PostgisGeometrySet($field['settings']['type'], $field['settings']['srid']);
  foreach ($items as $delta => $item) {
    // Handle WKT as well as WKB from mass updates of nodes.
    if (isset($item['wkt']) && is_string($item['wkt'])) {
      $set->fromText($item['wkt']);
    }
    else {
      $set->fromGeometry($item);
    }
  }
  $items = $set->getItems();
}


/**
 * Implements hook_field_update_field().
 *
 * Keep table "geometry_columns" up to date.
 *
 */
function postgis_field_update_field($field, $prior_field, $has_data) {

  if (!$has_data && isset($field['settings']['srid'])) {

    $srid = $field['settings']['srid'];
    $type = drupal_strtoupper($field['settings']['type']);
    $storage = field_sql_storage_field_storage_details($field);
    $connection = Database::getConnection();

    foreach ($storage['sql'] as $tables) {

      foreach ($tables as $table => $columns) {

        $prefix = $connection->tablePrefix($table);

        foreach ($columns as $field => $column) {

          db_drop_field($table, $column);

          db_query(
            'SELECT AddGeometryColumn(:prefix, :table, :column, :srid, :type, 2)',
            array(
              ':prefix' => trim($prefix, '.'),
              ':table' => $table,
              ':column' => $column,
              ':srid' => $srid,
              ':type' => $type,
            )
          );

          db_query('CREATE INDEX ' . $table . '_' . $column . '_idx ON {' . $table . '} USING GIST ( ' . $column . ' )');
        }
      }
    }
  }
}

/**
 * Get spatial extent of a given field.
 */
function postgis_field_extent($field) {
  if ($field['type'] === 'postgis') {
    $connection = Database::getConnection();
    $prefix = $connection->tablePrefix('field_data_' . $field['field_name']);
    $extent = db_query('SELECT ST_Extent(' . $field['field_name'] . '_geometry) from ' . $prefix . 'field_data_' . $field['field_name'] . ';')->fetchField();
    $bbox = preg_split('/[\, ]/', drupal_substr($extent, 4, -1));
    return (object) array(
      'minx' => $bbox[0],
      'miny' => $bbox[1],
      'maxx' => $bbox[2],
      'maxy' => $bbox[3],
    );
  }
}