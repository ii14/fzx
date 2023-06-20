local uv = vim.loop
local api = vim.api
local ns = api.nvim_create_namespace('fzx')

local index = {}

function index:redraw()
  local height = self:win_height()
  local res = self.fzx:get_results(height, self.offset)
  self.offset = res.offset

  local lines = {}
  for _, item in ipairs(res.items) do
    table.insert(lines, item.text)
    item.positions = require('fzxlib').match_positions(self.query, item.text).positions
  end

  api.nvim_buf_set_lines(self.display_buf, 0, -1, false, lines)
  self.pending = false

  api.nvim_buf_clear_namespace(self.prompt_buf, ns, 0, -1)
  api.nvim_buf_set_extmark(self.prompt_buf, ns, 0, 0, {
    virt_text = {{ ('%d/%d'):format(res.matched, res.total), 'Comment' }},
    virt_text_pos = 'right_align',
  })

  api.nvim_buf_clear_namespace(self.display_buf, ns, 0, -1)
  for lnum, item in ipairs(res.items) do
    for _, pos in ipairs(item.positions) do
      api.nvim_buf_set_extmark(self.display_buf, ns, lnum - 1, pos, {
        end_row = lnum - 1,
        end_col = pos + 1,
        hl_group = 'Search',
      })
    end
  end
end

function index:close()
  self.fzx:stop()
  self.poll:close()
end

function index:win_height()
  return api.nvim_win_get_height(self.display_win)
end

local mt = { __index = index }

local function create_buf()
  local bufnr = api.nvim_create_buf(false, true)
  vim.bo[bufnr].buftype = 'nofile'
  vim.bo[bufnr].undolevels = -1
  vim.bo[bufnr].undofile = false
  vim.bo[bufnr].swapfile = false
  vim.bo[bufnr].textwidth = 0
  return bufnr
end

local function new()
  local self = setmetatable({}, mt)

  self.fzx = require('fzxlib').new()
  self.poll = assert(uv.new_poll(self.fzx:get_fd()))

  self.pending = false
  self.offset = 0
  self.query = ''

  self.display_buf = create_buf()
  self.prompt_buf = create_buf()

  local height = math.floor(vim.o.lines / 2)

  self.display_win = api.nvim_open_win(self.display_buf, false, {
    relative = 'editor',
    width = 99999,
    height = height,
    row = 0,
    col = 0,
    style = 'minimal',
  })

  self.prompt_win = api.nvim_open_win(self.prompt_buf, true, {
    relative = 'editor',
    width = 99999,
    height = 1,
    row = height,
    col = 0,
    style = 'minimal',
  })

  vim.wo[self.display_win].winhighlight = 'Normal:Normal,CursorLine:Normal'
  vim.wo[self.prompt_win].winhighlight = 'Normal:CursorLine,CursorLine:CursorLine'
  vim.cmd('startinsert')

  api.nvim_create_autocmd({ 'TextChanged', 'TextChangedI', 'TextChangedP' }, {
    buffer = self.prompt_buf,
    callback = function()
      local line = api.nvim_buf_get_lines(self.prompt_buf, 0, 1, false)[1]
      self.query = line
      self.fzx:set_query(line)
    end,
  })

  vim.keymap.set({ 'i', 'n' }, '<Down>', function()
    self.offset = self.offset + 1
    self:redraw()
  end, { buffer = self.prompt_buf })
  vim.keymap.set({ 'i', 'n' }, '<Up>', function()
    self.offset = self.offset - 1
    self:redraw()
  end, { buffer = self.prompt_buf })
  vim.keymap.set({ 'i', 'n' }, '<Home>', function()
    self.offset = 0
    self:redraw()
  end, { buffer = self.prompt_buf })
  vim.keymap.set({ 'i', 'n' }, '<End>', function()
    self.offset = require('fzxlib').MAX_OFFSET
    self:redraw()
  end, { buffer = self.prompt_buf })
  vim.keymap.set({ 'i', 'n' }, '<PageDown>', function()
    self.offset = self.offset + self:win_height() / 2
    self:redraw()
  end, { buffer = self.prompt_buf })
  vim.keymap.set({ 'i', 'n' }, '<PageUp>', function()
    self.offset = self.offset - self:win_height() / 2
    self:redraw()
  end, { buffer = self.prompt_buf })

  self.poll:start('r', function(err)
    assert(not err, err)
    if self.fzx:load_results() and not self.pending then
      self.pending = true
      vim.schedule(function()
        self:redraw()
      end)
    end
  end)

  api.nvim_buf_call(self.display_buf, function()
    vim.cmd([[
      syn match fzxGrepFile     "^[^:]*"  nextgroup=fzxGrepFileSep
      syn match fzxGrepFileSep  ":"       nextgroup=fzxGrepLine contained
      syn match fzxGrepLine     "[^:]*"   nextgroup=fzxGrepLineSep contained
      syn match fzxGrepLineSep  ":"       nextgroup=fzxGrepCol contained
      syn match fzxGrepCol      "[^:]*"   nextgroup=fzxGrepColSep contained
      syn match fzxGrepColSep   ":"       contained
      hi def link fzxGrepFile     Directory
      hi def link fzxGrepLine     String
      hi def link fzxGrepCol      String
      hi def link fzxGrepFileSep  Comment
      hi def link fzxGrepLineSep  Comment
      hi def link fzxGrepColSep   Comment
    ]])
  end)

  self.fzx:start()

  do
    local stdout = uv.new_pipe()
    local proc
    proc = uv.spawn('/bin/sh', {
      args = { '-c', 'rg --line-number --column ""' },
      stdio = { nil, stdout, nil }
    }, function()
      self.fzx:scan_end()
      proc:close()
    end)
    stdout:read_start(function(err, data)
      assert(not err, err)
      if data then
        self.fzx:scan_feed(data)
      else
        self.fzx:scan_end()
        stdout:close()
      end
    end)
  end
end

return { new = new }
