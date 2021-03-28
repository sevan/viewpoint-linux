let SessionLoad = 1
let s:so_save = &so | let s:siso_save = &siso | set so=0 siso=0
let v:this_session=expand("<sfile>:p")
silent only
cd ~/Code/Tools/bc-git/bc/manuals
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  let s:wipebuf = bufnr('%')
endif
set shortmess=aoO
badd +0 ~/Code/Tools/bc-git/bc/include/program.h
badd +0 ~/Code/Tools/bc-git/bc/src/program.c
badd +0 ~/Code/Tools/bc-git/bc/include/num.h
badd +0 ~/Code/Tools/bc-git/bc/src/num.c
badd +0 ~/Code/Tools/bc-git/bc/include/vector.h
badd +70 ~/Code/Tools/bc-git/bc/src/parse.c
badd +0 ~/Code/Tools/bc-git/bc/src/vector.c
badd +0 ~/Code/Tools/bc-git/bc/include/vm.h
badd +0 ~/Code/Tools/bc-git/bc/src/vm.c
badd +0 ~/Code/Tools/bc-git/bc/include/lang.h
badd +0 ~/Code/Tools/bc-git/bc/src/lang.c
badd +0 ~/Code/Tools/bc-git/bc/include/rand.h
badd +0 ~/Code/Tools/bc-git/bc/src/rand/rand.
badd +0 ~/Code/Tools/bc-git/bc/src/rand/rand.c
badd +0 dc.1.ronn
badd +0 bc.1.ronn
badd +0 FAR\ 1
badd +0 FAR\ 2
badd +0 include/args.h
argglobal
%argdel
set stal=2
edit ~/Code/Tools/bc-git/bc/include/program.h
set splitbelow splitright
wincmd _ | wincmd |
vsplit
wincmd _ | wincmd |
vsplit
2wincmd h
wincmd w
wincmd w
wincmd t
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 127 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 126 + 191) / 382)
exe 'vert 3resize ' . ((&columns * 127 + 191) / 382)
argglobal
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
let s:l = 119 - ((74 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
119
normal! 063|
wincmd w
argglobal
if bufexists("~/Code/Tools/bc-git/bc/src/program.c") | buffer ~/Code/Tools/bc-git/bc/src/program.c | else | edit ~/Code/Tools/bc-git/bc/src/program.c | endif
setlocal fdm=syntax
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
308
normal! zo
374
normal! zo
722
normal! zo
1113
normal! zo
1159
normal! zo
let s:l = 1270 - ((57 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
1270
normal! 035|
wincmd w
argglobal
if bufexists("~/Code/Tools/bc-git/bc/src/program.c") | buffer ~/Code/Tools/bc-git/bc/src/program.c | else | edit ~/Code/Tools/bc-git/bc/src/program.c | endif
setlocal fdm=syntax
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
185
normal! zo
189
normal! zo
192
normal! zo
722
normal! zo
819
normal! zo
915
normal! zo
923
normal! zo
986
normal! zo
1113
normal! zo
1159
normal! zo
1213
normal! zo
1763
normal! zo
1785
normal! zo
1791
normal! zo
2047
normal! zo
let s:l = 1908 - ((54 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
1908
normal! 036|
wincmd w
exe 'vert 1resize ' . ((&columns * 127 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 126 + 191) / 382)
exe 'vert 3resize ' . ((&columns * 127 + 191) / 382)
tabedit ~/Code/Tools/bc-git/bc/include/rand.h
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
wincmd w
wincmd t
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 190 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 191 + 191) / 382)
argglobal
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
let s:l = 222 - ((8 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
222
normal! 037|
wincmd w
argglobal
if bufexists("~/Code/Tools/bc-git/bc/src/rand/rand.c") | buffer ~/Code/Tools/bc-git/bc/src/rand/rand.c | else | edit ~/Code/Tools/bc-git/bc/src/rand/rand.c | endif
setlocal fdm=syntax
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
let s:l = 399 - ((83 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
399
normal! 047|
wincmd w
exe 'vert 1resize ' . ((&columns * 190 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 191 + 191) / 382)
tabedit ~/Code/Tools/bc-git/bc/include/lang.h
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
wincmd w
wincmd t
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 190 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 191 + 191) / 382)
argglobal
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
let s:l = 317 - ((91 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
317
normal! 034|
wincmd w
argglobal
if bufexists("~/Code/Tools/bc-git/bc/src/lang.c") | buffer ~/Code/Tools/bc-git/bc/src/lang.c | else | edit ~/Code/Tools/bc-git/bc/src/lang.c | endif
setlocal fdm=syntax
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
185
normal! zo
200
normal! zo
216
normal! zo
224
normal! zo
let s:l = 220 - ((35 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
220
normal! 025|
wincmd w
exe 'vert 1resize ' . ((&columns * 190 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 191 + 191) / 382)
tabedit ~/Code/Tools/bc-git/bc/include/num.h
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
wincmd w
wincmd t
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 190 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 191 + 191) / 382)
argglobal
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
let s:l = 165 - ((44 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
165
normal! 01|
wincmd w
argglobal
if bufexists("~/Code/Tools/bc-git/bc/src/num.c") | buffer ~/Code/Tools/bc-git/bc/src/num.c | else | edit ~/Code/Tools/bc-git/bc/src/num.c | endif
setlocal fdm=syntax
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
let s:l = 2043 - ((103 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
2043
normal! 025|
wincmd w
exe 'vert 1resize ' . ((&columns * 190 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 191 + 191) / 382)
tabedit ~/Code/Tools/bc-git/bc/include/vector.h
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
wincmd w
wincmd t
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 190 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 191 + 191) / 382)
argglobal
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
let s:l = 67 - ((66 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
67
normal! 046|
wincmd w
argglobal
if bufexists("~/Code/Tools/bc-git/bc/src/vector.c") | buffer ~/Code/Tools/bc-git/bc/src/vector.c | else | edit ~/Code/Tools/bc-git/bc/src/vector.c | endif
setlocal fdm=syntax
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
130
normal! zo
148
normal! zo
165
normal! zo
257
normal! zo
let s:l = 127 - ((81 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
127
normal! 024|
wincmd w
exe 'vert 1resize ' . ((&columns * 190 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 191 + 191) / 382)
tabedit ~/Code/Tools/bc-git/bc/include/vm.h
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
wincmd w
wincmd t
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 190 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 191 + 191) / 382)
argglobal
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
let s:l = 183 - ((103 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
183
normal! 0
wincmd w
argglobal
if bufexists("~/Code/Tools/bc-git/bc/src/vm.c") | buffer ~/Code/Tools/bc-git/bc/src/vm.c | else | edit ~/Code/Tools/bc-git/bc/src/vm.c | endif
setlocal fdm=syntax
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
67
normal! zo
118
normal! zo
538
normal! zo
562
normal! zo
566
normal! zo
580
normal! zo
let s:l = 553 - ((11 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
553
normal! 0
wincmd w
exe 'vert 1resize ' . ((&columns * 190 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 191 + 191) / 382)
tabedit dc.1.ronn
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
wincmd w
wincmd t
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 190 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 191 + 191) / 382)
argglobal
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
let s:l = 78 - ((74 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
78
normal! 0
wincmd w
argglobal
if bufexists("bc.1.ronn") | buffer bc.1.ronn | else | edit bc.1.ronn | endif
setlocal fdm=indent
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=99
setlocal fml=3
setlocal fdn=20
setlocal fen
let s:l = 75 - ((5 * winheight(0) + 54) / 109)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
75
normal! 05|
wincmd w
exe 'vert 1resize ' . ((&columns * 190 + 191) / 382)
exe 'vert 2resize ' . ((&columns * 191 + 191) / 382)
tabnext 7
set stal=1
if exists('s:wipebuf') && getbufvar(s:wipebuf, '&buftype') isnot# 'terminal'
  silent exe 'bwipe ' . s:wipebuf
endif
unlet! s:wipebuf
set winheight=1 winwidth=20 winminheight=1 winminwidth=1 shortmess=filnxtToOFc
let s:sx = expand("<sfile>:p:r")."x.vim"
if file_readable(s:sx)
  exe "source " . fnameescape(s:sx)
endif
let &so = s:so_save | let &siso = s:siso_save
doautoall SessionLoadPost
unlet SessionLoad
" vim: set ft=vim :
