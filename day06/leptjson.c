#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

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
	char* stack;	/* 利用堆栈制作的存放字符串等的缓冲区， 用 char* 是因为 char 是一个字节，这个堆栈不是普通堆栈，而是以字节储存的，每次可要求压入任意大小的数据 */
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

static const char* lept_parse_hex4(const char* p, unsigned* u) {
	int i;
	*u = 0;
	for (i = 0; i < 4; ++i) {
		char ch = *p++;
		*u <<= 4;
		if (ch >= '0' && ch <= '9')	*u |= ch - '0';
		else if (ch >= 'A' && ch <= 'F')	*u |= ch - 'A' + 10;
		else if (ch >= 'a' && ch <= 'f')	*u |= ch - 'a' + 10;
		else return NULL;
	}
	return p;
}

static void lept_encode_utf8(lept_context* c, unsigned u) {
	/*
		最终也是写进一个 char，为什么要做 x & 0xFF 这种操作呢？
		这是因为 u 是 unsigned 类型，一些编译器可能会警告这个转型可能会截断数据。
		但实际上，配合了范围的检测然后右移之后，可以保证写入的是 0~255 内的值。
		为了避免一些编译器的警告误判，我们加上 x & 0xFF。
		一般来说，编译器在优化之后，这与操作是会被消去的，不会影响性能。
		其实超过 1 个字符输出时，可以只调用 1 次 lept_context_push()。
		这里全用 PUTC() 只是为了代码看上去简单一点。
	*/
	if (u <= 0x7F) {
		PUTC(c, u & 0xFF);
	} else if (u <= 0x7FF) {
		PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
		PUTC(c, 0x80 | (u & 0x3F));
	} else if (u <= 0xFFFF) {
		PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
		PUTC(c, 0x80 | ((u >> 6) & 0x3F));
		PUTC(c, 0x80 | (u & 0x3F));
	} else {
		assert(u <= 0x10FFFF);
		PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
		PUTC(c, 0x80 | ((u >> 12) & 0x3F));
		PUTC(c, 0x80 | ((u >> 6) & 0x3F));
		PUTC(c, 0x80 | (u & 0x3F));
	}
}

#define STRING_ERROR(ret) do { c->top = head; return ret; } while (0)

/* 解析 JSON 字符串，把结果写入 str 和 len */
/* str 指向 c->stack 中的元素，需要在 c->stack  */
static int lept_parse_string_raw(lept_context* c, char** str, size_t* len) {
	size_t head = c->top;
	unsigned u, u2;
	const char* p;
	EXPECT(c, '\"');
	p = c->json;
	for (;;) {
		char ch = *p++;
		switch (ch) {
			case '\"':
				*len = c->top - head;
				*str = lept_context_pop(c, *len);
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
					case 'u':
						if (!(p = lept_parse_hex4(p, &u))) {
							STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
						}
						if (u >= 0xD800 && u <= 0xDBFF) {  /* surrogate pair */
							/* 高代理项: 0xDB00 - 0xDBFF */
							/* 低代理项: 0xDC00 - 0xDFFF */
							if (*p++ != '\\') {
								STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
							}
							if (*p++ != 'u') {
								STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
							}
							if (!(p = lept_parse_hex4(p, &u2))) {
								STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
							}
							if (u2 < 0xDC00 || u2 > 0xDFFF) {
								STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
							}
							/*
								码点计算公式：
								codepoint = 0x10000 + (H − 0xD800) × 0x400 + (L − 0xDC00)
								H：高代理项	L：低代理项
							*/
							u = 0x10000 + (((u - 0xD800) << 10) | (u2 - 0xDC00));
						}
						lept_encode_utf8(c, u);
						break;
					default:
						STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
				}
				break;
			case '\0':
				STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
			default:
				if ((unsigned char)ch < 0x20) {
					STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
				}
				PUTC(c, ch);
		}
	}
}

static int lept_parse_string(lept_context* c, lept_value* v) {
	int ret;
	char* s;
	size_t len;
	if ((ret = lept_parse_string_raw(c, &s, &len)) == LEPT_PARSE_OK)
		lept_set_string(v, s, len);
	return ret;
}

static int lept_parse_value(lept_context* c, lept_value* v);  /* 前向声明 */

static int lept_parse_array(lept_context* c, lept_value* v) {
	size_t i, size = 0;
	int ret;
	EXPECT(c, '[');
	lept_parse_whitespace(c);
	if (*c->json == ']') {
		++c->json;
		v->type = LEPT_ARRAY;
		v->u.a.e = NULL;
		v->u.a.size = 0;
		return LEPT_PARSE_OK;
	}
	for (;;) {
		/*
			如果此处写成以下代码，就会出现 bug：
			lept_value* e = lept_context_push(c, sizeof(lept_value));
			lept_init(e);
			size++;
			if ((ret = lept_parse_value(c, e)) != LEPT_PARSE_OK)
				return ret;
			原因：e 是一个指向堆区的指针，当我们的栈满的时候，就会自动扩容，原来的一片空间可能
			就到其他地方去了，而 e 仍然指向原来的位置，成为悬空指针。
		*/
		lept_value e;
		lept_init(&e);
		if ((ret = lept_parse_value(c, &e)) != LEPT_PARSE_OK) {
			break;  /* 解析失败，堆栈中会存入这些非法值，在返回之前应当清空 */
		}
		memcpy(lept_context_push(c, sizeof(lept_value)), &e, sizeof(lept_value));
		++size;
		lept_parse_whitespace(c);
		if (*c->json == ',') {
			++c->json;
			lept_parse_whitespace(c);
		} else if (*c->json == ']') {
			++c->json;
			v->type = LEPT_ARRAY;
			v->u.a.size = size;
			size *= sizeof(lept_value);
			memcpy(v->u.a.e = (lept_value*)malloc(size), lept_context_pop(c, size), size);
			return LEPT_PARSE_OK;
		} else {
			ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
			break;  /* 解析失败，堆栈中会存入非法值之前的合法值，在返回之前应当清空 */
		}
	}
	for (i = 0; i < size; ++i) {
		lept_free((lept_value*)lept_context_pop(c, sizeof(lept_value)));
	}
	return ret;
	/*
		如果当解析数组时，第一个元素就出现错误，如果第一个元素是 null 或者数字等，
		因为没有将保存数字的 lept_value 放在堆上，解析的时候也没有利用堆，所以不需要额外处理。
		但是如果第一个元素是一个字符串，这个字符串前面一部分都合法，结尾时不合法，
		那么前面的这一部分字符会先在堆上开辟一部分空间，然后把它们存进去，但是当遇到非法字符时，
		就会调用 STRING_ERROR 来使得 c->top 回到一开始的地点，栈空间虽然变大了，
		但 c-> top 回到了原来的地方，相当于并没有占用栈空间（这部分空间后面仍然可以用），
		这部分的空间会在 lept_parse 的最后一起释放。
	*/
}

static int lept_parse_object(lept_context* c, lept_value* v) {
	size_t i, size;
	lept_member m;
	int ret;
	EXPECT(c, '{');
	lept_parse_whitespace(c);
	if (*c->json == '}') {
		++c->json;
		v->type = LEPT_OBJECT;
		v->u.o.m = NULL;
		v->u.o.size = 0;
		return LEPT_PARSE_OK;
	}
	m.k = NULL;
	size = 0;
	for (;;) {
		char* str;
		lept_init(&m.v);
		/* 解析 key */
		if (*c->json != '"') {
			ret = LEPT_PARSE_MISS_KEY;
			break;
		}
		if ((ret = lept_parse_string_raw(c, &str, &m.klen)) != LEPT_PARSE_OK) {
			break;
		}
		memcpy(m.k = (char*)malloc(m.klen + 1), str, m.klen);
		m.k[m.klen] = '\0';
		/* 解析空白和冒号 */
		lept_parse_whitespace(c);
		if (*c->json != ':') {
			ret = LEPT_PARSE_MISS_COLON;
			break;
		}
		++c->json;
		lept_parse_whitespace(c);
		/* 解析值 value */
		if ((ret = lept_parse_value(c, &m.v)) != LEPT_PARSE_OK) {
			break;
		}
		memcpy(lept_context_push(c, sizeof(lept_member)), &m, sizeof(lept_member));
		++size;
		m.k = NULL;
		/*
			如果之前缺乏冒号，或是这里解析值失败，在函数返回前我们要释放 m.k。
			如果我们成功地解析整个成员，那么就要把 m.k 设为空指针，
			其意义是说明该键的字符串的拥有权已转移至栈，之后如遇到错误，
			我们不会重覆释放栈里成员的键和这个临时成员的键。
		*/
		/* 解析逗号或者右花括号 */
		lept_parse_whitespace(c);
		if (*c->json == ',') {
			++c->json;
			lept_parse_whitespace(c);
		} else if (*c->json == '}') {
			size_t s = sizeof(lept_member) * size;
			++c->json;
			v->type = LEPT_OBJECT;
			v->u.o.size = size;
			memcpy(v->u.o.m = (lept_member*)malloc(s), lept_context_pop(c, s), s);
			return LEPT_PARSE_OK;
		} else {
			ret = LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
			break;
		}
	}
	/*
		最后，当 for (;;) 中遇到任何错误便会到达这步，
		要释放临时的 key 字符串及栈上的成员。
	*/
	free(m.k);
	for (i = 0; i < size; ++i) {
		lept_member* m = (lept_member*)lept_context_pop(c, sizeof(lept_member));
		free(m->k);
		lept_free(&m->v);
	}
	v->type = LEPT_NULL;
	return ret;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
	switch (*c->json) {
		case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
		case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
		case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
		default:   return lept_parse_number(c, v);
		case '"':  return lept_parse_string(c, v);
		case '[':  return lept_parse_array(c, v);
		case '{':  return lept_parse_object(c, v);
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
	size_t i;
	assert(v != NULL);
	switch (v->type) {
		case LEPT_STRING:
			free(v->u.s.s);
			break;
		case LEPT_ARRAY:
			for (i = 0; i < v->u.a.size; ++i) {
				lept_free(&v->u.a.e[i]);
			}
			free(v->u.a.e);
			break;
		case LEPT_OBJECT:
			for (i = 0; i < v->u.o.size; ++i) {
				free(v->u.o.m[i].k);
				lept_free(&v->u.o.m[i].v);
			}
			free(v->u.o.m);
			break;
		default:
			break;
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

size_t lept_get_array_size(const lept_value* v) {
	assert(v != NULL && v->type == LEPT_ARRAY);
	return v->u.a.size;
}

lept_value* lept_get_array_element(const lept_value* v, size_t index) {
	assert(v != NULL && v->type == LEPT_ARRAY);
	assert(index < v->u.a.size);
	return &v->u.a.e[index];
}

size_t lept_get_object_size(const lept_value* v) {
	assert(v != NULL && v->type == LEPT_OBJECT);
	return v->u.o.size;
}

const char* lept_get_object_key(const lept_value* v, size_t index) {
	assert(v != NULL && v->type == LEPT_OBJECT);
	assert(index < v->u.o.size);
	return v->u.o.m[index].k;
}

size_t lept_get_object_key_length(const lept_value* v, size_t index) {
	assert(v != NULL && v->type == LEPT_OBJECT);
	assert(index < v->u.o.size);
	return v->u.o.m[index].klen;
}

lept_value* lept_get_object_value(const lept_value* v, size_t index) {
	assert(v != NULL && v->type == LEPT_OBJECT);
	assert(index < v->u.o.size);
	return &v->u.o.m[index].v;
}