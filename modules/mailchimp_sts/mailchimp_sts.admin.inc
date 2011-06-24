<?php
/**
 * @file
 * Administrative forms for MailChimp STS module.
 */

/**
 * Page callback for mailchimp sts admin settings.
 */
function mailchimp_sts_admin_page() {
  $mailchimp_sts = mailchimp_sts_get_api_object();
  $response = $mailchimp_sts->list_verified_email_addresses();
  if (!$mailchimp_sts->errorCode) {
    $emails = array();
    if (!isset($response['email_addresses']) || count($response['email_addresses']) < 1) {
      drupal_set_message(t('You need to add at least one verified email address to use MailChimp STS.'), 'warning');
    }
    else {
      foreach ($response['email_addresses'] AS $email) {
        // filter out MailChimp transactional emails
        if (!empty($email) && !stripos($email, 'transact.mcsv.net')) {
          $emails[$email] = $email;        
        }
      }      
    }

    return l(t('View MailChimp STS statistics'), 'admin/reports/mailchimp_sts') .
      drupal_render(drupal_get_form('mailchimp_sts_admin_settings', $emails));  
  }
  else {
    drupal_set_message(
      t('There was a problem accessing the MailChimp STS service: %msg', 
      array('%msg' => $mailchimp_sts->errorMessage)
    ));
    
    return t('Refer to the !link for properly configuring MailChimp STS.', 
      array('!link' => l('MailChimp Documentation', 
      'http://kb.mailchimp.com/article/how-does-mailchimp-integrate-with-amazon-ses'
    )));
  }

}

/**
 * Administrative settings.
 *
 * @return
 *   An array containing form items to place on the module settings page.
 */
function mailchimp_sts_admin_settings($form, &$form_state, $verified_emails = array()) {
  $form['onoff'] = array(
    '#type'  => 'fieldset',
    '#collapsible' => TRUE,
    '#title' => t('Install options'),
  );  
  $form['onoff']['mailchimp_sts_on'] = array(
    '#type'          => 'radios',
    '#title'         => t('Enable the MailChimp STS Mail interface'),
    '#default_value' => variable_get('mailchimp_sts_on', 0),
    '#options'       => array(1 => t('On'), 0 => t('Off')),
    '#description'   => t('To uninstall this module you must turn it off here first.'),
  );
  $form['onoff']['mailchimp_sts_debugging'] = array(
    '#type'          => 'checkbox',
    '#title'         => t('Enable debugging'),
    '#default_value' => variable_get('mailchimp_sts_debugging', 0),
    '#description'   => t('Checking this box will print Amazon SES messages from the server for every email that is sent.'),
  );

  $form['email_options'] = array(
    '#type'  => 'fieldset',
    '#collapsible' => TRUE,
    '#title' => t('Email options'),
  );
  $form['email_options']['mailchimp_sts_from'] = array(
    '#type'          => 'select',
    '#title'         => t('Email from address'),
    '#default_value' => variable_get('mailchimp_sts_from', ''),
    '#description'   => t('Select a verified email address. All email will be sent as this address.'),
    '#options' => $verified_emails,
  );
  $form['email_options']['mailchimp_sts_from_new'] = array(
    '#type' => 'fieldset',
    '#title' => t('Verify a new email address'),
    '#description' => t('A confirmation message will be sent to this address.'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['email_options']['mailchimp_sts_from_new']['new_address'] = array(
    '#type' => 'textfield',
    '#title' => 'New Email address',
  );
  $form['email_options']['mailchimp_sts_from_new']['new_address_submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add email'),
    '#submit' => array('mailchimp_sts_admin_new_address_submit'),
  );

  $formats = filter_formats();
  $options = array('' => t('-- Select --'));
  foreach ($formats AS $v => $format) {          
    $options[$v] = $format->name;
  }
  $form['email_options']['mailchimp_sts_filter_format'] = array(
    '#type' => 'select',
    '#title' => t('Input format'),
    '#description' => t('If selected, the input format to apply to the message 
      body before sending to the STS API.'),
    '#options' => $options,
    '#default_value' => array(variable_get('mailchimp_sts_filter_format', 'full_html')),
  );
 
  $form['#submit'][] = 'mailchimp_sts_admin_settings_submit';
  return system_settings_form($form);
}

/**
 * Validataion for the administrative settings form.
 *
 * @param form
 *   An associative array containing the structure of the form.
 * @param form_state
 *   A keyed array containing the current state of the form.
 */
function mailchimp_sts_admin_settings_validate($form, &$form_state) {
  if (isset($form_state['submit_handlers'][0]) &&
      $form_state['submit_handlers'][0] == 'mailchimp_sts_admin_new_address_submit') {
    if (empty($form_state['values']['new_address']) || !valid_email_address($form_state['values']['new_address'])) {
      form_set_error('new_address', t('A valid email address is required.'));      
    }
  }
  else {
    // unset form fields we don't want to save
    unset($form_state['values']['new_address']);
  }
}

/**
 * Submission for the administrative settings form.
 *
 * @param form
 *   An associative array containing the structure of the form.
 * @param form_state
 *   A keyed array containing the current state of the form.
 */
function mailchimp_sts_admin_settings_submit($form, &$form_state) {
  $values = $form_state['values'];

  if ($values['mailchimp_sts_on']) {
    variable_set('mail_system', array('default-system' => 'MailChimpSTSMailSystem'));
    drupal_set_message(t('MailChimp STS will be used to deliver all site emails.'));
    watchdog('mailchimp_sts', 'MailChimp STS has been enabled.');
  }
  else {
    variable_set('mail_system', array('default-system' => 'DefaultMailSystem'));
    drupal_set_message(t('The default mail system will be used to deliver all site emails.'));
    watchdog('mailchimp_sts', 'MailChimp STS has been disabled.');
  }
}

/**
 * Submission for the administrative settings form - new address.
 *
 * @param form
 *   An associative array containing the structure of the form.
 * @param form_state
 *   A keyed array containing the current state of the form.
 */
function mailchimp_sts_admin_new_address_submit($form, &$form_state) {
  $address = $form_state['values']['new_address'];
  if ($address) {
    $mailchimp_sts = mailchimp_sts_get_api_object();
    $new_address = $mailchimp_sts->verify_email_address($address);
    if ($mailchimp_sts->errorCode) {
      drupal_set_message(t('There has been an error verifying your message: %msg.', array('%msg' => $mailchimp_sts->errorMessage)));
    }
    else {
      drupal_set_message(t('The address %address has been verified. Please check your email for a confirmation message from Amazon.', array('%address' => $address)));
    }
  }
}

/**
 * Return a form for sending a test email.
 *
 * @param string $form 
 * @param string $form_state 
 * @return array $form
 */
function mailchimp_sts_test_form($form, &$form_state) {
  drupal_set_title(t('Send test email'));
  
  $form['mailchimp_sts_test_address'] = array(
    '#type' => 'textfield',
    '#title' => t('Email address to send a test email to'),
    '#default_value' => variable_get('site_mail', ''),
    '#description' => t('Type in an address to have a test email sent there.'),
    '#required' => TRUE
  );
  $form['mailchimp_sts_test_body'] = array(
    '#type' => 'textarea',
    '#title' => t('Test body contents'),
    '#default_value' => t('If you receive this message it means your site is capable of using MailChimp STS to send email.'),
  );
  $form['test_submit'] = array(
    '#type' => 'submit',
    '#value' => t('Send test email'),
  );
  $form['test_cancel'] = array(
    '#type' => 'link',
    '#href' => 'admin/config/services/mailchimp/sts',
    '#title' => t('Cancel'),
  );
  
  return $form;
}

/**
 * Submit handler for mailchimp_sts_test_form(), sends the test email.
 *
 * @param string $form 
 * @param string $form_state 
 * @return void
 */
function mailchimp_sts_test_form_submit($form, &$form_state) {
  // If an address was given, send a test email message.
  $test_address = $form_state['values']['mailchimp_sts_test_address'];
  global $language;
  $params['subject'] = t('Drupal MailChimp STS test email');
  $params['body'] = $form_state['values']['mailchimp_sts_test_body'];
  drupal_mail('mailchimp_sts', 'mailchimp-sts-test', $test_address, $language, $params, variable_get('mailchimp_sts_from', ''));
  drupal_set_message(t('A test email has been sent to @email.', array('@email' => $test_address)));
}