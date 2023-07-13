local function new_process(fzx, exe, args)
  local stdout = vim.loop.new_pipe()
  local proc
  proc = vim.loop.spawn(exe, {
    args = args,
    stdio = { nil, stdout, nil }
  }, function()
    if fzx._fzx and not fzx._fzx:is_nil() then
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

vim.api.nvim_create_user_command('Frg', function(ctx)
  local fzx = require('fzx').new({
    prompt = 'rg> ',
    on_select = function(text)
      local file, line, col = text:match('^(.+):(%d+):(%d+):')
      if not file then return end
      line, col = tonumber(line), tonumber(col)
      vim.cmd(('edit %s'):format(vim.fn.fnameescape(file)))
      pcall(vim.api.nvim_win_set_cursor, 0, { line, col - 1 })
    end,
  })

  vim.api.nvim_buf_call(fzx._ui._dbuf, function()
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

  new_process(fzx, 'rg', { '--line-number', '--column', ctx.args })
end, { nargs = '*' })

vim.api.nvim_create_user_command('Ffd', function(ctx)
  local fzx = require('fzx').new({
    prompt = 'fd> ',
    on_select = function(text)
      vim.cmd(('edit %s'):format(vim.fn.fnameescape(text)))
    end,
  })
  -- TODO: args
  new_process(fzx, 'fd', { '-t', 'f', '.', '.' })
end, { nargs = '*' })

local function get_helptags()
  local files = vim.api.nvim_get_runtime_file('doc/tags', true)
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

vim.api.nvim_create_user_command('Fhelp', function()
  local strings, meta = get_helptags()
  local fzx = require('fzx').new({
    prompt = 'help> ',
    on_select = function(text)
      vim.cmd(('help %s'):format(text))
    end,
  })
  fzx._fzx:push(strings)
  fzx._fzx:commit()
end, {})

local function bufname(bufnr)
  local name = vim.api.nvim_buf_get_name(bufnr)
  if #name > 0 then
    return name
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

vim.api.nvim_create_user_command('Fbuffers', function()
  local bufs = {}
  for _, bufnr in ipairs(vim.api.nvim_list_bufs()) do
    if vim.bo[bufnr].buflisted then
      local name = bufname(bufnr)
      bufs[#bufs+1] = ('%3d  %s'):format(bufnr, name)
    end
  end

  local fzx = require('fzx').new({
    prompt = 'buffers> ',
    on_select = function(text)
      local bufnr = tonumber(text:match('^%s*(%d+)'))
      vim.api.nvim_set_current_buf(bufnr)
    end,
  })
  fzx._fzx:push(bufs)
  fzx._fzx:commit()
end, {})
