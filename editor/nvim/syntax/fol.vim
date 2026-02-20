if exists('b:current_syntax')
  finish
endif

" Comments
syn match folComment '#.*$'

" String literals (include paths)
syn region folString start='"' end='"' contained

" Include directive
syn match folInclude 'include' nextgroup=folString skipwhite

" Def annotation: @def(name)
syn match folDefAnnotation '@def(\w\+)'

" Top-level statement keywords
syn keyword folStatement axiom claim theorem
syn keyword folStatement proof

" Proof commands
syn keyword folCommand fix use assume let qed

" Inference rules
syn keyword folRule and_intro and_elim_l and_elim_r
syn keyword folRule or_intro_l or_intro_r or_elim
syn keyword folRule implies_intro implies_elim
syn keyword folRule not_intro not_elim bottom_elim
syn keyword folRule iff_intro iff_elim_l iff_elim_r
syn keyword folRule forall_intro forall_elim
syn keyword folRule exists_intro exists_elim
syn keyword folRule eq_subst double_neg_elim excluded_middle

" Quantifiers
syn keyword folQuantifier forall exists

" Logical connective keywords
syn keyword folConnective and or not implies iff

" Logical operators
syn match folOperator '&\||\|\~\|!\|->\|<->'

" Bottom / false
syn match folBottom '_|_'
syn keyword folBottom false bottom

" Step label assignment: name =
syn match folLabel '\<[a-zA-Z_][a-zA-Z0-9_]*\>\s*=' contains=folEquals
syn match folEquals '=' contained

" Predicate application: name(
syn match folPredicate '\<[a-zA-Z_][a-zA-Z0-9_]*\>\ze('

" Statement name after axiom/claim/proof keyword: "axiom name:"
syn match folName '\<\(axiom\|claim\|theorem\|proof\)\s\+\zs[a-zA-Z_][a-zA-Z0-9_]*\ze\s*:'

" Highlighting links
hi def link folComment    Comment
hi def link folString     String
hi def link folInclude    Include
hi def link folDefAnnotation PreProc
hi def link folStatement  Statement
hi def link folCommand    Keyword
hi def link folRule        Function
hi def link folQuantifier Repeat
hi def link folConnective Operator
hi def link folOperator   Operator
hi def link folBottom     Constant
hi def link folPredicate  Identifier
hi def link folName       Define
hi def link folLabel      Special
hi def link folEquals     Delimiter

let b:current_syntax = 'fol'
