local uv = vim.loop
local api = vim.api
local fn = vim.fn

local M = {}

local function new_process(fzx, exe, args)
  -- TODO: read stderr, check exit code
  local stdout = uv.new_pipe()
  local proc
  proc = uv.spawn(exe, {
    args = args,
    stdio = { nil, stdout, nil }
  }, function()
    if fzx._fzx then
      fzx._fzx:scan_end()
    end
    proc:close()
  end)

  stdout:read_start(function(err, data)
    assert(not err, err)
    if not fzx._fzx or fzx._fzx:is_nil() then
      stdout:close()
    elseif data then
      fzx._fzx:scan_feed(data)
    else
      fzx._fzx:scan_end()
      stdout:close()
    end
  end)
end

function M.rg(query)
  local fzx = require('fzx').new({
    prompt = 'grep> ',
    on_select = function(text)
      local file, line, col = text:match('^(.+):(%d+):(%d+):')
      if not file then return end
      line, col = tonumber(line), tonumber(col)
      vim.cmd(('edit %s'):format(fn.fnameescape(file)))
      pcall(api.nvim_win_set_cursor, 0, { line, col - 1 })
    end,
  })

  api.nvim_buf_call(fzx._ui.display.buf, function()
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

  new_process(fzx, 'rg', { '--line-number', '--column', query or '' })
end

function M.fd()
  local fzx = require('fzx').new({
    prompt = 'files> ',
    on_select = function(text)
      vim.cmd(('edit %s'):format(fn.fnameescape(text)))
    end,
  })
  -- TODO: args
  new_process(fzx, 'fd', { '-t', 'f', '.', '.' })
end

local function get_helptags()
  local files = api.nvim_get_runtime_file('doc/tags', true)
  local tags = {}
  for _, file in ipairs(files) do
    local f = io.open(file, 'r')
    if f then
      local base = file:match('^(.*)tags$')
      local text = f:read('*a')
      if text then
        for _, line in ipairs(vim.split(text, '\n', { plain = true })) do
          local tag, path, search = line:match('^([^\t]+)\t([^\t]+)\t([^\t]+)$')
          if tag then
            tags[#tags+1] = {
              name = tag,
              path = path,
              full_path = base .. path,
              search = search,
            }
          end
        end
      end
      f:close()
    end
  end
  table.sort(tags, function(a, b) return a.name < b.name end)
  local strings = {}
  for i, tag in ipairs(tags) do
    strings[i] = tag.name
  end
  return strings, tags
end

function M.help()
  local strings = get_helptags()
  local fzx = require('fzx').new({
    prompt = 'help> ',
    on_select = function(text)
      vim.cmd(('help %s'):format(text))
    end,
  })
  fzx._fzx:push(strings)
  fzx._fzx:commit()
end

local function bufname(bufnr)
  local name = api.nvim_buf_get_name(bufnr)
  if #name > 0 then
    return fn.fnamemodify(name, ':~')
  end
  local bt = vim.bo[bufnr].buftype
  if bt == 'quickfix' then
    -- TODO: "[Location List]"
    return '[Quickfix List]'
  elseif bt == 'prompt' then
    return '[Prompt]'
  elseif bt == 'nofile' or bt == 'acwrite' or bt == 'terminal' then
    return '[Scratch]'
  else
    return '[No Name]'
  end
end

function M.buffers()
  local bufs = {}
  local curr = api.nvim_get_current_buf()
  for _, bufnr in ipairs(api.nvim_list_bufs()) do
    if bufnr ~= curr and vim.bo[bufnr].buflisted then
      local name = bufname(bufnr)
      bufs[#bufs+1] = ('%3d  %s'):format(bufnr, name)
    end
  end

  if #bufs == 0 then
    api.nvim_echo({{ 'fzx: No buffers', 'WarningMsg' }}, false, {})
    return
  end

  local fzx = require('fzx').new({
    prompt = 'buffers> ',
    on_select = function(text)
      local bufnr = tonumber(text:match('^%s*(%d+)'))
      api.nvim_set_current_buf(bufnr)
    end,
  })
  fzx._fzx:push(bufs)
  fzx._fzx:commit()
end

return M
