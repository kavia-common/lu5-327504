#include "io.h"

#include <stdlib.h>
#include <string.h>

#include "../lu5_types.h"
#include "../lu5_print.h"
#include "../lu5_logger.h"

#include "../lu5_fs.h"

int print(lua_State *L)
{
	int argc = lua_gettop(L);
	
	if (argc == 0) {
		putchar('\n');
		return 0;
	}
	
	// Print all arguments
	for (int i = 1; i < argc; i++) {
		lu5_print_any(L, i, 0, ' ');
	}

	lu5_print_any(L, argc, 0, LU5_NEWLINE);

	fflush(stdout);

	return 0;
}

int loadJSON(lua_State *L) {
	return 0;
}

int loadText(lua_State *L) {

	const char* file_path = lu5_assert_string(L, 1, "loadText");

	long file_size = 0;
	
	char* content = lu5_read_file(file_path, &file_size);
	if (content == NULL) {
		lua_pushnil(L);
		return 1;
	}

	lua_pushlstring(L, content, file_size);

	if (content != NULL) 
		free(content);

	return 1;
}

int loadStrings(lua_State *L) {

	const char* file_path = lu5_assert_string(L, 1, "loadStrings");

	FILE* file = lu5_open_file(file_path, "r");
	if (file == NULL) {
		lua_pushnil(L);
		return 1;
	}

    lua_newtable(L);

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    int line_number = 1;
    while ((read = getline(&line, &len, file)) != -1) {
        if (line[read - 1] == '\n') {
			// Remove trailing newline character
            line[read - 1] = '\0';
        }
        lua_pushstring(L, line);
        lua_rawseti(L, -2, line_number++);
    }

    if (line) {
        free(line);
    }

    fclose(file);
    return 1;
}

/**
 * lu5_split - Split a string by a delimiter pattern
 *
 * Delta robot API compatibility function. Returns a 1-indexed Lua table of
 * substrings split at each occurrence of pat in str.
 *
 * Flow: validate inputs -> iterate finding delimiter -> push substrings -> return table
 * Invariant: returned table is always 1-indexed (Lua convention).
 */

/* PUBLIC_INTERFACE */
int lu5_split(lua_State *L)
{
    const char *str = lu5_assert_string(L, 1, "split");
    const char *pat = lu5_assert_string(L, 2, "split");

    lua_newtable(L);

    /* Empty delimiter: return whole string as single element */
    if (pat == NULL || pat[0] == '\0') {
        lua_pushstring(L, str);
        lua_rawseti(L, -2, 1);
        return 1;
    }

    int         idx     = 1;
    int         pat_len = (int)strlen(pat);
    const char *cursor  = str;

    while (*cursor != '\0') {
        const char *found = strstr(cursor, pat);
        if (found == NULL) {
            /* No more delimiters: push remainder */
            lua_pushstring(L, cursor);
            lua_rawseti(L, -2, idx++);
            break;
        }
        /* Push substring before delimiter */
        lua_pushlstring(L, cursor, (size_t)(found - cursor));
        lua_rawseti(L, -2, idx++);
        cursor = found + pat_len;
    }

    return 1;
}
