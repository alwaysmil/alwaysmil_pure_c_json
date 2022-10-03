#ifndef LEPTJSON_H__
#define LEPTJSON_H__

#include <stddef.h>  /* size_t */

typedef enum {
	LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT
} lept_type;

typedef struct {
	union {
		struct { char* s; size_t len; }s;  /* string: null-terminated string, string length */
		double n;						   /* number */
	}u;
	lept_type type;
} lept_value;

enum {
	LEPT_PARSE_OK = 0,						/* 解析正常 */
	LEPT_PARSE_EXPECT_VALUE,				/* 只含有空白，缺少值 */
	LEPT_PARSE_INVALID_VALUE,				/* 无效值 */
	LEPT_PARSE_ROOT_NOT_SINGULAR,			/* 值后面，空白后面还有其他字符 */
	LEPT_PARSE_NUMBER_TOO_BIG,				/* 值太大  */
	LEPT_PARSE_MISS_QUOTATION_MARK,			/* 缺少引号 */
	LEPT_PARSE_INVALID_STRING_ESCAPE,		/* 不合法的转义字符 */
	LEPT_PARSE_INVALID_STRING_CHAR,			/* ch < 0x20 的字符 */
	LEPT_PARSE_INVALID_UNICODE_HEX,			/* 不是 4 位十六进制数字 */
	LEPT_PARSE_INVALID_UNICODE_SURROGATE    /* 不正确的代理对 */
};

/* 由于在 lept_free() 函数会检查 v 的类型，在调用所有访问函数之前，我们必须初始化该类型 */
#define lept_init(v) do { (v)->type = LEPT_NULL; } while (0)

int lept_parse(lept_value* v, const char* json);

void lept_free(lept_value* v);

lept_type lept_get_type(const lept_value* v);

#define lept_set_null(v) lept_free(v)  /* lept_free() 函数可以实现置空的功能 */

int lept_get_boolean(const lept_value* v);
void lept_set_boolean(lept_value* v, int b);

double lept_get_number(const lept_value* v);
void lept_set_number(lept_value* v, double n);

const char* lept_get_string(const lept_value* v);
size_t lept_get_string_length(const lept_value* v);
void lept_set_string(lept_value* v, const char* s, size_t len);

#endif /* LEPTJSON_H__ */