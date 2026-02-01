#ifndef MNEMONIC_HPP
#define MNEMONIC_HPP

#include <string>

enum class Mnemonic {
    eq,
    cons,
    car,
    cdr,
    jt,
    jf,
    jmp,
    push,
    call,
    pop,
    ret,
    set,
    halt,
    print,
    mkstr,
    strpush,
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
        case Mnemonic::push:
            return "push";
        case Mnemonic::call:
            return "call";
        case Mnemonic::pop:
            return "pop";
        case Mnemonic::ret:
            return "ret";
        case Mnemonic::set:
            return "set";
        case Mnemonic::halt:
            return "halt";
        case Mnemonic::print:
            return "print";
        case Mnemonic::mkstr:
            return "mkstr";
        case Mnemonic::strpush:
            return "strpush";
    }
}

#endif
