" Test filter() and map()

" list with expression string
func Test_filter_map_list_expr_string()
  " filter()
  call assert_equal([2, 3, 4], filter([1, 2, 3, 4], 'v:val > 1'))
  call assert_equal([3, 4], filter([1, 2, 3, 4], 'v:key > 1'))
  call assert_equal([], filter([1, 2, 3, 4], 0))

  " map()
  call assert_equal([2, 4, 6, 8], map([1, 2, 3, 4], 'v:val * 2'))
  call assert_equal([0, 2, 4, 6], map([1, 2, 3, 4], 'v:key * 2'))
  call assert_equal([9, 9, 9, 9], map([1, 2, 3, 4], 9))
  call assert_equal([7, 7, 7], map([1, 2, 3], ' 7 '))
endfunc

" dict with expression string
func Test_filter_map_dict_expr_string()
  let dict = {"foo": 1, "bar": 2, "baz": 3}

  " filter()
  call assert_equal({"bar": 2, "baz": 3}, filter(copy(dict), 'v:val > 1'))
  call assert_equal({"foo": 1, "baz": 3}, filter(copy(dict), 'v:key > "bar"'))
  call assert_equal({}, filter(copy(dict), 0))

  " map()
  call assert_equal({"foo": 2, "bar": 4, "baz": 6}, map(copy(dict), 'v:val * 2'))
  call assert_equal({"foo": "f", "bar": "b", "baz": "b"}, map(copy(dict), 'v:key[0]'))
  call assert_equal({"foo": 9, "bar": 9, "baz": 9}, map(copy(dict), 9))
endfunc

" list with funcref
func Test_filter_map_list_expr_funcref()
  " filter()
  func! s:filter1(index, val) abort
    return a:val > 1
  endfunc
  call assert_equal([2, 3, 4], filter([1, 2, 3, 4], function('s:filter1')))

  func! s:filter2(index, val) abort
    return a:index > 1
  endfunc
  call assert_equal([3, 4], filter([1, 2, 3, 4], function('s:filter2')))

  " map()
  func! s:filter3(index, val) abort
    return a:val * 2
  endfunc
  call assert_equal([2, 4, 6, 8], map([1, 2, 3, 4], function('s:filter3')))

  func! s:filter4(index, val) abort
    return a:index * 2
  endfunc
  call assert_equal([0, 2, 4, 6], map([1, 2, 3, 4], function('s:filter4')))
endfunc

func Test_filter_map_nested()
  let x = {"x":10}
  let r = map(range(2), 'filter(copy(x), "1")')
  call assert_equal([x, x], r)

  let r = map(copy(x), 'filter(copy(x), "1")')
  call assert_equal({"x": x}, r)
endfunc

" dict with funcref
func Test_filter_map_dict_expr_funcref()
  let dict = {"foo": 1, "bar": 2, "baz": 3}

  " filter()
  func! s:filter1(key, val) abort
    return a:val > 1
  endfunc
  call assert_equal({"bar": 2, "baz": 3}, filter(copy(dict), function('s:filter1')))

  func! s:filter2(key, val) abort
    return a:key > "bar"
  endfunc
  call assert_equal({"foo": 1, "baz": 3}, filter(copy(dict), function('s:filter2')))

  " map()
  func! s:filter3(key, val) abort
    return a:val * 2
  endfunc
  call assert_equal({"foo": 2, "bar": 4, "baz": 6}, map(copy(dict), function('s:filter3')))

  func! s:filter4(key, val) abort
    return a:key[0]
  endfunc
  call assert_equal({"foo": "f", "bar": "b", "baz": "b"}, map(copy(dict), function('s:filter4')))
endfunc

func Test_map_filter_fails()
  call assert_fails('call map([1], "42 +")', 'E15:')
  call assert_fails('call filter([1], "42 +")', 'E15:')
  call assert_fails("let l = map('abc', '\"> \" . v:val')", 'E896:')
  call assert_fails("let l = filter('abc', '\"> \" . v:val')", 'E896:')
  call assert_fails("let l = filter([1, 2, 3], '{}')", 'E728:')
  call assert_fails("let l = filter({'k' : 10}, '{}')", 'E728:')
  call assert_fails("let l = filter([1, 2], {})", 'E731:')
  call assert_equal(test_null_list(), filter(test_null_list(), 0))
  call assert_equal(test_null_dict(), filter(test_null_dict(), 0))
  call assert_equal(test_null_list(), map(test_null_list(), '"> " .. v:val'))
  call assert_equal(test_null_dict(), map(test_null_dict(), '"> " .. v:val'))
  call assert_equal([1, 2, 3], filter([1, 2, 3], test_null_function()))
  call assert_fails("let l = filter([1, 2], function('min'))", 'E118:')
  call assert_equal([1, 2, 3], filter([1, 2, 3], test_null_partial()))
  call assert_fails("let l = filter([1, 2], {a, b, c -> 1})", 'E119:')
endfunc

func Test_map_and_modify()
  let l = ["abc"]
  " cannot change the list halfway a map()
  call assert_fails('call map(l, "remove(l, 0)[0]")', 'E741:')

  let d = #{a: 1, b: 2, c: 3}
  call assert_fails('call map(d, "remove(d, v:key)[0]")', 'E741:')
  call assert_fails('echo map(d, {k,v -> remove(d, k)})', 'E741:')
endfunc

func Test_mapnew_dict()
  let din = #{one: 1, two: 2}
  let dout = mapnew(din, {k, v -> string(v)})
  call assert_equal(#{one: 1, two: 2}, din)
  call assert_equal(#{one: '1', two: '2'}, dout)

  const dconst = #{one: 1, two: 2, three: 3}
  call assert_equal(#{one: 2, two: 3, three: 4}, mapnew(dconst, {_, v -> v + 1}))
endfunc

func Test_mapnew_list()
  let lin = [1, 2, 3]
  let lout = mapnew(lin, {k, v -> string(v)})
  call assert_equal([1, 2, 3], lin)
  call assert_equal(['1', '2', '3'], lout)

  const lconst = [1, 2, 3]
  call assert_equal([2, 3, 4], mapnew(lconst, {_, v -> v + 1}))
endfunc

func Test_mapnew_blob()
  let bin = 0z123456
  let bout = mapnew(bin, {k, v -> k == 1 ? 0x99 : v})
  call assert_equal(0z123456, bin)
  call assert_equal(0z129956, bout)
endfunc

" vim: shiftwidth=2 sts=2 expandtab
