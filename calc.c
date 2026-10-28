// Serhii Beilakh 260ADB195
// https://github.com/Serhii5555/Arithmetic-Parser-in-C
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: Gemini, Google, Stack Overflow

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <math.h>

#define STUDENT_ID "260ADB195"

typedef struct {
  long long integer;
  double floating;
  int is_float;
}
Number;

static void skip_spaces(const char ** p) {
  while ( ** p == ' ' || ** p == '\t' || ** p == '\r') {
    ( * p) ++;
  }
}

static int parse_expression(const char ** p,
  const char * line,
    Number * result, size_t * error_pos);

static Number make_integer(long long value) {
  Number n = {
    value,
    (double) value,
    0
  };
  return n;
}

static Number make_float(double value) {
  Number n = {
    0,
    value,
    1
  };
  return n;
}

static double number_value(Number n) {
  return n.is_float ? n.floating : (double) n.integer;
}

static int multiply_checked(long long a, long long b, long long * result) {
  if (a > 0) {
    if ((b > 0 && a > LLONG_MAX / b) ||
      (b < 0 && b < LLONG_MIN / a)) {
      return 0;
    }
  } else if (a < 0) {
    if ((b > 0 && a < LLONG_MIN / b) ||
      (b < 0 && a < LLONG_MAX / b)) {
      return 0;
    }
  }

  * result = a * b;
  return 1;
}

static int parse_primary(const char ** p,
  const char * line,
    Number * result, size_t * error_pos) {
  skip_spaces(p);

  if ( ** p == '(') {
    ( * p) ++;

    if (!parse_expression(p, line, result, error_pos)) {
      return 0;
    }

    skip_spaces(p);

    if ( ** p != ')') {
      * error_pos = (size_t)( * p - line) + 1;
      return 0;
    }

    ( * p) ++;
    return 1;
  }

  if ( ** p < '0' || ** p > '9') {
    * error_pos = (size_t)( * p - line) + 1;
    return 0;
  }

  const char * start = * p;
  long long integer = 0;

  while ( ** p >= '0' && ** p <= '9') {
    int digit = ** p - '0';

    if (integer > (LLONG_MAX - digit) / 10) {
      * error_pos = (size_t)( * p - line) + 1;
      return 0;
    }

    integer = integer * 10 + digit;
    ( * p) ++;
  }

  if ( ** p != '.') {
    * result = make_integer(integer);
    return 1;
  }

  const char * dot = * p;
  ( * p) ++;

  if ( ** p < '0' || ** p > '9') {
    * error_pos = (size_t)(dot - line) + 1;
    return 0;
  }

  while ( ** p >= '0' && ** p <= '9') {
    ( * p) ++;
  }

  char * end;
  errno = 0;
  double value = strtod(start, & end);

  if (end != * p || !isfinite(value)) {
    * error_pos = (size_t)(start - line) + 1;
    return 0;
  }

  * result = make_float(value);
  return 1;
}

static int parse_power(const char ** p,
  const char * line,
    Number * result, size_t * error_pos) {
  Number base;

  if (!parse_primary(p, line, & base, error_pos)) {
    return 0;
  }

  skip_spaces(p);

  if (( * p)[0] != '*' || ( * p)[1] != '*') {
    * result = base;
    return 1;
  }

  const char * operator_pos = * p;
  ( * p) += 2;
  skip_spaces(p);

  const char * rhs_start = * p;
  Number exponent;

  if (!parse_power(p, line, & exponent, error_pos)) {
    return 0;
  }

  if (!base.is_float && !exponent.is_float) {
    if (exponent.integer < 0) {
      * error_pos = (size_t)(rhs_start - line) + 1;
      return 0;
    }

    long long value = 1;
    long long factor = base.integer;
    long long remaining = exponent.integer;

    while (remaining > 0) {
      if (remaining % 2 != 0 &&
        !multiply_checked(value, factor, & value)) {
        * error_pos = (size_t)(operator_pos - line) + 1;
        return 0;
      }

      remaining /= 2;

      if (remaining > 0 &&
        !multiply_checked(factor, factor, & factor)) {
        * error_pos = (size_t)(operator_pos - line) + 1;
        return 0;
      }
    }

    * result = make_integer(value);
    return 1;
  }

  double value = pow(number_value(base), number_value(exponent));

  if (!isfinite(value)) {
    * error_pos = (size_t)(operator_pos - line) + 1;
    return 0;
  }

  * result = make_float(value);
  return 1;
}

static int parse_term(const char ** p,
  const char * line,
    Number * result, size_t * error_pos) {
  Number value;

  if (!parse_power(p, line, & value, error_pos)) {
    return 0;
  }

  for (;;) {
    skip_spaces(p);

    char operation = ** p;

    if (operation != '*' && operation != '/') {
      break;
    }

    ( * p) ++;
    skip_spaces(p);

    const char * rhs_start = * p;
    Number right;

    if (!parse_power(p, line, & right, error_pos)) {
      return 0;
    }

    double a = number_value(value);
    double b = number_value(right);

    if (operation == '/' && b == 0.0) {
      * error_pos = (size_t)(rhs_start - line) + 1;
      return 0;
    }

    if (!value.is_float && !right.is_float) {
      if (operation == '/') {
        if (value.integer == LLONG_MIN && right.integer == -1) {
          * error_pos = (size_t)(rhs_start - line) + 1;
          return 0;
        }

        value = make_integer(value.integer / right.integer);
      } else {
        long long product;

        if (!multiply_checked(value.integer, right.integer, & product)) {
          * error_pos = (size_t)(rhs_start - line) + 1;
          return 0;
        }

        value = make_integer(product);
      }
    } else {
      double calculated = operation == '*' ? a * b : a / b;

      if (!isfinite(calculated)) {
        * error_pos = (size_t)(rhs_start - line) + 1;
        return 0;
      }

      value = make_float(calculated);
    }
  }

  * result = value;
  return 1;
}

static int parse_expression(const char ** p,
  const char * line,
    Number * result, size_t * error_pos) {
  Number value;

  if (!parse_term(p, line, & value, error_pos)) {
    return 0;
  }

  for (;;) {
    skip_spaces(p);

    char operation = ** p;

    if (operation != '+' && operation != '-') {
      break;
    }

    ( * p) ++;
    skip_spaces(p);

    const char * rhs_start = * p;
    Number right;

    if (!parse_term(p, line, & right, error_pos)) {
      return 0;
    }

    if (!value.is_float && !right.is_float) {
      long long a = value.integer;
      long long b = right.integer;
      long long calculated;

      if (operation == '+') {
        if ((b > 0 && a > LLONG_MAX - b) ||
          (b < 0 && a < LLONG_MIN - b)) {
          * error_pos = (size_t)(rhs_start - line) + 1;
          return 0;
        }

        calculated = a + b;
      } else {
        if ((b < 0 && a > LLONG_MAX + b) ||
          (b > 0 && a < LLONG_MIN + b)) {
          * error_pos = (size_t)(rhs_start - line) + 1;
          return 0;
        }

        calculated = a - b;
      }

      value = make_integer(calculated);
    } else {
      double a = number_value(value);
      double b = number_value(right);
      double calculated = operation == '+' ? a + b : a - b;

      if (!isfinite(calculated)) {
        * error_pos = (size_t)(rhs_start - line) + 1;
        return 0;
      }

      value = make_float(calculated);
    }
  }

  * result = value;
  return 1;
}

static int calculate_line(const char * line, Number * result,
  size_t * error_pos) {
  const char * p = line;

  if (!parse_expression( & p, line, result, error_pos)) {
    return 0;
  }

  skip_spaces( & p);

  if ( * p == '\n' || * p == '\0') {
    return 1;
  }

  * error_pos = (size_t)(p - line) + 1;
  return 0;
}

static int read_line(FILE * input, char ** line, size_t * capacity) {
  size_t length = 0;
  int ch;

  while ((ch = fgetc(input)) != EOF) {
    if (length + 1 >= * capacity) {
      size_t new_capacity = * capacity == 0 ? 128 : * capacity * 2;

      if (new_capacity < * capacity) {
        return -1;
      }

      char * new_line = realloc( * line, new_capacity);

      if (new_line == NULL) {
        return -1;
      }

      * line = new_line;
      * capacity = new_capacity;
    }

    ( * line)[length++] = (char) ch;

    if (ch == '\n') {
      break;
    }
  }

  if (ferror(input)) {
    return -1;
  }

  if (length == 0 && ch == EOF) {
    return 0;
  }

  ( * line)[length] = '\0';
  return 1;
}

static int process_file(const char * input_path,
  const char * requested_outdir) {
  FILE * input = fopen(input_path, "r");

  if (input == NULL) {
    perror(input_path);
    return 1;
  }

  const char * base = strrchr(input_path, '/');
  base = base == NULL ? input_path : base + 1;

  char name[4096];
  size_t len = strlen(base);

  if (len >= sizeof(name)) {
    fprintf(stderr, "Input filename is too long\n");
    fclose(input);
    return 1;
  }

  memcpy(name, base, len + 1);

  char * extension = strrchr(name, '.');

  if (extension != NULL && strcmp(extension, ".txt") == 0) {
    * extension = '\0';
  }

  char default_outdir[4096];

  const char * outdir = requested_outdir;

  if (outdir == NULL) {
    int length = snprintf(
      default_outdir,
      sizeof(default_outdir),
      "%s_serhii_beilakh_%s",
      name,
      STUDENT_ID
    );

    if (length < 0 || (size_t) length >= sizeof(default_outdir)) {
      fprintf(stderr, "Output directory path is too long\n");
      fclose(input);
      return 1;
    }

    outdir = default_outdir;
  }

  if (mkdir(outdir, 0777) != 0 && errno != EEXIST) {
    perror(outdir);
    fclose(input);
    return 1;
  }

  struct stat info;

  if (stat(outdir, & info) != 0 || !S_ISDIR(info.st_mode)) {
    fprintf(stderr, "Output path is not a directory\n");
    fclose(input);
    return 1;
  }

  char output_path[8192];

  int written = snprintf(
    output_path,
    sizeof(output_path),
    "%s/%s_serhii_beilakh_%s.txt",
    outdir,
    name,
    STUDENT_ID
  );

  if (written < 0 || (size_t) written >= sizeof(output_path)) {
    fprintf(stderr, "Output path is too long\n");
    fclose(input);
    return 1;
  }

  FILE * output = fopen(output_path, "w");

  if (output == NULL) {
    perror(output_path);
    fclose(input);
    return 1;
  }

  char * line = NULL;
  size_t capacity = 0;
  size_t file_position = 1;
  int failed = 0;
  int status;

  while ((status = read_line(input, & line, & capacity)) == 1) {
    Number result;
    size_t error_pos;
    size_t leading_spaces = strspn(line, " \t\r\n");

    if (line[leading_spaces] == '\0' || line[leading_spaces] == '#') {
      file_position += strlen(line);
      continue;
    }

    if (!calculate_line(line, & result, & error_pos)) {
      size_t absolute_error_pos = file_position + error_pos - 1;

      if (fclose(output) != 0) {
        output = NULL;
        failed = 1;
        break;
      }

      output = fopen(output_path, "w");

      if (output == NULL) {
        perror(output_path);
        failed = 1;
        break;
      }

      if (fprintf(output, "ERROR:%zu\n", absolute_error_pos) < 0) {
        failed = 1;
      }

      break;
    }

    int write_result;

    if (result.is_float) {
      write_result = fprintf(output, "%.15g\n", result.floating);
    } else {
      write_result = fprintf(output, "%lld\n", result.integer);
    }

    if (write_result < 0) {
      failed = 1;
      break;
    }

    file_position += strlen(line);
  }

  free(line);

  if (status == -1) {
    fprintf(stderr, "Error reading input file\n");
    failed = 1;
  }

  if (fclose(input) != 0) {
    failed = 1;
  }

  if (output != NULL && fclose(output) != 0) {
    failed = 1;
  }

  return failed ? 1 : 0;
}

static int process_directory(const char * input_dir,
  const char * requested_outdir) {
  DIR * directory = opendir(input_dir);

  if (directory == NULL) {
    perror(input_dir);
    return 1;
  }

  char default_outdir[4096];
  const char * outdir = requested_outdir;

  if (outdir == NULL) {
    const char * base = strrchr(input_dir, '/');
    base = base == NULL ? input_dir : base + 1;

    if ( * base == '\0') {
      base = "output";
    }

    int length = snprintf(
      default_outdir,
      sizeof(default_outdir),
      "%s_serhii_beilakh_%s",
      base,
      STUDENT_ID
    );

    if (length < 0 || (size_t) length >= sizeof(default_outdir)) {
      fprintf(stderr, "Output directory path is too long\n");
      closedir(directory);
      return 1;
    }

    outdir = default_outdir;
  }

  if (mkdir(outdir, 0777) != 0 && errno != EEXIST) {
    perror(outdir);
    closedir(directory);
    return 1;
  }

  struct stat out_info;

  if (stat(outdir, & out_info) != 0 || !S_ISDIR(out_info.st_mode)) {
    fprintf(stderr, "Output path is not a directory\n");
    closedir(directory);
    return 1;
  }

  struct dirent * entry;

  int failed = 0;
  size_t processed = 0;

  while ((entry = readdir(directory)) != NULL) {
    size_t name_len = strlen(entry -> d_name);

    if (name_len <= 4 ||
      strcmp(entry -> d_name + name_len - 4, ".txt") != 0) {
      continue;
    }

    if (strcmp(entry -> d_name, ".") == 0 ||
      strcmp(entry -> d_name, "..") == 0) {
      continue;
    }

    char input_path[8192];

    int written = snprintf(
      input_path,
      sizeof(input_path),
      "%s/%s",
      input_dir,
      entry -> d_name
    );

    if (written < 0 || (size_t) written >= sizeof(input_path)) {
      fprintf(stderr, "Input path is too long\n");
      failed = 1;
      continue;
    }

    struct stat info;

    if (stat(input_path, & info) != 0) {
      perror(input_path);
      failed = 1;
      continue;
    }

    if (!S_ISREG(info.st_mode)) {
      continue;
    }

    if (process_file(input_path, outdir) != 0) {
      failed = 1;
    }

    processed++;
  }

  if (closedir(directory) != 0) {
    failed = 1;
  }

  if (processed == 0) {
    fprintf(stderr, "No .txt files found in directory: %s\n", input_dir);
    failed = 1;
  }

  return failed ? 1 : 0;
}

int main(int argc, char * argv[]) {
  const char * input_path = NULL;
  const char * input_dir = NULL;
  const char * outdir = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0) {
      printf("Usage:\n");
      printf("  %s [--output-dir OUTDIR] input.txt\n", argv[0]);
      printf("  %s -o OUTDIR input.txt\n", argv[0]);
      printf("  %s -d DIR [-o OUTDIR]\n", argv[0]);
      printf("  %s --dir DIR [--output-dir OUTDIR]\n", argv[0]);
      return 0;
    }

    if (strcmp(argv[i], "-o") == 0 ||
      strcmp(argv[i], "--output-dir") == 0) {
      if (outdir != NULL || i + 1 >= argc) {
        fprintf(stderr, "Invalid output directory option\n");
        return 1;
      }

      outdir = argv[++i];
      continue;
    }

    if (strcmp(argv[i], "-d") == 0 ||
      strcmp(argv[i], "--dir") == 0) {
      if (input_dir != NULL || input_path != NULL || i + 1 >= argc) {
        fprintf(stderr, "Invalid input directory option\n");
        return 1;
      }

      input_dir = argv[++i];
      continue;
    }

    if (argv[i][0] == '-' || input_path != NULL || input_dir != NULL) {
      fprintf(stderr, "Invalid argument: %s\n", argv[i]);
      return 1;
    }

    input_path = argv[i];
  }

  if ((input_path == NULL) == (input_dir == NULL)) {
    fprintf(stderr, "Specify an input file or a directory\n");
    return 1;
  }

  if (input_dir != NULL) {
    return process_directory(input_dir, outdir);
  }

  return process_file(input_path, outdir);
}