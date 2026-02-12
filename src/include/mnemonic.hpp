#ifndef MNEMONIC_HPP
#define MNEMONIC_HPP

#include <optional>
#include <string>
#include <string_view>

enum class Mnemonic {
    eq = 0,
    cons = 1,
    car = 2,
    cdr = 3,
    jt = 4,
    jf = 5,
    jmp = 6,
    call = 7,
    indjmp = 8,
    indcall = 9,
    push = 10,
    pop = 11,
    ret = 12,
    set = 13,
    print = 14,
    // These two should not be string specific, but array specific.
    mkstr = 15,
    strpush = 16,
    closure = 17,
    capture = 18,
    iadd = 19,
    isub = 20,
    iless = 21,
    imod = 22,
    setret = 23,
};

inline std::string to_string(Mnemonic m) {
    switch (m) {
        case Mnemonic::eq:
            return "eq";
        case Mnemonic::cons:
            return "cons";
        case Mnemonic::car:
            return "car";
        case Mnemonic::cdr:
            return "cdr";
        case Mnemonic::jt:
            return "jt";
        case Mnemonic::jf:
            return "jf";
        case Mnemonic::jmp:
            return "jmp";
        case Mnemonic::call:
            return "call";
        case Mnemonic::indjmp:
            return "indjmp";
        case Mnemonic::indcall:
            return "indcall";
        case Mnemonic::push:
            return "push";
        case Mnemonic::pop:
            return "pop";
        case Mnemonic::ret:
            return "ret";
        case Mnemonic::set:
            return "set";
        case Mnemonic::print:
            return "print";
        case Mnemonic::mkstr:
            return "mkstr";
        case Mnemonic::strpush:
            return "strpush";
        case Mnemonic::closure:
            return "closure";
        case Mnemonic::capture:
            return "capture";
        case Mnemonic::iadd:
            return "iadd";
        case Mnemonic::isub:
            return "isub";
        case Mnemonic::iless:
            return "iless";
        case Mnemonic::imod:
            return "imod";
        case Mnemonic::setret:
            return "setret";
    }
    return "unknown";
}

inline std::optional<Mnemonic> from_string(std::string_view s) {
    if (s == "eq") {
        return Mnemonic::eq;
    } else if (s == "cons") {
        return Mnemonic::cons;
    } else if (s == "car") {
        return Mnemonic::car;
    } else if (s == "cdr") {
        return Mnemonic::cdr;
    } else if (s == "jt") {
        return Mnemonic::jt;
    } else if (s == "jf") {
        return Mnemonic::jf;
    } else if (s == "jmp") {
        return Mnemonic::jmp;
    } else if (s == "call") {
        return Mnemonic::call;
    } else if (s == "indjmp") {
        return Mnemonic::indjmp;
    } else if (s == "indcall") {
        return Mnemonic::indcall;
    } else if (s == "push") {
        return Mnemonic::push;
    } else if (s == "pop") {
        return Mnemonic::pop;
    } else if (s == "ret") {
        return Mnemonic::ret;
    } else if (s == "set") {
        return Mnemonic::set;
    } else if (s == "print") {
        return Mnemonic::print;
    } else if (s == "mkstr") {
        return Mnemonic::mkstr;
    } else if (s == "strpush") {
        return Mnemonic::strpush;
    } else if (s == "closure") {
        return Mnemonic::closure;
    } else if (s == "capture") {
        return Mnemonic::capture;
    } else if (s == "iadd") {
        return Mnemonic::iadd;
    } else if (s == "isub") {
        return Mnemonic::isub;
    } else if (s == "iless") {
        return Mnemonic::iless;
    } else if (s == "imod") {
        return Mnemonic::imod;
    } else if (s == "setret") {
        return Mnemonic::setret;
    } else {
        return std::nullopt;
    }
}

#endif
