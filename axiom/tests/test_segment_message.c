/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "nr_header.h"
#include "nr_segment_message.h"
#include "test_segment_helpers.h"
#include "nr_attributes.h"
#include "nr_attributes_private.h"
#include "util_hash.h"
#include "util_memory.h"
#include "util_object.h"
#include "util_reply.h"
#include "util_strings.h"
#include "util_text.h"

#include "tlib_main.h"

typedef struct {
  const char* test_name;
  const char* name;
  const char* txn_rollup_metric;
  const char* library_metric;
  uint32_t num_metrics;
  const char* destination_name;
  const char* cloud_region;
  const char* cloud_account_id;
  const char* messaging_system;
  const char* cloud_resource_id;
  const char* server_address;
  const char* aws_operation;
  char* messaging_destination_publish_name;
  char* messaging_destination_routing_key;
  uint64_t server_port;
} segment_message_expecteds_t;

static nr_segment_t* mock_txn_segment(void) {
  nrtxn_t* txn = new_txn(0);

  return nr_segment_start(txn, NULL, NULL);
}

static void test_message_segment(nr_segment_message_params_t* params,
                                 nr_segment_cloud_attrs_t* cloud_attrs,
                                 bool message_attributes_enabled,
                                 segment_message_expecteds_t expecteds) {
  uint32_t all = NR_ATTRIBUTE_DESTINATION_ALL;
  nrobj_t* obj = NULL;
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;
  seg->txn->options.message_tracer_segment_parameters_enabled
      = message_attributes_enabled;

  nr_segment_traces_add_cloud_attributes(seg, cloud_attrs);
  /* Check the agent cloud attributes. */
  obj = nr_attributes_agent_to_obj(seg->attributes, all);
  tlib_pass_if_str_equal(expecteds.test_name, expecteds.aws_operation,
                         nro_get_hash_string(obj, NR_ATTR_AWS_OPERATION, 0));
  tlib_pass_if_str_equal(
      expecteds.test_name, expecteds.cloud_resource_id,
      nro_get_hash_string(obj, NR_ATTR_CLOUD_RESOURCE_ID, 0));
  tlib_pass_if_str_equal(expecteds.test_name, expecteds.cloud_account_id,
                         nro_get_hash_string(obj, NR_ATTR_CLOUD_ACCOUNT_ID, 0));
  tlib_pass_if_str_equal(expecteds.test_name, expecteds.cloud_region,
                         nro_get_hash_string(obj, NR_ATTR_CLOUD_REGION, 0));
  nro_delete(obj);

  test_segment_message_end_and_keep(&seg, params);
  /* Check the metrics and txn naming. */
  tlib_pass_if_str_equal(expecteds.test_name, expecteds.name,
                         nr_string_get(seg->txn->trace_strings, seg->name));
  test_txn_metric_created(expecteds.test_name, txn->unscoped_metrics,
                          expecteds.txn_rollup_metric);
  test_txn_metric_created(expecteds.test_name, txn->unscoped_metrics,
                          expecteds.library_metric);
  test_metric_vector_size(seg->metrics, expecteds.num_metrics);

  /* Check the segment settings and typed attributes. */
  tlib_pass_if_true(expecteds.test_name, NR_SEGMENT_MESSAGE == seg->type,
                    "NR_SEGMENT_MESSAGE");
  tlib_pass_if_str_equal(expecteds.test_name,
                         seg->typed_attributes->message.destination_name,
                         expecteds.destination_name);
  tlib_pass_if_str_equal(expecteds.test_name,
                         seg->typed_attributes->message.messaging_system,
                         expecteds.messaging_system);
  tlib_pass_if_str_equal(expecteds.test_name,
                         seg->typed_attributes->message.server_address,
                         expecteds.server_address);
  tlib_pass_if_str_equal(
      expecteds.test_name,
      seg->typed_attributes->message.messaging_destination_publish_name,
      expecteds.messaging_destination_publish_name);
  tlib_pass_if_str_equal(
      expecteds.test_name,
      seg->typed_attributes->message.messaging_destination_routing_key,
      expecteds.messaging_destination_routing_key);
  tlib_pass_if_int_equal(expecteds.test_name,
                         seg->typed_attributes->message.server_port,
                         expecteds.server_port);
  nr_txn_destroy(&txn);
}

static void test_bad_parameters(void) {
  nr_segment_t seg_null = {0};
  nr_segment_t* seg_null_ptr;
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;
  nr_segment_message_params_t params = {0};

  tlib_pass_if_false("bad parameters", nr_segment_message_end(NULL, &params),
                     "expected false");

  seg_null_ptr = NULL;

  tlib_pass_if_false("bad parameters",
                     nr_segment_message_end(&seg_null_ptr, &params),
                     "expected false");

  seg_null_ptr = &seg_null;
  tlib_pass_if_false("bad parameters",
                     nr_segment_message_end(&seg_null_ptr, &params),
                     "expected false");

  tlib_pass_if_false("bad parameters", nr_segment_message_end(&seg, NULL),
                     "expected false");
  test_metric_vector_size(seg->metrics, 0);

  nr_txn_destroy(&txn);
}

static void test_segment_message_destination_type(void) {
  /*
   * The following values are used to create metrics:
   * library
   * destination_type
   * message_action
   * destination_name
   */
  /* Test NR_MESSAGE_DESTINATION_TYPE_TEMP_TOPIC destination type */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TEMP_TOPIC,
          .destination_name = "my_queue_or_topic"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name
          = "Test NR_MESSAGE_DESTINATION_TYPE_TEMP_TOPIC destination type",
          .name = "MessageBroker/SQS/Topic/Produce/Temp",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test NR_MESSAGE_DESTINATION_TYPE_TEMP_QUEUE destination type */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TEMP_QUEUE,
          .destination_name = "my_queue_or_topic"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name
          = "Test NR_MESSAGE_DESTINATION_TYPE_TEMP_QUEUE destination type",
          .name = "MessageBroker/SQS/Queue/Produce/Temp",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test NR_MESSAGE_DESTINATION_TYPE_EXCHANGE destination type */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_EXCHANGE,
          .destination_name = "my_queue_or_topic"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name
          = "Test NR_MESSAGE_DESTINATION_TYPE_EXCHANGE destination type",
          .name = "MessageBroker/SQS/Exchange/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test NR_MESSAGE_DESTINATION_TYPE_TOPIC destination type */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name
          = "Test NR_MESSAGE_DESTINATION_TYPE_EXCHANGE destination type",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test NR_MESSAGE_DESTINATION_TYPE_QUEUE destination type */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_QUEUE,
          .destination_name = "my_queue_or_topic"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name
          = "Test NR_MESSAGE_DESTINATION_TYPE_QUEUE destination type",
          .name = "MessageBroker/SQS/Queue/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});
}

static void test_segment_message_message_action(void) {
  /*
   * The following values are used to create metrics:
   * library
   * destination_type
   * message_action
   * destination_name
   */

  /* Test NR_SPANKIND_PRODUCER message action */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test NR_SPANKIND_PRODUCER message action",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test NR_SPANKIND_CONSUMER message action */

  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_CONSUMER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test NR_SPANKIND_CONSUMER message action",
          .name = "MessageBroker/SQS/Topic/Consume/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /*
   * Test NR_SPANKIND_CLIENT message action; this is not
   * allowed for message segments, should show unknown.
   */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_CLIENT,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test NR_SPANKIND_CLIENT message action",
          .name = "MessageBroker/SQS/Topic/<unknown>/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});
}

static void test_segment_message_library(void) {
  /*
   * The following values are used to create metrics:
   * library
   * destination_type
   * message_action
   * destination_name
   */
  /* Test null library */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = NULL,
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null library",
          .name
          = "MessageBroker/<unknown>/Topic/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/<unknown>/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test empty library */

  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty library",
          .name
          = "MessageBroker/<unknown>/Topic/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/<unknown>/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test valid library */

  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_queue_or_topic"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid library",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_queue_or_topic",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_queue_or_topic",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});
}

static void test_segment_message_destination_name(void) {
  /*
   * The following values are used to create metrics:
   * library
   * destination_type
   * message_action
   * destination_name
   */
  /* Test null destination_name */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = NULL},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null destination_name",
          .name = "MessageBroker/SQS/Topic/Produce/Named/<unknown>",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = NULL,
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test empty destination_name */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = ""},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty destination_name",
          .name = "MessageBroker/SQS/Topic/Produce/Named/<unknown>",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = NULL,
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test valid destination_name */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid destination_name",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});
}

static void test_segment_message_cloud_region(void) {
  /*
   * cloud_region values should NOT impact the creation of
   * metrics.
   */

  /* Test null cloud_region */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null cloud_region",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test empty cloud_region */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){.cloud_region = ""},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty cloud_region",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test valid cloud_region */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){.cloud_region = "wild-west-1"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid cloud_region",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = "wild-west-1",
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});
}

static void test_segment_message_cloud_account_id(void) {
  /*
   * cloud_account_id values should NOT impact the creation
   * of metrics.
   */

  /* Test null cloud_account_id */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null cloud_account_id",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test empty cloud_account_id */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){.cloud_account_id = ""},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty cloud_account_id",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test valid cloud_account_id */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){.cloud_account_id = "12345678"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid cloud_account_id",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = "12345678",
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});
}

static void test_segment_message_messaging_system(void) {
  /*
   * messaging_system values should NOT impact the creation
   * of metrics.
   */

  /* Test null messaging_system */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_system = NULL,
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null messaging_system",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test empty messaging_system */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_system = "",
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty messaging_system",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test valid messaging_system */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_system = "my_messaging_system",
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid messaging_system",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = "my_messaging_system",
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});
}

static void test_segment_message_cloud_resource_id(void) {
  /*
   * cloud_resource_id values should NOT impact the creation
   * of metrics.
   */

  /* Test null cloud_resource_id */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null cloud_resource_id ",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test empty cloud_resource_id */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){.cloud_resource_id = ""},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty cloud_resource_id ",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test valid cloud_resource_id */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){.cloud_resource_id = "my_resource_id"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid cloud_resource_id ",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = "my_resource_id",
          .server_address = NULL,
          .aws_operation = NULL});
}

static void test_segment_message_server_address(void) {
  /*
   * server_address values should NOT impact the creation
   * of metrics.
   */

  /* Test null server_address */
  test_message_segment(
      &(nr_segment_message_params_t){
          .server_address = "localhost",
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null server_address",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = "localhost",
          .aws_operation = NULL});

  /* Test empty server_address */
  test_message_segment(
      &(nr_segment_message_params_t){
          .server_address = "",
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty server_address",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test valid server_address */
  test_message_segment(
      &(nr_segment_message_params_t){
          .server_address = "localhost",
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid server_address",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = "localhost",
          .aws_operation = NULL});
}

static void test_segment_message_aws_operation(void) {
  /*
   * aws_operation values should NOT impact the creation
   * of metrics.
   */

  /* Test null aws_operation */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test null aws_operation",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test empty aws_operation */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){.aws_operation = ""},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test empty aws_operation",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = NULL});

  /* Test valid aws_operation */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){.aws_operation = "sendMessage"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid aws_operation",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = NULL,
          .cloud_account_id = NULL,
          .messaging_system = NULL,
          .cloud_resource_id = NULL,
          .server_address = NULL,
          .aws_operation = "sendMessage"});
}

static void test_segment_message_server_port(void) {
  /*
   * server port values should NOT impact the creation
   * of metrics.
   */

  /* Test server port not set, implicitly unset */
  test_message_segment(
      &(nr_segment_message_params_t){
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "server port not set, implicitly unset",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .server_port = 0});

  /* Test server port explicitly set to 0 (unset) */
  test_message_segment(
      &(nr_segment_message_params_t){
          .server_port = 0,
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "server port explicitly set to 0 (unset)",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .server_port = 0});

  /* Test valid server_port */
  test_message_segment(
      &(nr_segment_message_params_t){
          .server_port = 1234,
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid aws_operation",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .server_port = 1234});
}

static void test_segment_messaging_destination_publishing_name(void) {
  /*
   * messaging_destination_publish_name values should NOT impact the creation
   * of metrics.
   */

  /* Test messaging_destination_publish_name is NULL */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_destination_publish_name = NULL,
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name
          = "messaging_destination_publish_name is NULL, attribute "
            "should be NULL, destination_name is used for metric/txn",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .messaging_destination_publish_name = NULL});

  /* Test destination_publishing_name is empty string */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_destination_publish_name = "",
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name
          = "messaging_destination_publish_name is empty string, "
            "attribute should be NULL, destination_name is used for metric/txn",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .messaging_destination_publish_name = NULL});

  /* Test valid messaging_destination_publish_name */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_destination_publish_name = "publish_name",
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid messaging_destination_publish_name is "
                       "non-empty string, attribute should be the string, "
                       "should be used for metric/txn",
          .name = "MessageBroker/SQS/Topic/Produce/Named/publish_name",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .messaging_destination_publish_name = "publish_name"});
}

static void test_segment_messaging_destination_routing_key(void) {
  /*
   * messaging_destination_routing_key values should NOT impact the creation
   * of metrics.
   */

  /* Test messaging_destination_routing_key is NULL */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_destination_routing_key = NULL,
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "messaging_destination_routing_key is NULL, attribute "
                       "should be NULL",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .messaging_destination_routing_key = NULL});

  /* Test messaging_destination_routing_key is empty string */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_destination_routing_key = "",
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "messaging_destination_routing_key is empty string, "
                       "attribute should be NULL",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .messaging_destination_routing_key = NULL});

  /* Test valid messaging_destination_routing_key */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_destination_routing_key = "key to the kingdom",
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){0}, true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test valid messaging_destination_routing_key is "
                       "non-empty string, attribute should be the string",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .messaging_destination_routing_key = "key to the kingdom"});
}

static void test_segment_message_parameters_enabled(void) {
  /*
   * Attributes should be set based on value of parameters_enabled.
   */

  /* Test true message_parameters_enabled */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_destination_routing_key = "key to the kingdom",
          .messaging_destination_publish_name = "publish_name",
          .server_port = 1234,
          .server_address = "localhost",
          .messaging_system = "my_system",
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){.aws_operation = "sendMessage",
                                  .cloud_region = "wild-west-1",
                                  .cloud_account_id = "12345678",
                                  .cloud_resource_id = "my_resource_id"},
      true /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test true message_parameters_enabled",
          .name = "MessageBroker/SQS/Topic/Produce/Named/publish_name",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = "my_destination",
          .cloud_region = "wild-west-1",
          .cloud_account_id = "12345678",
          .messaging_system = "my_system",
          .cloud_resource_id = "my_resource_id",
          .server_address = "localhost",
          .messaging_destination_routing_key = "key to the kingdom",
          .server_port = 1234,
          .messaging_destination_publish_name = "publish_name",
          .aws_operation = "sendMessage"});

  /*
   * Test false message_parameters_enabled. Message attributes should not show,
   * but cloud attributes should be unaffected.
   */
  test_message_segment(
      &(nr_segment_message_params_t){
          .messaging_destination_routing_key = "key to the kingdom",
          .server_port = 1234,
          .server_address = "localhost",
          .messaging_system = "my_system",
          .library = "SQS",
          .message_action = NR_SPANKIND_PRODUCER,
          .destination_type = NR_MESSAGE_DESTINATION_TYPE_TOPIC,
          .destination_name = "my_destination"},
      &(nr_segment_cloud_attrs_t){.aws_operation = "sendMessage",
                                  .cloud_region = "wild-west-1",
                                  .cloud_account_id = "12345678",
                                  .cloud_resource_id = "my_resource_id"},
      false /* enable attributes */,
      (segment_message_expecteds_t){
          .test_name = "Test false message_parameters_enabled",
          .name = "MessageBroker/SQS/Topic/Produce/Named/my_destination",
          .txn_rollup_metric = "MessageBroker/all",
          .library_metric = "MessageBroker/SQS/all",
          .num_metrics = 1,
          .destination_name = NULL,
          .cloud_region = "wild-west-1",
          .cloud_account_id = "12345678",
          .messaging_system = NULL,
          .cloud_resource_id = "my_resource_id",
          .server_address = NULL,
          .messaging_destination_routing_key = NULL,
          .server_port = 0,
          .messaging_destination_publish_name = NULL,
          .aws_operation = "sendMessage"});
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_bad_parameters();
  test_segment_message_destination_type();
  test_segment_message_message_action();
  test_segment_message_library();
  test_segment_message_destination_name();
  test_segment_message_cloud_region();
  test_segment_message_cloud_account_id();
  test_segment_message_messaging_system();
  test_segment_message_cloud_resource_id();
  test_segment_message_server_address();
  test_segment_message_server_port();
  test_segment_messaging_destination_publishing_name();
  test_segment_messaging_destination_routing_key();
  test_segment_message_aws_operation();
  test_segment_message_parameters_enabled();
}
