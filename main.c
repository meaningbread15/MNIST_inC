#define _CRT_SECURE_NO_WARNINGS

// in-built inclusion
#include <math.h>
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

// loading the matrix in
matrix* load_matrix(mem_arena* arena, u32 rows, u32 cols, const char* filename);

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

// 
void draw_MNIST_digits(f32* data);

int main() {
  mem_arena* permanent_arena = arena_create(GiB(1), MiB(1));

  matrix* train_images = load_matrix(permanent_arena, 60000, 784, "train_images.mat");
  matrix* test_images = load_matrix(permanent_arena, 10000, 784, "test_images.mat");
  matrix* train_labels = create_matrix(permanent_arena, 60000, 10);
  matrix* test_labels = create_matrix(permanent_arena, 10000, 10);

  {
    matrix* train_labels_file = load_matrix(permanent_arena, 60000, 1, "train_labels.mat");
    matrix* test_labels_file = load_matrix(permanent_arena, 10000, 1,"test_labels.mat");

    for (u32 i = 0; i < 60000; i++) {
      u32 num = train_labels_file->data[i];

      train_labels->data[i * 10 + num] = 1.0f;
    }
    for (u32 i = 0; i < 10000; i++) {
      u32 num = test_labels_file->data[i];

      test_labels->data[i * 10 + num] = 1.0f;
    }
  }

  draw_MNIST_digits(&train_images->data[0 * 784]);
  draw_MNIST_digits(&test_images->data[0 * 784]);

  for (u32 i = 0; i < 10; i++) {
    printf("%.0f", train_labels->data[i]);
  }

  printf("\n");

  arena_destroy(permanent_arena);

  return 0;
}

void draw_MNIST_digits(f32* data){
  for (u32 y = 0; y < 28; y++) {
    for (u32 x = 0; x < 28; x++) {
      f32 num = data[x + y * 28];
      u32 col = 232 + (u32)(num * 24);

      printf("\x1b[48;5;%dm  ", col);
    }

    printf("\n");
  }
  printf("\x1b[0m");
}

matrix* create_matrix(mem_arena* arena, u32 rows, u32 cols){
  matrix* mat = PUSH_STRUCT(arena, matrix);

  mat->rows = rows;
  mat->cols= cols;
  mat->data= PUSH_ARRAY(arena, f32, (u64)rows * cols);

  return mat;
}

matrix* load_matrix(mem_arena* arena, u32 rows, u32 cols, const char* filename){
  matrix* mat = create_matrix(arena, rows, cols);

  FILE* f = fopen(filename, "rb");
  if (!f) {
      fprintf(stderr, "Failed to open %s\n", filename);
      return NULL;
  }

  fseek(f, 0, SEEK_END);
  u64 size = ftell(f);
  fseek(f, 0, SEEK_SET);

  size = MIN(size, sizeof(f32)*rows*cols);

  fread(mat->data, 1, size, f);

  fclose(f);

  return mat;
}

b32 copy_matrix(matrix* dst, matrix* src){
  if (dst->rows != src->rows || dst->cols != src->cols) {
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

void scale_matrix(matrix* mat, f32 scale) {
  u64 size = (u64)mat->rows * mat->cols;

  for(u64 i = 0; i < size; i++){
    mat->data[i] *= scale;
  }
}

b32 sum_of_matrix(matrix* mat, f32 scale){
  u64 size = (u64)mat->rows * mat->cols;

  f32 sum = 0.0f;
  for(u64 i = 0; i < size; i++){
    sum += mat->data[i];
  }

  return sum;
}

b32 add_matrix(matrix* out, const matrix* a, const matrix* b){
  if (a->rows != b->rows || a->cols != b->cols) {
    return false;
  }
  if (out->rows != a->rows || out->cols != a->cols) {
    return false;
  }

  u64 size = (u64)out->rows * out->cols;
  for (u64 i = 0; i < size; i++) {
    out->data[i] = a->data[i] + b->data[i];
  }

  return true;
}

b32 sub_matrix(matrix* out, const matrix* a, const matrix* b){
  if (a->rows != b->rows || a->cols != b->cols) {
    return false;
  }
  if (out->rows != a->rows || out->cols != a->cols) {
    return false;
  }

  u64 size = (u64)out->rows * out->cols;
  for (u64 i = 0; i < size; i++) {
    out->data[i] = a->data[i] - b->data[i];
  }

  return true;
}

// n stands for non-transpose
// t stands for tranpose
void mat_mul_nn(matrix* out, const matrix* a, const matrix* b){
    for (u64 i = 0; i < out->rows; i++){
        for (u64 j = 0; j < out->cols; j++){
            for (u64 k = 0; k < a->cols; k++){
                out->data[i*out->cols + j] +=
                    a->data[i*a->cols + k] *
                    b->data[k*b->cols + j];
            }
        }
    }
}

void mat_mul_nt(matrix* out, const matrix* a, const matrix* b){
    for (u64 i = 0; i < out->rows; i++){
        for (u64 j = 0; j < out->cols; j++){
            for (u64 k = 0; k < a->cols; k++){
                out->data[i*out->cols + j] +=
                    a->data[i*a->cols + k] *
                    b->data[j*b->cols + k];
            }
        }
    }
}

void mat_mul_tn(matrix* out, const matrix* a, const matrix* b){
    for (u64 i = 0; i < out->rows; i++){
        for (u64 j = 0; j < out->cols; j++){
            for (u64 k = 0; k < a->rows; k++){
                out->data[i*out->cols + j] +=
                    a->data[k*a->cols + i] *
                    b->data[k*b->cols + j];
            }
        }
    }
}

void mat_mul_tt(matrix* out, const matrix* a, const matrix* b){
    for (u64 i = 0; i < out->rows; i++){
        for (u64 j = 0; j < out->cols; j++){
            for (u64 k = 0; k < a->rows; k++){
                out->data[i*out->cols + j] +=
                    a->data[k*a->cols + i] *
                    b->data[j*b->cols + k];
            }
        }
    }
}

b32 mul_matrix(matrix* out, const matrix* a, const matrix* b, b8 zero_output, b8 transpose_a, b8 transpose_b){

  u32 a_rows = transpose_a ? a->cols : a->rows;
  u32 a_cols = transpose_a ? a->rows : a->cols;
  u32 b_rows = transpose_b ? b->cols : b->rows;
  u32 b_cols = transpose_b ? b->rows : b->cols;

  if(a_cols != b_rows)
    return false;

  if(out->rows != a_rows || out->cols != b_cols)
    return false;

  if(zero_output)
    clear_matrix(out);

  u32 transpose = (transpose_a << 1) | transpose_b;
  switch (transpose){
    case 0b00: {mat_mul_nn(out, a, b);} break;
    case 0b01: {mat_mul_nt(out, a, b);} break;
    case 0b10: {mat_mul_tn(out, a, b);} break;
    case 0b11: {mat_mul_tt(out, a, b);} break;
  }

  return true;
}

b32 relu_matrix(matrix* out, const matrix* in){
  if (out->rows != in->rows || out->cols != in->cols) {
    return false;
  }

  u64 size = (u64)out->rows * out->cols;
  for (u64 i = 0; i < size; i++) {
    out->data[i] = MAX(0, in->data[i]);
  }

  return true;
}

b32 softmax_matrix(matrix* out, const matrix* in){
  if (out->rows != in->rows || out->cols != in->cols) {
    return false;
  }

  u64 size = (u64)out->rows * out->cols;

  f32 max = in->data[0];
  for (u64 i = 1; i < size; i++)
      if (in->data[i] > max) max = in->data[i];

  f32 sum = 0.0f;
  for (u64 i = 0; i < size; i++) {
      out->data[i] = expf(in->data[i] - max);
      sum += out->data[i];
  }

  scale_matrix(out, 1.0f / sum);

  return true;
}

b32 cross_entropy_matrix(matrix* out, const matrix* expected_probab, const matrix* actual_probab){
  if (expected_probab->rows != actual_probab->rows || expected_probab->cols != actual_probab->cols) {
    return false;
  }
  if (out->rows != expected_probab->rows || out->cols != expected_probab->cols) {
    return false;
  }

  u64 size = (u64)out->rows * out->cols;
  for (u64 i = 0; i < size; i++) {
    out->data[i] = expected_probab->data[i] == 0.0f ? 0.0f : expected_probab->data[i] * -logf(expected_probab->data[i]);
  }

  return true;
}

b32 grad_relu_add_matrix(matrix* out, const matrix* in);
b32 grad_softmax_add_matrix(matrix* out, const matrix* softmax_out);
b32 grad_cross_entropy_add_matrix(matrix* out, const matrix* expected_probab, const matrix* actual_probab);
