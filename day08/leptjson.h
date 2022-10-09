#ifndef LEPTJSON_H__
#define LEPTJSON_H__

#include <stddef.h>  /* size_t */

typedef enum {
	LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT
} lept_type;

#define LEPT_KEY_NOT_EXIST ((size_t)-1)

/*	由于 lept_value 内使用了自身类型的指针，
	我们必须前向声明（forward declare）此类型
*/
typedef struct lept_value lept_value;
typedef struct lept_member lept_member;

struct lept_value {
	union {
		struct { lept_member* m; size_t size; size_t capacity; }o;		/* object: members, member count, capacity */
		struct { lept_value* e; size_t size; size_t capacity; }a;		/* array:  elements, element count, capacity */
		struct { char* s; size_t len; }s;								/* string: null-terminated string, string length */
		double n;														/* number */
	}u;
	lept_type type;
};

struct lept_member {
	char* k; size_t klen;  /* member key string, key string length */
	lept_value v;		   /* member value */
	/*
		成员结构 lept_member 是一个 lept_value 加上键的字符串。
		如同 JSON 字符串的值，我们也需要同时保留字符串的长度，
		因为字符串本身可能包含空字符 \u0000
	*/
};

enum {
	LEPT_PARSE_OK = 0,						 /* 解析正常						*/
	LEPT_PARSE_EXPECT_VALUE,				 /* 只含有空白，缺少值			*/
	LEPT_PARSE_INVALID_VALUE,				 /* 无效值						*/
	LEPT_PARSE_ROOT_NOT_SINGULAR,			 /* 值后面，空白后面还有其他字符	*/
	LEPT_PARSE_NUMBER_TOO_BIG,				 /* 值太大						*/
	LEPT_PARSE_MISS_QUOTATION_MARK,			 /* 缺少引号						*/
	LEPT_PARSE_INVALID_STRING_ESCAPE,		 /* 不合法的转义字符				*/
	LEPT_PARSE_INVALID_STRING_CHAR,			 /* ch < 0x20 的字符				*/
	LEPT_PARSE_INVALID_UNICODE_HEX,			 /* 不是 4 位十六进制数字			*/
	LEPT_PARSE_INVALID_UNICODE_SURROGATE,    /* 不正确的代理对				*/
	LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, /* 缺少逗号或者中括号			*/
	LEPT_PARSE_MISS_KEY,					 /* 缺少 key 关键字				*/
	LEPT_PARSE_MISS_COLON,					 /* 缺少冒号						*/
	LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET   /* 缺少逗号或者花括号			*/
};

/* 由于在 lept_free() 函数会检查 v 的类型，在调用所有访问函数之前，我们必须初始化该类型 */
#define lept_init(v) do { (v)->type = LEPT_NULL; } while (0)

int lept_parse(lept_value* v, const char* json);
char* lept_stringify(const lept_value* v, size_t* length);  /* length 参数是可选的，它会存储 JSON 的长度，传入 NULL 可忽略此参数。使用方需负责用 free() 释放内存 */

void lept_copy(lept_value* dst, const lept_value* src);
void lept_move(lept_value* dst, lept_value* src);
void lept_swap(lept_value* lhs, lept_value* rhs);

void lept_free(lept_value* v);

lept_type lept_get_type(const lept_value* v);
int lept_is_equal(const lept_value* lhs, const lept_value* rhs);

#define lept_set_null(v) lept_free(v)  /* lept_free() 函数可以实现置空的功能 */

int lept_get_boolean(const lept_value* v);
void lept_set_boolean(lept_value* v, int b);

double lept_get_number(const lept_value* v);
void lept_set_number(lept_value* v, double n);

const char* lept_get_string(const lept_value* v);
size_t lept_get_string_length(const lept_value* v);
void lept_set_string(lept_value* v, const char* s, size_t len);

void lept_set_array(lept_value* v, size_t capacity);
size_t lept_get_array_size(const lept_value* v);
size_t lept_get_array_capacity(const lept_value* v);
void lept_reserve_array(lept_value* v, size_t capacity);
void lept_shrink_array(lept_value* v);
void lept_clear_array(lept_value* v);
lept_value* lept_get_array_element(const lept_value* v, size_t index);
lept_value* lept_pushback_array_element(lept_value* v);
void lept_popback_array_element(lept_value* v);
lept_value* lept_insert_array_element(lept_value* v, size_t index);
void lept_erase_array_element(lept_value* v, size_t index, size_t count);

void lept_set_object(lept_value* v, size_t capacity);
size_t lept_get_object_size(const lept_value* v);
size_t lept_get_object_capacity(const lept_value* v);
void lept_reserve_object(lept_value* v, size_t capacity);
void lept_shrink_object(lept_value* v);
void lept_clear_object(lept_value* v);
const char* lept_get_object_key(const lept_value* v, size_t index);
size_t lept_get_object_key_length(const lept_value* v, size_t index);
lept_value* lept_get_object_value(const lept_value* v, size_t index);
size_t lept_find_object_index(const lept_value* v, const char* key, size_t klen);
lept_value* lept_find_object_value(const lept_value* v, const char* key, size_t klen);
lept_value* lept_set_object_value(lept_value* v, const char* key, size_t klen);
void lept_remove_object_value(lept_value* v, size_t index);

#endif /* LEPTJSON_H__ */