This directory contains tests for various Vim features.
For testing an indent script see runtime/indent/testdir/README.txt.

If it makes sense, add a new test method to an already existing file.  You may
want to separate it from other tests with comment lines.

TO ADD A NEW STYLE TEST:

1) Create a test_<subject>.vim file.
2) Add test_<subject>.res to NEW_TESTS_RES in Make_all.mak in alphabetical
   order.
3) Also add an entry "test_<subject>" to NEW_TESTS in Make_all.mak.
4) Use make test_<subject> to run a single test.

At 2), instead of running the test separately, it can be included in
"test_alot".  Do this for quick tests without side effects.  The test runs a
bit faster, because Vim doesn't have to be started, one Vim instance runs many
tests.

At 4), to run a test in GUI, add "GUI_FLAG=-g" to the make command.


What you can use (see test_assert.vim for an example):

- Call assert_equal(), assert_true(), assert_false(), etc.

- Use assert_fails() to check for expected errors.

- Use try/catch to avoid an exception aborts the test.

- Use test_alloc_fail() to have memory allocation fail.  This makes it possible
  to check memory allocation failures are handled gracefully.  You need to
  change the source code to add an ID to the allocation.  Add a new one to
  alloc_id_T, before aid_last.

- Use test_override() to make Vim behave differently, e.g.  if char_avail()
  must return FALSE for a while.  E.g. to trigger the CursorMovedI autocommand
  event. See test_cursor_func.vim for an example.

- If the bug that is being tested isn't fixed yet, you can throw an exception
  with "Skipped" so that it's clear this still needs work.  E.g.: throw
  "Skipped: Bug with <c-e> and popupmenu not fixed yet"

- See the start of runtest.vim for more help.


TO ADD A SCREEN DUMP TEST:

Mostly the same as writing a new style test.  Additionally, see help on
"terminal-dumptest".  Put the reference dump in "dumps/Test_func_name.dump".


OLD STYLE TESTS:

There are a few tests that are used when Vim was built without the +eval
feature.  These cannot use the "assert" functions, therefore they consist of a
.in file that contains Normal mode commands between STARTTEST and ENDTEST.
They modify the file and the result gets writtein in the test.out file.  This
is then compared with the .ok file.  If they are equal the test passed.  If
they differ the test failed.
