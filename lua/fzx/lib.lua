local mt = { __index = {} }

-- TODO: luv style error handling. kinda pointless to wrap everything here

function mt.__index:destroy()
  if self.fzx then
    if not self.fzx:is_nil() then
      self.fzx:stop()
    end
    self.fzx = nil
  end
  if self._poll then
    if self._poll:is_active() and not self._poll:is_closing() then
      self._poll:close()
    end
    self._poll = nil
  end
end

function mt.__index:is_nil()
  return not self.fzx or self.fzx:is_nil()
end

function mt.__index:start()
  if not self:is_nil() then
    self.fzx:start()
  end
end

function mt.__index:stop()
  if not self:is_nil() then
    self.fzx:stop()
  end
end

function mt.__index:scan_end()
  if not self:is_nil() then
    self.fzx:scan_end()
  end
end

function mt.__index:scan_feed(data)
  if not self:is_nil() then
    self.fzx:scan_feed(data)
  end
end

function mt.__index:push(...)
  if not self:is_nil() then
    self.fzx:push(...)
  end
end

function mt.__index:commit()
  if not self:is_nil() then
    self.fzx:commit()
  end
end

function mt.__index:set_query(query)
  if not self:is_nil() then
    self.fzx:set_query(query)
  end
end

function mt.__index:get_results(...)
  if not self:is_nil() then
    return self.fzx:get_results(...)
  end
end

local function new(opts)
  assert(type(opts) == 'table', 'opts has to be a table')
  assert(opts.on_update == nil or type(opts.on_update) == 'function',
    'opts.on_update has to be a function')

  local on_update = opts.on_update
  local self = setmetatable({}, mt)
  self.fzx = require('fzxlib').new({
    threads = vim.loop.available_parallelism(),
  })
  self._poll = assert(vim.loop.new_poll(self.fzx:get_fd()))
  self._poll:start('r', function(err)
    if err then
      self._poll:stop()
      -- TODO: kill fzx?
      error(err)
    elseif self.fzx:load_results() then
      on_update()
    end
  end)

  return self
end

return new
