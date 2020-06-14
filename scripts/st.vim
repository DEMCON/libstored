" Vim syntax file
" Language: libstored
"
" Copy or link this file to $HOME/.vim/syntax/st.vim

if exists("b:current_syntax")
  finish
endif

let b:current_syntax = "st"

syn keyword stTypes contained bool int8 uint8 int16 uint16 int32 uint32 int64 uint64 float double string blob
hi def link stTypes Keyword

syn match stFloat contained '\([-+]\?[0-9]*\.[0-9]\([eE][-+]\?[0-9]\+\)\?\|[Nn]a[Nn]\|[-+]\?[Ii]nf\(inity\)\?\)\>'
hi def link stFloat Float

syn match stInt contained '\<\([0-9]\+\|0x[0-9A-Fa-f]\+\)\>'
hi def link stInt Number

syn keyword stBool contained true false
hi def link stBool Boolean

syn match stFullType '^\s*(\?\<[a-z0-9]\+\>\(:[0-9A-Fa-fx]\+\)\?)\?\(\[[0-9A-Fa-fx]\+\]\)\?\(=[^[:space:]]\+\)\?' contains=stTypes,stFloat,stInt,stBool nextgroup=stName skipnl
hi def link stFullType Type

syn match stName contained '.*' contains=stComment
"hi def link stName Identifier

syn region stScope start='{' end='}' nextgroup=stRegionType fold transparent contains=stFullType,stScope,stComment
"hi def link stScope Type

syn match stRegionType contained '\(\[[0-9A-Fa-fx]\+\]\)\?' contains=stInt nextgroup=stName skipnl
hi def link stRegionType Type

syn keyword stTodo contained TODO FIXME XXX NOTE
hi def link stTodo Todo

syn match stComment "//.*$" contains=stTodo
hi def link stComment Comment

