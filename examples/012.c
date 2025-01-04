#pragma clang diagnostic ignored "-Wxor-used-as-pow"
#pragma clang diagnostic ignored "-Wlogical-not-parentheses"

int main() {
    //-------------------------------------------------------------------------
    // 1) Test prefix operators: -, !, ~
    //-------------------------------------------------------------------------

    // prefix "-"
    if (-1 != -1) { __builtin_trap(); }
    int a = 5;
    if (-a != -5) { __builtin_trap(); }

    // prefix "!"
    if (!0 != 1) { __builtin_trap(); }
    if (!1 != 0) { __builtin_trap(); }
    if (!(-1) != 0) { __builtin_trap(); }  // nonzero => true => !true = false => 0

    // prefix "~"
    if (~0 != -1) { __builtin_trap(); }
    if (~1 != -2) { __builtin_trap(); }
    if (~(-1) != 0) { __builtin_trap(); } // ~(-1) => ~0xFFFFFFFF => 0x00000000 (on 32-bit)

    //-------------------------------------------------------------------------
    // 2) Test infix operators
    //-------------------------------------------------------------------------

    // (a) "*", "/", "%"
    if (2 * 3 != 6) { __builtin_trap(); }
    if (6 / 3 != 2) { __builtin_trap(); }
    if (7 / 2 != 3) { __builtin_trap(); }    // integer division
    if (7 % 2 != 1) { __builtin_trap(); }

    // (b) "+", "-"
    if (2 + 3 != 5) { __builtin_trap(); }
    if (5 - 2 != 3) { __builtin_trap(); }
    if (0 + 0 != 0) { __builtin_trap(); }
    if (0 - 5 != -5) { __builtin_trap(); }

    // (c) "<<", ">>"
    if ((1 << 1) != 2) { __builtin_trap(); }
    if ((1 << 2) != 4) { __builtin_trap(); }
    if ((3 << 1) != 6) { __builtin_trap(); }
    if ((4 >> 1) != 2) { __builtin_trap(); }
    if ((4 >> 2) != 1) { __builtin_trap(); }

    // (d) "<", "<=", ">", ">="
    if (!(2 < 3))  { __builtin_trap(); }
    if (!(2 <= 2)) { __builtin_trap(); }
    if (!(3 > 2))  { __builtin_trap(); }
    if (!(3 >= 3)) { __builtin_trap(); }
    if (3 < 3)     { __builtin_trap(); }
    if (2 > 3)     { __builtin_trap(); }

    // (e) "==", "!="
    if (!(2 == 2)) { __builtin_trap(); }
    if (2 == 3)    { __builtin_trap(); }
    if (!(2 != 3)) { __builtin_trap(); }
    if (3 != 3)    { __builtin_trap(); }

    // (f) "&&", "||"
    if (!(1 && 1)) { __builtin_trap(); }   // true && true => true
    if (1 && 0)    { __builtin_trap(); }   // true && false => false
    if (!(0 || 1)) { __builtin_trap(); }   // false || true => true
    if (0 || 0)    { __builtin_trap(); }   // false || false => false

    //-------------------------------------------------------------------------
    // 3) Additional bitwise tests: &, |, ^
    //-------------------------------------------------------------------------

    // "&"
    if ((1 & 1) != 1) { __builtin_trap(); }
    if ((1 & 0) != 0) { __builtin_trap(); }
    if ((3 & 2) != 2) { __builtin_trap(); }

    // "|"
    if ((1 | 1) != 1) { __builtin_trap(); }
    if ((1 | 0) != 1) { __builtin_trap(); }
    if ((2 | 1) != 3) { __builtin_trap(); }

    // "^"
    if ((1 ^ 1) != 0) { __builtin_trap(); }
    if ((1 ^ 0) != 1) { __builtin_trap(); }
    if ((2 ^ 1) != 3) { __builtin_trap(); }

    //-------------------------------------------------------------------------
    // 4) Test operator precedence
    //-------------------------------------------------------------------------

    // a) * / % before + -
    //    2 + 3*4 = 2 + 12 = 14
    if (2 + 3 * 4 != 14) { __builtin_trap(); }
    //    (2+3)*4 = 5*4 = 20
    if ((2 + 3) * 4 != 20) { __builtin_trap(); }

    // b) prefix ! is applied before ==, etc.
    //    !0 == 1 => true
    if (!0 != 1) { __builtin_trap(); }

    // c) test mixing logical and relational
    //    2 + 3 < 10 && 5 > 0 => true
    if (!((2 + 3 < 10) && (5 > 0))) { __builtin_trap(); }

    // d) check that + and - happen before <, which happens before &&
    //    (2 - 1) < 3 && 1 < 2 => true
    if (!(((2 - 1) < 3) && (1 < 2))) { __builtin_trap(); }

    // e) final combined precedence test
    //    !5 + 10 / 2 > 3  =>  !5 => !true => 0;
    //    0 + (10 / 2 = 5) => 5;
    //    5 > 3 => true
    if (!(!5 + 10 / 2 > 3)) { __builtin_trap(); }

    // If we get here, everything passed!
    return 0;
}
