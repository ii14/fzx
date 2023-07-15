local api = vim.api

local mt = { __index = {} }

function mt.__index:show(opts)
  assert(type(opts) == 'table')
  assert(type(opts.width) == 'number')
  assert(type(opts.height) == 'number')
  assert(type(opts.row) == 'number')
  assert(type(opts.col) == 'number')

  if opts.width < 1 then opts.width = 1 end
  if opts.height < 1 then opts.height = 1 end

  local changed = false
  if self._width ~= opts.width or self._height ~= opts.height or
    self._row ~= opts.row or self._col ~= opts.col then
    changed = true
    self._width = opts.width
    self._height = opts.height
    self._row = opts.row
    self._col = opts.col
  end

  if not self.win or not api.nvim_win_is_valid(self.win) then
    if not self.buf or not api.nvim_buf_is_valid(self.buf) then
      return
    end

    self.win = api.nvim_open_win(self.buf, opts.enter and true or false, {
      relative = 'editor',
      width = self._width,
      height = self._height,
      row = self._row,
      col = self._col,
      focusable = self._focusable,
      zindex = self._zindex,
      style = 'minimal',
    })

    if self._on_show then
      self._on_show(self)
    end

    if self._hide_autocmd then
      pcall(api.nvim_del_autocmd, self._hide_autocmd)
    end
    self._hide_autocmd = api.nvim_create_autocmd('WinClosed', {
      desc = 'fzx: window hide callback',
      callback = function()
        self.win = nil
        if self._on_hide then
          self._on_hide(self)
        end
      end,
      pattern = tostring(self.win),
      nested = true,
    })

    return true
  elseif changed then
    api.nvim_win_set_config(self.win, {
      relative = 'editor',
      width = self._width,
      height = self._height,
      row = self._row,
      col = self._col,
    })
  end

  return false
end

function mt.__index:hide()
  if self._hide_autocmd then
    pcall(api.nvim_del_autocmd, self._hide_autocmd)
    self._hide_autocmd = nil
  end
  if not self.win then
    return
  end
  if api.nvim_win_is_valid(self.win) then
    api.nvim_win_close(self.win, true)
  end
  self.win = nil
end

function mt.__index:destroy()
  self:hide()
  if not self.buf then
    return
  end
  if api.nvim_buf_is_valid(self.buf) then
    api.nvim_buf_delete(self.buf, { force = true })
  end
  self.buf = nil
end

local function new(opts)
  opts = opts or {}
  assert(type(opts) == 'table')
  assert(opts.zindex == nil or type(opts.zindex) == 'number')
  assert(opts.on_show == nil or type(opts.on_show) == 'function')
  assert(opts.on_hide == nil or type(opts.on_hide) == 'function')
  assert(opts.on_destroy == nil or type(opts.on_destroy) == 'function')

  local self = setmetatable({}, mt)
  self._focusable = opts.focusable and true or false
  self._zindex = opts.zindex
  self._on_show = opts.on_show
  self._on_hide = opts.on_hide
  self._on_destroy = opts.on_destroy

  self.buf = api.nvim_create_buf(false, true)
  vim.bo[self.buf].buftype = 'nofile'
  vim.bo[self.buf].undofile = false
  vim.bo[self.buf].swapfile = false
  vim.bo[self.buf].textwidth = 0

  api.nvim_create_autocmd('BufWipeout', {
    desc = 'fzx: window destroy callback',
    callback = function()
      self.buf = nil
      self:hide()
      if self._on_destroy then
        self._on_destroy(self)
      end
    end,
    buffer = self.buf,
    nested = true,
  })

  return self
end

return new
