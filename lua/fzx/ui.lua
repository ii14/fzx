local api = vim.api

local mt = { __index = {} }

local function create_buf()
  local buf = api.nvim_create_buf(false, true)
  vim.bo[buf].buftype = 'nofile'
  vim.bo[buf].undofile = false
  vim.bo[buf].swapfile = false
  vim.bo[buf].textwidth = 0
  return buf
end

function mt.__index:_calculate_win_config()
  local max_height = vim.o.lines - vim.o.cmdheight
  local max_width = vim.o.columns
  local height = math.floor(math.min(self._height, max_height / 2))
  if height < 1 then height = 1 end
  local prompt_len = #self._prompt
  self._preferred_height = math.floor(max_height / 2)

  local config = {
    dwin = {
      relative = 'editor',
      width = max_width,
      height = height,
      row = max_height - height - 1,
      col = 0,
    },
    pwin = {
      relative = 'editor',
      width = prompt_len,
      height = 1,
      row = max_height - 1,
      col = 0,
      focusable = false,
    },
    iwin = {
      relative = 'editor',
      width = max_width - prompt_len,
      height = 1,
      row = max_height - 1,
      col = prompt_len,
    },
  }

  if vim.deep_equal(self._config, config) then
    return config, false
  end
  self._config = config
  return config, true
end

-- recalculate and update window position
function mt.__index:update_position()
  assert(self:is_valid(), 'instance was deleted')
  local positions, updated = self:_calculate_win_config()
  if updated then
    api.nvim_win_set_config(self._dwin, positions.dwin)
    api.nvim_win_set_config(self._pwin, positions.pwin)
    api.nvim_win_set_config(self._iwin, positions.iwin)
  end
end

-- set preferred display window height
function mt.__index:set_height(height)
  assert(type(height) == 'number' and height > 0, 'height has to be a positive integer')
  assert(self:is_valid(), 'instance was deleted')
  self._height = height
  self:update_position()
end

-- get real display window height
function mt.__index:get_height()
  assert(self:is_valid(), 'instance was deleted')
  return self._config.dwin.height
end

function mt.__index:get_preferred_height()
  assert(self:is_valid(), 'instance was deleted')
  return self._preferred_height
end

-- set prompt text
function mt.__index:set_prompt(prompt)
  assert(type(prompt) == 'string' and #prompt > 0, 'prompt has to be a non empty string')
  assert(self:is_valid(), 'instance was deleted')
  self._prompt = prompt
  api.nvim_buf_set_lines(self._pbuf, 0, -1, false, { self._prompt })
  self:update_position()
end

local function buf_delete(bufnr)
  if bufnr and api.nvim_buf_is_valid(bufnr) then
    api.nvim_buf_delete(bufnr, { force = true })
  end
end

function mt.__index:close()
  buf_delete(self._dbuf)
  buf_delete(self._pbuf)
  buf_delete(self._ibuf)
end

function mt.__index:is_valid()
  return (self._dbuf and self._pbuf and self._ibuf) and true or false
end

local function new(opts)
  opts = opts or {}
  assert(type(opts) == 'table', 'opts has to be a table')
  assert(opts.on_close == nil or type(opts.on_close) == 'function',
    'opts.on_close has to be a function')
  assert(opts.prompt == nil or (type(opts.prompt) == 'string' and #opts.prompt > 1),
    'opts.prompt has to be a non empty string')

  local self = setmetatable({}, mt)

  self._on_close = opts.on_close
  self._prompt = opts.prompt or 'fzx> '
  self._height = 99999

  self._dbuf = create_buf() -- Display buffer
  self._pbuf = create_buf() -- Prompt buffer
  self._ibuf = create_buf() -- Input buffer
  vim.bo[self._dbuf].bufhidden = 'wipe'
  vim.bo[self._pbuf].bufhidden = 'wipe'
  vim.bo[self._ibuf].bufhidden = 'wipe'
  vim.bo[self._dbuf].undolevels = -1
  vim.bo[self._pbuf].undolevels = -1

  -- closing one window closes all of them
  for _, buf_var_name in ipairs({ '_dbuf', '_pbuf', '_ibuf' }) do
    api.nvim_create_autocmd('BufWipeout', {
      callback = function()
        self[buf_var_name] = nil
        self:close()
      end,
      buffer = self[buf_var_name],
      nested = true,
    })
  end

  local positions = self:_calculate_win_config()
  positions.dwin.style = 'minimal'
  positions.pwin.style = 'minimal'
  positions.iwin.style = 'minimal'
  self._dwin = api.nvim_open_win(self._dbuf, false, positions.dwin) -- Display window
  self._pwin = api.nvim_open_win(self._pbuf, false, positions.pwin) -- Prompt window
  self._iwin = api.nvim_open_win(self._ibuf, true, positions.iwin) -- Input window

  api.nvim_buf_set_lines(self._pbuf, 0, -1, false, { self._prompt })

  api.nvim_set_hl(0, 'FzxNormal', { link = 'Normal', default = true })
  api.nvim_set_hl(0, 'FzxPrompt', { link = 'CursorLine', default = true })
  vim.wo[self._dwin].winhighlight = 'Normal:FzxNormal,CursorLine:FzxNormal'
  vim.wo[self._pwin].winhighlight = 'Normal:FzxPrompt,CursorLine:FzxPrompt'
  vim.wo[self._iwin].winhighlight = 'Normal:FzxPrompt,CursorLine:FzxPrompt'

  vim.cmd('startinsert')

  return self
end

return { new = new }
