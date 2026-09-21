#include "text_format.h"

#include <stdarg.h>
#include <stdint.h>

typedef struct
{
  char *buffer;
  size_t size;
  size_t length;
} TextWriter;

static void write_character(TextWriter *writer, char value)
{
  if ((writer->size > 0U) && (writer->length < (writer->size - 1U)))
  {
    writer->buffer[writer->length] = value;
  }
  ++writer->length;
}

static void write_string(TextWriter *writer, const char *value,
                         int precision)
{
  int count = 0;

  if (value == NULL)
  {
    value = "(null)";
  }
  while ((*value != '\0') && ((precision < 0) || (count < precision)))
  {
    write_character(writer, *value++);
    ++count;
  }
}

static void write_unsigned(TextWriter *writer, uint32_t value, uint8_t width,
                           char padding)
{
  char digits[10];
  uint8_t count = 0U;

  do
  {
    digits[count++] = (char)('0' + (value % 10U));
    value /= 10U;
  } while ((value != 0U) && (count < sizeof(digits)));

  while (count < width)
  {
    write_character(writer, padding);
    --width;
  }
  while (count > 0U)
  {
    write_character(writer, digits[--count]);
  }
}

static void write_signed(TextWriter *writer, int32_t value, uint8_t width,
                         char padding)
{
  uint32_t magnitude;

  if (value < 0)
  {
    write_character(writer, '-');
    if (width > 0U)
    {
      --width;
    }
    magnitude = (uint32_t)(-(value + 1)) + 1U;
  }
  else
  {
    magnitude = (uint32_t)value;
  }
  write_unsigned(writer, magnitude, width, padding);
}

int TextFormat(char *buffer, size_t size, const char *format, ...)
{
  TextWriter writer = {buffer, size, 0U};
  va_list arguments;

  if ((buffer == NULL) || (format == NULL))
  {
    return -1;
  }

  va_start(arguments, format);
  while (*format != '\0')
  {
    uint8_t width = 0U;
    int precision = -1;
    char padding = ' ';
    uint8_t long_argument = 0U;

    if (*format != '%')
    {
      write_character(&writer, *format++);
      continue;
    }
    ++format;
    if (*format == '%')
    {
      write_character(&writer, *format++);
      continue;
    }
    if (*format == '0')
    {
      padding = '0';
      ++format;
    }
    while ((*format >= '0') && (*format <= '9'))
    {
      width = (uint8_t)(width * 10U + (uint8_t)(*format - '0'));
      ++format;
    }
    if (*format == '.')
    {
      precision = 0;
      ++format;
      while ((*format >= '0') && (*format <= '9'))
      {
        precision = precision * 10 + (*format - '0');
        ++format;
      }
    }
    if (*format == 'l')
    {
      long_argument = 1U;
      ++format;
    }

    switch (*format)
    {
      case 'c':
        write_character(&writer, (char)va_arg(arguments, int));
        break;
      case 's':
        write_string(&writer, va_arg(arguments, const char *), precision);
        break;
      case 'd':
        write_signed(&writer,
                     (long_argument != 0U)
                     ? (int32_t)va_arg(arguments, long)
                     : (int32_t)va_arg(arguments, int),
                     width, padding);
        break;
      case 'u':
        write_unsigned(&writer,
                       (long_argument != 0U)
                       ? (uint32_t)va_arg(arguments, unsigned long)
                       : (uint32_t)va_arg(arguments, unsigned int),
                       width, padding);
        break;
      default:
        write_character(&writer, '%');
        if (*format != '\0')
        {
          write_character(&writer, *format);
        }
        break;
    }
    if (*format != '\0')
    {
      ++format;
    }
  }
  va_end(arguments);

  if (size > 0U)
  {
    size_t terminator = (writer.length < size) ? writer.length : (size - 1U);
    buffer[terminator] = '\0';
  }
  return (writer.length > (size_t)INT32_MAX) ? INT32_MAX
                                              : (int)writer.length;
}
