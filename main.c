// in-built inclusion
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

// my-built inclusion
#include "base.h"
#include "arena.h"
#include "arena.c"

typedef struct{
  u32 rows, cols;
  f32* data;
} matrix;

// simple operations
matrix* create_matrix(mem_arena* arena, u32 rows, u32 cols);
void clear_matrix(matrix* mat);
b32 copy_matrix(matrix* dst, matrix* src);
void fill_matrix(matrix* mat, f32 x);
void scale_matrix(matrix* mat, f32 scale);

// arithmetic operators
b32 add_matrix(matrix* out, const matrix* a, const matrix* b);
b32 sub_matrix(matrix* out, const matrix* a, const matrix* b);
b32 mul_matrix(matrix* out, const matrix* a, const matrix* b, b8 zero_output, b8 transpose_a, b8 transpose_b);

// activation functions
b32 relu_matrix(matrix* out, const matrix* in);
b32 softmax_matrix(matrix* out, const matrix* in);

// cost function
b32 cross_entropy_matrix(matrix* out, const matrix* expected_probab, const matrix* actual_probab);

// get the gradient
b32 grad_relu_add_matrix(matrix* out, const matrix* in);
b32 grad_softmax_add_matrix(matrix* out, const matrix* softmax_out);
b32 grad_cross_entropy_add_matrix(matrix* out, const matrix* expected_probab, const matrix* actual_probab);

int main() {
  printf("Hello, world!\n");

  mem_arena* permanent_arena = arena_create(GiB(1), MiB(1));
  arena_destroy(permanent_arena);

  hello();
  return 0;
}

matrix* create_matrix(mem_arena* arena, u32 rows, u32 cols){
  matrix* mat = PUSH_STRUCT(arena, matrix);

  mat->rows = rows;
  mat->cols= cols;
  mat->data= PUSH_ARRAY(arena, f32, (u64)rows * cols);

  return mat;
}

b32 copy_matrix(matrix* dst, matrix* src){
  if (dst->rows != src->rows || dst->cols != src->rows) {
    return false;
  }

  memcpy(dst->data, src->data, sizeof(f32)*(u64)dst->rows * dst->cols);

  return true;
}

void clear_matrix(matrix* mat){
  memset(mat->data, 0, sizeof(f32)*(u64)mat->rows * mat->cols);
}

void fill_matrix(matrix* mat, f32 x){
  u64 size = (u64)mat->rows * mat->cols;

  for(u64 i = 0; i < size; i++){
    mat->data[i] = x;
  }
}

