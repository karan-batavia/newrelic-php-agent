/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Functions relating to instrumenting AWS-SDK-PHP.
 */
#ifndef LIB_AWS_SDK_PHP_HDR
#define LIB_AWS_SDK_PHP_HDR

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO /* PHP8.1+ */
/* Service instrumentation only supported above PHP 8.1+*/
#define SQS_LIBRARY_NAME "SQS"
#define AWS_SQS_MESSAGING_SERVICE "aws_sqs"
#define AWS_SDK_PHP_SQSCLIENT_CLASS "Aws\\Sqs\\SqsClient"
#define AWS_SDK_PHP_SQSCLIENT_QUEUEURL_ARG "QueueUrl"
#define AWS_SDK_PHP_DYNAMODBCLIENT_CLASS "Aws\\DynamoDb\\DynamoDbClient"
#define AWS_QUEUEURL_LEN_MAX 512
#endif /* PHP 8.1+ */

#define PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX \
  "Supportability/PHP/AWS/Services/"
#define MAX_METRIC_NAME_LEN 256
#define PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX_LEN \
  sizeof(PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX)
#define MAX_AWS_SERVICE_NAME_LEN \
  (MAX_METRIC_NAME_LEN - PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX_LEN)

extern void nr_aws_sdk_php_enable();
extern void nr_lib_aws_sdk_php_handle_version();
extern void nr_lib_aws_sdk_php_add_supportability_service_metric(
    const char* service_name);

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO /* PHP8.1+ */
/* Aside from service class and version detection, instrumentation is only
 * supported with PHP 8.1+ */

/*
 * Purpose : Parses the QueueUrl to extract cloud_region, cloud_account_id, and
 * destination_name.  The extraction sets all or none since the values from the
 * same string and if it is malformed, it cannot be used.
 *
 * Params  : 1. The QueueUrl
 *           2. message_params: the extracted values will be set to
 * message_params.cloud_region, message_params.cloud_account_id, and
 * message_params.destination_name Returns : Note: caller is responsible for
 * freeing message_params.cloud_region, message_params.cloud_account_id, and
 * message_params.destination_name
 */
extern void nr_lib_aws_sdk_php_sqs_parse_queueurl(
    const char* sqs_queueurl,
    nr_segment_message_params_t* message_params);

/*
 * Purpose : Handle when an SqsClient initiates a command
 *
 * Params  : 1. NR_EXECUTE_ORIG_ARGS (execute_data, func_return_value)
 *           2. segment : if we instrument the commandName, we'll need to end
 * the segment as a message segment Returns :
 *
 */
extern void nr_lib_aws_sdk_php_sqs_handle(nr_segment_t* segment,
                                          NR_EXECUTE_PROTO);

/*
 * Purpose : Extracts the array of arguments passed to Aws/AwsClient::__call
 * which handles aws-sdk-php service commands
 *
 * Params  : 1. NR_EXECUTE_ORIG_ARGS (execute_data, func_return_value)
 *           2. arg_name: name of argument to extract from command arg array
 *
 * Returns : the value of the arg_name; NULL if does not exist
 *
 * Note: The caller is responsible for freeing the returned string value
 *
 */
extern char* nr_lib_aws_sdk_php_get_command_arg_value(char* command_arg_name,
                                                      NR_EXECUTE_PROTO);
#endif /* PHP8.1+ */

#endif /* LIB_AWS_SDK_PHP_HDR */
