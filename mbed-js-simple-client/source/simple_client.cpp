/* Copyright (c) 2016 ARM Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "jerryscript-mbed-util/logging.h"
#include "jerryscript-mbed-event-loop/EventLoop.h"
#include "jerryscript-mbed-library-registry/wrap_tools.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <string>
#include "mbed.h"
#include "security.h" // This is expected to be in the project that depends on this library!
//#include "NetworkStack.h"
#include "simple-mbed-client.h"

#include "mbed.h"

DECLARE_CLASS_FUNCTION(SimpleClient, setup)
{
	CHECK_ARGUMENT_COUNT(SimpleClient, setup, (args_count == 1));
	CHECK_ARGUMENT_TYPE_ALWAYS(SimpleClient, setup, 0, object);

	// Extract the NetworkInterface pointer from the first argument.
	NetworkInterface* network_interface =
		(NetworkInterface*) jsmbed_wrap_get_native_handle(args[0]);

	uintptr_t native_handle = jsmbed_wrap_get_native_handle(this_obj);
	SimpleMbedClient* native_ptr = (SimpleMbedClient*) native_handle;
	bool result = native_ptr->setup(network_interface);

	return jerry_create_boolean(result);
}

DECLARE_CLASS_FUNCTION(SimpleClient, keep_alive)
{
	CHECK_ARGUMENT_COUNT(SimpleClient, setup, (args_count == 0));
	uintptr_t native_handle = jsmbed_wrap_get_native_handle(this_obj);
	SimpleMbedClient* native_ptr = (SimpleMbedClient*) native_handle;
	native_ptr->keep_alive();
	return jerry_create_undefined();
}

DECLARE_CLASS_FUNCTION(SimpleClient, on_registered)
{
	CHECK_ARGUMENT_COUNT(SimpleClient, on_registered, (args_count == 1));
	CHECK_ARGUMENT_TYPE_ALWAYS(SimpleClient, on_registered, 0, function);
	uintptr_t native_handle;
	jerry_get_object_native_handle(this_obj, &native_handle);

	jerry_value_t f = args[0];
	jerry_acquire_value(f);

	SimpleMbedClient *this_client = (SimpleMbedClient*) native_handle;
	mbed::Callback<void()> cb = mbed::js::EventLoop::getInstance().wrapFunction(f);
	this_client->on_registered(cb);

	return jerry_create_undefined();
}

DECLARE_CLASS_FUNCTION(SimpleClient, on_unregistered)
{
	CHECK_ARGUMENT_COUNT(SimpleClient, on_unregistered, (args_count == 1));
	CHECK_ARGUMENT_TYPE_ALWAYS(SimpleClient, on_unregistered, 0, function);
	uintptr_t native_handle;
	jerry_get_object_native_handle(this_obj, &native_handle);

	jerry_value_t f = args[0];
	jerry_acquire_value(f);

	SimpleMbedClient *this_client = (SimpleMbedClient*) native_handle;
	mbed::Callback<void()> cb = mbed::js::EventLoop::getInstance().wrapFunction(f);
	this_client->on_unregistered(cb);

	return jerry_create_undefined();
}

DECLARE_CLASS_FUNCTION(SimpleClient, define_resource)
{
  CHECK_ARGUMENT_COUNT(SimpleClient, define_resource, (args_count >= 1 && args_count <= 5));
  CHECK_ARGUMENT_TYPE_ALWAYS(SimpleClient, define_resource, 0, string);
  // -- Note we don't check argument 1, because it could be anything, technically!
  CHECK_ARGUMENT_TYPE_ON_CONDITION(SimpleClient, define_resource, 2, number, (args_count >= 3));
  CHECK_ARGUMENT_TYPE_ON_CONDITION(SimpleClient, define_resource, 3, boolean, (args_count >= 4));
  CHECK_ARGUMENT_TYPE_ON_CONDITION(SimpleClient, define_resource, 4, function, (args_count == 5));

  // Get argument 0, route
	jerry_size_t string_size = jerry_get_string_size(args[0]);
	jerry_char_t *route = (jerry_char_t*) calloc(string_size + 1, sizeof(jerry_char_t));
	jerry_string_to_char_buffer(args[0], route, string_size);

	// Get argument 1. For now, just assume it's a number.
  stringstream ss;
  if (jerry_value_is_number(args[1]))
  {
    double value = jerry_get_number_value(args[1]);

    // Now turn it into a string based on what type of number it is...
    // Wouldn't be necessary if sprintf worked.
    bool is_integer = (floor(value) == value);

    // Print the first part. This will print the int if it's an int,
    // and print the whole part if it's a float
    ss << ((int) value);

    // Just hard code it at 3 decimal places right now
    if (!is_integer && value > 0.0)
    {
      int fractional = (int) ((value - floor(value)) * 1000.0);
      ss << "." << fractional;
    }
    else if (!is_integer && value < 0.0)
    {
      int fractional = (int) ((value - ceil(value)) * -1000.0);
      ss << "." << fractional;
    }
  }
  else if (jerry_value_is_boolean(args[1]))
  {
    // Initial value of a boolean will be 0/1, not true/false.
    bool value = jerry_get_boolean_value(args[1]);
    ss << value;
  }
  else if (jerry_value_is_string(args[1]))
  {
		jerry_size_t value_string_size = jerry_get_string_size(args[1]);
		jerry_char_t *value = (jerry_char_t*) calloc(value_string_size + 1, sizeof(jerry_char_t));
		jerry_string_to_char_buffer(args[1], value, value_string_size);
    ss << value;
    free(value);
  }
  else if (jerry_value_is_object(args[1]))
  {
    printf("Second argument given to define_resource cannot be an object!\n");
    free(route);
    return jerry_create_undefined();
  }
  std::string initial_value = ss.str();

  // Get argument 2, operation
  // Default is M2MBase::GET_ALLOWED if not set.
  M2MBase::Operation operation = M2MBase::GET_ALLOWED;
  if (args_count >= 3)
  {
    operation = (M2MBase::Operation) (int) jerry_get_number_value(args[2]);
  }

  // Get argument 3, observable
  // Default is true if not set.
  bool observable = true;
  if (args_count >= 4)
  {
    observable = jerry_get_boolean_value(args[3]);
  }

  // Get argument 4, update_function
  // Default is NULL if not set.
  jerry_value_t update_function;
  if (args_count == 5)
  {
    update_function = args[4];
    jerry_acquire_value(update_function);
  }

  uintptr_t native_handle = jsmbed_wrap_get_native_handle(this_obj);
	SimpleMbedClient *native_ptr = (SimpleMbedClient*) native_handle;

	bool is_ok = native_ptr->define_resource_internal((char*) route, initial_value, operation, observable);
  if (!is_ok)
  {
    printf("ERROR: Failed to define resource using native SimpleMbedClient.\n");
    free(route);
    return jerry_create_undefined();
  }

  // Now start creating the special resource object that we'll return.
	jerry_value_t result_object = jerry_create_object();

	// TODO: reimplement SimpleResource
  //SimpleResource *simple_resource = new SimpleResource(native_ptr, (const char*) route);

	// TODO: call defineProperty with the new SimpleResource, to make sure our get/set functions
	// are called

	return jerry_create_undefined();
}

DECLARE_CLASS_FUNCTION(SimpleClient, define_function)
{
  CHECK_ARGUMENT_COUNT(SimpleClient, define_function, (args_count == 2));
  CHECK_ARGUMENT_TYPE_ALWAYS(SimpleClient, define_function, 0, string);
  CHECK_ARGUMENT_TYPE_ALWAYS(SimpleClient, define_function, 1, function);
	uintptr_t native_handle;
	jerry_get_object_native_handle(this_obj, &native_handle);

  // Get argument 0, route
	jerry_size_t string_size = jerry_get_string_size(args[0]);
	jerry_char_t *route = (jerry_char_t*) calloc(string_size + 1, sizeof(jerry_char_t));
	jerry_string_to_char_buffer(args[0], route, string_size);

  // Get argument 1, function
	jerry_value_t f = args[1];
	jerry_acquire_value(f);

	SimpleMbedClient *this_client = (SimpleMbedClient*) native_handle;

	// TODO: actually should be void(void*) - functions can be passed in an argument
	mbed::Callback<void()> cb = mbed::js::EventLoop::getInstance().wrapFunction(f);

	// TODO: normally define_function expects a function pointer or execute_callback - how
	// can we send a Callback object instead, that it can call? do we need to hack in some thunk
	// to pass to define_function, which does Callback.call()?
	this_client->define_function((char*) route, cb);

	free(route);
	return jerry_create_undefined();
}

void js_SimpleClient_destructor(const uintptr_t native_handle) {
	delete (SimpleMbedClient*) native_handle;
}

DECLARE_CLASS_CONSTRUCTOR(SimpleClient)
{
	CHECK_ARGUMENT_COUNT(SimpleClient, __constructor, (args_count == 0));

	uintptr_t native_ptr = (uintptr_t) new SimpleMbedClient();

	jerry_value_t js_object = jerry_create_object();
	jerry_set_object_native_handle(js_object, native_ptr, js_SimpleClient_destructor);

	// attach methods
  ATTACH_CLASS_FUNCTION(js_object, SimpleClient, setup);
  ATTACH_CLASS_FUNCTION(js_object, SimpleClient, keep_alive);
  ATTACH_CLASS_FUNCTION(js_object, SimpleClient, on_registered);
  ATTACH_CLASS_FUNCTION(js_object, SimpleClient, on_unregistered);
  ATTACH_CLASS_FUNCTION(js_object, SimpleClient, define_resource);
  ATTACH_CLASS_FUNCTION(js_object, SimpleClient, define_function);

	return js_object;
}

/*
 * Native callback attached to getter for .value of JS resource objects.
 */
jerry_value_t called_get (const jerry_value_t function_obj,
    const jerry_value_t this_val,
    const jerry_value_t args_p[],
    const jerry_length_t args_count)
{
  //SimpleResource *simple_resource = (SimpleResource*) jsmbed_wrap_get_native_handle(this_p);
  //*ret_val_p = simple_resource->get();
	return jerry_create_undefined();
}

/*
 * Native callback attached to setter for .value of JS resource objects.
 */
jerry_value_t called_set (const jerry_value_t function_obj,
    const jerry_value_t this_val,
    const jerry_value_t args_p[],
    const jerry_length_t args_count)
{
  //SimpleResource *simple_resource = (SimpleResource*) jsmbed_wrap_get_native_handle(this_p);
  //simple_resource->set(args_p[0]);
	return jerry_create_undefined();
}

jerry_value_t smc_cached_Object_val;
jerry_value_t smc_cached_defineProperty_val;
jerry_value_t smc_cached_defineProperty_args[3];

static void prepare_cached_objects_for_define_resource()
{
  // Cache some objects we'll be using multiple times.
  // Since they're built-ins of the global object, one would hope they won't
  // be garbage collected, but could they ever be moved?

  jerry_value_t global_obj = jerry_get_global_object();

  // CACHE global.Object
	jerry_value_t object_string = jerry_create_string((const jerry_char_t*) "Object");
  smc_cached_Object_val = jerry_get_property(global_obj, object_string);

  // CACHE global.Object.defineProperty
	jerry_value_t defprop_string = jerry_create_string((const jerry_char_t*) "defineProperty");
  smc_cached_defineProperty_val = jerry_get_property(smc_cached_Object_val, defprop_string);

  jerry_value_t properties_object = jerry_create_object();
  jerry_acquire_value(properties_object);

  jerry_value_t get_function_obj =
		jerry_create_external_function((jerry_external_handler_t) called_get);
  jerry_value_t set_function_obj =
		jerry_create_external_function((jerry_external_handler_t) called_set);

	jerry_value_t get_string = jerry_create_string((const jerry_char_t*) "get");
	jerry_value_t set_string = jerry_create_string((const jerry_char_t*) "set");
  jerry_set_property(properties_object, get_string, get_function_obj);
  jerry_set_property(properties_object, set_string, set_function_obj);

	jerry_value_t value_string = jerry_create_string((const jerry_char_t*) "value");
  jerry_acquire_value(value_string);

  // Cache the args for the Object.defineProperty call
  // NB: args[0] is set just before Object.defineProperty is called!!
  smc_cached_defineProperty_args[1] = value_string;
  smc_cached_defineProperty_args[2] = properties_object;
	return;
}

#define REGISTER_ENUM(NAME) \
  enum_val = jerry_create_number((double) M2MBase::NAME); \
	enum_name = jerry_create_string((const jerry_char_t*) #NAME); \
  jerry_set_property(m2mbase_obj, enum_name, enum_val);

static void create_m2mbase_enum_object()
{
  // Register M2M objects
  jerry_value_t m2mbase_obj = jerry_create_object();

  jerry_value_t enum_val;
	jerry_value_t enum_name;

  REGISTER_ENUM(NOT_ALLOWED)
  REGISTER_ENUM(GET_ALLOWED)
  REGISTER_ENUM(PUT_ALLOWED)
  REGISTER_ENUM(GET_PUT_ALLOWED)
  REGISTER_ENUM(POST_ALLOWED)
  REGISTER_ENUM(GET_POST_ALLOWED)
  REGISTER_ENUM(PUT_POST_ALLOWED)
  REGISTER_ENUM(GET_PUT_POST_ALLOWED)
  REGISTER_ENUM(DELETE_ALLOWED)
  REGISTER_ENUM(GET_DELETE_ALLOWED)
  REGISTER_ENUM(PUT_DELETE_ALLOWED)
  REGISTER_ENUM(GET_PUT_DELETE_ALLOWED)
  REGISTER_ENUM(POST_DELETE_ALLOWED)
  REGISTER_ENUM(GET_POST_DELETE_ALLOWED)
  REGISTER_ENUM(PUT_POST_DELETE_ALLOWED)
  REGISTER_ENUM(GET_PUT_POST_DELETE_ALLOWED)

  jerry_value_t global_obj = jerry_get_global_object();
	jerry_value_t m2mbase_name_string = jerry_create_string((const jerry_char_t*) "M2MBase");
  jerry_set_property(global_obj, m2mbase_name_string, m2mbase_obj);
}

void simple_client_one_time_setup()
{
	prepare_cached_objects_for_define_resource();
	create_m2mbase_enum_object();
}
