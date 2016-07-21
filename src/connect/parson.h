/*
 Parson ( http://kgabis.github.com/parson/ )
 Copyright (c) 2012 - 2016 Krzysztof Gabis

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
*/

#ifndef parson_parson_h
#define parson_parson_h

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>   /* size_t */

/* Types and enums */
typedef struct x_json_object_t x_JSON_Object;
typedef struct x_json_array_t  x_JSON_Array;
typedef struct x_json_value_t  x_JSON_Value;

enum x_json_value_type {
    JSONError   = -1,
    JSONNull    = 1,
    JSONString  = 2,
    JSONNumber  = 3,
    JSONObject  = 4,
    JSONArray   = 5,
    JSONBoolean = 6
};
typedef int x_JSON_Value_Type;

enum x_json_result_t {
    JSONSuccess = 0,
    JSONFailure = -1
};
typedef int x_JSON_Status;

typedef void * (*x_JSON_Malloc_Function)(size_t);
typedef void   (*x_JSON_Free_Function)(void *);

/* Call only once, before calling any other function from parson API. If not called, malloc and free
   from stdlib will be used for all allocations */
void x_json_set_allocation_functions(x_JSON_Malloc_Function malloc_fun, x_JSON_Free_Function free_fun);

/* Parses first JSON value in a file, returns NULL in case of error */
x_JSON_Value * x_json_parse_file(const char *filename);

/* Parses first JSON value in a file and ignores comments (/ * * / and //),
   returns NULL in case of error */
x_JSON_Value * x_json_parse_file_with_comments(const char *filename);

/*  Parses first JSON value in a string, returns NULL in case of error */
x_JSON_Value * x_json_parse_string(const char *string);

/*  Parses first JSON value in a string and ignores comments (/ * * / and //),
    returns NULL in case of error */
x_JSON_Value * x_json_parse_string_with_comments(const char *string);

/* Serialization */
size_t      x_json_serialization_size(const x_JSON_Value *value); /* returns 0 on fail */
x_JSON_Status x_json_serialize_to_buffer(const x_JSON_Value *value, char *buf, size_t buf_size_in_bytes);
x_JSON_Status x_json_serialize_to_file(const x_JSON_Value *value, const char *filename);
char *      x_json_serialize_to_string(const x_JSON_Value *value);

/* Pretty serialization */
size_t      x_json_serialization_size_pretty(const x_JSON_Value *value); /* returns 0 on fail */
x_JSON_Status x_json_serialize_to_buffer_pretty(const  x_JSON_Value *value, char *buf, size_t buf_size_in_bytes);
x_JSON_Status x_json_serialize_to_file_pretty(const  x_JSON_Value *value, const char *filename);
char *      x_json_serialize_to_string_pretty(const  x_JSON_Value *value);

void        x_json_free_serialized_string(char *string); /* frees string from json_serialize_to_string and json_serialize_to_string_pretty */

/* Comparing */
int  x_json_value_equals(const  x_JSON_Value *a, const  x_JSON_Value *b);

/* Validation
   This is *NOT* JSON Schema. It validates json by checking if object have identically
   named fields with matching types.
   For example schema {"name":"", "age":0} will validate
   {"name":"Joe", "age":25} and {"name":"Joe", "age":25, "gender":"m"},
   but not {"name":"Joe"} or {"name":"Joe", "age":"Cucumber"}.
   In case of arrays, only first value in schema is checked against all values in tested array.
   Empty objects ({}) validate all objects, empty arrays ([]) validate all arrays,
   null validates values of every type.
 */
x_JSON_Status json_validate(const  x_JSON_Value *schema, const  x_JSON_Value *value);

/*
 * JSON Object
 */
x_JSON_Value  * x_json_object_get_value  (const x_JSON_Object *object, const char *name);
const char  * x_json_object_get_string (const x_JSON_Object *object, const char *name);
x_JSON_Object * x_json_object_get_object (const x_JSON_Object *object, const char *name);
x_JSON_Array  * x_json_object_get_array  (const x_JSON_Object *object, const char *name);
double        x_json_object_get_number (const x_JSON_Object *object, const char *name); /* returns 0 on fail */
int           x_json_object_get_boolean(const x_JSON_Object *object, const char *name); /* returns -1 on fail */

/* dotget functions enable addressing values with dot notation in nested objects,
 just like in structs or c++/java/c# objects (e.g. objectA.objectB.value).
 Because valid names in JSON can contain dots, some values may be inaccessible
 this way. */
x_JSON_Value  * x_json_object_dotget_value  (const x_JSON_Object *object, const char *name);
const char  * x_json_object_dotget_string (const x_JSON_Object *object, const char *name);
x_JSON_Object * x_json_object_dotget_object (const x_JSON_Object *object, const char *name);
x_JSON_Array  * x_json_object_dotget_array  (const x_JSON_Object *object, const char *name);
double        x_json_object_dotget_number (const x_JSON_Object *object, const char *name); /* returns 0 on fail */
int           x_json_object_dotget_boolean(const x_JSON_Object *object, const char *name); /* returns -1 on fail */

/* Functions to get available names */
size_t        x_json_object_get_count   (const x_JSON_Object *object);
const char  * x_json_object_get_name    (const x_JSON_Object *object, size_t index);
x_JSON_Value  * x_json_object_get_value_at(const x_JSON_Object *object, size_t index);

/* Creates new name-value pair or frees and replaces old value with a new one.
 * json_object_set_value does not copy passed value so it shouldn't be freed afterwards. */
x_JSON_Status x_json_object_set_value(x_JSON_Object *object, const char *name,  x_JSON_Value *value);
x_JSON_Status x_json_object_set_string(x_JSON_Object *object, const char *name, const char *string);
x_JSON_Status x_json_object_set_number(x_JSON_Object *object, const char *name, double number);
x_JSON_Status x_json_object_set_boolean(x_JSON_Object *object, const char *name, int boolean);
x_JSON_Status x_json_object_set_null(x_JSON_Object *object, const char *name);

/* Works like dotget functions, but creates whole hierarchy if necessary.
 * json_object_dotset_value does not copy passed value so it shouldn't be freed afterwards. */
x_JSON_Status x_json_object_dotset_value(x_JSON_Object *object, const char *name,  x_JSON_Value *value);
x_JSON_Status x_json_object_dotset_string(x_JSON_Object *object, const char *name, const char *string);
x_JSON_Status x_json_object_dotset_number(x_JSON_Object *object, const char *name, double number);
x_JSON_Status x_json_object_dotset_boolean(x_JSON_Object *object, const char *name, int boolean);
x_JSON_Status x_json_object_dotset_null(x_JSON_Object *object, const char *name);

/* Frees and removes name-value pair */
x_JSON_Status x_json_object_remove(x_JSON_Object *object, const char *name);

/* Works like dotget function, but removes name-value pair only on exact match. */
x_JSON_Status x_json_object_dotremove(x_JSON_Object *object, const char *key);

/* Removes all name-value pairs in object */
x_JSON_Status x_json_object_clear(x_JSON_Object *object);

/*
 *JSON Array
 */
x_JSON_Value  * x_json_array_get_value  (const x_JSON_Array *array, size_t index);
const char  * x_json_array_get_string (const x_JSON_Array *array, size_t index);
x_JSON_Object * x_json_array_get_object (const x_JSON_Array *array, size_t index);
x_JSON_Array  * x_json_array_get_array  (const x_JSON_Array *array, size_t index);
double        x_json_array_get_number (const x_JSON_Array *array, size_t index); /* returns 0 on fail */
int           x_json_array_get_boolean(const x_JSON_Array *array, size_t index); /* returns -1 on fail */
size_t        x_json_array_get_count  (const x_JSON_Array *array);

/* Frees and removes value at given index, does nothing and returns JSONFailure if index doesn't exist.
 * Order of values in array may change during execution.  */
x_JSON_Status json_array_remove(x_JSON_Array *array, size_t i);

/* Frees and removes from array value at given index and replaces it with given one.
 * Does nothing and returns JSONFailure if index doesn't exist.
 * json_array_replace_value does not copy passed value so it shouldn't be freed afterwards. */
x_JSON_Status x_json_array_replace_value(x_JSON_Array *array, size_t i,  x_JSON_Value *value);
x_JSON_Status x_json_array_replace_string(x_JSON_Array *array, size_t i, const char* string);
x_JSON_Status x_json_array_replace_number(x_JSON_Array *array, size_t i, double number);
x_JSON_Status x_json_array_replace_boolean(x_JSON_Array *array, size_t i, int boolean);
x_JSON_Status x_json_array_replace_null(x_JSON_Array *array, size_t i);

/* Frees and removes all values from array */
x_JSON_Status x_json_array_clear(x_JSON_Array *array);

/* Appends new value at the end of array.
 * json_array_append_value does not copy passed value so it shouldn't be freed afterwards. */
x_JSON_Status x_json_array_append_value(x_JSON_Array *array,  x_JSON_Value *value);
x_JSON_Status x_json_array_append_string(x_JSON_Array *array, const char *string);
x_JSON_Status x_json_array_append_number(x_JSON_Array *array, double number);
x_JSON_Status x_json_array_append_boolean(x_JSON_Array *array, int boolean);
x_JSON_Status x_json_array_append_null(x_JSON_Array *array);

/*
 *JSON Value
 */
x_JSON_Value * x_json_value_init_object (void);
x_JSON_Value * x_json_value_init_array  (void);
x_JSON_Value * x_json_value_init_string (const char *string); /* copies passed string */
x_JSON_Value * x_json_value_init_number (double number);
x_JSON_Value * x_json_value_init_boolean(int boolean);
x_JSON_Value * x_json_value_init_null   (void);
x_JSON_Value * x_json_value_deep_copy   (const  x_JSON_Value *value);
void         x_json_value_free        (x_JSON_Value *value);

x_JSON_Value_Type x_json_value_get_type   (const  x_JSON_Value *value);
x_JSON_Object *   x_json_value_get_object (const  x_JSON_Value *value);
x_JSON_Array  *   x_json_value_get_array  (const  x_JSON_Value *value);
const char  *   x_json_value_get_string (const  x_JSON_Value *value);
double          x_json_value_get_number (const  x_JSON_Value *value);
int             x_json_value_get_boolean(const  x_JSON_Value *value);

/* Same as above, but shorter */
x_JSON_Value_Type x_json_type   (const  x_JSON_Value *value);
x_JSON_Object *   x_json_object (const  x_JSON_Value *value);
x_JSON_Array  *   x_json_array  (const  x_JSON_Value *value);
const char  *   x_json_string (const  x_JSON_Value *value);
double          x_json_number (const  x_JSON_Value *value);
int             x_json_boolean(const  x_JSON_Value *value);

#ifdef __cplusplus
}
#endif

#endif
