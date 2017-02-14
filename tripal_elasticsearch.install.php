<?php

/**
 * @file
 * Input, update, and delete data for build search blocks.
 */


/**
 * Implements hook_schema().
 */
function tripal_elasticsearch_schema()
{
    $schema['tripal_elasticsearch'] = array(
        'description' => 'The table for store data for building search blocks',
        'fields' => array(
            'table_name' => array(
                'type' => 'varchar',
                'length' => '255',
            ),
            'table_field' => array(
                'type' => 'varchar',
                'length' => '255',
            ),
            'form_field_type' => array(
                'type' => 'varchar',
                'length' => '255',
            ),
            'form_field_title' => array(
                'type' => 'varchar',
                'length' => '255',
            ),
            'form_field_default_value' => array(
                'type' => 'varchar',
                'length' => '255',
            ),
            'form_field_options' => array(
                'type' => 'text',
                //'length' => '255',
            ),
            'form_field_weight' => array(
                'type' => 'varchar',
                'length' => '255',
            ),
        ),
    );

    $schema['tripal_elasticsearch_add_links'] = array(
        'description' => t('A table for storing data for adding page links to search results'),
        'fields' => array(
            'table_name' => array(
                'type' => 'varchar',
                'length' => '255',
            ),
            'table_field' => array(
                'type' => 'varchar',
                'length' => '255',
            ),
            'page_link' => array(
                'type' => 'varchar',
                'length' => '255',
            ),
        ),
    );

    $schema['tripal_elasticsearch_search_forms'] = [
        'description' => t('A table for storing data for building search forms'),
        'fields' => [
            'table_name' => [
                'type' => 'varchar',
                'length' => '255',
            ],
            'table_field' => [
                'type' => 'varchar',
                'length' => '255',
            ],
            'form_field_label' => [
                'type' => 'varchar',
                'length' => '255',
            ],
            'form_field_type' => [
                'type' => 'varchar',
                'length' => '255',
            ],
            'form_field_weight' => [
                'type' => 'int',
            ]
        ],

    ];

  return $schema;
}
  


