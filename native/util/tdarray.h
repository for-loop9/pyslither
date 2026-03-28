#ifndef TDARRAY_H
#define TDARRAY_H

#include <stdlib.h>

typedef enum _tdarray_field {
  _TDARRAY_LENGTH = 0,
  _TDARRAY_STRIDE = 1,
  _TDARRAY_CAPACITY = 2,
  _TDARRAY_FIELD_LENGTH = 3
} _tdarray_field;

size_t* _tdarray_get_fields(void* darray);
void* _tdarray_create(size_t stride, size_t capacity);
void _tdarray_insert(void** darray, size_t i, void* value_ptr);
void _tdarray_push(void** darray_ptr, void* value_ptr);
void _tdarray_pop(void* darray);
void _tdarray_remove(void* darray, size_t i);
size_t _tdarray_length(void* darray);
size_t _tdarray_memory(void* darray);
int _tdarray_find(void* darray, void* value_ptr);
void _tdarray_clear(void* darray);
void _tdarray_destroy(void* darray);

#define tdarray_create(type, capacity) \
  (type*)_tdarray_create(sizeof(type), capacity)
#define tdarray_insert(darray_ptr, i, value_ptr) \
  _tdarray_insert((void**)darray_ptr, i, value_ptr)
#define tdarray_push(darray_ptr, value_ptr) \
  _tdarray_push((void**)darray_ptr, value_ptr)
#define tdarray_pop(darray) _tdarray_pop(darray)
#define tdarray_remove(darray, i) _tdarray_remove(darray, i)
#define tdarray_length(darray) _tdarray_length(darray)
#define tdarray_memory(darray) _tdarray_memory(darray)
#define tdarray_find(darray, value_ptr) _tdarray_find(darray, value_ptr)
#define tdarray_clear(darray) _tdarray_clear(darray)
#define tdarray_destroy(darray) _tdarray_destroy(darray)

#endif