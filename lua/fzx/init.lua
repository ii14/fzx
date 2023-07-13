local api = vim.api
local uv = vim.loop

local registry = setmetatable({}, { __mode = 'k' })
local resize_autocmd

local ns = api.nvim_create_namespace('fzx')
local ns2 = api.nvim_create_namespace('fzx_cursor')

local function clamp(n, lower, upper)
  if n < lower then
    return lower
  elseif n > upper then
    return upper
  else
    return n
  end
end

local function reverse(t)
  local len = #t
  for i = 0, len / 2 - 1 do
    t[i+1], t[len-i] = t[len-i], t[i+1]
  end
  return t
end

local mt = { __index = {} }

function mt.__index:close()
  if self._fzx and not self._fzx:is_nil() then
    self._fzx:stop()
    self._fzx = nil
  end
  if self._poll and self._poll:is_active() and not self._poll:is_closing() then
    self._poll:close()
    self._poll = nil
  end
  if self._ui and self._ui:is_valid() then
    self._ui:close()
    self._ui = nil
  end
end

function mt.__index:redraw_prompt(res)
  if not self._fzx or not self._ui then
    return
  end

  if not res then
    res = self._fzx:get_results(0, 0)
  end

  self._processing = res.processing
  api.nvim_buf_clear_namespace(self._ui._ibuf, ns, 0, -1)
  local status
  if self._processing then
    status = ('processing... %d/%d '):format(res.matched, res.total)
  else
    status = ('%d/%d '):format(res.matched, res.total)
  end
  api.nvim_buf_set_extmark(self._ui._ibuf, ns, 0, 0, {
    virt_text = {{ status, 'Constant' }},
    virt_text_pos = 'right_align',
  })
end

function mt.__index:redraw_display()
  self._pending = false

  if not self._fzx or not self._ui then
    return
  end

  local height = self._ui:get_preferred_height()
  local res = self._fzx:get_results(height, self._offset)
  reverse(res.items)
  self._results = res
  self._offset = res.offset
  self._processing = res.processing

  self:redraw_prompt(res)

  if res.matched == 0 then
    self._ui:set_height(1)
    api.nvim_buf_set_lines(self._ui._dbuf, 0, -1, false, { '-- No results --' })
    api.nvim_buf_clear_namespace(self._ui._dbuf, ns, 0, -1)
    api.nvim_buf_set_extmark(self._ui._dbuf, ns, 0, 0, {
      end_row = 1,
      end_col = 0,
      hl_group = 'ErrorMsg',
      priority = 100,
    })
    return
  end

  self._ui:set_height(res.matched)

  local lines = {}
  for _, item in ipairs(res.items) do
    table.insert(lines, item.text)
    item.positions = require('fzxlib').match_positions(self._query, item.text).positions
  end

  api.nvim_buf_set_lines(self._ui._dbuf, 0, -1, false, lines)
  api.nvim_buf_clear_namespace(self._ui._dbuf, ns, 0, -1)
  for lnum, item in ipairs(res.items) do
    for _, pos in ipairs(item.positions) do
      api.nvim_buf_set_extmark(self._ui._dbuf, ns, lnum - 1, pos, {
        end_row = lnum - 1,
        end_col = pos + 1,
        hl_group = 'Search',
        priority = 150,
      })
    end
  end

  self:update_cursor()
end

function mt.__index:scroll_relative(n)
  self._offset = math.floor(clamp(self._offset + n, 0, require('fzxlib').MAX_OFFSET))
  self:redraw_display()
end

function mt.__index:scroll_absolute(n)
  self._offset = math.floor(clamp(n, 0, require('fzxlib').MAX_OFFSET))
  self:redraw_display()
end

function mt.__index:update_cursor()
  if not self._fzx or not self._ui then
    return
  end

  local height = api.nvim_buf_line_count(self._ui._dbuf)
  api.nvim_buf_clear_namespace(self._ui._dbuf, ns2, 0, -1)
  self._cursor = clamp(self._cursor, 0, height - 1)
  local pos = height - self._cursor - 1
  api.nvim_buf_set_extmark(self._ui._dbuf, ns2, pos, 0, {
    end_row = pos + 1,
    end_col = 0,
    hl_group = 'Visual',
    hl_eol = true,
    priority = 10,
  })
end

local function new(opts)
  opts = opts or {}
  assert(type(opts) == 'table', 'opts has to be a table')
  assert(opts.prompt == nil or (type(opts.prompt) == 'string' and #opts.prompt > 0),
    'opts.prompt has to be a non empty string')
  assert(opts.on_select == nil or type(opts.on_select) == 'function',
    'opts.on_select has to be a function')

  local self = setmetatable({}, mt)
  self._fzx = require('fzxlib').new()
  self._poll = assert(uv.new_poll(self._fzx:get_fd()))
  self._pending = false
  self._processing = false
  self._offset = 0
  self._cursor = 0
  self._query = ''
  self._on_select = opts.on_select
  self._ui = require('fzx.ui').new({
    prompt = opts.prompt or 'fzx> ',
    on_close = function()
      self:close()
    end,
  })

  self._poll:start('r', function(err)
    assert(not err, err)
    local ok = self._fzx:load_results()
    if ok and not self._pending then
      self._pending = true
      vim.schedule(function()
        self:redraw_display()
      end)
    end
  end)

  self._fzx:start()

  api.nvim_create_autocmd({ 'TextChanged', 'TextChangedI', 'TextChangedP' }, {
    buffer = self._ui._ibuf,
    callback = function()
      local line = api.nvim_buf_get_lines(self._ui._ibuf, 0, 1, false)[1]
      self._processing = true
      self._query = line
      self._fzx:set_query(line)
      self:redraw_prompt()
    end,
  })

  registry[self] = true
  if resize_autocmd == nil then
    resize_autocmd = api.nvim_create_autocmd('VimResized', {
      nested = true,
      callback = function()
        for instance in pairs(registry) do
          if instance._ui and instance._ui:is_valid() then
            instance._ui:update_position()
            instance:redraw_display()
          end
        end
      end,
    })
  end

  -- TODO: scroll when cursor goes out of bounds and make scrolling move the cursor
  vim.keymap.set({ 'i', 'n' }, '<Up>', function()
    self._cursor = self._cursor + 1
    self:update_cursor()
  end, { buffer = self._ui._ibuf })
  vim.keymap.set({ 'i', 'n' }, '<C-P>', function()
    self._cursor = self._cursor + 1
    self:update_cursor()
  end, { buffer = self._ui._ibuf })
  vim.keymap.set({ 'i', 'n' }, '<Down>', function()
    self._cursor = self._cursor - 1
    self:update_cursor()
  end, { buffer = self._ui._ibuf })
  vim.keymap.set({ 'i', 'n' }, '<C-N>', function()
    self._cursor = self._cursor - 1
    self:update_cursor()
  end, { buffer = self._ui._ibuf })
  vim.keymap.set({ 'i', 'n' }, '<C-E>', function()
    self:scroll_relative(-1)
  end, { buffer = self._ui._ibuf })
  vim.keymap.set({ 'i', 'n' }, '<C-Y>', function()
    self:scroll_relative(1)
  end, { buffer = self._ui._ibuf })
  vim.keymap.set({ 'i', 'n' }, '<Home>', function()
    self:scroll_absolute(math.huge)
  end, { buffer = self._ui._ibuf })
  vim.keymap.set({ 'i', 'n' }, '<End>', function()
    self:scroll_absolute(0)
  end, { buffer = self._ui._ibuf })
  vim.keymap.set({ 'i', 'n' }, '<PageDown>', function()
    self:scroll_relative(-(self._ui:get_height() / 2))
  end, { buffer = self._ui._ibuf })
  vim.keymap.set({ 'i', 'n' }, '<PageUp>', function()
    self:scroll_relative(self._ui:get_height() / 2)
  end, { buffer = self._ui._ibuf })

  vim.keymap.set('n', 'q', function()
    self:close()
  end, { buffer = self._ui._ibuf, nowait = true })
  vim.keymap.set('n', '<Enter>', function()
    local items = self._results.items
    local item = items[#items - self._cursor]
    self:close()
    if item and self._on_select then
      self._on_select(item.text, item.index)
    end
  end, { buffer = self._ui._ibuf })
  vim.keymap.set('i', '<Enter>', function()
    local items = self._results.items
    local item = items[#items - self._cursor]
    self:close()
    vim.cmd('stopinsert')
    if item and self._on_select then
      -- :stopinsert interferes with cursor position
      vim.schedule(function()
        self._on_select(item.text, item.index)
      end)
    end
  end, { buffer = self._ui._ibuf })

  api.nvim_buf_call(self._ui._pbuf, function()
    vim.cmd([[
      syn match FzxPromptText "^.*"
      hi def link FzxPromptText Type
    ]])
  end)

  return self
end

return {
  new = new,
  registry = registry,
}
