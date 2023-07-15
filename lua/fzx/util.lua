local M = {}

-- clamp a number
function M.clamp(n, lower, upper)
  if n < lower then
    return lower
  elseif n > upper then
    return upper
  else
    return n
  end
end

-- reverse table in place
function M.reverse(t)
  local len = #t
  for i = 0, len / 2 - 1 do
    t[i+1], t[len-i] = t[len-i], t[i+1]
  end
  return t
end

return M
