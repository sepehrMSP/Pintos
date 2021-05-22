# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(merge-writes) begin
(merge-writes) create "cache"
(merge-writes) open "cache"
(merge-writes) end
EOF
pass;
