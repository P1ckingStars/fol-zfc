setlocal commentstring=#\ %s
setlocal comments=:#
setlocal shiftwidth=4
setlocal softtabstop=4
setlocal expandtab

nnoremap <buffer> <silent> gd <cmd>lua require('fol-lsp.definition').goto_definition()<CR>
