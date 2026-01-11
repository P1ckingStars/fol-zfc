#pragma once
#include <set>
#include <variant>
#include "formula.h"

namespace logic {

namespace zfc {

class Predicate {};

class EmptySet {};

class Set {
    std::variant<std::set<Set>, EmptySet, Predicate> data;
};

}

}
