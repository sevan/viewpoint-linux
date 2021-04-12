" Test for various indent options

func Test_preserveindent()
  new
  " Test for autoindent copying indent from the previous line
  setlocal autoindent
  call setline(1, [repeat(' ', 16) .. 'line1'])
  call feedkeys("A\nline2", 'xt')
  call assert_equal("\t\tline2", getline(2))
  setlocal autoindent&

  " Test for using CTRL-T with and without 'preserveindent'
  set shiftwidth=4
  call cursor(1, 1)
  call setline(1, "    \t    ")
  call feedkeys("Al\<C-T>", 'xt')
  call assert_equal("\t\tl", getline(1))
  set preserveindent
  call setline(1, "    \t    ")
  call feedkeys("Al\<C-T>", 'xt')
  call assert_equal("    \t    \tl", getline(1))
  set pi& sw&

  " Test for using CTRL-T with 'expandtab' and 'preserveindent'
  call cursor(1, 1)
  call setline(1, "\t    \t")
  set shiftwidth=4 expandtab preserveindent
  call feedkeys("Al\<C-T>", 'xt')
  call assert_equal("\t    \t    l", getline(1))
  set sw& et& pi&

  close!
endfunc

" Test for indent()
func Test_indent_func()
  call assert_equal(-1, indent(-1))
  new
  call setline(1, "\tabc")
  call assert_equal(8, indent(1))
  call setline(1, "    abc")
  call assert_equal(4, indent(1))
  call setline(1, "    \t    abc")
  call assert_equal(12, indent(1))
  close!
endfunc

" Test for reindenting a line using the '=' operator
func Test_reindent()
  new
  call setline(1, 'abc')
  set nomodifiable
  call assert_fails('normal ==', 'E21:')
  set modifiable

  call setline(1, ['foo', 'bar'])
  call feedkeys('ggVG=', 'xt')
  call assert_equal(['foo', 'bar'], getline(1, 2))
  close!
endfunc

" Test for shifting a line with a preprocessor directive ('#')
func Test_preproc_indent()
  new
  set sw=4
  call setline(1, '#define FOO 1')
  normal >>
  call assert_equal('    #define FOO 1', getline(1))

  " with 'smartindent'
  call setline(1, '#define FOO 1')
  set smartindent
  normal >>
  call assert_equal('#define FOO 1', getline(1))
  set smartindent&

  " with 'cindent'
  set cindent
  normal >>
  call assert_equal('#define FOO 1', getline(1))
  set cindent&

  close!
endfunc

" Test for 'copyindent'
func Test_copyindent()
  new
  set shiftwidth=4 autoindent expandtab copyindent
  call setline(1, "    \t    abc")
  call feedkeys("ol", 'xt')
  call assert_equal("    \t    l", getline(2))
  set noexpandtab
  call setline(1, "    \t    abc")
  call feedkeys("ol", 'xt')
  call assert_equal("    \t    l", getline(2))
  set sw& ai& et& ci&
  close!
endfunc

" Test for changing multiple lines with lisp indent
func Test_lisp_indent_change_multiline()
  new
  setlocal lisp autoindent
  call setline(1, ['(if a', '  (if b', '    (return 5)))'])
  normal! jc2j(return 4))
  call assert_equal('  (return 4))', getline(2))
  close!
endfunc

func Test_lisp_indent()
  new
  setlocal lisp autoindent
  call setline(1, ['(if a', '  ;; comment', '  \ abc', '', '  " str1\', '  " st\b', '  (return 5)'])
  normal! jo;; comment
  normal! jo\ abc
  normal! jo;; ret
  normal! jostr1"
  normal! jostr2"
  call assert_equal(['  ;; comment', '  ;; comment', '  \ abc', '  \ abc', '', '  ;; ret', '  " str1\', '  str1"', '  " st\b', '  str2"'], getline(2, 11))
  close!
endfunc

" Test for setting the 'indentexpr' from a modeline
func Test_modeline_indent_expr()
  let modeline = &modeline
  set modeline
  func GetIndent()
    return line('.') * 2
  endfunc
  call writefile(['# vim: indentexpr=GetIndent()'], 'Xfile.txt')
  set modelineexpr
  new Xfile.txt
  call assert_equal('GetIndent()', &indentexpr)
  exe "normal Oa\nb\n"
  call assert_equal(['  a', '    b'], getline(1, 2))

  set modelineexpr&
  delfunc GetIndent
  let &modeline = modeline
  close!
  call delete('Xfile.txt')
endfunc

" vim: shiftwidth=2 sts=2 expandtab
