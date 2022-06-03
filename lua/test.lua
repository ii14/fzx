local fzx = require 'fzx'

local set_lines = vim.api.nvim_buf_set_lines
vim.opt_local.buftype = 'nofile'
vim.opt_local.undolevels = -1
vim.opt_local.swapfile = false
vim.opt_local.undofile = false

local t
t = fzx.start(function(done, res)
  if not done then
    local lines = { tostring(res.total) .. ' processing...', }
    for _, item in ipairs(res.items) do
      lines[#lines+1] = item
    end
    set_lines(0, 0, -1, false, lines)
  else
    local lines = { tostring(res.total) .. ' done', }
    for _, item in ipairs(res.items) do
      lines[#lines+1] = item
    end
    set_lines(0, 0, -1, false, lines)
    fzx.stop(t)
  end
end)
