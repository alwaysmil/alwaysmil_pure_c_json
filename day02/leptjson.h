#ifndef LEPTJSON_H__
#define LEPTJSON_H__

typedef enum {
	LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT
} lept_type;

typedef struct {
	double n;
	lept_type type;
} lept_value;

enum {
	LEPT_PARSE_OK = 0,				// 解析正常
	LEPT_PARSE_EXPECT_VALUE,		// 只含有空白，缺少值
	LEPT_PARSE_INVALID_VALUE,		// 无效值
	LEPT_PARSE_ROOT_NOT_SINGULAR,	// 值后面，空白后面还有其他字符
	LEPT_PARSE_NUMBER_TOO_BIG		// 值太大
};

int lept_parse(lept_value* v, const char* json);

lept_type lept_get_type(const lept_value* v);

double lept_get_number(const lept_value* v);

#endif /* LEPTJSON_H__ */