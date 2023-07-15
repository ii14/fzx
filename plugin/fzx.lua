local create_cmd = vim.api.nvim_create_user_command

create_cmd('Frg', function(ctx)
  require('fzx.builtin').rg(ctx)
end, { nargs = '*' })

create_cmd('Ffd', function()
  require('fzx.builtin').fd()
end, {})

create_cmd('Fhelp', function()
  require('fzx.builtin').help()
end, {})

create_cmd('Fbuffers', function()
  require('fzx.builtin').buffers()
end, {})
