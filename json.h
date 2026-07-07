/* parse JSON format */
long json_ws(char *json);
long json_len(char *json);
char **json_list(char *json);
char **json_dict(char *json);
char *json_list_get(char *json, int key);
char *json_dict_get(char *json, char *key);
int json_str(char *json, char *dst, long len);
long json_copy(char *json, char *dst, long len);
