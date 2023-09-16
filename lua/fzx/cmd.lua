local M = {}

local SUBCMDS = {
  'buffers',
  'files',
  'grep',
  'help',
}

local function cmd_name_regex(name)
  assert(type(name) == 'string')
  assert(name:match('^[A-Z][A-Za-z0-9]*$'), 'invalid command name')
  if #name == 1 then
    return vim.regex('^[ :]*' .. name .. '\\s*\\zs.*')
  else
    return vim.regex('^[ :]*' .. name:sub(1, 1) .. '\\%[' .. name:sub(2) .. ']\\s*\\zs.*')
  end
end

local function filter(candidates, arg)
  local res = {}
  for _, subcmd in ipairs(candidates) do
    if subcmd:sub(1, #arg) == arg then
      table.insert(res, subcmd)
    end
  end
  table.sort(res)
  return res
end

function M.complete(name, arg, cmdline, pos)
  cmdline = cmdline:sub(1, pos)
  local start = cmd_name_regex(name):match_str(cmdline)
  cmdline = cmdline:sub(start + 1, -#arg - 1)
  local args = vim.split(cmdline, ' +', { trimempty = true })

  -- TODO: parse options
  if #args == 0 then
    return filter(SUBCMDS, arg)
  end
  return {}
end

function M.command(ctx)
  local args = vim.split(ctx.args, ' +', { trimempty = true })
  if #args == 0 then
    vim.api.nvim_err_writeln('fzx: Expected subcommand')
    return
  end

  local subcmd = filter(SUBCMDS, args[1])[1]
  if not subcmd then
    vim.api.nvim_err_writeln(('fzx: Unknown subcommand: %s'):format(args[1]))
    return
  end

  -- TODO: abstraction
  if subcmd == 'buffers' then
    require('fzx.builtin').buffers()
  elseif subcmd == 'files' then
    require('fzx.builtin').fd()
  elseif subcmd == 'grep' then
    require('fzx.builtin').rg(args[2])
  elseif subcmd == 'help' then
    require('fzx.builtin').help()
  end
end

return M
