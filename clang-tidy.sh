run-clang-tidy -p build -checks='\
bugprone-*,\
-bugprone-exception-escape,\
-bugprone-easily-swappable-parameters,\
cppcoreguidelines-*,\
-cppcoreguidelines-pro-type-static-cast-downcast,\
-cppcoreguidelines-pro-bounds-pointer-arithmetic,\
-cppcoreguidelines-avoid-const-or-ref-data-members,\
clang-analyzer-*,\
misc-*,\
-misc-no-recursion,\
-misc-non-private-member-variables-in-classes,\
performance-*'
