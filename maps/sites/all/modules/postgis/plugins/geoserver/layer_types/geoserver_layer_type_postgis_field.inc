<?php
/**
 * @file
 * GeoServer layer type for PostGIS fields.
 */

/**
 * Define the Ctools plugin options.
 */
$plugin = array(
  'title' => t('PostGIS'),
  'description' => t('An entity containing a PostGIS field.'),
  'layer_type' => array(
    'file' => 'geoserver_layer_type_postgis_field.inc',
    'class' => 'geoserver_layer_type_postgis_field',
    'parent' => 'geoserver_layer_type',
  ),
);

/**
 * GeoServer PostGIS layer class.
 */
class geoserver_layer_type_postgis_field extends geoserver_layer_type_feature_type {

  /**
   * Constructor.
   */
  function __construct($layer = array()) {
    parent::__construct($layer);
    $this->datastore = variable_get('geoserver_postgis_datastore', '');
  }

  /**
   * Import from remote feature type definition.
   */
  function from_feature_type($data) {

    $feature_type = isset($data->featureType) ? $data->featureType : new stdClass();

    if (isset($feature_type->metadata) && isset($feature_type->metadata->entry)) {
      foreach ($feature_type->metadata->entry as $entry) {
        if (isset($entry->virtualTable)) {
          $virtual_table = $entry->virtualTable;
          break;
        }
      }
    }
    $virtual_table = isset($virtual_table) ? $virtual_table : new stdClass();
    $geometry = isset($virtual_table->geometry) ? $virtual_table->geometry : new stdClass();
    $bbox = isset($feature_type->nativeBoundingBox) ? $feature_type->nativeBoundingBox : new stdClass();

    $this->name = isset($feature_type->name) ? $feature_type->name : '';
    $this->title = isset($feature_type->title) ? $feature_type->title : '';
    $this->data['options']['srid'] = isset($feature_type->srs) ? drupal_substr($feature_type->srs, 5) : '';
    $this->data['options']['geometry_name'] = isset($geometry->name) ? $geometry->name : '';
    $this->data['options']['geometry_type'] = isset($geometry->type) ? $geometry->type : '';
    $this->data['options']['bounding_box'] = array(
      'minx' => isset($bbox->minx) ? $bbox->minx : '',
      'miny' => isset($bbox->miny) ? $bbox->miny : '',
      'maxx' => isset($bbox->maxx) ? $bbox->maxx : '',
      'maxy' => isset($bbox->maxy) ? $bbox->maxy : '',
    );
    $this->data['options']['sql'] = isset($virtual_table->sql) ? $virtual_table->sql : '';
  }

  /**
   * Export to remote feature type definition.
   */
  function to_feature_type() {

    $options = isset($this->data['options']) ? $this->data['options'] : array();
    $bbox = isset($options['bounding_box']) ? $options['bounding_box'] : array();

    return array(
      'featureType' => array(
        'name'       => $this->name,
        'nativeName' => $this->name,
        'namespace'  => array('name' => $this->workspace),
        'title'      => $this->title,
        'nativeCRS'  => isset($options['srid']) ? 'EPSG:' . $options['srid'] : '',
        'srs'        => isset($options['srid']) ? 'EPSG:' . $options['srid'] : '',
        'nativeBoundingBox' => array(
          'minx' => isset($bbox['minx']) ? $bbox['minx'] : '',
          'miny' => isset($bbox['miny']) ? $bbox['miny'] : '',
          'maxx' => isset($bbox['maxx']) ? $bbox['maxx'] : '',
          'maxy' => isset($bbox['maxy']) ? $bbox['maxy'] : '',
        ),
        'latLonBoundingBox' => array(
          'minx' => isset($bbox['minx']) ? $bbox['minx'] : '',
          'miny' => isset($bbox['miny']) ? $bbox['miny'] : '',
          'maxx' => isset($bbox['maxx']) ? $bbox['maxx'] : '',
          'maxy' => isset($bbox['maxy']) ? $bbox['maxy'] : '',
        ),
        'projectionPolicy' => 'FORCE_DECLARED',
        'enabled' => TRUE,
        'metadata' => array(
          'entry' => array(
            array(
              '@key' => 'cachingEnabled',
              '$' => 'false',
            ),
            array(
              '@key' => 'JDBC_VIRTUAL_TABLE',
              'virtualTable' => array(
                'name' => $this->name,
                'sql' => isset($options['sql']) ? $options['sql'] : '',
                'keyColumn' => 'id',
                'geometry' => array(
                  'name' => isset($options['geometry_name']) ? $options['geometry_name'] : '',
                  'type' => isset($options['geometry_type']) ? $options['geometry_type'] : '',
                  'srid' => isset($options['srid']) ? $options['srid'] : '',
                ),
              ),
            ),
          ),
        ),
        'store' => array(
          '@class' => 'dataStore',
          'name' => $this->datastore,
        ),
        'maxFeatures' => 0,
        'numDecimals' => 0,
        'attributes' => array(),
      ),
    );
  }

  /**
   * Load field configuration.
   */
  function load_field($entity_type, $bundle_name, $field_name) {

    // Map geometry types from PostGIS to GeoServer.
    $mapping = array(
      'point' => 'Point',
      'linestring' => 'LineString',
      'polygon' => 'Polygon',
      'multipoint' => 'MultiPoint',
      'multilinestring' => 'MultiLineString',
      'multipolygon'  => 'MultiPolygon',
      'geometrycollection' => 'GeometryCollection',
    );
    $field = field_info_field($field_name);
    $bbox = postgis_field_extent($field);

    $this->data['options']['srid'] = $field['settings']['srid'];
    $this->data['options']['geometry_name'] = $field_name . '_geometry';
    $this->data['options']['geometry_type'] = $mapping[$field['settings']['type']];
    $this->data['options']['bounding_box']['minx'] = $bbox->minx;
    $this->data['options']['bounding_box']['miny'] = $bbox->miny;
    $this->data['options']['bounding_box']['maxx'] = $bbox->maxx;
    $this->data['options']['bounding_box']['maxy'] = $bbox->maxy;
    $this->data['options']['sql'] = $this->get_sql($entity_type, $bundle_name, $field_name);
  }

  /**
   * Get layer SQL definition.
   */
  protected function get_sql($entity_type, $bundle_name, $field_name) {

    $connection = Database::getConnection();
    $prefix = $connection->tablePrefix();

    $sql = array(
        'fields' => array("SELECT $entity_type.nid as id, $entity_type.title"),
        'tables' => array("\nFROM $prefix$entity_type"),
        'where' => array("\nWHERE $entity_type.type = '$bundle_name' and $entity_type.status = 1"),
    );

    $fields = field_info_instances($entity_type, $bundle_name);

    foreach ($fields as $field_name => $field) {

      $field = field_info_field($field_name);

      if ($field['cardinality'] === '1') {
        foreach ($field['storage']['details']['sql']['FIELD_LOAD_CURRENT'] as $table => $table_fields) {
          foreach ($table_fields as $table_field) {
            $sql['fields'][] = $table_field;
          }
          $sql['tables'][] = "\nLEFT JOIN $prefix$table ON $entity_type.nid = $table.entity_id";
        }
      }
    }
    return implode(', ', $sql['fields']) . ' ' . implode(' ', $sql['tables']) . ' ' . implode(' ', $sql['where']);
  }

  /**
   * Options form which generates layers
   */
  function options_form() {

    $field_options = array('');
    $fields = field_info_instances();

    foreach ($fields as $entity_type => $bundles) {

      $info = entity_get_info($entity_type);

      foreach ($bundles as $bundle_name => $fields) {

        foreach ($fields as $field_name => $field) {

          $field = field_info_field($field_name);

          if ($field['type'] === 'postgis') {
            $key = $entity_type . '.' . $bundle_name . '.' . $field_name;
            $value = $info['bundles'][$bundle_name]['label'] . ' (' . $field_name . ')';
            $field_options[$info['label']][$key] = $value;
            break;
          }
        }
      }
    }

    return array(
      'field' => array(
        '#type' => 'select',
        '#title' => t('Field'),
        '#options' => $field_options,
        '#default_value' => isset($this->data['options']['field']) ?
                $this->data['options']['field'] : '',
        '#ajax' => array(
          'callback' => 'postgis_geoserver_layer_type_form',
          'wrapper' => 'geoserver_layer_type_postgis_field',
        ),
      ),
      'srid' => array(
        '#type' => 'textfield',
        '#title' => t('Projection'),
        '#prefix' => '<div id="geoserver_layer_type_postgis_field">',
        '#default_value' => isset($this->data['options']['srid']) ?
                $this->data['options']['srid'] : '',
      ),
      'geometry_name' => array(
        '#type' => 'textfield',
        '#title' => t('Geometry name'),
        '#default_value' => isset($this->data['options']['geometry_name']) ?
                $this->data['options']['geometry_name'] : '',
      ),
      'geometry_type' => array(
        '#type' => 'select',
        '#title' => t('Geometry type'),
        '#default_value' => isset($this->data['options']['geometry_type']) ?
                $this->data['options']['geometry_type'] : '',
        '#options' => array(
          'Geometry' => 'Geometry',
          'GeometryCollection' => 'GeometryCollection',
          'Point' => 'Point',
          'MultiPoint' => 'MultiPoint',
          'LineString' => 'LineString',
          'MultiLineString' => 'MultiLineString',
          'Polygon' => 'Polygon',
          'MultiPolygon' => 'MultiPolygon',
        ),
      ),
      'bounding_box' => array(
        '#type' => 'fieldset',
        '#title' => t('Bounding Box'),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
        'minx' => array(
          '#type' => 'textfield',
          '#title' => t('Min X'),
          '#default_value' => isset($this->data['options']['bounding_box']['minx']) ?
                  $this->data['options']['bounding_box']['minx'] : '',
        ),
        'miny' => array(
          '#type' => 'textfield',
          '#title' => t('Min Y'),
          '#default_value' => isset($this->data['options']['bounding_box']['miny']) ?
                  $this->data['options']['bounding_box']['miny'] : '',
        ),
        'maxx' => array(
          '#type' => 'textfield',
          '#title' => t('Max X'),
          '#default_value' => isset($this->data['options']['bounding_box']['maxx']) ?
                  $this->data['options']['bounding_box']['maxx'] : '',
        ),
        'maxy' => array(
          '#type' => 'textfield',
          '#title' => t('Max Y'),
          '#default_value' => isset($this->data['options']['bounding_box']['maxy']) ?
                  $this->data['options']['bounding_box']['maxy'] : '',
        ),
      ),
      'sql' => array(
        '#type' => 'textarea',
        '#title' => t('SQL'),
        '#default_value' => isset($this->data['options']['sql']) ?
                $this->data['options']['sql'] : '',
        '#suffix' => '</div>',
      ),
      'sld' => array(
        '#type' => 'textarea',
        '#title' => t('SLD'),
        '#default_value' => isset($this->data['options']['sld']) ?
                $this->data['options']['sld'] : '',
      ),
    );
  }

  /**
   * Layer type settings form.
   */
  function settings_form() {

    $datastores = array('');
    $workspace  = variable_get('geoserver_workspace', '');

    $result = geoserver_get('rest/workspaces/' . $workspace . '/datastores.json');
    if ($result->code == 200 && isset($result->data->dataStores)) {
      foreach ($result->data->dataStores->dataStore as $datastore) {
        $datastores[$datastore->name] = $datastore->name;
      }
    }

    return array(
      'geoserver_postgis_datastore' => array(
        '#type' => 'select',
        '#title' => t('Datastore'),
        '#options' => $datastores,
        '#description' => t('Select the PostGIS datastore where Drupal stores its data. This one needs to be created manually for now.'),
        '#default_value' => variable_get('geoserver_postgis_datastore', ''),
        '#suffix' => '</div>',
      ),
    );
  }
}
