<?php

/**
 * @param $form
 * @param $form_state
 *
 * @return mixed
 */
function tripal_elasticsearch_advanced_search_form($form, &$form_state) {
  try {
    $instance = new \ES\Common\Instance();
    $bundles = $instance->getAllCategories();
  } catch (Exception $exception) {
    $bundles = [];
  }

  $helper = new \ES\Common\BundleHelper();

  $form['wrapper'] = [
    '#prefix' => '<div id="advanced_search_wrapper" class="container-inline">',
    '#suffix' => '</div>',
  ];

  $category = trim(
    $_GET['category'] ?? $form_state['values']['category'] ?? ''
  );

  $form['wrapper']['category'] = [
    '#type' => 'select',
    '#options' => ['' => 'Any Category'] + $bundles,
    '#ajax' => [
      'callback' => 'tripal_elasticsearch_advanced_search_form_callback',
      'wrapper' => 'advanced_search_wrapper',
    ],
    '#default_value' => $category,
  ];

  if (!empty($category)) {
    $fields = $helper->getFieldsByBundle($helper->getBundleByName($category));
    $field_options = [
      '' => 'Any Field',
    ];

    foreach ($fields as $field) {
      $field_options[$field->name] = $field->label;
    }

    $form['wrapper']['field'] = [
      '#type' => 'select',
      '#options' => $field_options,
      '#default_value' => trim($_GET['field'] ?? ''),
    ];
  }

  $form['wrapper']['term'] = [
    '#type' => 'textfield',
    '#default_value' => $_GET['term'] ?? '',
  ];

  $form['wrapper']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Search'),
  ];

  $form['#method'] = 'GET';

  //if (isset($_GET['term']) && !empty($_GET['term'])) {
  $form['wrapper']['results'] = [
    '#markup' => tripal_elasticsearch_advanced_search_results($_GET),
  ];
  //}

  return $form;
}

/**
 * @param array $values
 *
 * @return string
 */
function tripal_elasticsearch_advanced_search_results(array $values) {
  if (empty($values['category']) && empty($values['term'])) {
    return '';
  }

  try {
    $instance = new \ES\Common\Instance();
    $results = tripal_elasticsearch_perform_advanced_search($values, 15);
    if($results['total'] == 0 && !empty($values['term'])) {
      $values['term'] = strtolower($values['term']);
      $results = tripal_elasticsearch_perform_advanced_search($values, 15);
    }
    $hits = $instance->formatHits($results['results']);

    if (count($hits) === 0) {
      return theme(
        'elasticsearch_tips',
        [
          'message' => '0 results found.',
        ]
      );
    }

    $content = theme('elasticsearch_results_header', [
      'page' => $results['page'],
      'total' => $results['total'],
      'pages' => $results['pages'],
      'time' => $results['time']
    ]);
    $content .= tripal_elasticsearch_get_website_search_result_table($hits, false);
    $content .= $results['pager'];

    return $content;
  } catch (Exception $exception) {
    watchdog(
      'tripal_elasticsearch',
      $exception->getMessage(),
      [],
      WATCHDOG_ERROR
    );
    drupal_set_message(
      'The search service is currently unavailable. Please contact us to resolve the issue.'
    );
  }
}

/**
 * @param $values
 *
 * @return array
 * @throws \Exception
 */
function tripal_elasticsearch_perform_advanced_search($values, $per_page = NULL) {
  $fields = ['content.*', 'title'];

  if (isset($values['field']) && !empty($values['field'])) {
    $fields = ["content.{$_GET['field']}"];
  }

  $model = new \ES\Models\Model();
  $model->setIndexName('entities');

  if (!empty($values['term'])) {
    $model->where($fields, trim($values['term']));
  }

  $category = trim($values['category'] ?? '');
  if (!empty($category)) {
    $model->where('bundle_label', "\"$category\"");
  }

  $model->highlight(['content.*', 'title']);

  if (is_null($per_page)) {
    return $model->search();
  }

  return $model->paginate($per_page);
}

/**
 * AJAX Callback.
 *
 * @param array $form
 * @param array $form_state
 *
 * @return array
 */
function tripal_elasticsearch_advanced_search_form_callback($form, &$form_state) {
  return $form['wrapper'];
}