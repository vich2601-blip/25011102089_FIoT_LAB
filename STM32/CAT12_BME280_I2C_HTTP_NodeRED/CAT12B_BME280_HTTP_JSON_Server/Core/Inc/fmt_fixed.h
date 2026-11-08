/**
  * @file    fmt_fixed.h
  * @brief   Print a float with 2 decimals WITHOUT printf("%f").
  *          newlib-nano (the CubeIDE default) does not support %f unless the
  *          "-u _printf_float" linker option is enabled, which adds ~8 KB.
  *          Integer maths is smaller and always works.
  */
#ifndef FMT_FIXED_H
#define FMT_FIXED_H

#include <stdio.h>
#include <stdint.h>

static inline void fmt_2dp(char *out, size_t size, float value)
{
  int32_t scaled = (int32_t)(value * 100.0f + (value >= 0.0f ? 0.5f : -0.5f));
  const char *sign = "";
  if (scaled < 0)
  {
    sign   = "-";
    scaled = -scaled;
  }
  snprintf(out, size, "%s%ld.%02ld", sign, (long)(scaled / 100), (long)(scaled % 100));
}

#endif
