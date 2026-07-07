#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"

long json_ws(char *json)
{
	char *s = json;
	while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
		s++;
	return s - json;
}

long json_len(char *json)
{
	char *s = json;
	s += json_ws(s);
	if (*s == '"') {
		for (s++; *s && *s != '"'; s++)
			if (*s == '\\')
				s++;
		return *s == '"' ? s - json + 1 : s - json;
	}
	if (*s == '[') {
		s += json_ws(s + 1) + 1;
		while (*s && *s != ']') {
			s += json_len(s);
			s += json_ws(s);
			if (*s != ',')
				break;
			s += json_ws(s + 1) + 1;
		}
		return *s == ']' ? s - json + 1 : s - json;
	}
	if (*s == '{') {
		s += json_ws(s + 1) + 1;
		while (*s && *s != '}') {
			s += json_len(s);
			s += json_ws(s);
			if (*s != ':')
				break;
			s += json_len(s + 1) + 1;
			s += json_ws(s);
			if (*s != ',')
				break;
			s += json_ws(s + 1) + 1;
		}
		return *s == '}' ? s - json + 1 : s - json;
	}
	if (isalnum((unsigned char) *s) || *s == '.' || *s == '-') {
		while (isalnum((unsigned char) *s) || *s == '.' || *s == '-' || *s == '+')
			s++;
		return s - json;
	}
	return 0;
}

char *json_list_get(char *json, int key)
{
	char *s = json;
	int i = 0;
	if (*s != '[')
		return NULL;
	s += json_ws(s + 1) + 1;
	while (*s && *s != ']') {
		s += json_ws(s);
		if (i++ == key)
			return s;
		s += json_len(s);
		s += json_ws(s);
		if (*s != ',')
			return NULL;
		s += json_ws(s + 1) + 1;
	}
	return NULL;
}

int json_str(char *json, char *dst, long len)
{
	if (!json)
		return -1;
	json += json_ws(json);
	if (*json != '"')
		return -1;
	json++;
	while (*json != '"' && len > 1) {
		if (*json == '\\' && json[1])
			json++;
		*dst++ = *json++;
		len--;
	}
	*dst = '\0';
	return 0;
}

char **json_list(char *json)
{
	char *s = json;
	char **list = NULL;
	int list_n = 0;
	int list_sz = 0;
	if (*s != '[')
		return NULL;
	s += json_ws(s + 1) + 1;
	while (*s && *s != ']') {
		if (list_n + 1 >= list_sz) {
			list_sz = list_sz + (list_sz ? list_sz : 512);
			list = realloc(list, list_sz * sizeof(list[0]));
		}
		s += json_ws(s);
		list[list_n++] = s;
		s += json_len(s);
		s += json_ws(s);
		if (*s != ',')
			break;
		s += json_ws(s + 1) + 1;
	}
	if (list)
		list[list_n] = NULL;
	return list;
}

char **json_dict(char *json)
{
	char *s = json;
	char **dict = NULL;
	int dict_n = 0;
	int dict_sz = 0;
	if (*s != '{')
		return NULL;
	s += json_ws(s + 1) + 1;
	while (*s && *s != '}') {
		if (dict_n + 1 >= dict_sz) {
			dict_sz = dict_sz + (dict_sz ? dict_sz : 512);
			dict = realloc(dict, dict_sz * sizeof(dict[0]));
		}
		dict[dict_n++] = s;
		s += json_len(s);
		s += json_ws(s);
		if (*s != ':')
			return NULL;
		s += json_ws(s + 1) + 1;
		s += json_len(s);
		s += json_ws(s);
		if (*s != ',')
			break;
		s += json_ws(s + 1) + 1;
	}
	if (dict)
		dict[dict_n] = NULL;
	return dict;
}

char *json_dict_get(char *json, char *key)
{
	char *s = json;
	char cur[256];
	if (*s != '{')
		return NULL;
	s += json_ws(s + 1) + 1;
	while (*s && *s != '}') {
		if (json_str(s, cur, sizeof(cur)))
			break;
		s += json_len(s);
		s += json_ws(s);
		if (*s != ':')
			return NULL;
		s += json_ws(s + 1) + 1;
		if (!strcmp(cur, key))
			return s;
		s += json_len(s);
		s += json_ws(s);
		if (*s != ',')
			break;
		s += json_ws(s + 1) + 1;
	}
	return NULL;
}

long json_copy(char *json, char *dst, long maxlen)
{
	long len = json_len(json);
	if (len > maxlen - 1)
		len = maxlen - 1;
	memcpy(dst, json, len);
	dst[len] = '\0';
	return len;
}
