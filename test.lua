local uv = vim.loop
local api = vim.api
local fzx = require('fzxlib')
local f = fzx.new()

vim.cmd('enew')
local buf = api.nvim_get_current_buf()
vim.bo.buftype = 'nofile'
vim.bo.undolevels = -1
vim.bo.undofile = false
vim.bo.swapfile = false
vim.bo.textwidth = 0
vim.opt_local.number = false
vim.opt_local.relativenumber = false
local win = api.nvim_get_current_win()
local function win_height()
  return api.nvim_win_get_height(win)
end

vim.cmd('new')
vim.cmd('resize 1')
local prompt = api.nvim_get_current_buf()
vim.bo.buftype = 'nofile'
vim.bo.undolevels = -1
vim.bo.undofile = false
vim.bo.swapfile = false
vim.bo.textwidth = 0
vim.opt_local.number = false
vim.opt_local.relativenumber = false

local ns = api.nvim_create_namespace('fzx')

local query = ''
local offset = 0

local pending = false
local function redraw()
  local res = f:get_results(win_height() - 1, offset)
  local lines = {}
  offset = res.offset
  table.insert(lines, ('%d/%d'):format(res.matched, res.total))
  for _, item in ipairs(res.items) do
    table.insert(lines, item.text)
    item.positions = fzx.match_positions(query, item.text).positions
  end

  api.nvim_buf_set_lines(buf, 0, -1, false, lines)
  pending = false

  api.nvim_buf_clear_namespace(buf, ns, 0, -1)
  for lnum, item in ipairs(res.items) do
    for _, pos in ipairs(item.positions) do
      api.nvim_buf_set_extmark(buf, ns, lnum, pos, {
        end_row = lnum,
        end_col = pos + 1,
        hl_group = 'Search',
      })
    end
  end
end

local poll = assert(uv.new_poll(f:get_fd()))
poll:start('r', function(err)
  assert(not err, err)
  if f:load_results() and not pending then
    pending = true
    vim.schedule(redraw)
  end
end)

f:start()

api.nvim_create_autocmd({ 'TextChanged', 'TextChangedI', 'TextChangedP' }, {
  buffer = prompt,
  callback = function()
    local line = api.nvim_buf_get_lines(prompt, 0, 1, false)[1]
    query = line
    f:set_query(line)
  end,
})
vim.cmd('startinsert')

do
  local stdout = uv.new_pipe()
  local proc
  proc = uv.spawn('/bin/sh', {
    args = { '-c', 'cd ..; rg ""' },
    stdio = { nil, stdout, nil }
  }, function()
    f:scan_end()
    proc:close()
  end)
  stdout:read_start(function(err, data)
    assert(not err, err)
    if data then
      f:scan_feed(data)
    else
      f:scan_end()
      stdout:close()
      print('end')
    end
  end)
end

-- local lines = vim.fn.readfile(vim.env.HOME .. '/repos/neovim/src/nvim/main.c')
-- if false then
--   coroutine.resume(coroutine.create(function()
--     local co = coroutine.running()
--     for _, line in ipairs(lines) do
--       f:push(line)
--       f:commit()
--       vim.defer_fn(function()
--         coroutine.resume(co)
--       end, 100)
--       coroutine.yield()
--     end
--   end))
-- else
--   f:push(lines)
--   f:commit()
-- end


vim.keymap.set({ 'i', 'n' }, '<Down>', function()
  offset = offset + 1
  redraw()
end, { buffer = prompt })
vim.keymap.set({ 'i', 'n' }, '<Up>', function()
  offset = offset - 1
  redraw()
end, { buffer = prompt })
vim.keymap.set({ 'i', 'n' }, '<Home>', function()
  offset = 0
  redraw()
end, { buffer = prompt })
vim.keymap.set({ 'i', 'n' }, '<End>', function()
  offset = fzx.MAX_OFFSET
  redraw()
end, { buffer = prompt })
vim.keymap.set({ 'i', 'n' }, '<PageDown>', function()
  offset = offset + win_height() / 2
  redraw()
end, { buffer = prompt })
vim.keymap.set({ 'i', 'n' }, '<PageUp>', function()
  offset = offset - win_height() / 2
  redraw()
end, { buffer = prompt })
