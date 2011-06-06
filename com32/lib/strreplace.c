/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2011 Erwan Velu - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * -----------------------------------------------------------------------
 */

#include <string.h>
#include <stdlib.h>

char *strreplace( const char *string, const char *string_to_replace, const char *string_to_insert ){
  char *token = NULL;
  char *output_buffer = NULL;

  token = strstr(string, string_to_replace);

  if(token == NULL) 
	return strdup( string );

  output_buffer = malloc(strlen( string )-strlen( string_to_replace )+strlen(string_to_insert )+ 1);

  if(output_buffer == NULL) 
	return NULL;

  memcpy(output_buffer, string, token-string);
  memcpy(output_buffer + (token - string), string_to_insert, strlen(string_to_insert));
  memcpy(output_buffer + (token - string) + strlen(string_to_insert),
	token + strlen(string_to_replace), 
	strlen(string) - strlen(string_to_replace) - (token - string));
  memset(output_buffer + strlen(string) - strlen(string_to_replace) + strlen(string_to_insert), 0, 1 );

  return output_buffer;
}
