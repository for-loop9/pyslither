#include "tdarray.h"

#include <stdio.h>
#include <string.h>

size_t* _tdarray_get_fields(void* darray) {
  return (size_t*)((char*)darray - _TDARRAY_FIELD_LENGTH * sizeof(size_t));
}

void* _tdarray_create(size_t stride, size_t capacity) {
  if (capacity == 0) capacity = 1;  // optional safety
  size_t header_size = _TDARRAY_FIELD_LENGTH * sizeof(size_t);
  char* raw = malloc(header_size + stride * capacity);
  void* r = raw + header_size;
  size_t* fields = (size_t*)raw;

  fields[_TDARRAY_LENGTH] = 0;
  fields[_TDARRAY_STRIDE] = stride;
  fields[_TDARRAY_CAPACITY] = capacity;

  return r;
}

void _tdarray_insert(void** darray, size_t i, void* value_ptr) {
  size_t* fields = _tdarray_get_fields(*darray);

  if (fields[_TDARRAY_LENGTH] >= fields[_TDARRAY_CAPACITY]) {
    char* beg = (char*)(*darray) - _TDARRAY_FIELD_LENGTH * sizeof(size_t);

    fields[_TDARRAY_CAPACITY] *= 2;

    beg = realloc(beg, _TDARRAY_FIELD_LENGTH * sizeof(size_t) +
                           fields[_TDARRAY_CAPACITY] * fields[_TDARRAY_STRIDE]);

    *darray = beg + _TDARRAY_FIELD_LENGTH * sizeof(size_t);
    fields = _tdarray_get_fields(*darray);
  }

  char* data = (char*)(*darray);

  memmove(data + (i + 1) * fields[_TDARRAY_STRIDE],
          data + i * fields[_TDARRAY_STRIDE],
          (fields[_TDARRAY_LENGTH] - i) * fields[_TDARRAY_STRIDE]);

  memcpy(data + i * fields[_TDARRAY_STRIDE], value_ptr,
         fields[_TDARRAY_STRIDE]);

  fields[_TDARRAY_LENGTH]++;
}

void _tdarray_push(void** darray, void* value_ptr) {
  size_t* fields = _tdarray_get_fields(*darray);

  if (fields[_TDARRAY_LENGTH] >= fields[_TDARRAY_CAPACITY]) {
    char* beg = (char*)(*darray) - _TDARRAY_FIELD_LENGTH * sizeof(size_t);

    fields[_TDARRAY_CAPACITY] *= 2;

    beg = realloc(beg, _TDARRAY_FIELD_LENGTH * sizeof(size_t) +
                           fields[_TDARRAY_CAPACITY] * fields[_TDARRAY_STRIDE]);

    *darray = beg + _TDARRAY_FIELD_LENGTH * sizeof(size_t);
    fields = _tdarray_get_fields(*darray);
  }

  char* data = (char*)(*darray);

  memcpy(data + fields[_TDARRAY_LENGTH] * fields[_TDARRAY_STRIDE], value_ptr,
         fields[_TDARRAY_STRIDE]);

  fields[_TDARRAY_LENGTH]++;
}

void _tdarray_pop(void* darray) {
  size_t* fields = _tdarray_get_fields(darray);
  fields[_TDARRAY_LENGTH]--;
}

void _tdarray_remove(void* darray, size_t i) {
  size_t* fields = _tdarray_get_fields(darray);
  char* data = (char*)darray;

  fields[_TDARRAY_LENGTH]--;

  memmove(data + i * fields[_TDARRAY_STRIDE],
          data + (i + 1) * fields[_TDARRAY_STRIDE],
          (fields[_TDARRAY_LENGTH] - i) * fields[_TDARRAY_STRIDE]);
}

size_t _tdarray_length(void* darray) {
  return _tdarray_get_fields(darray)[_TDARRAY_LENGTH];
}

size_t _tdarray_memory(void* darray) {
  size_t* fields = _tdarray_get_fields(darray);
  return fields[_TDARRAY_CAPACITY] * fields[_TDARRAY_STRIDE];
}

int _tdarray_find(void* darray, void* value_ptr) {
  size_t* fields = _tdarray_get_fields(darray);
  char* data = (char*)darray;

  for (size_t i = 0; i < fields[_TDARRAY_LENGTH]; i++) {
    if (memcmp(data + i * fields[_TDARRAY_STRIDE], value_ptr,
               fields[_TDARRAY_STRIDE]) == 0) {
      return (int)i;
    }
  }

  return -1;
}

void _tdarray_clear(void* darray) {
  size_t* fields = _tdarray_get_fields(darray);
  fields[_TDARRAY_LENGTH] = 0;
}

void _tdarray_destroy(void* darray) {
  char* beg = (char*)darray - _TDARRAY_FIELD_LENGTH * sizeof(size_t);
  free(beg);
}