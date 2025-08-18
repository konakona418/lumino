#include "value.h"
#include "bytecode.h"

lm_value_t lm_value_make_undefined() {
    return (lm_value_t){.type = LM_VALUE_TYPE_UNDEFINED};
}

lm_value_t lm_value_make_null() {
    return (lm_value_t){.type = LM_VALUE_TYPE_NULL};
}

lm_value_t lm_value_make_boolean(lm_bool value) {
    return (lm_value_t){.type = LM_VALUE_TYPE_BOOLEAN, .v.bool_value = value};
}

lm_value_t lm_value_make_int(lm_int value) {
    return (lm_value_t){.type = LM_VALUE_TYPE_INT, .v.int_value = value};
}

lm_value_t lm_value_make_float(lm_float value) {
    return (lm_value_t){.type = LM_VALUE_TYPE_FLOAT, .v.float_value = value};
}

lm_value_t lm_value_to_boolean(lm_value_t value) {
    switch (value.type) {
        case LM_VALUE_TYPE_NULL:
            return lm_value_make_boolean(LM_FALSE);
        case LM_VALUE_TYPE_UNDEFINED:
            return lm_value_make_boolean(LM_FALSE);
        case LM_VALUE_TYPE_INT:
            return lm_value_make_boolean(value.v.int_value != 0);
        case LM_VALUE_TYPE_FLOAT:
            return lm_value_make_boolean(value.v.float_value != 0.0f);
        case LM_VALUE_TYPE_BOOLEAN:
            return value;
        default:
            _LM_ASSERT(0, "dynamic dispatch not implemented");
    }
}

#define _LM_SELECT_ARITH_OP(make_what, lhs_value, rhs_value) \
    switch (opcode) {                                        \
        case LM__OP_ADD:                                     \
            return make_what(lhs_value + rhs_value);         \
        case LM__OP_SUB:                                     \
            return make_what(lhs_value - rhs_value);         \
        case LM__OP_MUL:                                     \
            return make_what(lhs_value * rhs_value);         \
        case LM__OP_DIV:                                     \
            return make_what(lhs_value / rhs_value);         \
        default:                                             \
            return lm_value_make_undefined();                \
    }

lm_value_t lm_value_dispatch_arith(lm_value_t lhs, lm_value_t rhs, lm__opcode_value_t opcode) {
    if (lhs.type == LM_VALUE_TYPE_INT && rhs.type == LM_VALUE_TYPE_INT) {
        if (opcode == LM__OP_MOD) {
            return lm_value_make_int(lhs.v.int_value % rhs.v.int_value);
        }
        _LM_SELECT_ARITH_OP(lm_value_make_int, lhs.v.int_value, rhs.v.int_value)
    } else if (lhs.type == LM_VALUE_TYPE_FLOAT && rhs.type == LM_VALUE_TYPE_FLOAT) {
        _LM_SELECT_ARITH_OP(lm_value_make_float, lhs.v.float_value, rhs.v.float_value)
    } else if (lhs.type == LM_VALUE_TYPE_INT && rhs.type == LM_VALUE_TYPE_FLOAT) {
        _LM_SELECT_ARITH_OP(lm_value_make_float, (lm_float) lhs.v.int_value, rhs.v.float_value)
    } else if (lhs.type == LM_VALUE_TYPE_FLOAT && rhs.type == LM_VALUE_TYPE_INT) {
        _LM_SELECT_ARITH_OP(lm_value_make_float, lhs.v.float_value, (lm_float) rhs.v.int_value)
    } else {
        _LM_ASSERT(0, "dynamic dispatch not implemented");
    }
}

lm_value_t lm_value_dispatch_logical(lm_value_t lhs, lm_value_t rhs, lm__opcode_value_t opcode) {
    lhs = lm_value_to_boolean(lhs);
    rhs = lm_value_to_boolean(rhs);

    switch (opcode) {
        case LM__OP_AND:
            return lm_value_make_boolean(lhs.v.bool_value && rhs.v.bool_value);
        case LM__OP_OR:
            return lm_value_make_boolean(lhs.v.bool_value || rhs.v.bool_value);
        default:
            return lm_value_make_undefined();
    }
}

#define _LM_SELECT_COMPARE_OP(make_what, lhs_value, rhs_value) \
    switch (opcode) {                                          \
        case LM__OP_EQ:                                        \
            return make_what(lhs_value == rhs_value);          \
        case LM__OP_NEQ:                                       \
            return make_what(lhs_value != rhs_value);          \
        case LM__OP_LT:                                        \
            return make_what(lhs_value < rhs_value);           \
        case LM__OP_LTE:                                       \
            return make_what(lhs_value <= rhs_value);          \
        case LM__OP_GT:                                        \
            return make_what(lhs_value > rhs_value);           \
        case LM__OP_GTE:                                       \
            return make_what(lhs_value >= rhs_value);          \
        default:                                               \
            return lm_value_make_null();                       \
    }

lm_value_t lm_value_dispatch_comparison(lm_value_t lhs, lm_value_t rhs, lm__opcode_value_t opcode) {
    if (lhs.type == LM_VALUE_TYPE_INT && rhs.type == LM_VALUE_TYPE_INT) {
        _LM_SELECT_COMPARE_OP(lm_value_make_boolean, lhs.v.int_value, rhs.v.int_value)
    } else if (lhs.type == LM_VALUE_TYPE_FLOAT && rhs.type == LM_VALUE_TYPE_FLOAT) {
        _LM_SELECT_COMPARE_OP(lm_value_make_boolean, lhs.v.float_value, rhs.v.float_value)
    } else if (lhs.type == LM_VALUE_TYPE_INT && rhs.type == LM_VALUE_TYPE_FLOAT) {
        _LM_SELECT_COMPARE_OP(lm_value_make_boolean, (float) lhs.v.int_value, rhs.v.float_value)
    } else if (lhs.type == LM_VALUE_TYPE_FLOAT && rhs.type == LM_VALUE_TYPE_INT) {
        _LM_SELECT_COMPARE_OP(lm_value_make_boolean, lhs.v.float_value, (float) rhs.v.int_value)
    } else {
        return lm_value_make_undefined();
    }
}

lm_value_t lm_value_dispatch_unary(lm_value_t value, lm__opcode_value_t opcode) {
    switch (opcode) {
        case LM__OP_NEG: {
            if (value.type == LM_VALUE_TYPE_INT) {
                return lm_value_make_int(-value.v.int_value);
            } else if (value.type == LM_VALUE_TYPE_FLOAT) {
                return lm_value_make_float(-value.v.float_value);
            } else {
                _LM_ASSERT(0, "dynamic dispatch not implemented");
            }
        }
        case LM__OP_NOT: {
            if (value.type == LM_VALUE_TYPE_BOOLEAN) {
                value = lm_value_to_boolean(value);
                return lm_value_make_boolean(!value.v.bool_value);
            } else {
                _LM_ASSERT(0, "dynamic dispatch not implemented");
            }
        }
        default:
            return lm_value_make_undefined();
    }
}