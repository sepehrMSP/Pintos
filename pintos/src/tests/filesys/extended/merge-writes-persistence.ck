# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_archive ({"cache" => ["\0" x 65536]});
pass;
