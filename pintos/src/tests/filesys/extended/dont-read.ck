# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(dont-read) begin
(dont-read) create "cache"
(dont-read) open "cache"
(dont-read) end
EOF
pass;
