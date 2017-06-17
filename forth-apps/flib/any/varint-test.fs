\ tests for varint.fs

: rf-send ;
include varint.fs
include buffers.fs
include testing.fs


8 buffer: test-buf
8 buffer: test-buf2


: test:>var ( n b1 b2 ... bi i -- )
  test-buf stack>buffer rot
  ." Testing " dup .
  <v >var v> test-buf2 -rot buffer-cpy
  \ ." got: " 2dup buffer. cr
  \ ." exp: " 2over buffer. cr
  compare always
  ;

0   $80 1 test:>var
1   $82 1 test:>var
2   $84 1 test:>var
-1  $81 1 test:>var
-2  $83 1 test:>var

\ test positive numbers
63  $fe 1 test:>var
64  1 $80 2 test:>var
127 1 $fe 2 test:>var
12   6 lshift 34 +  12 196 2 test:>var
12  13 lshift 34 +  12 0 128 68 + 3 test:>var
$7f 24 lshift       $0f $70 0 0 128 5 test:>var

\ test negative numbers
-64  $ff 1 test:>var
-65  1 $81 2 test:>var
-127 1 $fd 2 test:>var
-128 1 $ff 2 test:>var
12   6 lshift negate 34 +  11 187 2 test:>var
12  13 lshift negate 34 +  11 $7f 187 3 test:>var
$80000000                  $0f $7f $7f $7f $ff 5 test:>var

: test:var> ( n b1 b2 ... bi i -- )
  test-buf stack>buffer 
  ." Testing " 2dup buffer.
  var-init var> always
  =always
  ;

0   $80 1 test:var>
1   $82 1 test:var>
2   $84 1 test:var>
-1  $81 1 test:var>
-2  $83 1 test:var>

\ test positive numbers
63  $fe 1 test:var>
64  1 $80 2 test:var>
127 1 $fe 2 test:var>
12   6 lshift 34 +  12 196 2 test:var>
12  13 lshift 34 +  12 0 128 68 + 3 test:var>
$7f 24 lshift       $0f $70 0 0 128 5 test:var>

\ test negative numbers
-64  $ff 1 test:var>
-65  1 $81 2 test:var>
-127 1 $fd 2 test:var>
-128 1 $ff 2 test:var>
12   6 lshift negate 34 +  11 187 2 test:var>
12  13 lshift negate 34 +  11 $7f 187 3 test:var>
$80000000                  $0f $7f $7f $7f $ff 5 test:var>

test-summary
