<?php

/**
 * Provides web services support to Elasticsearch server.
 *
 * @file includes/tripal_elasticsearch.ws.inc
 */

/**
 * Report the status of the elastic search index.
 *
 * @return bool
 */
function tripal_elasticsearch_api_v1_status() {
  try {
    $params['v'] = TRUE;
    $es          = new ESInstance();
    $health      = current($es->client->cat()->health($params));
  } catch (Exception $exception) {
    return ESResponse::error(
      [
        'status'  => 'Inactive',
        'healthy' => FALSE,
      ]
    );
  }

  if (empty($health)) {
    return ESResponse::error(
      [
        'status'  => 'Inactive',
        'healthy' => FALSE,
      ]
    );
  }

  return ESResponse::success(
    [
      'status'  => 'Active',
      'healthy' => TRUE,
    ]
  );
}

/**
 * Allow https/http connections to communicate.
 *
 * @param $remote_id
 *
 * @return bool
 */
function tripal_elasticsearch_api_v1_remote_status($remote_id) {
  $remote_id = abs(intval(trim($remote_id)));

  if (!is_int($remote_id)) {
    return ESResponse::error(
      [
        'remote_id' => ['Invalid remote server id'],
      ]
    );
  }

  // Get the remote host url
  $host = db_query(
    'SELECT url FROM {tripal_elasticsearch_servers} WHERE id=:id',
    [':id' => $remote_id]
  )->fetchObject();
  if (!$host) {
    return ESResponse::error(
      [
        'remote_id' => ['Remote server record does not exist'],
      ]
    );
  }

  $url = trim(trim($host->url), '/') . '/elasticsearch/api/v1/status';

  try {
    $response = ESRequest::get($url);
  } catch (Exception $exception) {
    return ESResponse::error(['server' => $exception->getMessage()], 500);
  }

  return ESResponse::success($response->data);
}

/**
 * Send a search request to the given host and return rendered results.
 *
 * @param $remote_host_id
 *
 * @return bool
 */
function tripal_elasticsearch_api_v1_search($remote_host_id) {
  $remote_host_id = abs(intval(trim($remote_host_id)));

  if (!isset($_GET['terms'])) {
    return ESResponse::error(['terms' => ['Please provide search terms']]);
  }

  $size = 0;
  if (isset($_GET['size'])) {
    $size = abs(intval($_GET['size']));
  }

  if ($remote_host_id === 0) {
    $response = tripal_elasticsearch_api_v1_local_search(FALSE);
    $host     = tripal_elasticsearch_get_local_server_entry();
  }
  else {
    $host = db_query(
      'SELECT * FROM {tripal_elasticsearch_servers} WHERE id=:id',
      [':id' => $remote_host_id]
    )->fetchObject();
    $url  = rtrim(trim($host->url), '/') . '/elasticsearch/api/v1/local-search';

    try {
      $response = ESRequest::get(
        $url, [
          'terms'    => $_GET['terms'],
          'category' => isset($_GET['category']) ? $_GET['category'] : '',
          'size'     => $size,
        ]
      );
    } catch (Exception $exception) {
      return ESResponse::error(['server' => $exception->getMessage()], 500);
    }
  }

  $markup = tripal_elasticsearch_get_website_search_result_table(
    $response->data->results, FALSE, $host->url
  );

  // Render results into fields and send them back
  return ESResponse::success(
    [
      'count'  => $response->data->count,
      'url'    => $response->data->url,
      'markup' => $markup,
    ]
  );
}

/**
 * Perform a local search.
 * This method is normally called by other sites looking for
 * results from our site.
 *
 * @param bool $http  Whether this is an http request or an internal
 *                    request. (TRUE for http)
 *                    Set this option to FALSE to retrieve an object instead
 *                    of printing a JSON string.
 */
function tripal_elasticsearch_api_v1_local_search($http = TRUE) {
  global $base_url;

  $exposed = tripal_elasticsearch_is_index_exposed(['website', 'entities']);

  if (!$exposed) {
    return ESResponse::error(
      '403 Forbidden. The request index is not available
     for cross site search. Contact the admin to mark the index as public', 403
    );
  }

  // Validate the request
  $terms    = isset($_GET['terms']) ? $_GET['terms'] : '';
  $size     = isset($_GET['size']) ? abs(intval($_GET['size'])) : 0;
  $category = isset($_GET['category']) ? $_GET['category'] : NULL;

  if ($size !== 0 && $size > 20) {
    return ESResponse::error(
      [
        'size' => ['Please provide a valid size between 1 and 20'],
      ]
    );
  }

  if ($size === 0) {
    $size = 5;
  }

  try {
    $es      = new ESInstance();
    $results = $es->searchWebIndices($terms, $size, $category);
  } catch (Exception $exception) {
    return ESResponse::error($exception->getMessage(), 500);
  }

  return ESResponse::success(
    array_merge(
      $results, [
        'url' => url(
          rtrim($base_url, '/') . '/tripal_elasticsearch/search_website/'
          . $category, [
            'query' => [
              'search_box' => $terms,
              'category'   => $category,
            ],
          ]
        ),
      ]
    ), $http
  );
}

/**
 * Perform a table index search.
 *
 * @param $index_name
 * @param $remote_host_id
 *
 * @return array|bool|object
 */
function tripal_elasticsearch_api_v1_table_index_search(
  $index_name, $remote_host_id
) {
  $remote_id = abs(intval(trim($remote_host_id)));

  if (!tripal_elasticsearch_is_index_exposed($index_name)) {
    return ESResponse::error(
      '403 Forbidden. The request index is not available
     for cross site search. Contact the admin to mark the index as public', 403
    );
  }

  if ($remote_id === 0) {
    $host = tripal_elasticsearch_get_local_server_entry();
    try {
      $results = tripal_elasticsearch_api_v1_table_index_local_search(
        $index_name, FALSE
      );
    } catch (Exception $exception) {
      return ESResponse::error(
        [
          'results' => [$exception->getMessage()],
        ]
      );
    }
  }
  else {
    $host = db_query(
      'SELECT * FROM {tripal_elasticsearch_servers} WHERE id=:id',
      [':id' => $remote_id]
    )->fetchObject();

    if (!$host) {
      return ESResponse::error(
        [
          'remote_id' => ['Invalid remote host provided. ID: ' . $remote_id],
        ]
      );
    }

    $url = trim($host->url, '/') . '/elasticsearch/api/v1/index-search/'
      . $index_name;
    try {
      $data = $_GET;
      if (isset($data['q'])) {
        unset($data['q']);
      }
      $results = ESRequest::get($url, $data);
    } catch (Exception $exception) {
      return ESResponse::error(
        [
          'remote_id' => [
            'The host returned an invalid response',
            $exception->getMessage(),
          ],
        ]
      );
    }
  }

  $markup = tripal_elasticsearch_results_formatter(
    $results->data->results, $index_name, $host
  );
  return ESResponse::success(
    [
      'url'    => $results->data->url,
      'markup' => $markup,
      'count'  => $results->data->count,
    ]
  );
}

/**
 * Perform a local table index search.
 *
 * @param      $index_name
 * @param bool $http
 *
 * @return array|bool|object
 * @throws \Exception
 */
function tripal_elasticsearch_api_v1_table_index_local_search(
  $index_name, $http = TRUE
) {
  try {
    $es                  = new ESInstance();
    $fields              = $es->getIndexFields($index_name);
    $field_content_pairs = [];

    foreach ($fields as $field) {
      if (isset($_GET[$field])) {
        $field_content_pairs[$field] = $_GET[$field];
      }
    }

    if (function_exists(
      'tripal_elasticsearch_' . $index_name . '_query_mapper'
    )
    ) {
      $query = call_user_func(
        'tripal_elasticsearch_' . $index_name . '_query_mapper', $_GET
      );
    }
    else {
      $query = tripal_elasticsearch_build_search_query_from_field_content_pairs(
        $field_content_pairs
      );
    }

    $results = $es->setTableSearchParams($index_name, '', $query, [0, 2], TRUE);
  } catch (Exception $exception) {
    if ($http) {
      return ESResponse::error(
        [
          'results' => [$exception->getMessage()],
        ]
      );
    }
    else {
      throw $exception;
    }
  }

  $count          = $results->count();
  $search_results = $results->search();

  // Get the url for the index
  $url = db_query(
    'SELECT url FROM {tripal_elasticsearch_indices} WHERE index_name=:index LIMIT 1',
    [
      ':index' => $index_name,
    ]
  )->fetchField();

  global $base_url;
  $query = $_GET;
  unset($query['q']);
  return ESResponse::success(
    [
      'count'   => $count,
      'url'     => $url ? url($base_url . '/' . $url, ['query' => $query])
        : $base_url,
      'results' => $search_results,
    ], $http
  );
}

/**
 * Results formatter.
 *
 * @param      $search_results
 * @param null $index_name
 *
 * @return mixed|string
 */
function tripal_elasticsearch_results_formatter(
  $search_results, $index_name = NULL, $host = NULL
) {
  if ($index_name !== NULL
    && function_exists(
      'tripal_elasticsearch_' . $index_name . '_results_formatter'
    )
  ) {
    return call_user_func(
      'tripal_elasticsearch_' . $index_name . '_results_formatter',
      $search_results, $host
    );
  }

  $rows = [];
  foreach ($search_results as $row) {
    $fields = [];
    foreach ($row as $entry) {
      if (is_array($entry)) {
        $fields[] = elasticsearch_recursive_implode('<br>', $entry);
      }
      else {
        $fields[] = $entry;
      }
    }
    $rows[] = $fields;
  }

  return theme(
    'table', [
      'header' => isset($search_results[0]) ? array_keys($search_results[0])
        : [],
      'rows'   => $rows,
    ]
  );
}

/**
 * Recursively implode an array.
 *
 * @param $glue
 * @param $array
 *
 * @return string
 */
function elasticsearch_recursive_implode($glue, $array) {
  $result = [];

  elasticsearch_recursive_flatten($array, $result);

  $unique = array_unique($result);

  return implode($glue, $unique);
}

/**
 * Flatten a nested array.
 *
 * @param array $array  The input nested array
 * @param array $result The resulting flattened array
 */
function elasticsearch_recursive_flatten($array, &$result) {
  if (!is_array($array)) {
    $result[] = $array;
    return;
  }

  foreach ($array as $value) {
    if (is_array($value)) {
      elasticsearch_recursive_flatten($value, $result);
    }
    else {
      $result[] = $value;
    }
  }
}

/**
 * Get local categories.
 *
 * @return array|bool|object
 */
function tripal_elasticsearch_api_v1_categories() {
  return ESResponse::success(
    [
      [
        'label' => 'Gene',
        'table' => 'chado.feature',
      ],
      [
        'label' => 'mRNA',
        'table' => 'chado.feature',
      ],
      [
        'label' => 'Organism',
        'table' => 'chado.organism',
      ],
    ]
  );
}

/**
 * Get the search page.
 *
 * @param $form
 * @param $form_state
 *
 * @return mixed
 */
function tripal_elasticsearch_cross_site_search_form($form, &$form_state) {
  global $base_url;

  $remotes =
    db_query('SELECT * FROM {tripal_elasticsearch_servers}')->fetchAll();

  // Add local server as the first remote
  array_unshift($remotes, tripal_elasticsearch_get_local_server_entry());

  tripal_elasticsearch_load_logos($remotes);

  $form['options'] = [
    '#type'       => 'fieldset',
    '#attributes' => [
      'class' => [
        'container-inline',
        'es-cross-site-search-form',
      ],
    ],
  ];

  $default_category = ['Any Type' => 'Any Type'];
  try {
    $es         = new ESInstance();
    $categories = drupal_map_assoc($es->getAllCategories());
  } catch (Exception $exception) {
    $categories = [];
  }

  $form['options']['category'] = [
    '#type'          => 'select',
    '#attributes'    => [
      'id' => 'tripal-elasticsearch-search-category',
    ],
    '#options'       => array_merge($default_category, $categories),
    '#default_value' => 'Any Type',
    '#required'      => TRUE,
  ];

  $form['options']['search_term'] = [
    '#type'       => 'textfield',
    '#size'       => '50',
    '#attributes' => [
      'placeholder' => t('E,g. Fraxinus Excelsior mRNA'),
      'id'          => 'tripal-elasticsearch-search-field',
    ],
    '#required'   => TRUE,
  ];

  $form['options']['search_button'] = [
    '#type'       => 'submit',
    '#value'      => t('Search'),
    '#attributes' => [
      'id' => 'tripal-elasticsearch-search-button',
    ],
  ];

  $logos_table = tripal_elasticsearch_make_logos_table($remotes);

  $form['results_block'] = [
    '#type'   => 'markup',
    '#markup' => '<div id="tripal-elasticsearch-results-block">'.$logos_table.'</div>',
  ];

  $form['#atached']['js'][] = [
    'data' => drupal_add_js(
      drupal_get_path('module', 'tripal_elasticsearch') . '/js/axios.min.js'
    ),
    'type' => 'file',
  ];

  drupal_add_js(
    drupal_get_path('module', 'tripal_elasticsearch')
    . '/js/tripal_elasticsearch.js'
  );
  drupal_add_js(
    [
      'remotes' => $remotes,
      'action'  => 'setupSearchPage',
      'base'    => $base_url,
    ], 'setting'
  );

  return $form;
}

/**
 * Mock the local server as a remote server for AJAX to work.
 *
 * @return object
 */
function tripal_elasticsearch_get_local_server_entry() {
  global $base_url;

  return (object) [
    'id'          => 0,
    'label'       => variable_get('site_name', 'Drupal'),
    'description' => '',
    'url'         => $base_url,
  ];
}

/**
 * Checks if an index is exposed.
 *
 * @param $index_name
 *
 * @return bool
 */
function tripal_elasticsearch_is_index_exposed($index_name) {
  // Check if the index is publicly exposed
  $indices = db_query(
    'SELECT * FROM {tripal_elasticsearch_indices} WHERE index_name IN (:indices)',
    [
      ':indices' => is_array($index_name) ? $index_name : [$index_name],
    ]
  )->fetchAll();

  $exposed = FALSE;
  foreach ($indices as $index) {
    if ($index->exposed) {
      $exposed = TRUE;
      break;
    }
  }

  return $exposed;
}

function tripal_elasticsearch_load_logos(array &$remotes) {
  foreach ($remotes as $remote) {
    if ($remote->id === 0) {
      $remote->logo_url = variable_get('elasticsearch_host_logo', '');
      continue;
    }

    $remote->logo_url =
      $remote->logo ? file_create_url(file_load($remote->logo)->uri) : '';
  }
}

function tripal_elasticsearch_make_logos_table($remotes) {
  $header = ['Logo', 'Database'];
  $rows   = [];

  foreach ($remotes as $remote) {
    $img = '';
    if (!empty($remote->logo_url)) {
      $img = '<img src="' . $remote->logo_url
        . '" style="width:auto; height: 50px"/>';
    }

    $rows[] = [$img, l($remote->label, !empty($remote->url) ? $remote->url : '<front>')];
  }

  return '<h4>Available Databases</h4>' . theme(
      'table', [
        'header' => $header,
        'rows'   => $rows,
      ]
    );
}
