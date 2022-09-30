#include "leptjson.h"
#include <assert.h> /* assert() */
#include <stdlib.h> /* NULL, strtod(), malloc(), realloc(), free() */
#include <math.h>   /* HUGE_VAL */
#include <errno.h>  /* errno, ERANGE */
#include <string.h> /* memcpy() */

/*
	使用 #ifndef X #define X ... #endif 方式的好处是，
	使用者可在编译选项中自行设置宏，没设置的话就用缺省值。
*/
#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)		do { assert(*c->json == (ch)); c->json++; } while (0)
#define ISDIGIT(ch)			((ch) >= '0' && (ch) <= '9')  /* 加括号是防止取指针的值的时候发生错误 */
#define ISDIGIT1TO9(ch)		((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)         do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while (0)

typedef struct {
	const char* json;
	char* stack;	/* 利用堆栈制作的存放字符串等的缓冲区 */
	size_t size;	/* 栈 stack 的容量 */
	size_t top;		/* 栈顶位置，因为会扩展 stack，所以 top 不以指针形式储存 */
}lept_context;

static void* lept_context_push(lept_context* c, size_t size) {
	void* ret;
	assert(size > 0);
	if (c->top + size >= c->size) {
		if (c->size == 0) {
			c->size = LEPT_PARSE_STACK_INIT_SIZE;
		}
		while (c->top + size >= c->size) {
			c->size += c->size >> 1;  /* 扩容 1.5 倍 */
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
/* 以下三个函数被合并到 lept_parse_literal() 函数中*/
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
		解析字面量: "true", "false", "null", 将这三者情况合并
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
	用 end 来接收数字后面的字符串，如果一开始就没有数字，直接返回 LEPT_PARSE_INVALID_VALUE
	*/

	const char* p = c->json;
	/* 负号 */
	if (*p == '-') {
		++p;
	}

	/* 整数 */
	if (*p == '0') {
		++p;
	} else {
		if (!ISDIGIT1TO9(*p)) {
			return LEPT_PARSE_INVALID_VALUE;
		}
		for (++p; ISDIGIT1TO9(*p); ++p);
	}

	/* 小数 */
	if (*p == '.') {
		++p;
		if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
		for (++p; ISDIGIT(*p); ++p);
	}

	/* 指数 */
	if (*p == 'e' || *p == 'E') {
		++p;
		if (*p == '+' || *p == '-') ++p;
		if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
		for (++p; ISDIGIT(*p); ++p);
	}

	/* 值过大 */
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
			v->type = LEPT_NULL;  /* 如果不置空，那么 c->json = "0123" 就会将 type 改成 LEPT_NUMBER */
			ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
		}
	}
	assert(c.top == 0);
	free(c.stack);  /* 解析完毕后，要将堆区申请的空间释放 */
	return ret;
}

void lept_free(lept_value* v) {
	/*
	如果当前指针 v 指向的内存块曾经是一个 LEPT_STRING 类型的块，
	那么其中存放字符串的内存并没有 free 掉，所以要执行这个函数，
	当前只考虑字符串类型，后面还会添加数组和对象的释放
*/
	assert(v != NULL);
	if (v->type == LEPT_STRING) {
		free(v->u.s.s);
	}
	v->type = LEPT_NULL;  /* 把类型变为 LEPT_NULL 可以避免重复释放 */
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
		为什么要加 lept_free() 函数 ？ 之我的理解：
		首先，在解析 json 时会申请一个 lept_value 的变量 v，然后将 &v 传入
		lept_set_string() 中，这个变量所占用的内存块可能是之前用过并写入过值的内存块，
		如果之前是字符串类型的占用了这块内存，那么内存中会有一个堆区的字符串地址，而堆区
		这块的内存并没有在之前 v 的销毁中回收，回收的只是存放这个字符串的地址，如果在写值
		之前不进行 lept_free() 操作，那么堆区中的空间一直没有被回收，直至内存泄露
	*/
}