local api = vim.api

local mt = { __index = {} }

function mt.__index:show()
  if not self.display or not self.prompt or not self.input then
    return
  end

  local max_height = vim.o.lines - vim.o.cmdheight
  local max_width = vim.o.columns
  local height = math.floor(math.min(self._height, max_height / 2))
  if height < 1 then height = 1 end
  local prompt_len = #self._prompt
  self._preferred_height = math.floor(max_height / 2)

  self.display:show({
    width = max_width,
    height = height,
    row = max_height - height - 1,
    col = 0,
  })

  self.prompt:show({
    width = prompt_len,
    height = 1,
    row = max_height - 1,
    col = 0,
  })

  if self.input:show({
    width = max_width - prompt_len,
    height = 1,
    row = max_height - 1,
    col = prompt_len,
    enter = true,
  }) then
    vim.cmd('startinsert')
  end

  if self._on_show then
    self._on_show(self)
  end
end

function mt.__index:hide()
  if not self.display or not self.prompt or not self.input then
    return
  end

  self.display:hide()
  self.prompt:hide()
  self.input:hide()

  if self._on_hide then
    self._on_hide(self)
  end
end

function mt.__index:destroy()
  if self.display then
    self.display:destroy()
    self.display = nil
  end
  if self.prompt then
    self.prompt:destroy()
    self.prompt = nil
  end
  if self.input then
    self.input:destroy()
    self.input = nil
  end

  if self._on_destroy then
    local cb = self._on_destroy
    self._on_destroy = nil
    cb(self)
  end
end

function mt.__index:set_height(height)
  assert(type(height) == 'number' and height > 0, 'height has to be a positive integer')
  self._height = height
  self:show()
end

function mt.__index:get_height()
  return assert(self.display)._height
end

function mt.__index:get_preferred_height()
  return assert(self._preferred_height)
end

local function new(opts)
  opts = opts or {}
  assert(type(opts) == 'table')
  assert(opts.prompt == nil or (type(opts.prompt) == 'string' and #opts.prompt > 1),
    'opts.prompt has to be a non empty string')
  assert(opts.on_show == nil or type(opts.on_show) == 'function')
  assert(opts.on_hide == nil or type(opts.on_hide) == 'function')
  assert(opts.on_destroy == nil or type(opts.on_destroy) == 'function')

  local self = setmetatable({}, mt)

  local new_window = require('fzx.window')

  self._prompt = opts.prompt or 'fzx> '
  self._height = 1
  self._on_show = opts.on_show
  self._on_hide = opts.on_hide
  self._on_destroy = opts.on_destroy

  local removing = false
  local function on_destroy()
    if not removing then
      removing = true
      vim.schedule(function()
        self:destroy()
      end)
    end
  end

  require('fzx.hl')

  self.display = new_window({
    focusable = true,
    on_show = function(w)
      vim.wo[w.win].winhighlight = 'Normal:FzxNormal,CursorLine:FzxCursorLine'
      vim.wo[w.win].winblend = 13
    end,
    on_destroy = on_destroy,
    on_hide = on_destroy,
  })
  vim.bo[self.display.buf].undolevels = -1
  api.nvim_buf_set_lines(self.display.buf, 0, -1, false, { '...' })

  local function on_show(w)
    vim.wo[w.win].winhighlight = 'Normal:FzxPrompt,CursorLine:FzxCursorLine'
  end

  self.prompt = new_window({
    focusable = false,
    on_show = on_show,
    on_destroy = on_destroy,
    on_hide = on_destroy,
  })
  vim.bo[self.prompt.buf].undolevels = -1
  api.nvim_buf_set_lines(self.prompt.buf, 0, -1, false, { self._prompt })

  self.input = new_window({
    focusable = true,
    on_show = on_show,
    on_destroy = on_destroy,
    on_hide = on_destroy,
  })

  self:show()

  return self
end

return new
