vim.api.nvim_create_user_command('Fzx', function(ctx)
  local fzx = require('fzx').new()

  local stdout = vim.loop.new_pipe()
  local proc
  proc = vim.loop.spawn('rg', {
    args = { '--line-number', '--column', ctx.args },
    stdio = { nil, stdout, nil }
  }, function()
    fzx.fzx:scan_end()
    proc:close()
  end)

  stdout:read_start(function(err, data)
    assert(not err, err)
    if data then
      fzx.fzx:scan_feed(data)
    else
      fzx.fzx:scan_end()
      stdout:close()
    end
  end)
end, { nargs = '*' })
