#ifndef NOC_INTERNAL_H_INCLUDED
#define NOC__INDIVIDUAL_SOURCE 1
#include "internal.h"
#endif
#if defined(NOC_IMPLEMENTATION) || defined(NOC__INDIVIDUAL_SOURCE)
#ifndef NOC_CONDITIONAL_IMPLEMENTATION_INCLUDED
#define NOC_CONDITIONAL_IMPLEMENTATION_INCLUDED

#include <limits.h>

#define NOC__PREPROCESSOR_EXPRESSION_MAX_DEPTH 256u

typedef struct {
    intmax_t signed_value;
    uintmax_t unsigned_value;
    bool is_unsigned;
} Noc__Preprocessor_Value;

typedef struct {
    const Noc_Macro_Expansion *expansion;
    size_t cursor;
    size_t depth;
    size_t problem_token_index;
    Noc_Preprocessor_Expression_Status status;
} Noc__Preprocessor_Expression_Parser;

static Noc__Preprocessor_Value noc__preprocessor_signed_value(intmax_t value)
{
    Noc__Preprocessor_Value result;
    result.signed_value = value;
    result.unsigned_value = 0;
    result.is_unsigned = false;
    return result;
}

static Noc__Preprocessor_Value noc__preprocessor_unsigned_value(uintmax_t value)
{
    Noc__Preprocessor_Value result;
    result.signed_value = 0;
    result.unsigned_value = value;
    result.is_unsigned = true;
    return result;
}

static bool noc__preprocessor_value_is_true(Noc__Preprocessor_Value value)
{
    return value.is_unsigned ? value.unsigned_value != 0 : value.signed_value != 0;
}

static uintmax_t noc__preprocessor_value_as_unsigned(
    Noc__Preprocessor_Value value)
{
    return value.is_unsigned ? value.unsigned_value :
                               (uintmax_t)value.signed_value;
}

static void noc__preprocessor_expression_fail(
    Noc__Preprocessor_Expression_Parser *parser,
    Noc_Preprocessor_Expression_Status status,
    size_t token_index)
{
    if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) return;
    if (token_index >= parser->expansion->count) {
        token_index = NOC_TOKEN_INDEX_NONE;
    }
    parser->status = status;
    parser->problem_token_index = token_index;
}

static size_t noc__preprocessor_expression_next(
    const Noc__Preprocessor_Expression_Parser *parser,
    size_t cursor)
{
    while (cursor < parser->expansion->count &&
           noc_token_is_trivia(parser->expansion->items[cursor].token)) {
        cursor += 1;
    }
    return cursor;
}

static size_t noc__preprocessor_expression_peek(
    const Noc__Preprocessor_Expression_Parser *parser)
{
    return noc__preprocessor_expression_next(parser, parser->cursor);
}

static bool noc__preprocessor_expression_take_punct(
    Noc__Preprocessor_Expression_Parser *parser,
    const char *punctuator,
    size_t *token_index)
{
    size_t index = noc__preprocessor_expression_peek(parser);
    if (index >= parser->expansion->count ||
        !noc_token_is_punct(parser->expansion->items[index].token, punctuator)) {
        return false;
    }
    parser->cursor = index + 1;
    if (token_index) *token_index = index;
    return true;
}

static int noc__preprocessor_digit_value(char character)
{
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

static Noc_Preprocessor_Expression_Status noc__preprocessor_integer_literal(
    Noc_Token token,
    Noc__Preprocessor_Value *value)
{
    Noc_Buffer logical = {0};
    const char *text;
    size_t count;
    size_t cursor = 0;
    size_t digit_begin;
    unsigned int base = 10;
    uintmax_t number = 0;
    bool is_unsigned = false;
    bool saw_digit = false;
    Noc_Preprocessor_Expression_Status status =
        NOC_PREPROCESSOR_EXPRESSION_MALFORMED;
    if (!noc_token_logical_text(token, &logical)) {
        return NOC_PREPROCESSOR_EXPRESSION_OUT_OF_MEMORY;
    }
    text = logical.items;
    count = logical.count;
    if (count == 0) goto done;
    if (count >= 2 && text[0] == '0' &&
        (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        cursor = 2;
    } else if (text[0] == '0') {
        base = 8;
    }
    digit_begin = cursor;
    while (cursor < count) {
        int digit = noc__preprocessor_digit_value(text[cursor]);
        if (digit < 0 || (unsigned int)digit >= base) break;
        saw_digit = true;
        if (number > (UINTMAX_MAX - (uintmax_t)digit) / base) goto done;
        number = number * base + (uintmax_t)digit;
        cursor += 1;
    }
    if (!saw_digit || cursor == digit_begin) goto done;
    if (cursor < count && (text[cursor] == 'u' || text[cursor] == 'U')) {
        is_unsigned = true;
        cursor += 1;
    }
    if (cursor < count && (text[cursor] == 'l' || text[cursor] == 'L')) {
        char letter = text[cursor++];
        if (cursor < count && text[cursor] == letter) cursor += 1;
    }
    if (!is_unsigned && cursor < count &&
        (text[cursor] == 'u' || text[cursor] == 'U')) {
        is_unsigned = true;
        cursor += 1;
    }
    if (cursor != count) goto done;
    if (!is_unsigned && number > (uintmax_t)INTMAX_MAX) {
        if (base == 10) goto done;
        is_unsigned = true;
    }
    *value = is_unsigned ? noc__preprocessor_unsigned_value(number) :
                           noc__preprocessor_signed_value((intmax_t)number);
    status = NOC_PREPROCESSOR_EXPRESSION_OK;

done:
    noc_buffer_free(&logical);
    return status;
}

static Noc_Preprocessor_Expression_Status noc__preprocessor_character_literal(
    Noc_Token token,
    Noc__Preprocessor_Value *value)
{
    Noc_Buffer logical = {0};
    const char *text;
    size_t count;
    size_t cursor = 1;
    size_t end;
    unsigned int character = 0;
    Noc_Preprocessor_Expression_Status status =
        NOC_PREPROCESSOR_EXPRESSION_MALFORMED;
    if (!noc_token_logical_text(token, &logical)) {
        return NOC_PREPROCESSOR_EXPRESSION_OUT_OF_MEMORY;
    }
    text = logical.items;
    count = logical.count;
    if (count < 3 || text[0] != '\'' || text[count - 1] != '\'') {
        status = NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT;
        goto done;
    }
    end = count - 1;
    if (text[cursor] != '\\') {
        status = NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT;
        goto done;
    } else {
        cursor += 1;
        if (cursor >= end) goto done;
        switch (text[cursor]) {
        case '\'':
        case '"':
        case '?':
        case '\\':
        case 'a':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
        case 'v':
            status = NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT;
            goto done;
        case 'x': {
            size_t digits = 0;
            cursor += 1;
            while (cursor < end) {
                int digit = noc__preprocessor_digit_value(text[cursor]);
                if (digit < 0) break;
                if (character > (UINT_MAX - (unsigned int)digit) / 16u) {
                    status = NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT;
                    goto done;
                }
                character = character * 16u + (unsigned int)digit;
                cursor += 1;
                digits += 1;
            }
            if (digits == 0) goto done;
            break;
        }
        case 'u':
        case 'U':
            status = NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT;
            goto done;
        default:
            if (text[cursor] >= '0' && text[cursor] <= '7') {
                size_t digits = 0;
                while (cursor < end && digits < 3 &&
                       text[cursor] >= '0' && text[cursor] <= '7') {
                    character = character * 8u +
                                (unsigned int)(text[cursor] - '0');
                    cursor += 1;
                    digits += 1;
                }
            } else {
                goto done;
            }
            break;
        }
    }
    if (cursor != end || character > 127u) {
        status = NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT;
        goto done;
    }
    *value = noc__preprocessor_signed_value((intmax_t)character);
    status = NOC_PREPROCESSOR_EXPRESSION_OK;

done:
    noc_buffer_free(&logical);
    return status;
}

static Noc__Preprocessor_Value noc__preprocessor_parse_conditional(
    Noc__Preprocessor_Expression_Parser *, bool);
static Noc__Preprocessor_Value noc__preprocessor_parse_expression(
    Noc__Preprocessor_Expression_Parser *, bool);

static Noc__Preprocessor_Value noc__preprocessor_parse_primary(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    size_t index = noc__preprocessor_expression_peek(parser);
    Noc__Preprocessor_Value value = noc__preprocessor_signed_value(0);
    Noc_Token token;
    if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) return value;
    if (index >= parser->expansion->count) {
        noc__preprocessor_expression_fail(parser,
                                          NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
                                          NOC_TOKEN_INDEX_NONE);
        return value;
    }
    token = parser->expansion->items[index].token;
    if (noc_token_is_punct(token, "(")) {
        parser->cursor = index + 1;
        if (parser->depth == NOC__PREPROCESSOR_EXPRESSION_MAX_DEPTH) {
            noc__preprocessor_expression_fail(
                parser,
                NOC_PREPROCESSOR_EXPRESSION_DEPTH_LIMIT,
                index);
            return value;
        }
        parser->depth += 1;
        value = noc__preprocessor_parse_expression(parser, evaluate);
        parser->depth -= 1;
        if (!noc__preprocessor_expression_take_punct(parser, ")", NULL)) {
            noc__preprocessor_expression_fail(
                parser,
                NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
                noc__preprocessor_expression_peek(parser));
        }
        return value;
    }
    parser->cursor = index + 1;
    if (token.kind == NOC_TOKEN_NUMBER) {
        Noc_Preprocessor_Expression_Status status =
            noc__preprocessor_integer_literal(token, &value);
        if (status != NOC_PREPROCESSOR_EXPRESSION_OK) {
            noc__preprocessor_expression_fail(parser, status, index);
        }
        return value;
    }
    if (token.kind == NOC_TOKEN_CHARACTER) {
        Noc_Preprocessor_Expression_Status status =
            noc__preprocessor_character_literal(token, &value);
        if (status != NOC_PREPROCESSOR_EXPRESSION_OK &&
            (evaluate || status != NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT)) {
            noc__preprocessor_expression_fail(parser, status, index);
        }
        return value;
    }
    if (token.kind == NOC_TOKEN_IDENTIFIER) {
        return value;
    }
    noc__preprocessor_expression_fail(parser,
                                      NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
                                      index);
    return value;
}

static Noc__Preprocessor_Value noc__preprocessor_parse_unary(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    size_t operator_index;
    Noc__Preprocessor_Value value;
    size_t index = noc__preprocessor_expression_peek(parser);
    if (index < parser->expansion->count &&
        noc_token_is_identifier(parser->expansion->items[index].token,
                                "defined")) {
        size_t operand;
        bool parenthesized;
        bool is_defined = false;
        parser->cursor = index + 1;
        parenthesized = noc__preprocessor_expression_take_punct(parser, "(", NULL);
        operand = noc__preprocessor_expression_peek(parser);
        if (operand >= parser->expansion->count ||
            parser->expansion->items[operand].token.kind != NOC_TOKEN_IDENTIFIER) {
            noc__preprocessor_expression_fail(
                parser,
                NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
                operand < parser->expansion->count ? operand : NOC_TOKEN_INDEX_NONE);
            return noc__preprocessor_signed_value(0);
        }
        parser->cursor = operand + 1;
        if (evaluate) {
            Noc_Slice name = parser->expansion->items[operand].token.text;
            is_defined = noc_macro_environment_lookup_before(
                             parser->expansion->environment,
                             name,
                             parser->expansion->environment_entry_limit) != NULL ||
                         noc_macro_builtin_kind_from_name(name) !=
                             NOC_MACRO_BUILTIN_NONE;
        }
        if (parenthesized &&
            !noc__preprocessor_expression_take_punct(parser, ")", NULL)) {
            noc__preprocessor_expression_fail(
                parser,
                NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
                noc__preprocessor_expression_peek(parser));
        }
        return noc__preprocessor_signed_value(is_defined ? 1 : 0);
    }
    if (noc__preprocessor_expression_take_punct(parser, "+", &operator_index) ||
        noc__preprocessor_expression_take_punct(parser, "-", &operator_index) ||
        noc__preprocessor_expression_take_punct(parser, "!", &operator_index) ||
        noc__preprocessor_expression_take_punct(parser, "~", &operator_index)) {
        Noc_Token operator_token =
            parser->expansion->items[operator_index].token;
        if (parser->depth == NOC__PREPROCESSOR_EXPRESSION_MAX_DEPTH) {
            noc__preprocessor_expression_fail(
                parser,
                NOC_PREPROCESSOR_EXPRESSION_DEPTH_LIMIT,
                operator_index);
            return noc__preprocessor_signed_value(0);
        }
        parser->depth += 1;
        value = noc__preprocessor_parse_unary(parser, evaluate);
        parser->depth -= 1;
        if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) {
            return value;
        }
        if (noc_token_is_punct(operator_token, "!")) {
            if (!evaluate) return noc__preprocessor_signed_value(0);
            return noc__preprocessor_signed_value(
                noc__preprocessor_value_is_true(value) ? 0 : 1);
        }
        if (!evaluate) {
            return value.is_unsigned ? noc__preprocessor_unsigned_value(0) :
                                       noc__preprocessor_signed_value(0);
        }
        if (noc_token_is_punct(operator_token, "+")) return value;
        if (noc_token_is_punct(operator_token, "~")) {
            return value.is_unsigned ?
                       noc__preprocessor_unsigned_value(~value.unsigned_value) :
                       noc__preprocessor_signed_value(~value.signed_value);
        }
        if (value.is_unsigned) {
            return noc__preprocessor_unsigned_value((uintmax_t)0 -
                                                     value.unsigned_value);
        }
        if (value.signed_value == INTMAX_MIN) {
            noc__preprocessor_expression_fail(
                parser,
                NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW,
                operator_index);
            return noc__preprocessor_signed_value(0);
        }
        return noc__preprocessor_signed_value(-value.signed_value);
    }
    return noc__preprocessor_parse_primary(parser, evaluate);
}

static bool noc__preprocessor_signed_multiply(intmax_t left,
                                              intmax_t right,
                                              intmax_t *result)
{
    if (left > 0) {
        if ((right > 0 && left > INTMAX_MAX / right) ||
            (right < 0 && right < INTMAX_MIN / left)) {
            return false;
        }
    } else if (left < 0) {
        if ((right > 0 && left < INTMAX_MIN / right) ||
            (right < 0 && left < INTMAX_MAX / right)) {
            return false;
        }
    }
    *result = left * right;
    return true;
}

static Noc__Preprocessor_Value noc__preprocessor_parse_multiplicative(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    Noc__Preprocessor_Value left = noc__preprocessor_parse_unary(parser, evaluate);
    for (;;) {
        size_t operator_index;
        const char *operator_name;
        Noc__Preprocessor_Value right;
        if (noc__preprocessor_expression_take_punct(parser, "*", &operator_index)) {
            operator_name = "*";
        } else if (noc__preprocessor_expression_take_punct(parser,
                                                           "/",
                                                           &operator_index)) {
            operator_name = "/";
        } else if (noc__preprocessor_expression_take_punct(parser,
                                                           "%",
                                                           &operator_index)) {
            operator_name = "%";
        } else {
            return left;
        }
        right = noc__preprocessor_parse_unary(parser, evaluate);
        if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) {
            continue;
        }
        if (!evaluate) {
            left = left.is_unsigned || right.is_unsigned ?
                       noc__preprocessor_unsigned_value(0) :
                       noc__preprocessor_signed_value(0);
            continue;
        }
        if (left.is_unsigned || right.is_unsigned) {
            uintmax_t lhs = noc__preprocessor_value_as_unsigned(left);
            uintmax_t rhs = noc__preprocessor_value_as_unsigned(right);
            if ((operator_name[0] == '/' || operator_name[0] == '%') && rhs == 0) {
                noc__preprocessor_expression_fail(
                    parser,
                    NOC_PREPROCESSOR_EXPRESSION_DIVISION_BY_ZERO,
                    operator_index);
            } else if (operator_name[0] == '*') {
                left = noc__preprocessor_unsigned_value(lhs * rhs);
            } else if (operator_name[0] == '/') {
                left = noc__preprocessor_unsigned_value(lhs / rhs);
            } else {
                left = noc__preprocessor_unsigned_value(lhs % rhs);
            }
        } else {
            intmax_t result;
            if ((operator_name[0] == '/' || operator_name[0] == '%') &&
                right.signed_value == 0) {
                noc__preprocessor_expression_fail(
                    parser,
                    NOC_PREPROCESSOR_EXPRESSION_DIVISION_BY_ZERO,
                    operator_index);
            } else if ((operator_name[0] == '/' || operator_name[0] == '%') &&
                       left.signed_value == INTMAX_MIN &&
                       right.signed_value == -1) {
                noc__preprocessor_expression_fail(
                    parser,
                    NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW,
                    operator_index);
            } else if (operator_name[0] == '*' &&
                       !noc__preprocessor_signed_multiply(left.signed_value,
                                                         right.signed_value,
                                                         &result)) {
                noc__preprocessor_expression_fail(
                    parser,
                    NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW,
                    operator_index);
            } else if (operator_name[0] == '*') {
                left = noc__preprocessor_signed_value(result);
            } else if (operator_name[0] == '/') {
                left = noc__preprocessor_signed_value(left.signed_value /
                                                      right.signed_value);
            } else {
                left = noc__preprocessor_signed_value(left.signed_value %
                                                      right.signed_value);
            }
        }
    }
}

static Noc__Preprocessor_Value noc__preprocessor_parse_additive(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    Noc__Preprocessor_Value left =
        noc__preprocessor_parse_multiplicative(parser, evaluate);
    for (;;) {
        size_t operator_index;
        bool subtract;
        Noc__Preprocessor_Value right;
        if (noc__preprocessor_expression_take_punct(parser, "+", &operator_index)) {
            subtract = false;
        } else if (noc__preprocessor_expression_take_punct(parser,
                                                           "-",
                                                           &operator_index)) {
            subtract = true;
        } else {
            return left;
        }
        right = noc__preprocessor_parse_multiplicative(parser, evaluate);
        if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) {
            continue;
        }
        if (!evaluate) {
            left = left.is_unsigned || right.is_unsigned ?
                       noc__preprocessor_unsigned_value(0) :
                       noc__preprocessor_signed_value(0);
            continue;
        }
        if (left.is_unsigned || right.is_unsigned) {
            uintmax_t lhs = noc__preprocessor_value_as_unsigned(left);
            uintmax_t rhs = noc__preprocessor_value_as_unsigned(right);
            left = noc__preprocessor_unsigned_value(subtract ? lhs - rhs :
                                                               lhs + rhs);
        } else if (!subtract) {
            if ((right.signed_value > 0 &&
                 left.signed_value > INTMAX_MAX - right.signed_value) ||
                (right.signed_value < 0 &&
                 left.signed_value < INTMAX_MIN - right.signed_value)) {
                noc__preprocessor_expression_fail(
                    parser,
                    NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW,
                    operator_index);
            } else {
                left = noc__preprocessor_signed_value(left.signed_value +
                                                      right.signed_value);
            }
        } else {
            if ((right.signed_value > 0 &&
                 left.signed_value < INTMAX_MIN + right.signed_value) ||
                (right.signed_value < 0 &&
                 left.signed_value > INTMAX_MAX + right.signed_value)) {
                noc__preprocessor_expression_fail(
                    parser,
                    NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW,
                    operator_index);
            } else {
                left = noc__preprocessor_signed_value(left.signed_value -
                                                      right.signed_value);
            }
        }
    }
}

static Noc__Preprocessor_Value noc__preprocessor_parse_shift(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    Noc__Preprocessor_Value left = noc__preprocessor_parse_additive(parser, evaluate);
    for (;;) {
        size_t operator_index;
        bool right_shift;
        Noc__Preprocessor_Value right;
        uintmax_t count;
        if (noc__preprocessor_expression_take_punct(parser,
                                                    "<<",
                                                    &operator_index)) {
            right_shift = false;
        } else if (noc__preprocessor_expression_take_punct(parser,
                                                           ">>",
                                                           &operator_index)) {
            right_shift = true;
        } else {
            return left;
        }
        right = noc__preprocessor_parse_additive(parser, evaluate);
        if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) {
            continue;
        }
        if (!evaluate) {
            left = left.is_unsigned ? noc__preprocessor_unsigned_value(0) :
                                      noc__preprocessor_signed_value(0);
            continue;
        }
        if (!right.is_unsigned && right.signed_value < 0) {
            noc__preprocessor_expression_fail(
                parser,
                NOC_PREPROCESSOR_EXPRESSION_SHIFT_OUT_OF_RANGE,
                operator_index);
            continue;
        }
        count = right.is_unsigned ? right.unsigned_value :
                                    (uintmax_t)right.signed_value;
        if (count >= sizeof(uintmax_t) * CHAR_BIT) {
            noc__preprocessor_expression_fail(
                parser,
                NOC_PREPROCESSOR_EXPRESSION_SHIFT_OUT_OF_RANGE,
                operator_index);
            continue;
        }
        if (left.is_unsigned) {
            left = noc__preprocessor_unsigned_value(
                right_shift ? left.unsigned_value >> count :
                              left.unsigned_value << count);
        } else if (right_shift) {
            if (left.signed_value < 0) {
                noc__preprocessor_expression_fail(
                    parser,
                    NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT,
                    operator_index);
            } else {
                left = noc__preprocessor_signed_value(left.signed_value >> count);
            }
        } else if (left.signed_value < 0 ||
                   (count != 0 &&
                    (uintmax_t)left.signed_value >
                        (uintmax_t)INTMAX_MAX >> count)) {
            noc__preprocessor_expression_fail(
                parser,
                NOC_PREPROCESSOR_EXPRESSION_SHIFT_OUT_OF_RANGE,
                operator_index);
        } else {
            left = noc__preprocessor_signed_value(
                (intmax_t)((uintmax_t)left.signed_value << count));
        }
    }
}

static Noc__Preprocessor_Value noc__preprocessor_parse_relational(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    Noc__Preprocessor_Value left = noc__preprocessor_parse_shift(parser, evaluate);
    for (;;) {
        size_t operator_index;
        const char *operator_name;
        Noc__Preprocessor_Value right;
        bool result;
        if (noc__preprocessor_expression_take_punct(parser, "<=", &operator_index)) {
            operator_name = "<=";
        } else if (noc__preprocessor_expression_take_punct(parser,
                                                           ">=",
                                                           &operator_index)) {
            operator_name = ">=";
        } else if (noc__preprocessor_expression_take_punct(parser,
                                                           "<",
                                                           &operator_index)) {
            operator_name = "<";
        } else if (noc__preprocessor_expression_take_punct(parser,
                                                           ">",
                                                           &operator_index)) {
            operator_name = ">";
        } else {
            return left;
        }
        right = noc__preprocessor_parse_shift(parser, evaluate);
        if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) {
            continue;
        }
        if (!evaluate) {
            left = noc__preprocessor_signed_value(0);
            continue;
        }
        if (left.is_unsigned || right.is_unsigned) {
            uintmax_t lhs = noc__preprocessor_value_as_unsigned(left);
            uintmax_t rhs = noc__preprocessor_value_as_unsigned(right);
            result = operator_name[0] == '<' ?
                         (operator_name[1] == '=' ? lhs <= rhs : lhs < rhs) :
                         (operator_name[1] == '=' ? lhs >= rhs : lhs > rhs);
        } else {
            result = operator_name[0] == '<' ?
                         (operator_name[1] == '=' ?
                              left.signed_value <= right.signed_value :
                              left.signed_value < right.signed_value) :
                         (operator_name[1] == '=' ?
                              left.signed_value >= right.signed_value :
                              left.signed_value > right.signed_value);
        }
        left = noc__preprocessor_signed_value(result ? 1 : 0);
    }
}

static Noc__Preprocessor_Value noc__preprocessor_parse_equality(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    Noc__Preprocessor_Value left =
        noc__preprocessor_parse_relational(parser, evaluate);
    for (;;) {
        size_t operator_index;
        bool unequal;
        bool equal;
        Noc__Preprocessor_Value right;
        if (noc__preprocessor_expression_take_punct(parser,
                                                    "==",
                                                    &operator_index)) {
            unequal = false;
        } else if (noc__preprocessor_expression_take_punct(parser,
                                                           "!=",
                                                           &operator_index)) {
            unequal = true;
        } else {
            return left;
        }
        right = noc__preprocessor_parse_relational(parser, evaluate);
        if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) {
            continue;
        }
        if (!evaluate) {
            left = noc__preprocessor_signed_value(0);
            continue;
        }
        equal = left.is_unsigned || right.is_unsigned ?
                    noc__preprocessor_value_as_unsigned(left) ==
                        noc__preprocessor_value_as_unsigned(right) :
                    left.signed_value == right.signed_value;
        left = noc__preprocessor_signed_value((equal != unequal) ? 1 : 0);
    }
}

static Noc__Preprocessor_Value noc__preprocessor_parse_bitwise_and(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    Noc__Preprocessor_Value left = noc__preprocessor_parse_equality(parser, evaluate);
    size_t operator_index;
    while (noc__preprocessor_expression_take_punct(parser, "&", &operator_index)) {
        Noc__Preprocessor_Value right =
            noc__preprocessor_parse_equality(parser, evaluate);
        if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) {
            continue;
        } else if (!evaluate) {
            left = left.is_unsigned || right.is_unsigned ?
                       noc__preprocessor_unsigned_value(0) :
                       noc__preprocessor_signed_value(0);
        } else if (left.is_unsigned || right.is_unsigned) {
            left = noc__preprocessor_unsigned_value(
                noc__preprocessor_value_as_unsigned(left) &
                noc__preprocessor_value_as_unsigned(right));
        } else {
            left = noc__preprocessor_signed_value(left.signed_value &
                                                  right.signed_value);
        }
    }
    return left;
}

static Noc__Preprocessor_Value noc__preprocessor_parse_bitwise_xor(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    Noc__Preprocessor_Value left =
        noc__preprocessor_parse_bitwise_and(parser, evaluate);
    size_t operator_index;
    while (noc__preprocessor_expression_take_punct(parser, "^", &operator_index)) {
        Noc__Preprocessor_Value right =
            noc__preprocessor_parse_bitwise_and(parser, evaluate);
        if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) {
            continue;
        } else if (!evaluate) {
            left = left.is_unsigned || right.is_unsigned ?
                       noc__preprocessor_unsigned_value(0) :
                       noc__preprocessor_signed_value(0);
        } else if (left.is_unsigned || right.is_unsigned) {
            left = noc__preprocessor_unsigned_value(
                noc__preprocessor_value_as_unsigned(left) ^
                noc__preprocessor_value_as_unsigned(right));
        } else {
            left = noc__preprocessor_signed_value(left.signed_value ^
                                                  right.signed_value);
        }
    }
    return left;
}

static Noc__Preprocessor_Value noc__preprocessor_parse_bitwise_or(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    Noc__Preprocessor_Value left =
        noc__preprocessor_parse_bitwise_xor(parser, evaluate);
    size_t operator_index;
    while (noc__preprocessor_expression_take_punct(parser, "|", &operator_index)) {
        Noc__Preprocessor_Value right =
            noc__preprocessor_parse_bitwise_xor(parser, evaluate);
        if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) {
            continue;
        } else if (!evaluate) {
            left = left.is_unsigned || right.is_unsigned ?
                       noc__preprocessor_unsigned_value(0) :
                       noc__preprocessor_signed_value(0);
        } else if (left.is_unsigned || right.is_unsigned) {
            left = noc__preprocessor_unsigned_value(
                noc__preprocessor_value_as_unsigned(left) |
                noc__preprocessor_value_as_unsigned(right));
        } else {
            left = noc__preprocessor_signed_value(left.signed_value |
                                                  right.signed_value);
        }
    }
    return left;
}

static Noc__Preprocessor_Value noc__preprocessor_parse_logical_and(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    Noc__Preprocessor_Value left =
        noc__preprocessor_parse_bitwise_or(parser, evaluate);
    size_t operator_index;
    while (noc__preprocessor_expression_take_punct(parser, "&&", &operator_index)) {
        bool lhs = evaluate && noc__preprocessor_value_is_true(left);
        Noc__Preprocessor_Value right =
            noc__preprocessor_parse_bitwise_or(parser, lhs);
        if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) {
            continue;
        } else if (evaluate) {
            left = noc__preprocessor_signed_value(
                lhs && noc__preprocessor_value_is_true(right) ? 1 : 0);
        } else {
            left = noc__preprocessor_signed_value(0);
        }
    }
    return left;
}

static Noc__Preprocessor_Value noc__preprocessor_parse_logical_or(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    Noc__Preprocessor_Value left =
        noc__preprocessor_parse_logical_and(parser, evaluate);
    size_t operator_index;
    while (noc__preprocessor_expression_take_punct(parser, "||", &operator_index)) {
        bool evaluate_right = evaluate && !noc__preprocessor_value_is_true(left);
        Noc__Preprocessor_Value right =
            noc__preprocessor_parse_logical_and(parser, evaluate_right);
        if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) {
            continue;
        } else if (evaluate) {
            left = noc__preprocessor_signed_value(
                noc__preprocessor_value_is_true(left) ||
                        noc__preprocessor_value_is_true(right)
                    ? 1
                    : 0);
        } else {
            left = noc__preprocessor_signed_value(0);
        }
    }
    return left;
}

static Noc__Preprocessor_Value noc__preprocessor_parse_conditional(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    Noc__Preprocessor_Value condition =
        noc__preprocessor_parse_logical_or(parser, evaluate);
    size_t question_index;
    if (noc__preprocessor_expression_take_punct(parser,
                                                "?",
                                                &question_index)) {
        bool selected_true = evaluate &&
                             noc__preprocessor_value_is_true(condition);
        Noc__Preprocessor_Value when_true;
        Noc__Preprocessor_Value when_false;
        if (parser->depth == NOC__PREPROCESSOR_EXPRESSION_MAX_DEPTH) {
            noc__preprocessor_expression_fail(
                parser,
                NOC_PREPROCESSOR_EXPRESSION_DEPTH_LIMIT,
                question_index);
            return noc__preprocessor_signed_value(0);
        }
        parser->depth += 1;
        when_true = noc__preprocessor_parse_expression(parser, selected_true);
        if (!noc__preprocessor_expression_take_punct(parser, ":", NULL)) {
            noc__preprocessor_expression_fail(
                parser,
                NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
                noc__preprocessor_expression_peek(parser));
            parser->depth -= 1;
            return noc__preprocessor_signed_value(0);
        }
        when_false = noc__preprocessor_parse_conditional(
            parser,
            evaluate && !selected_true);
        parser->depth -= 1;
        if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) {
            return noc__preprocessor_signed_value(0);
        }
        if (when_true.is_unsigned || when_false.is_unsigned) {
            if (!evaluate) return noc__preprocessor_unsigned_value(0);
            return noc__preprocessor_unsigned_value(
                noc__preprocessor_value_as_unsigned(selected_true ? when_true :
                                                                    when_false));
        }
        if (evaluate) {
            return selected_true ? when_true : when_false;
        }
        return noc__preprocessor_signed_value(0);
    }
    return condition;
}

static Noc__Preprocessor_Value noc__preprocessor_parse_expression(
    Noc__Preprocessor_Expression_Parser *parser,
    bool evaluate)
{
    Noc__Preprocessor_Value value =
        noc__preprocessor_parse_conditional(parser, evaluate);
    size_t comma_index;
    while (noc__preprocessor_expression_take_punct(parser,
                                                   ",",
                                                   &comma_index)) {
        Noc__Preprocessor_Value right =
            noc__preprocessor_parse_conditional(parser, false);
        if (parser->status != NOC_PREPROCESSOR_EXPRESSION_OK) continue;
        if (evaluate) {
            noc__preprocessor_expression_fail(
                parser,
                NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
                comma_index);
        }
        value = right;
    }
    return value;
}

NOCDEF const char *noc_preprocessor_expression_status_name(
    Noc_Preprocessor_Expression_Status status)
{
    switch (status) {
    case NOC_PREPROCESSOR_EXPRESSION_OK: return "ok";
    case NOC_PREPROCESSOR_EXPRESSION_INVALID_ARGUMENT: return "invalid-argument";
    case NOC_PREPROCESSOR_EXPRESSION_STALE: return "stale";
    case NOC_PREPROCESSOR_EXPRESSION_MALFORMED: return "malformed";
    case NOC_PREPROCESSOR_EXPRESSION_DIVISION_BY_ZERO: return "division-by-zero";
    case NOC_PREPROCESSOR_EXPRESSION_SIGNED_OVERFLOW: return "signed-overflow";
    case NOC_PREPROCESSOR_EXPRESSION_SHIFT_OUT_OF_RANGE:
        return "shift-out-of-range";
    case NOC_PREPROCESSOR_EXPRESSION_TARGET_DEPENDENT: return "target-dependent";
    case NOC_PREPROCESSOR_EXPRESSION_DEPTH_LIMIT: return "depth-limit";
    case NOC_PREPROCESSOR_EXPRESSION_OUT_OF_MEMORY: return "out-of-memory";
    }
    return "unknown";
}

NOCDEF Noc_Preprocessor_Expression_Status noc_preprocessor_expression_evaluate(
    const Noc_Macro_Expansion *expansion,
    bool *value,
    size_t *problem_token_index)
{
    Noc__Preprocessor_Expression_Parser parser;
    Noc__Preprocessor_Value result;
    if (!expansion || !value || !problem_token_index) {
        return NOC_PREPROCESSOR_EXPRESSION_INVALID_ARGUMENT;
    }
    *problem_token_index = NOC_TOKEN_INDEX_NONE;
    if (!noc_macro_expansion_is_valid(expansion)) {
        return NOC_PREPROCESSOR_EXPRESSION_STALE;
    }
    memset(&parser, 0, sizeof(parser));
    parser.expansion = expansion;
    parser.problem_token_index = NOC_TOKEN_INDEX_NONE;
    parser.status = NOC_PREPROCESSOR_EXPRESSION_OK;
    result = noc__preprocessor_parse_expression(&parser, true);
    parser.cursor = noc__preprocessor_expression_next(&parser, parser.cursor);
    if (parser.status == NOC_PREPROCESSOR_EXPRESSION_OK &&
        parser.cursor != expansion->count) {
        noc__preprocessor_expression_fail(&parser,
                                          NOC_PREPROCESSOR_EXPRESSION_MALFORMED,
                                          parser.cursor);
    }
    if (parser.status != NOC_PREPROCESSOR_EXPRESSION_OK) {
        *problem_token_index = parser.problem_token_index;
        return parser.status;
    }
    *value = noc__preprocessor_value_is_true(result);
    return NOC_PREPROCESSOR_EXPRESSION_OK;
}

#undef NOC__PREPROCESSOR_EXPRESSION_MAX_DEPTH

#endif /* NOC_CONDITIONAL_IMPLEMENTATION_INCLUDED */
#endif /* NOC_IMPLEMENTATION || NOC__INDIVIDUAL_SOURCE */
