function! SetupTDD()
  nnoremap <leader>w :topleft 5split<CR>:term ++curwin npm run test:watch<CR>
  nnoremap <leader>t :topleft 5split<CR>:term ++curwin npm run test<CR>

  tnoremap <Esc> <C-w>N

  command! TDDStatus echo "Modo TDD: ON"
endfunction

call SetupTDD()
