#include "leptjson.h"
#include <assert.h> /* assert() */
#include <stdlib.h> /* NULL, strtod(), malloc(), realloc(), free() */
#include <math.h>   /* HUGE_VAL */
#include <errno.h>  /* errno, ERANGE */
#include <string.h> /* memcpy() */

/*
	ʹ�� #ifndef X #define X ... #endif ��ʽ�ĺô��ǣ�
	ʹ���߿��ڱ���ѡ�����������ú꣬û���õĻ�����ȱʡֵ��
*/
#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)		do { assert(*c->json == (ch)); c->json++; } while (0)
#define ISDIGIT(ch)			((ch) >= '0' && (ch) <= '9')  /* �������Ƿ�ֹȡָ���ֵ��ʱ�������� */
#define ISDIGIT1TO9(ch)		((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)         do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while (0)

typedef struct {
	const char* json;
	char* stack;	/* ���ö�ջ�����Ĵ���ַ����ȵĻ����� */
	size_t size;	/* ջ stack ������ */
	size_t top;		/* ջ��λ�ã���Ϊ����չ stack������ top ����ָ����ʽ���� */
}lept_context;

static void* lept_context_push(lept_context* c, size_t size) {
	void* ret;
	assert(size > 0);
	if (c->top + size >= c->size) {
		if (c->size == 0) {
			c->size = LEPT_PARSE_STACK_INIT_SIZE;
		}
		while (c->top + size >= c->size) {
			c->size += c->size >> 1;  /* ���� 1.5 �� */
		}
		c->stack = (char*)realloc(c->stack, c->size);
	}
	ret = c->stack + c->top;
	c->top += size;
	return ret;
}

static void* lept_context_pop(lept_context* c, size_t size) {
	assert(c->top >= size);
	c->top -= size;
	return c->stack + c->top;
}

static void lept_parse_whitespace(lept_context* c) {
	const char* p = c->json;
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
		p++;
	}
	c->json = p;
}

#if 0
/* ���������������ϲ��� lept_parse_literal() ������*/
static int lept_parse_null(lept_context* c, lept_value* v) {
	EXPECT(c, 'n');
	if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l') {
		return LEPT_PARSE_INVALID_VALUE;
	}
	c->json += 3;
	v->type = LEPT_NULL;
	return LEPT_PARSE_OK;
}

static int lept_parse_true(lept_context* c, lept_value* v) {
	EXPECT(c, 't');
	if (c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e') {
		return LEPT_PARSE_INVALID_VALUE;
	}
	c->json += 3;
	v->type = LEPT_TRUE;
	return LEPT_PARSE_OK;
}

static int lept_parse_false(lept_context* c, lept_value* v) {
	EXPECT(c, 'f');
	if (c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e') {
		return LEPT_PARSE_INVALID_VALUE;
	}
	c->json += 4;
	v->type = LEPT_FALSE;
	return LEPT_PARSE_OK;
}
#endif

static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
	/*
		����������: "true", "false", "null", ������������ϲ�
	*/
	EXPECT(c, literal[0]);
	size_t i;
	for (i = 0; literal[i + 1]; ++i) {
		if (c->json[i] != literal[i + 1]) {
			return LEPT_PARSE_INVALID_VALUE;
		}
	}
	c->json += i;
	v->type = type;
	return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context* c, lept_value* v) {
	/*
	char* end;
	�� end ���������ֺ�����ַ��������һ��ʼ��û�����֣�ֱ�ӷ��� LEPT_PARSE_INVALID_VALUE
	*/

	const char* p = c->json;
	/* ���� */
	if (*p == '-') {
		++p;
	}

	/* ���� */
	if (*p == '0') {
		++p;
	} else {
		if (!ISDIGIT1TO9(*p)) {
			return LEPT_PARSE_INVALID_VALUE;
		}
		for (++p; ISDIGIT1TO9(*p); ++p);
	}

	/* С�� */
	if (*p == '.') {
		++p;
		if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
		for (++p; ISDIGIT(*p); ++p);
	}

	/* ָ�� */
	if (*p == 'e' || *p == 'E') {
		++p;
		if (*p == '+' || *p == '-') ++p;
		if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
		for (++p; ISDIGIT(*p); ++p);
	}

	/* ֵ���� */
	errno = 0;
	v->u.n = strtod(c->json, NULL);
	if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL)) {
		return LEPT_PARSE_NUMBER_TOO_BIG;
	}

	c->json = p;
	v->type = LEPT_NUMBER;
	return LEPT_PARSE_OK;
}

static int lept_parse_string(lept_context* c, lept_value* v) {
	size_t head = c->top, len;
	const char* p;
	EXPECT(c, '\"');
	p = c->json;
	for (;;) {
		char ch = *p++;
		switch (ch) {
			case '\"':
				len = c->top - head;
				lept_set_string(v, (const char*)lept_context_pop(c, len), len);
				c->json = p;
				return LEPT_PARSE_OK;
			case '\\':
				switch (*p++) {
					case '\\': PUTC(c, '\\'); break;
					case '\"': PUTC(c, '\"'); break;
					case '/':  PUTC(c, '/'); break;
					case 'b':  PUTC(c, '\b'); break;
					case 'f':  PUTC(c, '\f'); break;
					case 'n':  PUTC(c, '\n'); break;
					case 'r':  PUTC(c, '\r'); break;
					case 't':  PUTC(c, '\t'); break;
					default:
						c->top = head;
						return LEPT_PARSE_INVALID_STRING_ESCAPE;
				}
				break;
			case '\0':
				c->top = head;
				return LEPT_PARSE_MISS_QUOTATION_MARK;
			default:
				if ((unsigned char)ch < 0x20) {
					c->top = head;
					return LEPT_PARSE_INVALID_STRING_CHAR;
				}
				PUTC(c, ch);
		}
	}
}

static int lept_parse_value(lept_context* c, lept_value* v) {
	switch (*c->json) {
		case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
		case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
		case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
		default:   return lept_parse_number(c, v);
		case '"':  return lept_parse_string(c, v);
		case '\0': return LEPT_PARSE_EXPECT_VALUE;
	}
}

int lept_parse(lept_value* v, const char* json) {
	lept_context c;
	int ret;
	assert(v != NULL);
	c.json = json;
	c.stack = NULL;
	c.size = c.top = 0;
	lept_init(v);
	lept_parse_whitespace(&c);
	ret = lept_parse_value(&c, v);
	if (ret == LEPT_PARSE_OK) {
		lept_parse_whitespace(&c);
		if (*c.json != '\0') {
			v->type = LEPT_NULL;  /* ������ÿգ���ô c->json = "0123" �ͻὫ type �ĳ� LEPT_NUMBER */
			ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
		}
	}
	assert(c.top == 0);
	free(c.stack);  /* ������Ϻ�Ҫ����������Ŀռ��ͷ� */
	return ret;
}

void lept_free(lept_value* v) {
	/*
	�����ǰָ�� v ָ����ڴ��������һ�� LEPT_STRING ���͵Ŀ飬
	��ô���д���ַ������ڴ沢û�� free ��������Ҫִ�����������
	��ǰֻ�����ַ������ͣ����滹���������Ͷ�����ͷ�
*/
	assert(v != NULL);
	if (v->type == LEPT_STRING) {
		free(v->u.s.s);
	}
	v->type = LEPT_NULL;  /* �����ͱ�Ϊ LEPT_NULL ���Ա����ظ��ͷ� */
}

lept_type lept_get_type(const lept_value* v) {
	return v->type;
}

int lept_get_boolean(const lept_value* v) {
	assert(v != NULL && (v->type == LEPT_FALSE || v->type == LEPT_TRUE));
	return v->type == LEPT_TRUE;
}

void lept_set_boolean(lept_value* v, int b) {
	lept_free(v);
	v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

double lept_get_number(const lept_value* v) {
	assert(v != NULL && v->type == LEPT_NUMBER);
	return v->u.n;
}

void lept_set_number(lept_value* v, double n) {
	lept_free(v);
	v->u.n = n;
	v->type = LEPT_NUMBER;
}

const char* lept_get_string(const lept_value* v) {
	assert(v != NULL && v->type == LEPT_STRING);
	return v->u.s.s;
}

size_t lept_get_string_length(const lept_value* v) {
	assert(v != NULL && v->type == LEPT_STRING);
	return v->u.s.len;
}

void lept_set_string(lept_value* v, const char* s, size_t len) {
	assert(v != NULL && (s != NULL || len == 0));
	lept_free(v);
	v->u.s.s = (char*)malloc(len + 1);
	memcpy(v->u.s.s, s, len);
	v->u.s.s[len] = '\0';
	v->u.s.len = len;
	v->type = LEPT_STRING;
	/*
		ΪʲôҪ�� lept_free() ���� �� ֮�ҵ���⣺
		���ȣ��ڽ��� json ʱ������һ�� lept_value �ı��� v��Ȼ�� &v ����
		lept_set_string() �У����������ռ�õ��ڴ�������֮ǰ�ù���д���ֵ���ڴ�飬
		���֮ǰ���ַ������͵�ռ��������ڴ棬��ô�ڴ��л���һ���������ַ�����ַ��������
		�����ڴ沢û����֮ǰ v �������л��գ����յ�ֻ�Ǵ������ַ����ĵ�ַ�������дֵ
		֮ǰ������ lept_free() ��������ô�����еĿռ�һֱû�б����գ�ֱ���ڴ�й¶
	*/
}