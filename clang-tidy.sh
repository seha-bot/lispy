run-clang-tidy -p build \
-header-filter=.* \
-checks='\
bugprone-*,\
-bugprone-exception-escape,\
-bugprone-easily-swappable-parameters,\
-bugprone-chained-comparison,\
-bugprone-return-const-ref-from-parameter,\
cppcoreguidelines-*,\
-cppcoreguidelines-pro-type-static-cast-downcast,\
-cppcoreguidelines-pro-bounds-pointer-arithmetic,\
-cppcoreguidelines-avoid-const-or-ref-data-members,\
-cppcoreguidelines-non-private-member-variables-in-classes,\
clang-analyzer-*,\
misc-*,\
-misc-no-recursion,\
-misc-non-private-member-variables-in-classes,\
performance-*'
