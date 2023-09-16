vim.api.nvim_create_user_command('Fzx', function(ctx)
  require('fzx.cmd').command(ctx)
end, {
  nargs = '*',
  complete = function(arg, cmdline, pos)
    return require('fzx.cmd').complete('Fzx', arg, cmdline, pos)
  end,
})
