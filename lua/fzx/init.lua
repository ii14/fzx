local uv = vim.loop
local api = vim.api
local ns = api.nvim_create_namespace('fzx')

local index = {}

function index:redraw_prompt(res)
  if not res then
    res = self.fzx:get_results(0, 0)
  end

  api.nvim_buf_clear_namespace(self.prompt_buf, ns, 0, -1)
  local status
  if self.processing then
    status = ('processing... %d/%d'):format(res.matched, res.total)
  else
    status = ('%d/%d'):format(res.matched, res.total)
  end
  api.nvim_buf_set_extmark(self.prompt_buf, ns, 0, 0, {
    virt_text = {{ status, 'Comment' }},
    virt_text_pos = 'right_align',
  })
end

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

  self:redraw_prompt(res)

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
  if self.fzx and not self.fzx:is_nil() then
    self.fzx:stop()
  end
  if self.poll and self.poll:is_active() and not self.poll:is_closing() then
    self.poll:close()
  end
end

function index:win_height()
  return api.nvim_win_get_height(self.display_win)
end

local function clamp(n, lower, upper)
  if n < lower then
    return lower
  elseif n > upper then
    return upper
  else
    return n
  end
end

function index:scroll_relative(n)
  self.offset = math.floor(clamp(self.offset + n, 0, require('fzxlib').MAX_OFFSET))
  self:redraw()
end

function index:scroll_absolute(n)
  self.offset = math.floor(clamp(n, 0, require('fzxlib').MAX_OFFSET))
  self:redraw()
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
  self.processing = false
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
      self.processing = true
      self.query = line
      self.fzx:set_query(line)
      self:redraw_prompt()
    end,
  })

  vim.bo[self.display_buf].bufhidden = 'wipe'
  vim.bo[self.prompt_buf].bufhidden = 'wipe'

  api.nvim_create_autocmd('BufWipeout', {
    callback = function()
      self:close()
      self.display_buf = nil
      if self.prompt_buf and api.nvim_buf_is_valid(self.prompt_buf) then
        api.nvim_buf_delete(self.prompt_buf, { force = true })
      end
    end,
    buffer = self.display_buf,
    nested = true,
  })
  api.nvim_create_autocmd('BufWipeout', {
    callback = function()
      self:close()
      self.prompt_buf = nil
      if self.display_buf and api.nvim_buf_is_valid(self.display_buf) then
        api.nvim_buf_delete(self.display_buf, { force = true })
      end
    end,
    buffer = self.prompt_buf,
    nested = true,
  })

  vim.keymap.set({ 'i', 'n' }, '<Down>', function()
    self:scroll_relative(1)
  end, { buffer = self.prompt_buf })
  vim.keymap.set({ 'i', 'n' }, '<Up>', function()
    self:scroll_relative(-1)
  end, { buffer = self.prompt_buf })
  vim.keymap.set({ 'i', 'n' }, '<Home>', function()
    self:scroll_absolute(0)
  end, { buffer = self.prompt_buf })
  vim.keymap.set({ 'i', 'n' }, '<End>', function()
    self:scroll_absolute(math.huge)
  end, { buffer = self.prompt_buf })
  vim.keymap.set({ 'i', 'n' }, '<PageDown>', function()
    self:scroll_relative(self:win_height() / 2)
  end, { buffer = self.prompt_buf })
  vim.keymap.set({ 'i', 'n' }, '<PageUp>', function()
    self:scroll_relative(-(self:win_height() / 2))
  end, { buffer = self.prompt_buf })

  -- just close on enter for now
  vim.keymap.set('n', '<Enter>', function()
    api.nvim_buf_delete(self.prompt_buf, { force = true })
  end, { buffer = self.prompt_buf })
  vim.keymap.set('i', '<Enter>', function()
    vim.cmd('stopinsert')
    api.nvim_buf_delete(self.prompt_buf, { force = true })
  end, { buffer = self.prompt_buf })

  self.poll:start('r', function(err)
    assert(not err, err)
    local ok, processing = self.fzx:load_results()
    if ok and not self.pending then
      self.pending = true
      self.processing = processing
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

  return self
end

return { new = new }
