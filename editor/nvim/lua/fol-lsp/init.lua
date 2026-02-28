local M = {}
local definition = require('fol-lsp.definition')
local highlight = require('fol-lsp.highlight')

local function setup_buffer(buf)
  vim.keymap.set('n', 'J', definition.goto_definition, { buffer = buf })
  vim.keymap.set('n', 'gd', definition.goto_definition, { buffer = buf })

  highlight.update(buf)
  vim.api.nvim_create_autocmd({ 'TextChanged', 'TextChangedI' }, {
    buffer = buf,
    callback = function()
      highlight.update(buf)
    end,
  })
end

function M.setup()
  vim.api.nvim_set_hl(0, 'FolDefinedPredicate', { link = 'Underlined' })

  vim.api.nvim_create_autocmd('FileType', {
    pattern = 'fol',
    callback = function(args)
      setup_buffer(args.buf)
    end,
  })

  -- Handle already-open FOL buffers (lazy.nvim loads after FileType fires)
  for _, buf in ipairs(vim.api.nvim_list_bufs()) do
    if vim.api.nvim_buf_is_loaded(buf) and vim.bo[buf].filetype == 'fol' then
      setup_buffer(buf)
    end
  end
end

return M
