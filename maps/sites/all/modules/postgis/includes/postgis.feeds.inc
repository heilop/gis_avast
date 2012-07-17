<?php

/**
 * @file
 * Basic Feeds API implementation.
 */

/**
 * Implements hook_feeds_node_processor_targets_alter().
 */
function postgis_feeds_processor_targets_alter(&$targets, $entity_type, $bundle_name) {

  foreach (field_info_instances($entity_type, $bundle_name) as $name => $instance) {
    $info = field_info_field($name);
    if ($info['type'] == 'postgis') {
      $targets[$name] = array(
        'name' => t($instance['label']),
        'callback' => 'postgis_feeds_set_target',
      );
    }
  }

}

/**
 * Callback function for Feeds processor to convert KML geometries to WKT.
 */
function postgis_feeds_set_target($source, $entity, $target, $value) {

  $geo = db_query("SELECT ST_AsText(ST_CollectionExtract(ST_GeomFromKML(:kml),2));",
          array(':kml' => $value))->fetchField();

  $entity->{$target}[$entity->language][0] = $geo;
}

