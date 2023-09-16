local hl = vim.api.nvim_set_hl
hl(0, 'FzxNormal', { link = 'Normal', default = true })
hl(0, 'FzxCursorLine', {})
hl(0, 'FzxCursor', { link = 'Visual', default = true })
hl(0, 'FzxPrompt', { link = 'CursorLine', default = true })
hl(0, 'FzxMatch', { link = 'Search', default = true })
hl(0, 'FzxNoResults', { link = 'ErrorMsg', default = true })
hl(0, 'FzxInfo', { link = 'Constant', default = true })
hl(0, 'FzxPromptText', { link = 'Type', default = true })
