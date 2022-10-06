#ifndef LEPTJSON_H__
#define LEPTJSON_H__

#include <stddef.h>  /* size_t */

typedef enum {
	LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT
} lept_type;

/*	���� lept_value ��ʹ�����������͵�ָ�룬
	���Ǳ���ǰ��������forward declare��������
*/
typedef struct lept_value lept_value;

struct lept_value {
	union {
		struct { lept_value* e; size_t size; }a;		/* array:  elements, element count */
		struct { char* s; size_t len; }s;			/* string: null-terminated string, string length */
		double n;									/* number */
	}u;
	lept_type type;
};

enum {
	LEPT_PARSE_OK = 0,						/* �������� */
	LEPT_PARSE_EXPECT_VALUE,				/* ֻ���пհף�ȱ��ֵ */
	LEPT_PARSE_INVALID_VALUE,				/* ��Чֵ */
	LEPT_PARSE_ROOT_NOT_SINGULAR,			 /* ֵ���棬�հ׺��滹�������ַ� */
	LEPT_PARSE_NUMBER_TOO_BIG,				 /* ֵ̫��  */
	LEPT_PARSE_MISS_QUOTATION_MARK,			 /* ȱ������ */
	LEPT_PARSE_INVALID_STRING_ESCAPE,		 /* ���Ϸ���ת���ַ� */
	LEPT_PARSE_INVALID_STRING_CHAR,			 /* ch < 0x20 ���ַ� */
	LEPT_PARSE_INVALID_UNICODE_HEX,			 /* ���� 4 λʮ���������� */
	LEPT_PARSE_INVALID_UNICODE_SURROGATE,    /* ����ȷ�Ĵ����� */
	LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET  /* ȱ�ٶ��Ż��������� */
};

/* ������ lept_free() �������� v �����ͣ��ڵ������з��ʺ���֮ǰ�����Ǳ����ʼ�������� */
#define lept_init(v) do { (v)->type = LEPT_NULL; } while (0)

int lept_parse(lept_value* v, const char* json);

void lept_free(lept_value* v);

lept_type lept_get_type(const lept_value* v);

#define lept_set_null(v) lept_free(v)  /* lept_free() ��������ʵ���ÿյĹ��� */

int lept_get_boolean(const lept_value* v);
void lept_set_boolean(lept_value* v, int b);

double lept_get_number(const lept_value* v);
void lept_set_number(lept_value* v, double n);

const char* lept_get_string(const lept_value* v);
size_t lept_get_string_length(const lept_value* v);
void lept_set_string(lept_value* v, const char* s, size_t len);

size_t lept_get_array_size(const lept_value* v);
lept_value* lept_get_array_element(const lept_value* v, size_t index);

#endif /* LEPTJSON_H__ */